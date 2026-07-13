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
{
}

NapiEnv::~NapiEnv()
{
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
