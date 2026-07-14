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

#include "NapiEnv.h"
#include "NapiPlatform.h"

#include <utility>

namespace Escargot {
namespace Napi {

static NapiPlatform* g_platform = nullptr;

void NapiEnv::globalInit()
{
    if (Globals::isInitialized()) {
        return;
    }
    g_platform = new NapiPlatform();
    Globals::initialize(g_platform);
}

void NapiEnv::globalFinalize()
{
    if (!Globals::isInitialized()) {
        return;
    }
    Globals::finalize();
    delete g_platform;
    g_platform = nullptr;
}

void NapiEnv::threadInit()
{
    Globals::initializeThread();
}

void NapiEnv::threadFinalize()
{
    Globals::finalizeThread();
}

NapiEnv* NapiEnv::create(const char* locale, const char* timezone)
{
    PersistentRefHolder<VMInstanceRef> vmInstance = VMInstanceRef::create(locale, timezone);
    PersistentRefHolder<ContextRef> context = ContextRef::create(vmInstance.get());
    return new NapiEnv(std::move(vmInstance), std::move(context));
}

NapiEnv* NapiEnv::create(VMInstanceRef* sharedVMInstance)
{
    PersistentRefHolder<VMInstanceRef> vmInstance(sharedVMInstance);
    PersistentRefHolder<ContextRef> context = ContextRef::create(sharedVMInstance);
    return new NapiEnv(std::move(vmInstance), std::move(context));
}

NapiEnv::NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context)
    : m_vmInstance(std::move(vmInstance))
    , m_context(std::move(context))
    , m_persistentValueRefMap(PersistentValueRefMap::create())
{
    m_env.napiEnv = this;
}

NapiEnv::~NapiEnv()
{
    // napi_set_instance_data's finalizer runs exactly once, here at
    // environment teardown - this is the only teardown hook this PoC has.
    if (m_env.instanceDataFinalizer != nullptr) {
        napi_finalize finalizer = m_env.instanceDataFinalizer;
        m_env.instanceDataFinalizer = nullptr;
        finalizer(&m_env, m_env.instanceData, m_env.instanceDataFinalizeHint);
    }

    // Best-effort: flush any napi_wrap'd/finalizer-bearing garbage created
    // through this env before its Context/VMInstance actually go away below
    // (member destructors run after this body, in reverse declaration
    // order). Left alone, such garbage can otherwise sit uncollected and get
    // opportunistically finalized far later - even mid-construction of a
    // totally unrelated NapiEnv's VMInstance - which is not a safe time to
    // run arbitrary addon finalizer code (this actually crashed a test once,
    // see napi-notes.md). Same "clear stack + churn + gc x5" pattern used in
    // test/cctest/testnapi.cpp and test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic.
    //
    // Must run before NapiEnv::globalFinalize() (Globals::finalize()) tears
    // down the GC itself; callers are expected to destroy every NapiEnv first.
    ContextRef* ctx = m_context.get();
    Evaluator::execute(ctx, [](ExecutionStateRef* state) -> ValueRef* {
        return ValueRef::create(100);
    });
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
}

void NapiEnv::drainPendingJobs()
{
    VMInstanceRef* instance = m_vmInstance.get();
    while (instance->hasPendingJob()) {
        instance->executePendingJob();
    }
}

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
