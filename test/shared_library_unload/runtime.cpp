// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "EscargotPublic.h"

using namespace Escargot;

class UnloadTestPlatform : public PlatformRef {
public:
    void markJSJobEnqueued(ContextRef*) override {}
    void markJSJobFromAnotherThreadExists(ContextRef*) override {}
    LoadModuleResult onLoadModule(ContextRef*, ScriptRef*, StringRef*, ModuleType) override
    {
        return LoadModuleResult(ErrorObjectRef::Code::TypeError, StringRef::createFromASCII("Modules are unused"));
    }
    void didLoadModule(ContextRef*, OptionalRef<ScriptRef>, ScriptRef*) override {}
};

extern "C" __attribute__((visibility("default"))) void unloadTestInitialize(unsigned option)
{
    Globals::initialize(new UnloadTestPlatform(), static_cast<Globals::InitializeOption>(option));
}

extern "C" __attribute__((visibility("default"))) bool unloadTestExercise()
{
    PersistentRefHolder<VMInstanceRef> vm = VMInstanceRef::create();
    PersistentRefHolder<ContextRef> context = ContextRef::create(vm.get());
    auto parsed = context->scriptParser()->initializeScript(StringRef::createFromASCII(
                                                                "(() => { const a = Array.from({length: 4096}, (_, i) => ({x: i}));"
                                                                "const m = new Map(a.map(v => [v, v.x]));"
                                                                "return m.get(a[42]) + Number(1n << 5n) - 32; })()"),
                                                            StringRef::createFromASCII("unload-test.js"), false);
    if (!parsed.script) {
        return false;
    }
    Memory::gc();
    auto result = Evaluator::execute(context.get(), [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* { return script->execute(state); }, parsed.script.get());
    Memory::gc();
    return result.isSuccessful() && result.result->isNumber() && result.result->asNumber() == 42;
}

extern "C" __attribute__((visibility("default"))) void unloadTestFinalize()
{
    Globals::finalize();
}

extern "C" __attribute__((visibility("default"))) void unloadTestDestroyRuntime()
{
    Globals::destroyRuntimeForSharedLibraryUnload();
}

extern "C" __attribute__((visibility("default"))) bool unloadTestIsInitialized()
{
    return Globals::isInitialized();
}

extern "C" __attribute__((visibility("default"))) bool unloadTestSupportsThreading()
{
    return Globals::supportsThreading();
}

extern "C" __attribute__((visibility("default"))) void unloadTestInitializeThread()
{
    Globals::initializeThread();
}

extern "C" __attribute__((visibility("default"))) void unloadTestFinalizeThread()
{
    Globals::finalizeThread();
}

extern "C" __attribute__((visibility("default"))) void* unloadTestRuntimeAddress()
{
    return reinterpret_cast<void*>(&Globals::destroyRuntimeForSharedLibraryUnload);
}

extern "C" __attribute__((visibility("default"))) unsigned unloadTestSwapGlobal(unsigned value)
{
    static unsigned global;
    unsigned previous = global;
    global = value;
    return previous;
}

extern "C" __attribute__((visibility("default"))) unsigned unloadTestSwapTLS(unsigned value)
{
    static thread_local unsigned tls;
    unsigned previous = tls;
    tls = value;
    return previous;
}
