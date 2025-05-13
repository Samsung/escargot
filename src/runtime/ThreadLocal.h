/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotThreadLocalRecord__
#define __EscargotThreadLocalRecord__

namespace WTF {
class BumpPointerAllocator;
}

#if defined(ENABLE_WASM)
struct wasm_engine_t;
struct wasm_store_t;
struct WASMContext {
    wasm_engine_t* engine;
    wasm_store_t* store;
    uint64_t lastGCCheckTime;
};
#endif

namespace Escargot {

class ASTAllocator;
class String;

class GCEventListenerSet {
public:
    typedef void (*OnEventListener)(void* data);
    typedef std::vector<std::pair<OnEventListener, void*>> EventListenerVector;

    GCEventListenerSet() {}
    ~GCEventListenerSet()
    {
        reset();
    }

    EventListenerVector* ensureMarkStartListeners();
    EventListenerVector* ensureMarkEndListeners();
    EventListenerVector* ensureReclaimStartListeners();
    EventListenerVector* ensureReclaimEndListeners();

    Optional<EventListenerVector*> markStartListeners() const
    {
        return m_markStartListeners;
    }

    Optional<EventListenerVector*> markEndListeners() const
    {
        return m_markEndListeners;
    }

    Optional<EventListenerVector*> reclaimStartListeners() const
    {
        return m_reclaimStartListeners;
    }

    Optional<EventListenerVector*> reclaimEndListeners() const
    {
        return m_reclaimEndListeners;
    }

    void reset();

private:
    Optional<EventListenerVector*> m_markStartListeners;
    Optional<EventListenerVector*> m_markEndListeners;
    Optional<EventListenerVector*> m_reclaimStartListeners;
    Optional<EventListenerVector*> m_reclaimEndListeners;
};

// ThreadLocal has thread-local values
// ThreadLocal should be created for each thread
// ThreadLocal is a non-GC global object which means that users who want to customize it should manage memory by themselves
class ThreadLocal {
    friend class StackOverflowDisabler;
    static MAY_THREAD_LOCAL bool inited;
#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
    static size_t g_stackLimitTlsOffset;
    static size_t g_emptyStringTlsOffset;
#endif

    // Global data per thread
    static MAY_THREAD_LOCAL size_t g_stackLimit;
    static MAY_THREAD_LOCAL std::mt19937* g_randEngine;
    static MAY_THREAD_LOCAL bf_context_t g_bfContext;
#if defined(ENABLE_WASM)
    static MAY_THREAD_LOCAL WASMContext g_wasmContext;
#endif
    static MAY_THREAD_LOCAL GCEventListenerSet* g_gcEventListenerSet;
    static MAY_THREAD_LOCAL ASTAllocator* g_astAllocator;
    static MAY_THREAD_LOCAL WTF::BumpPointerAllocator* g_bumpPointerAllocator;
    // custom data allocated by user through Platform::allocateThreadLocalCustomData
    static MAY_THREAD_LOCAL void* g_customData;

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
    static ALWAYS_INLINE char* tlsBaseAddress()
    {
#if defined(CPU_X86_64)
        char* fs;
        asm inline("mov %%fs:0, %0"
                   : "=r"(fs));
        return fs;
#elif defined(CPU_X86)
        char* gs;
        asm inline("mov %%gs:0, %0"
                   : "=r"(gs));
        return gs;
#elif defined(CPU_ARM32) || defined(CPU_ARM64)
        return reinterpret_cast<char*>(__builtin_thread_pointer());
#endif
    }

    static ALWAYS_INLINE char* tlsValueAddress(size_t offset)
    {
        return tlsBaseAddress() + offset;
    }

    static ALWAYS_INLINE size_t readTlsValue(size_t offset)
    {
#if defined(CPU_X86_64)
        size_t value;
        asm inline("mov %%fs:(%1), %0"
                   : "=r"(value)
                   : "r"(offset));
        return value;
#elif defined(CPU_X86)
        size_t value;
        asm inline("mov %%gs:(%1), %0"
                   : "=r"(value)
                   : "r"(offset));
        return value;
#elif defined(CPU_ARM32) || defined(CPU_ARM64)
        return *(reinterpret_cast<size_t*>(tlsBaseAddress() + offset));
#endif
    }
#endif

public:
    static void initialize();
    static void finalize();
    static bool isInited()
    {
        return inited;
    }

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
    static ALWAYS_INLINE String* emptyString()
    {
        return reinterpret_cast<String*>(readTlsValue(g_emptyStringTlsOffset));
    }
#endif

    // Global data getter
    static ALWAYS_INLINE size_t stackLimit()
    {
        ASSERT(inited);
#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
        ASSERT(g_stackLimit == readTlsValue(g_stackLimitTlsOffset));
        return readTlsValue(g_stackLimitTlsOffset);
#else
        return g_stackLimit;
#endif
    }

    // Use this limit if you want to use more stack
    static size_t extendedStackLimit()
    {
#ifdef STACK_GROWS_DOWN
        return stackLimit() + STACK_FREESPACE_FROM_LIMIT / 2;
#else
        return stackLimit() - STACK_FREESPACE_FROM_LIMIT / 2;
#endif
    }

    static std::mt19937& randEngine()
    {
        ASSERT(inited && !!g_randEngine);
        return *g_randEngine;
    }

    static bf_context_t* bfContext()
    {
        ASSERT(inited && !!g_bfContext.realloc_func);
        return &g_bfContext;
    }

#if defined(ENABLE_WASM)
    static wasm_store_t* wasmStore()
    {
        ASSERT(inited && !!g_wasmContext.store);
        return g_wasmContext.store;
    }
#endif

    static GCEventListenerSet& gcEventListenerSet()
    {
        ASSERT(inited && !!g_gcEventListenerSet);
        return *g_gcEventListenerSet;
    }

    static ASTAllocator* astAllocator()
    {
        ASSERT(inited && !!g_astAllocator);
        return g_astAllocator;
    }

    static WTF::BumpPointerAllocator* bumpPointerAllocator()
    {
        ASSERT(inited && !!g_bumpPointerAllocator);
        return g_bumpPointerAllocator;
    }

    static void* customData()
    {
        ASSERT(inited && !!g_customData);
        return g_customData;
    }
};

} // namespace Escargot

#endif
