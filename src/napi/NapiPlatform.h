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

#ifndef __EscargotNapiPlatform__
#define __EscargotNapiPlatform__

#include "api/EscargotPublic.h"

namespace Escargot {
namespace Napi {

// Minimal PlatformRef for the N-API host.
// There is no embedder event loop yet (that is node_api.h territory, deferred
// for later), so job draining is done explicitly by NapiEnv
// instead of being driven by markJSJobEnqueued.
class NapiPlatform : public PlatformRef {
public:
    void markJSJobEnqueued(ContextRef* relatedContext) override;
    void markJSJobFromAnotherThreadExists(ContextRef* relatedContext) override;

    LoadModuleResult onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc, ModuleType type) override;
    void didLoadModule(ContextRef* relatedContext, OptionalRef<ScriptRef> whereRequestFrom, ScriptRef* loadedModule) override;
};

} // namespace Napi
} // namespace Escargot

#endif
#endif // ENABLE_NAPI
