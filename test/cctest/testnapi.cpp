#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

// Drives the real, unmodified Node-API TCs vendored at
// test/napi-tc/test/js-native-api/*.
// There is no require()/module-loader integration yet, so this dlopen()s each
// compiled addon directly and calls its exported napi_register_module_v1,
// reproducing the same assertions each addon's own test.js makes.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <dlfcn.h>
#include <cstring>

typedef napi_value (*NapiRegisterModuleFn)(napi_env, napi_value);

TEST(Napi, TwoFunctionArguments)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_2_FUNCTION_ARGUMENTS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));

            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();
            ValueRef* addFn = exportsResult->get(state, StringRef::createFromASCII("add"));

            ValueRef* args[2] = { ValueRef::create(3), ValueRef::create(5) };
            return addFn->call(state, ValueRef::createUndefined(), 2, args);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_EQ(result.result->asNumber(), 8);

    dlclose(handle);
}

// captures what a JS-side callback was invoked with, so the C++ test body can
// inspect it afterward; plain file-static globals since NativeFunctionInfo
// only accepts capture-less function pointers (no per-instance user data
// short of the extraData() mechanism napi_create_function itself uses)
static ValueRef* g_callbackArg = nullptr;
static ValueRef* g_callbackThis = nullptr;

static ValueRef* RecordCallbackArgAndThis(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    g_callbackArg = argc > 0 ? argv[0] : ValueRef::createUndefined();
    g_callbackThis = thisValue;
    return ValueRef::createUndefined();
}

TEST(Napi, Callbacks)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_3_CALLBACKS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    g_callbackArg = nullptr;
    g_callbackThis = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            AtomicStringRef* cbName = AtomicStringRef::create(state->context(), "cb", 2);
            FunctionObjectRef* cb = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(cbName, RecordCallbackArgAndThis, 1, true, false));

            // RunCallback(cb) should call cb.call(global, 'hello world')
            ValueRef* runCallback = exportsResult->get(state, StringRef::createFromASCII("RunCallback"));
            ValueRef* runCallbackArgs[1] = { cb };
            runCallback->call(state, ValueRef::createUndefined(), 1, runCallbackArgs);

            return ValueRef::createUndefined();
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_NE(g_callbackArg, nullptr);
    ASSERT_TRUE(g_callbackArg->isString());
    EXPECT_TRUE(g_callbackArg->asString()->equalsWithASCIIString("hello world", strlen("hello world")));

    dlclose(handle);
}

TEST(Napi, CallbackRecv)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_3_CALLBACKS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    g_callbackArg = nullptr;
    g_callbackThis = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            AtomicStringRef* cbName = AtomicStringRef::create(state->context(), "cb", 2);
            FunctionObjectRef* cb = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(cbName, RecordCallbackArgAndThis, 0, true, false));

            ObjectRef* desiredRecv = ObjectRef::create(state);

            // RunCallbackWithRecv(cb, desiredRecv) should call cb.call(desiredRecv)
            ValueRef* runCallbackWithRecv = exportsResult->get(state, StringRef::createFromASCII("RunCallbackWithRecv"));
            ValueRef* args[2] = { cb, desiredRecv };
            runCallbackWithRecv->call(state, ValueRef::createUndefined(), 2, args);

            return desiredRecv;
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_NE(g_callbackThis, nullptr);
    // Escargot's GC (Boehm) never moves objects, so comparing the raw pointers
    // captured by the callback against the desiredRecv returned from the lambda
    // is a valid identity check
    EXPECT_EQ(g_callbackThis, result.result);

    dlclose(handle);
}

TEST(Napi, ObjectFactory)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_4_OBJECT_FACTORY_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ValueRef* factory = FromNapi(returnedExports);

            ValueRef* helloArgs[1] = { StringRef::createFromASCII("hello") };
            ValueRef* obj1 = factory->call(state, ValueRef::createUndefined(), 1, helloArgs);
            ValueRef* worldArgs[1] = { StringRef::createFromASCII("world") };
            ValueRef* obj2 = factory->call(state, ValueRef::createUndefined(), 1, worldArgs);

            ValueRef* msg1 = obj1->asObject()->get(state, StringRef::createFromASCII("msg"));
            ValueRef* msg2 = obj2->asObject()->get(state, StringRef::createFromASCII("msg"));

            bool ok = msg1->isString() && msg2->isString()
                && msg1->asString()->equalsWithASCIIString("hello", strlen("hello"))
                && msg2->asString()->equalsWithASCIIString("world", strlen("world"));
            return ValueRef::create(ok);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());

    dlclose(handle);
}

