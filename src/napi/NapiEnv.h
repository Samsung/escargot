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

namespace Escargot {
namespace Napi {

class NapiPlatform;

// Owns the VMInstanceRef + ContextRef pair that a napi_env will be backed by.
// This is scaffolding only: napi_env is not defined here yet (that needs the
// vendored node-api type headers); N-API function implementations will later
// wrap a NapiEnv* behind the opaque `napi_env` pointer.
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

    // runs every job queued on this env's VMInstance (e.g. resolved Promise reactions)
    void drainPendingJobs();

private:
    NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context);

    PersistentRefHolder<VMInstanceRef> m_vmInstance;
    PersistentRefHolder<ContextRef> m_context;
};

} // namespace Napi
} // namespace Escargot

#endif
#endif // ENABLE_NAPI
