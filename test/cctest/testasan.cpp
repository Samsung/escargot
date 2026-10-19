/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#include "Escargot.h"

#ifdef ASAN_ENABLED
#include "api/EscargotPublic.h"
#include "parser/Script.h"
#include "runtime/Value.h"
#include "gtest/gtest.h"

using namespace Escargot;

extern PersistentRefHolder<VMInstanceRef> g_instance;

extern "C" __attribute__((visibility("default"))) ATTRIBUTE_NO_SANITIZE_ADDRESS const char* __asan_default_options()
{
    // Match the CLI: BDWGC scans real stacks, not ASAN's fake stacks.
    return "detect_stack_use_after_return=0";
}

namespace {
ValueRef* gcDisabled(ExecutionStateRef*, ValueRef*, size_t, ValueRef**, bool)
{
    return ValueRef::create(!!GC_is_disabled());
}

ValueRef* collect(ExecutionStateRef*, ValueRef*, size_t, ValueRef**, bool)
{
    Memory::gc();
    return ValueRef::createUndefined();
}

ValueRef* nestedNativeCatch(ExecutionStateRef* state, ValueRef*, size_t, ValueRef**, bool)
{
    bool balanced = !GC_is_disabled();
    try {
        state->throwException(ValueRef::create(1));
    } catch (const Value& outer) {
        balanced &= GC_is_disabled() && outer.asInt32() == 1;
        try {
            state->throwException(ValueRef::create(2));
        } catch (const Value& inner) {
            balanced &= GC_is_disabled() && inner.asInt32() == 2;
        }
        // The inner exception must not release the outer exception's pause.
        balanced &= !!GC_is_disabled();
    }
    balanced &= !GC_is_disabled();
    return ValueRef::create(balanced);
}

class AsanExceptionGC : public testing::Test {
protected:
    using NativeCallback = ValueRef* (*)(ExecutionStateRef*, ValueRef*, size_t, ValueRef**, bool);

    static void install(ExecutionStateRef* state, const char* name, NativeCallback callback)
    {
        auto* context = state->context();
        FunctionObjectRef::NativeFunctionInfo info(AtomicStringRef::create(context, name, strlen(name)), callback, 0, true, false);
        auto* function = FunctionObjectRef::create(state, info);
        context->globalObject()->defineDataProperty(state, StringRef::createFromASCII(name, strlen(name)), function, true, true, true);
    }

    void SetUp() override
    {
        ASSERT_FALSE(GC_is_disabled());
        m_instance = g_instance.get();
        m_context = ContextRef::create(m_instance.get());
        auto result = Evaluator::execute(m_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
            install(state, "gcDisabled", gcDisabled);
            install(state, "collect", collect);
            install(state, "nestedNativeCatch", nestedNativeCatch);
            return ValueRef::createUndefined();
        });
        ASSERT_TRUE(result.isSuccessful());
    }

    void TearDown() override
    {
        EXPECT_FALSE(GC_is_disabled());
        // Restore the collector after a regression so other tests can run.
        for (size_t i = 0; i < 32 && GC_is_disabled(); ++i) {
            GC_enable();
        }
        m_context.release();
        m_instance.release();
    }