TEST(Napi, FunctionFactory)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_5_FUNCTION_FACTORY_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ValueRef* factory = FromNapi(returnedExports);

            ValueRef* fn = factory->call(state, ValueRef::createUndefined(), 0, nullptr);
            return fn->call(state, ValueRef::createUndefined(), 0, nullptr);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_TRUE(result.result->isString());
    EXPECT_TRUE(result.result->asString()->equalsWithASCIIString("hello world", strlen("hello world")));

    dlclose(handle);
}

TEST(Napi, ObjectWrap)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_MYOBJECT_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            ValueRef* cons = exportsResult->get(state, StringRef::createFromASCII("MyObject"));

            ValueRef* ctorArgs[1] = { ValueRef::create(9) };
            ValueRef* obj = cons->construct(state, 1, ctorArgs);
            ObjectRef* objRef = obj->asObject();

            bool ok = true;
            ok = ok && objRef->get(state, StringRef::createFromASCII("value"))->asNumber() == 9;

            objRef->set(state, StringRef::createFromASCII("value"), ValueRef::create(10));
            ok = ok && objRef->get(state, StringRef::createFromASCII("value"))->asNumber() == 10;
            ok = ok && objRef->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == 10;

            // valueReadonly has no setter (napi_define_class's `value`
            // descriptor's setter field is null), so this assignment must fail
            bool setSucceeded = objRef->set(state, StringRef::createFromASCII("valueReadonly"), ValueRef::create(14));
            ok = ok && !setSucceeded;
            ok = ok && objRef->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == 10;

            ValueRef* plusOne = objRef->get(state, StringRef::createFromASCII("plusOne"));
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 11;
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 12;
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 13;

            ValueRef* multiply = objRef->get(state, StringRef::createFromASCII("multiply"));

            ValueRef* noArgResult = multiply->call(state, objRef, 0, nullptr);
            ok = ok && noArgResult->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == 13;

            ValueRef* tenArgs[1] = { ValueRef::create(10) };
            ValueRef* tenResult = multiply->call(state, objRef, 1, tenArgs);
            ok = ok && tenResult->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == 130;

            ValueRef* negArgs[1] = { ValueRef::create(-1) };
            ValueRef* newObj = multiply->call(state, objRef, 1, negArgs);
            ok = ok && newObj->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == -13;
            ok = ok && newObj->asObject()->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == -13;
            ok = ok && (newObj != obj);

            return ValueRef::create(ok);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());

    // Flush out the wrapped MyObject instances created above before this test
    // ends. This is not about env safety anymore (env now lives as long as
    // NapiEnv itself, see NapiEnv::env()) - it is about not leaving
    // unreachable-but-uncollected napi_wrap'd garbage sitting in the shared
    // Boehm heap. Left alone, it can get opportunistically finalized at an
    // arbitrary later point (e.g. mid-construction of a *different* test's
    // brand-new VMInstance, which is an unsafe time to run addon code) -
    // this actually crashed FactoryWrap when this cleanup was missing. Same
    // "clear stack + churn + gc x5" pattern as
    // test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    dlclose(handle);
}

