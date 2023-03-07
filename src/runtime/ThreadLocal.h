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
    static MAY_THREAD_LOCAL bool inited;

    // Global data per thread
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

public:
    static void initialize();
    static void finalize();
    static bool isInited()
    {
        return inited;
    }

    // Global data getter
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
    static void wasmGC(uint64_t lastCheckTime);

    static wasm_store_t* wasmStore()
    {
        ASSERT(inited && !!g_wasmContext.store);
        return g_wasmContext.store;
    }

    static uint64_t wasmLastGCCheckTime()
    {
        ASSERT(inited && !!g_wasmContext.store);
        return g_wasmContext.lastGCCheckTime;
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