    void run(const char* source)
    {
        auto parsed = m_context->scriptParser()->initializeScript(StringRef::createFromASCII(source, strlen(source)), StringRef::createFromASCII("asan-exception-gc.js"), false);
        ASSERT_TRUE(parsed.script.hasValue());
        auto result = Evaluator::execute(m_context.get(), [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* { return script->execute(state); }, parsed.script.get());
        EXPECT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(m_context.get())->toStdUTF8String();
    }

    PersistentRefHolder<VMInstanceRef> m_instance;
    PersistentRefHolder<ContextRef> m_context;
};

TEST_F(AsanExceptionGC, JavaScriptCatchResumesCollection)
{
    run(R"JS(
        for (var i = 0; i < 20; ++i) {
            try {
                throw { index: i, payload: new Array(100).fill(i) };
            } catch (error) {
                if (gcDisabled()) throw new Error('GC disabled in JS catch');
                collect();
                if (error.index !== i || error.payload[99] !== i)
                    throw new Error('caught value was not rooted');
            }
        }
        if (gcDisabled()) throw new Error('GC disabled after JS catch');
    )JS");
}

TEST_F(AsanExceptionGC, FinallyAndRethrowResumeCollection)
{
    run(R"JS(
        var marker = { payload: 42 };
        var finalized = 0;
        try {
            try {
                throw marker;
            } catch (error) {
                if (gcDisabled()) throw new Error('GC disabled before rethrow');
                collect();
                throw error;
            } finally {
                if (gcDisabled()) throw new Error('GC disabled in finally');
                collect();
                ++finalized;
            }
        } catch (error) {
            if (gcDisabled()) throw new Error('GC disabled in outer catch');
            collect();
            if (error !== marker || error.payload !== 42 || finalized !== 1)
                throw new Error('rethrow changed the exception');
        }
    )JS");
}

TEST_F(AsanExceptionGC, BuiltinCatchesResumeCollection)
{
    run(R"JS(
        Promise.resolve({ get then() { throw new Error('then getter'); } }).catch(function () {});
        if (gcDisabled()) throw new Error('GC disabled after then getter');
        new Promise(function () { throw new Error('executor'); }).catch(function () {});
        if (gcDisabled()) throw new Error('GC disabled after Promise executor');
        collect();
    )JS");
    while (m_instance->hasPendingJob()) {
        auto result = m_instance->executePendingJob();
        EXPECT_TRUE(result.isSuccessful());
        EXPECT_FALSE(GC_is_disabled());
    }
}

TEST_F(AsanExceptionGC, GeneratorAndIteratorRethrows)
{
    run(R"JS(
        var marker = { payload: 42 };
        function* generator() { throw marker; }
        try { generator().next(); } catch (error) {
            if (gcDisabled()) throw new Error('GC disabled after generator rethrow');
            collect();
            if (error !== marker) throw new Error('generator changed the exception');
        }
        var iterator = {
            next() { return { done: false, get value() { throw marker; } }; },
            [Symbol.iterator]() { return this; }
        };
        try { Iterator.from(iterator).toArray(); } catch (error) {
            if (gcDisabled()) throw new Error('GC disabled after iterator rethrow');
            collect();
            if (error !== marker) throw new Error('iterator changed the exception');
        }
    )JS");
}

TEST_F(AsanExceptionGC, CatchResumeRestoresCollection)
{
    run(R"JS(
        var marker = { payload: 42 };
        function* generator() {
            try { throw marker; } catch (error) {
                if (gcDisabled()) throw new Error('GC disabled before yield');
                yield error;
                if (gcDisabled()) throw new Error('GC disabled after yield');
                collect();
                throw error;
            } finally {
                if (gcDisabled()) throw new Error('GC disabled in resumed finally');
                collect();
            }
        }
        var iterator = generator();
        if (iterator.next().value !== marker) throw new Error('yield lost the exception');
        if (gcDisabled()) throw new Error('GC disabled after suspended catch');
        try { iterator.next(); } catch (error) {
            if (gcDisabled()) throw new Error('GC disabled after resumed catch');
            if (error !== marker) throw new Error('resumed catch changed the exception');
        }
    )JS");
}

TEST_F(AsanExceptionGC, NestedNativeCatchesPreserveOwnership)
{
    run("if (!nestedNativeCatch()) throw new Error('unbalanced nested native catches'); collect();");
}

TEST_F(AsanExceptionGC, ExternalDisableIsPreserved)
{
    GC_disable();
    run("try { throw 1; } catch (error) { if (!gcDisabled()) throw new Error('external GC pause released'); }");
    EXPECT_TRUE(GC_is_disabled());
    GC_enable();
    EXPECT_FALSE(GC_is_disabled());
}

TEST_F(AsanExceptionGC, UncaughtExceptionRestoresCollection)
{
    for (size_t i = 0; i < 20; ++i) {
        auto result = Evaluator::execute(m_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
            state->throwException(ValueRef::create(42));
            return ValueRef::createUndefined();
        });
        EXPECT_FALSE(result.isSuccessful());
        EXPECT_FALSE(GC_is_disabled());
    }
}

TEST_F(AsanExceptionGC, SavedModuleExceptionOwnsPause)
{
    auto parsed = m_context->scriptParser()->initializeScript(StringRef::createFromASCII("throw 42;"), StringRef::createFromASCII("asan-exception-gc.mjs"), true);
    ASSERT_TRUE(parsed.script.hasValue());
    auto result = Evaluator::execute(m_context.get(), [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
        // This in-memory fixture does not need the platform's filesystem cache.
        reinterpret_cast<Script*>(script)->moduleData()->m_didCallLoadedCallback = true;
        script->moduleInstantiate(state);
        try {
            script->moduleEvaluate(state);
        } catch (const Value& error) {
            return ValueRef::create(GC_is_disabled() && error.isInt32() && error.asInt32() == 42);
        }
        return ValueRef::create(false); }, parsed.script.get());
    ASSERT_TRUE(result.isSuccessful());
    EXPECT_TRUE(result.result->asBoolean());
    EXPECT_FALSE(GC_is_disabled());
}
} // namespace
#endif // ASAN_ENABLED