TEST(Napi, FactoryWrap)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_7_FACTORY_WRAP_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    ObjectRef* exports = nullptr;

    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule, ObjectRef** exportsOut) -> ValueRef* {
            env->executionState = state;
            ObjectRef* exportsObj = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exportsObj));
            *exportsOut = FromNapi(returnedExports)->asObject();

            ValueRef* finalizeCount = (*exportsOut)->get(state, StringRef::createFromASCII("finalizeCount"));
            EXPECT_EQ(finalizeCount->asNumber(), 0);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), registerModule, &exports);
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    // napi_wrap creates a weak napi_ref (refcount 0, see NapiFunctions.cpp),
    // so nothing artificially roots the wrapped MyObject instance created
    // below - it becomes collectible the moment the JS side drops it.
    for (int round = 0; round < 2; round++) {
        int base = (round == 0) ? 10 : 20;

        // Create one wrapped MyObject and exercise it inside its own
        // Evaluator::execute call, so no local variable in FactoryWrap's own
        // stack frame keeps referencing it once this call returns.
        Evaluator::EvaluatorResult r2 = Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, napi_env env, ObjectRef* exports, int base) -> ValueRef* {
                env->executionState = state;
                ValueRef* createObject = exports->get(state, StringRef::createFromASCII("createObject"));
                ValueRef* args[1] = { ValueRef::create(base) };
                ValueRef* obj = createObject->call(state, ValueRef::createUndefined(), 1, args);
                ObjectRef* objRef = obj->asObject();
                ValueRef* plusOne = objRef->get(state, StringRef::createFromASCII("plusOne"));
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 1);
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 2);
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 3);
                return ValueRef::createUndefined();
            },
            napiEnv->env(), exports, base);
        ASSERT_TRUE(r2.isSuccessful()) << r2.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

        // "clear stack": an unrelated Evaluator::execute call reuses the
        // native stack frame the previous call's locals (obj/objRef/plusOne)
        // sat in, so Boehm's conservative stack scan doesn't keep seeing
        // those stale bit patterns as live roots. Same pattern as
        // test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic tests.
        Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
                return ValueRef::create(100);
            },
            StringRef::createFromUTF8("qwer"));

        for (size_t i = 0; i < 100; i++) {
            PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
        }
        Memory::gc();
        Memory::gc();
        Memory::gc();
        Memory::gc();
        Memory::gc();

        Evaluator::EvaluatorResult r3 = Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, napi_env env, ObjectRef* exports, int expectedCount) -> ValueRef* {
                env->executionState = state;
                ValueRef* finalizeCount = exports->get(state, StringRef::createFromASCII("finalizeCount"));
                EXPECT_EQ(finalizeCount->asNumber(), expectedCount);
                return ValueRef::createUndefined();
            },
            napiEnv->env(), exports, round + 1);
        ASSERT_TRUE(r3.isSuccessful()) << r3.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    }

    dlclose(handle);
}

TEST(Napi, WeakReferenceStaleAfterGC)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    napi_ref ref = nullptr;

    // Create the target object and the weak (refcount 0) napi_ref to it
    // inside their own Evaluator::execute call, so no local variable in this
    // test's own stack frame keeps referencing the object once this call
    // returns (same "own call frame" discipline as Napi.FactoryWrap above).
    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, napi_ref* refOut) -> ValueRef* {
            env->executionState = state;
            ObjectRef* obj = ObjectRef::create(state);
            napi_create_reference(env, ToNapi(obj), 0, refOut);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), &ref);
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    napi_value value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));
    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_NE(value, nullptr);

    // Overwrite the stale pointer bit pattern this test's own stack slot is
    // still holding before collecting, so Boehm's conservative stack scan
    // doesn't keep the object alive through it (the nested-call trick below
    // only clears the *inner* lambda's frame, not this function's own).
    value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));

    // "clear stack" + churn + gc x5, same pattern as Napi.FactoryWrap: nothing
    // else roots the object created above, so this must collect it.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_EQ(value, nullptr);

    napi_delete_reference(napiEnv->env(), ref);
}

static int g_removeWrapFinalizeCount = 0;

static void RemoveWrapFinalizeCallback(node_api_basic_env env, void* data, void* hint)
{
    g_removeWrapFinalizeCount++;
}

TEST(Napi, RemoveWrapSuppressesFinalizer)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_removeWrapFinalizeCount = 0;

    // Wrap an object with a finalizer, then immediately napi_remove_wrap it,
    // inside its own Evaluator::execute call so no local variable in this
    // test's own stack frame keeps referencing the object once this call
    // returns (same "own call frame" discipline as Napi.FactoryWrap above).
    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;
            ObjectRef* obj = ObjectRef::create(state);
            napi_wrap(env, ToNapi(obj), nullptr, RemoveWrapFinalizeCallback, nullptr, nullptr);

            void* removed = nullptr;
            napi_remove_wrap(env, ToNapi(obj), &removed);
            return ValueRef::createUndefined();
        },
        napiEnv->env());
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    // napi_remove_wrap itself must not invoke the finalizer
    EXPECT_EQ(g_removeWrapFinalizeCount, 0);

    // "clear stack" + churn + gc x5, same pattern as Napi.FactoryWrap: nothing
    // else roots the object created above, so this must collect it - and the
    // finalizer must stay suppressed even once that actually happens.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    EXPECT_EQ(g_removeWrapFinalizeCount, 0);
}

