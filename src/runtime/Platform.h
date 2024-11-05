/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotPlatform__
#define __EscargotPlatform__

namespace Escargot {

class Context;
class Script;
class String;
class PromiseObject;

class Platform {
public:
    virtual ~Platform() {}
    // ArrayBuffer
    virtual void* onMallocArrayBufferObjectDataBuffer(size_t sizeInByte) = 0;
    virtual void onFreeArrayBufferObjectDataBuffer(void* buffer, size_t sizeInByte, void* deleterData) = 0;
    virtual void* onReallocArrayBufferObjectDataBuffer(void* oldBuffer, size_t oldSizeInByte, size_t newSizeInByte) = 0;

    // Promise
    virtual void markJSJobEnqueued(Context* relatedContext) = 0;

    // may called from another thread. ex) notify or timed out of Atomics.waitAsync
    virtual void markJSJobFromAnotherThreadExists(Context* relatedContext) = 0;

    // Module
    enum ModuleType {
        ModuleES,
        ModuleJSON,
    };
    struct LoadModuleResult {
        Optional<Script*> script;
        String* errorMessage;
        int errorCode;
    };
    virtual LoadModuleResult onLoadModule(Context* relatedContext, Script* whereRequestFrom, String* moduleSrc, ModuleType type) = 0;
    virtual void didLoadModule(Context* relatedContext, Optional<Script*> whereRequestFrom, Script* loadedModule) = 0;
    virtual void hostImportModuleDynamically(Context* relatedContext, Script* referrer, String* src, ModuleType type, PromiseObject* promise) = 0;

    virtual bool canBlockExecution(Context* relatedContext) = 0;

    // ThreadLocal custom data (g_customData)
    virtual void* allocateThreadLocalCustomData() = 0;
    virtual void deallocateThreadLocalCustomData() = 0;

#ifdef ENABLE_CUSTOM_LOGGING
    // customized logging
    virtual void customInfoLogger(const char* format, va_list arg) = 0;
    virtual void customErrorLogger(const char* format, va_list arg) = 0;
#endif
};
} // namespace Escargot

#endif
