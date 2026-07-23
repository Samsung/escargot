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

#include "NapiPlatform.h"

namespace Escargot {
namespace Napi {

void NapiPlatform::markJSJobEnqueued(ContextRef* relatedContext)
{
    // no embedder event loop yet; NapiEnv::drainPendingJobs() is called
    // explicitly by callers instead of being driven from here
}

void NapiPlatform::markJSJobFromAnotherThreadExists(ContextRef* relatedContext)
{
}

PlatformRef::LoadModuleResult NapiPlatform::onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc, ModuleType type)
{
    return PlatformRef::LoadModuleResult(ErrorObjectRef::Code::TypeError, StringRef::createFromASCII("module loading is not supported yet"));
}

void NapiPlatform::didLoadModule(ContextRef* relatedContext, OptionalRef<ScriptRef> whereRequestFrom, ScriptRef* loadedModule)
{
}

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