TEST(Napi, ReferenceRefUnref)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    napi_ref ref = nullptr;

    // Create the target object and a strong (refcount 1) napi_ref to it
    // inside its own Evaluator::execute call, so no local variable in this
    // test's own stack frame keeps referencing the object once this call
    // returns (same "own call frame" discipline as Napi.FactoryWrap above).
    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, napi_ref* refOut) -> ValueRef* {
            env->executionState = state;
            ObjectRef* obj = ObjectRef::create(state);
            napi_create_reference(env, ToNapi(obj), 1, refOut);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), &ref);
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    uint32_t count = 0;
    ASSERT_EQ(napi_reference_ref(napiEnv->env(), ref, &count), napi_ok);
    EXPECT_EQ(count, 2u);

    ASSERT_EQ(napi_reference_unref(napiEnv->env(), ref, &count), napi_ok);
    EXPECT_EQ(count, 1u);

    // Still strong (count 1): must survive a collection pass. "clear stack" +
    // churn + gc x5, same pattern as Napi.FactoryWrap above.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    napi_value value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));
    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_NE(value, nullptr);

    ASSERT_EQ(napi_reference_unref(napiEnv->env(), ref, &count), napi_ok);
    EXPECT_EQ(count, 0u);

    // Overwrite the stale pointer bit pattern this test's own stack slot is
    // still holding before collecting, so Boehm's conservative stack scan
    // doesn't keep the object alive through it (same reasoning as
    // Napi.WeakReferenceStaleAfterGC above). Now weak (count 0): must
    // actually be collected this time.
    value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_EQ(value, nullptr);

    // Decrementing an already-weak (count 0) ref must error, not underflow.
    EXPECT_EQ(napi_reference_unref(napiEnv->env(), ref, &count), napi_generic_failure);

    // Strengthening a weak ref whose target is already gone must error too.
    EXPECT_EQ(napi_reference_ref(napiEnv->env(), ref, &count), napi_generic_failure);

    napi_delete_reference(napiEnv->env(), ref);
}

static int g_instanceDataFinalizeCount = 0;
static void* g_instanceDataFinalizeSeenData = nullptr;

static void InstanceDataFinalizeCallback(napi_env env, void* data, void* hint)
{
    g_instanceDataFinalizeCount++;
    g_instanceDataFinalizeSeenData = data;
}

TEST(Napi, SetInstanceDataFinalizerRunsOnEnvDestruction)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_instanceDataFinalizeCount = 0;
    g_instanceDataFinalizeSeenData = nullptr;

    int instanceData = 42;
    ASSERT_EQ(napi_set_instance_data(napiEnv->env(), &instanceData, InstanceDataFinalizeCallback, nullptr), napi_ok);

    void* got = nullptr;
    ASSERT_EQ(napi_get_instance_data(napiEnv->env(), &got), napi_ok);
    EXPECT_EQ(got, &instanceData);

    // must not fire before teardown
    EXPECT_EQ(g_instanceDataFinalizeCount, 0);

    // Unlike every other Napi.* test above, this NapiEnv must actually be
    // destroyed (not leaked) - the whole point here is exercising
    // ~NapiEnv()'s teardown hook. Safe because nothing in this test suite
    // calls NapiEnv::globalFinalize() afterward (see NapiEnv.h's ordering
    // requirement: every NapiEnv must be destroyed before that call).
    delete napiEnv;

    EXPECT_EQ(g_instanceDataFinalizeCount, 1);
    EXPECT_EQ(g_instanceDataFinalizeSeenData, &instanceData);
}

TEST(Napi, HandleScopeOpenCloseAndMismatch)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_handle_scope outer = nullptr;
    napi_handle_scope inner = nullptr;
    ASSERT_EQ(napi_open_handle_scope(env, &outer), napi_ok);
    ASSERT_EQ(napi_open_handle_scope(env, &inner), napi_ok);

    // Closing out of LIFO order must fail and must not actually pop anything.
    EXPECT_EQ(napi_close_handle_scope(env, outer), napi_handle_scope_mismatch);

    ASSERT_EQ(napi_close_handle_scope(env, inner), napi_ok);
    ASSERT_EQ(napi_close_handle_scope(env, outer), napi_ok);

    napi_escapable_handle_scope escapable = nullptr;
    ASSERT_EQ(napi_open_escapable_handle_scope(env, &escapable), napi_ok);

    napi_value escapee = ToNapi(ValueRef::create(7));
    napi_value escaped = nullptr;
    ASSERT_EQ(napi_escape_handle(env, escapable, escapee, &escaped), napi_ok);
    EXPECT_EQ(escaped, escapee);

    // A second escape from the same scope must be rejected.
    napi_value escapedAgain = nullptr;
    EXPECT_EQ(napi_escape_handle(env, escapable, escapee, &escapedAgain), napi_escape_called_twice);

    ASSERT_EQ(napi_close_escapable_handle_scope(env, escapable), napi_ok);
}

