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

#include <stdint.h>

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

#if defined(OS_BAREMETAL)
    // Bare-metal/RTOS embedders only (no OS_BAREMETAL build has any of the
    // POSIX/Win32 facilities Escargot otherwise uses for these). See
    // docs/porting/RTOS_PORTING_GUIDE.md for the full contract and a
    // reference implementation.

    // The LOW end of the current task/thread's stack (the address closest
    // to overflow). Used by ThreadLocal::initialize() to compute
    // g_stackLimit. Must NOT be confused with stackTop() below -- they are
    // opposite ends of the same stack, and conflating them once broke
    // script parsing entirely (every script, even a bare "1+2", failed
    // with "too many recursion in script").
    virtual void* stackBase() = 0;

    // The COLD end (HIGH address) of the current task/thread's stack --
    // where a full-descending stack starts from and grows down toward.
    // Used by Heap::initialize() to seed BDWGC's stack-bottom (via
    // GC_set_stackbottom()) before GC_init() runs, so the collector scans
    // the actual live task stack for roots instead of guessing.
    virtual void* stackTop() = 0;

    // Monotonic millisecond tick, used as a Date.now()/clock_gettime()
    // fallback when no RTC is available.
    virtual uint32_t tickMs() = 0;

    // Timezone abbreviation name -- index 0 is the standard-time name,
    // index 1 is the DST name (mirrors the standard libc ::tzname[0]/
    // ::tzname[1] array-of-two-strings convention). Consumed by
    // VMInstance::ensureTzname()'s non-ICU fallback route
    // (src/runtime/VMInstance.cpp) instead of calling ::tzset()/::tzname
    // directly, since bare-metal/RTOS libc's don't reliably provide those.
    // Named tzName(), NOT tzname() -- newlib's <time.h> (and some other
    // libcs) `#define tzname _tzname` for MT-safety, which silently
    // mangles a same-named virtual method/override depending on whether
    // <time.h> happened to be included yet at each declaration site in a
    // given translation unit (verified: this exact collision broke the
    // FreeRTOS reference port's build). Avoid the whole class of bug by
    // simply not colliding with the macro token.
    virtual const char* tzName(int index) = 0;
#endif
};
} // namespace Escargot

#endif
