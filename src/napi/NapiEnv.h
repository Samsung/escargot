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

#ifndef __EscargotNapiEnv__
#define __EscargotNapiEnv__

#include "api/EscargotPublic.h"

#include <node_api.h>

#include <string>

namespace Escargot {
namespace Napi {

class NapiPlatform;
class NapiEnv;

// the real napi_env__ definition (only forward-declared by node_api.h).
// Owned directly by NapiEnv (see NapiEnv::m_env below) instead of being
// stack-allocated per call/per test: a napi_callback (e.g. a napi_wrap
// finalizer) can run later than the call that created it, so the env it
// captures must stay valid for as long as NapiEnv itself does, not just for
// one Evaluator::execute invocation.
struct EnvData {
    NapiEnv* napiEnv = nullptr;
    ExecutionStateRef* executionState = nullptr; // only valid for the duration of the current call
    OptionalRef<ValueRef> pendingException;
    std::string lastErrorMessage;
    napi_extended_error_info lastErrorInfo;

    // napi_set_instance_data/napi_get_instance_data; the finalizer is not
    // invoked anywhere yet (no environment-teardown hook exists in this PoC)
    void* instanceData = nullptr;
    napi_finalize instanceDataFinalizer = nullptr;
    void* instanceDataFinalizeHint = nullptr;

    ContextRef* context(); // defined below, out-of-line (needs NapiEnv complete)
};

} // namespace Napi
} // namespace Escargot

// the opaque type node_api.h forward-declares; napi_env is `EnvData*` in disguise
struct napi_env__ : public Escargot::Napi::EnvData {
};

namespace Escargot {
namespace Napi {

// Owns the VMInstanceRef + ContextRef pair (and the napi_env__ itself) that a
// napi_env is backed by.
class NapiEnv {
public:
    // process-wide setup/teardown; call once before creating any NapiEnv, and once at shutdown
    static void globalInit();
    static void globalFinalize();

    // per-thread setup/teardown; call once per thread that will create/use a NapiEnv
    static void threadInit();
    static void threadFinalize();

    // creates a fresh, independent VMInstance + Context
    static NapiEnv* create(const char* locale = nullptr, const char* timezone = nullptr);
    // creates a Context on top of a VMInstance shared with other NapiEnv instances
    static NapiEnv* create(VMInstanceRef* sharedVMInstance);

    ~NapiEnv();

    VMInstanceRef* vmInstance()
    {
        return m_vmInstance.get();
    }

    ContextRef* context()
    {
        return m_context.get();
    }

    // the napi_env to pass across the N-API boundary; valid for as long as
    // this NapiEnv is. Callers must still set env()->executionState before
    // making napi_* calls, since that part is only valid for the duration of
    // the current Evaluator::execute call.
    napi_env env()
    {
        return &m_env;
    }

    // runs every job queued on this env's VMInstance (e.g. resolved Promise reactions)
    void drainPendingJobs();

    // backs napi_ref: PersistentValueRefMap::add()/remove() are the GC-root
    // primitives napi_create_reference/napi_reference_ref/unref/delete_reference
    // build on for strong (refcount > 0) references
    PersistentValueRefMap* persistentValueRefMap()
    {
        return m_persistentValueRefMap.get();
    }

private:
    NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context);

    PersistentRefHolder<VMInstanceRef> m_vmInstance;
    PersistentRefHolder<ContextRef> m_context;
    PersistentRefHolder<PersistentValueRefMap> m_persistentValueRefMap;
    napi_env__ m_env;
};

} // namespace Napi
} // namespace Escargot

inline Escargot::ContextRef* Escargot::Napi::EnvData::context()
{
    return napiEnv->context();
}

#endif
#endif // ENABLE_NAPI