// thrown from a native FunctionObjectRef passed into NewScopeWithException
// (test_handle_scope.c), standing in for the TC's own `() => { throw new
// RangeError(); }`
static ValueRef* ThrowRangeError(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    state->throwException(RangeErrorObjectRef::create(state, StringRef::createFromASCII("boom")));
    return ValueRef::createUndefined();
}

static int g_markerAfterCallFunction = 0;

TEST(Napi, CallFunctionReportsExceptionAsPendingStatus)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_markerAfterCallFunction = 0;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            AtomicStringRef* throwName = AtomicStringRef::create(state->context(), "throwRangeError", strlen("throwRangeError"));
            FunctionObjectRef* throwFn = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(throwName, ThrowRangeError, 0, true, false));

            napi_value callResult = nullptr;
            napi_status status = napi_call_function(env, ToNapi(ValueRef::createUndefined()), ToNapi(throwFn), 0, nullptr, &callResult);

            // Proves control actually returned here instead of a raw C++
            // exception unwinding straight past napi_call_function's caller.
            g_markerAfterCallFunction++;

            if (status != napi_pending_exception) {
                return ValueRef::create(1);
            }

            bool isPending = false;
            napi_is_exception_pending(env, &isPending);
            if (!isPending) {
                return ValueRef::create(2);
            }

            napi_value exception = nullptr;
            napi_get_and_clear_last_exception(env, &exception);
            if (!FromNapi(exception)->isObject()) {
                return ValueRef::create(3);
            }

            bool stillPending = true;
            napi_is_exception_pending(env, &stillPending);
            if (stillPending) {
                return ValueRef::create(4);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_EQ(g_markerAfterCallFunction, 1);
    EXPECT_TRUE(result.result->isUndefined());
}

TEST(Napi, HandleScope)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_TEST_HANDLE_SCOPE_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            // NewScope: open a handle scope, create an object in it, close it.
            ValueRef* newScope = exportsResult->get(state, StringRef::createFromASCII("NewScope"));
            newScope->call(state, ValueRef::createUndefined(), 0, nullptr);

            // NewScopeEscape: object created inside an escapable scope must
            // still be usable after the scope closes.
            ValueRef* newScopeEscape = exportsResult->get(state, StringRef::createFromASCII("NewScopeEscape"));
            ValueRef* escapeResult = newScopeEscape->call(state, ValueRef::createUndefined(), 0, nullptr);
            if (!escapeResult->isObject()) {
                state->throwException(StringRef::createFromASCII("NewScopeEscape did not return an object"));
            }

            // NewScopeEscapeTwice: the TC's own NODE_API_ASSERT aborts the
            // process if napi_escape_handle doesn't reject the second call,
            // so simply returning here is the pass condition.
            ValueRef* newScopeEscapeTwice = exportsResult->get(state, StringRef::createFromASCII("NewScopeEscapeTwice"));
            newScopeEscapeTwice->call(state, ValueRef::createUndefined(), 0, nullptr);

            // NewScopeWithException: the callback passed in throws a
            // RangeError; the TC's own NODE_API_ASSERT aborts the process if
            // napi_call_function doesn't report napi_pending_exception for
            // it. That RangeError is left pending on `env` on purpose (the
            // TC never clears it), so NapiCallbackTrampoline rethrows it
            // once this native function returns - propagating out as a real
            // JS exception, same as test.js's assert.throws(..., RangeError).
            AtomicStringRef* throwName = AtomicStringRef::create(state->context(), "throwRangeError", strlen("throwRangeError"));
            FunctionObjectRef* throwFn = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(throwName, ThrowRangeError, 0, true, false));
            ValueRef* newScopeWithException = exportsResult->get(state, StringRef::createFromASCII("NewScopeWithException"));
            ValueRef* excArgs[1] = { throwFn };
            newScopeWithException->call(state, ValueRef::createUndefined(), 1, excArgs);

            return ValueRef::createUndefined();
        },
        napiEnv->env(), registerModule);

    ASSERT_FALSE(result.isSuccessful());
    ASSERT_TRUE(result.error.value()->isObject());

    dlclose(handle);
}

#endif // ENABLE_NAPI
