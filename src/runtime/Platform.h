/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#ifndef __EscargotPlatform__
#define __EscargotPlatform__

#include "runtime/SandBox.h"

namespace Escargot {

class ArrayBufferObject;
class Context;
class Job;

class Platform : public gc {
public:
    virtual ~Platform() {}
    // ArrayBuffer
    virtual void* onArrayBufferObjectDataBufferMalloc(Context* whereObjectMade, ArrayBufferObject* obj, size_t sizeInByte) = 0;
    virtual void* onSharedArrayBufferObjectDataBufferMalloc(Context* whereObjectMade, ArrayBufferObject* obj, size_t sizeInByte) = 0;
    virtual void onArrayBufferObjectDataBufferFree(Context* whereObjectMade, ArrayBufferObject* obj, void* buffer) = 0;
    virtual void onSharedArrayBufferObjectDataBufferFree(Context* whereObjectMade, ArrayBufferObject* obj, void* buffer) = 0;

    // Promise
    virtual void didPromiseJobEnqueued(Context* relatedContext, PromiseObject* obj) = 0;

    // Module
    struct LoadModuleResult {
        Optional<Script*> script;
        String* errorMessage;
        int errorCode;
    };
    virtual LoadModuleResult onLoadModule(Context* relatedContext, Script* whereRequestFrom, String* moduleSrc) = 0;
    virtual void didLoadModule(Context* relatedContext, Optional<Script*> whereRequestFrom, Script* loadedModule) = 0;
};
}

#endif
