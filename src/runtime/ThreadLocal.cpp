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

#include "Escargot.h"
#include "runtime/ThreadLocal.h"
#include "heap/Heap.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "parser/ASTAllocator.h"
#include "BumpPointerAllocator.h"
#if defined(ENABLE_WASM)
#include "wasm.h"
#endif

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <processthreadsapi.h>
#ifndef _WIN32_WINNT
#define 0x0602
#endif
#else
#include <pthread.h>
#endif

#if defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
#include <unistd.h> // getpagesize
#endif

// we needs to define new macro here since wtfbridge redefine original macro
#define ESCARGOT_RELEASE_ASSERT(assertion)                                         \
    do {                                                                           \
        if (!(assertion)) {                                                        \
            ESCARGOT_LOG_ERROR("RELEASE_ASSERT at %s (%d)\n", __FILE__, __LINE__); \
            abort();                                                               \
        }                                                                          \
    } while (0);

namespace Escargot {

MAY_THREAD_LOCAL bool ThreadLocal::inited;

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
size_t ThreadLocal::g_stackLimitTlsOffset;

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
size_t ThreadLocal::g_emptyStringTlsOffset;
#endif

#elif defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
int ThreadLocal::g_stackLimitKeyOffset;
pthread_key_t ThreadLocal::g_stackLimitKey;
#endif

#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
static ASCIIStringFromExternalMemory g_emptyString("");
#endif

MAY_THREAD_LOCAL size_t ThreadLocal::g_stackLimit;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
MAY_THREAD_LOCAL String* ThreadLocal::g_emptyStringInstance;
#else
String* ThreadLocal::g_emptyStringInstance;
#endif

MAY_THREAD_LOCAL std::mt19937* ThreadLocal::g_randEngine;
MAY_THREAD_LOCAL bf_context_t ThreadLocal::g_bfContext;
#if defined(ENABLE_WASM)
MAY_THREAD_LOCAL WASMContext ThreadLocal::g_wasmContext;
#endif
MAY_THREAD_LOCAL GCEventListenerSet* ThreadLocal::g_gcEventListenerSet;
MAY_THREAD_LOCAL ASTAllocator* ThreadLocal::g_astAllocator;
MAY_THREAD_LOCAL WTF::BumpPointerAllocator* ThreadLocal::g_bumpPointerAllocator;
#if defined(ENABLE_TCO)
MAY_THREAD_LOCAL Value* ThreadLocal::g_tcoBuffer;
#endif
MAY_THREAD_LOCAL void* ThreadLocal::g_customData;

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureMarkStartListeners()
{
    if (!m_markStartListeners) {
        m_markStartListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_markStartListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureMarkEndListeners()
{
    if (!m_markEndListeners) {
        m_markEndListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_markEndListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureReclaimStartListeners()
{
    if (!m_reclaimStartListeners) {
        m_reclaimStartListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_reclaimStartListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureReclaimEndListeners()
{
    if (!m_reclaimEndListeners) {
        m_reclaimEndListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_reclaimEndListeners.value();
}

void GCEventListenerSet::reset()
{
    if (m_markStartListeners) {
        delete m_markStartListeners.value();
        m_markStartListeners.reset();
    }
    if (m_markEndListeners) {
        delete m_markEndListeners.value();
        m_markEndListeners.reset();
    }
    if (m_reclaimStartListeners) {
        delete m_reclaimStartListeners.value();
        m_reclaimStartListeners.reset();
    }
    if (m_reclaimEndListeners) {
        delete m_reclaimEndListeners.value();
        m_reclaimEndListeners.reset();
    }
}

static void genericGCEventListener(GC_EventType evtType)
{
    GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
    Optional<GCEventListenerSet::EventListenerVector*> listeners;

    switch (evtType) {
    case GC_EVENT_MARK_START:
        listeners = list.markStartListeners();
        break;
    case GC_EVENT_MARK_END:
        listeners = list.markEndListeners();
        break;
    case GC_EVENT_RECLAIM_START:
        listeners = list.reclaimStartListeners();
        break;
    case GC_EVENT_RECLAIM_END:
        listeners = list.reclaimEndListeners();
        break;
    default:
        break;
    }

    if (listeners) {
        for (size_t i = 0; i < listeners->size(); i++) {
            listeners->at(i).first(listeners->at(i).second);
        }
    }
}

#if defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
Optional<size_t*> checkPthreadKey(pthread_key_t key, char* tlsBase)
{
#if defined(ESCARGOT_32)
    pthread_setspecific(key, (void*)(0xbeefdead));
#else
    pthread_setspecific(key, (void*)(0xbeefdeaddeadbeefull));
#endif
    size_t* ptr = reinterpret_cast<size_t*>(tlsBase);
    size_t* tcbMayEnd = reinterpret_cast<size_t*>(tlsBase + getpagesize());

    while (ptr < tcbMayEnd) {
#if defined(ESCARGOT_32)
        if (*ptr == 0xbeefdead) {
#else
        if (*ptr == 0xbeefdeaddeadbeefull) {
#endif
            pthread_setspecific(key, nullptr);
            return ptr;
        }
        ptr++;
    }
    pthread_setspecific(key, nullptr);
    return nullptr;
}
#endif

void ThreadLocal::initialize()
{
    // initialize should be invoked only once in each thread
    ESCARGOT_RELEASE_ASSERT(!inited);

    // Heap is initialized for each thread
    Heap::initialize();

    if (!ThreadLocal::g_emptyStringInstance) {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        String* emptyStr = new (NoGC) ASCIIStringFromExternalMemory("");
#else
        String* emptyStr = &g_emptyString;
#endif
        // mark empty string as AtomicString source
        // because empty string is the default string value of empty AtomicString
        emptyStr->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)emptyStr;
        ASSERT(emptyStr->isAtomicStringSource());
        ThreadLocal::g_emptyStringInstance = emptyStr;
    }

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
    auto tlsBase = tlsBaseAddress();
    if (!g_stackLimitTlsOffset) {
        g_stackLimitTlsOffset = reinterpret_cast<char*>(&g_stackLimit) - tlsBase;
    } else {
        // runtime check
        size_t newDistance = reinterpret_cast<char*>(&g_stackLimit) - tlsBase;
        ESCARGOT_RELEASE_ASSERT(newDistance == g_stackLimitTlsOffset);
    }

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (!g_emptyStringTlsOffset) {
        g_emptyStringTlsOffset = reinterpret_cast<char*>(&g_emptyStringInstance) - tlsBase;
    } else {
        // runtime check
        size_t newDistance = reinterpret_cast<char*>(&g_emptyStringInstance) - tlsBase;
        ESCARGOT_RELEASE_ASSERT(newDistance == g_emptyStringTlsOffset);
    }
#endif

#elif defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
    int keyCreateReturn;
    auto baseAddr = tlsBaseAddress();
    if (!g_stackLimitKeyOffset) {
        keyCreateReturn = pthread_key_create(&g_stackLimitKey, nullptr);
        ESCARGOT_RELEASE_ASSERT(keyCreateReturn == 0);
        auto ptr = checkPthreadKey(g_stackLimitKey, baseAddr);
        ESCARGOT_RELEASE_ASSERT(ptr);

        g_stackLimitKeyOffset = reinterpret_cast<size_t>(ptr.value()) - reinterpret_cast<size_t>(baseAddr);
        ESCARGOT_RELEASE_ASSERT(g_stackLimitKeyOffset < getpagesize());
    }
    size_t** ptr = reinterpret_cast<size_t**>(tlsBaseAddress() + g_stackLimitKeyOffset);
    *ptr = &g_stackLimit;
#endif

    // g_stackLimit
#if defined(OS_WINDOWS)
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    void* stackStartAddress = reinterpret_cast<void*>(low);
    void* stackEndAddress = reinterpret_cast<void*>(high);
    size_t stackSize = reinterpret_cast<size_t>(stackEndAddress) - reinterpret_cast<size_t>(stackStartAddress);
#else
    void* stackStartAddress;
    void* stackEndAddress;
    size_t stackSize;
#if defined(OS_DARWIN)
    stackStartAddress = pthread_get_stackaddr_np(pthread_self());
    stackSize = pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &stackStartAddress, &stackSize);
    pthread_attr_destroy(&attr);
#endif

    stackSize = std::min(stackSize, (size_t)STACK_USAGE_LIMIT);
#ifdef STACK_GROWS_DOWN
    stackEndAddress = (char*)stackStartAddress - stackSize;
#if defined(OS_DARWIN)
    std::swap(stackStartAddress, stackEndAddress);
#endif
#else
    stackEndAddress = (char*)stackStartAddress + stackSize;
#endif
#endif

#ifdef STACK_GROWS_DOWN
    UNUSED_VARIABLE(stackEndAddress);
    g_stackLimit = reinterpret_cast<size_t>(stackStartAddress) + STACK_FREESPACE_FROM_LIMIT;
#else
    UNUSED_VARIABLE(stackStartAddress);
    g_stackLimit = reinterpret_cast<size_t>(stackEndAddress) - STACK_FREESPACE_FROM_LIMIT;
#endif

    // g_randEngine
    g_randEngine = new std::mt19937(static_cast<unsigned int>(time(NULL)));

    // g_bfContext
    bf_context_init(&g_bfContext, [](void* opaque, void* ptr, size_t size) -> void* { return realloc(ptr, size); }, nullptr);

#if defined(ENABLE_WASM)
    // g_wasmContext
    g_wasmContext.engine = wasm_engine_new();
    g_wasmContext.store = wasm_store_new(g_wasmContext.engine);
    g_wasmContext.lastGCCheckTime = 0;
#endif

    // g_gcEventListenerSet
    g_gcEventListenerSet = new GCEventListenerSet();
    // in addition, register genericGCEventListener here too
    GC_set_on_collection_event(genericGCEventListener);

    // g_astAllocator
    g_astAllocator = new ASTAllocator();

    // g_bumpPointerAllocator
    g_bumpPointerAllocator = new WTF::BumpPointerAllocator();

#if defined(ENABLE_TCO)
    // g_tcoBuffer
    g_tcoBuffer = reinterpret_cast<Value*>(GC_MALLOC_UNCOLLECTABLE(sizeof(Value) * TCO_ARGUMENT_COUNT_LIMIT));
#endif

    // g_customData
    g_customData = Global::platform()->allocateThreadLocalCustomData();

    inited = true;
}

void ThreadLocal::finalize()
{
    ESCARGOT_RELEASE_ASSERT(inited);

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    delete ThreadLocal::g_emptyStringInstance;
    ThreadLocal::g_emptyStringInstance = nullptr;
#endif

    // g_customData
    Global::platform()->deallocateThreadLocalCustomData();
    g_customData = nullptr;

#if defined(ENABLE_TCO)
    // g_tcoBuffer
    GC_FREE(g_tcoBuffer);
    g_tcoBuffer = nullptr;
#endif

    // full gc(Heap::finalize) should be invoked after g_customData deallocation
    // because g_customData might contain GC-object
    Heap::finalize();

    // g_randEngine does not need finalization
    delete g_randEngine;
    g_randEngine = nullptr;

    // g_bfContext
    bf_context_end(&g_bfContext);

#if defined(ENABLE_WASM)
    // g_wasmContext
    wasm_store_delete(g_wasmContext.store);
    wasm_engine_delete(g_wasmContext.engine);
    g_wasmContext.store = nullptr;
    g_wasmContext.engine = nullptr;
    g_wasmContext.lastGCCheckTime = 0;
#endif

    // g_gcEventListenerSet
    delete g_gcEventListenerSet;
    g_gcEventListenerSet = nullptr;
    GC_set_on_collection_event(nullptr);

    // g_astAllocator
    delete g_astAllocator;
    g_astAllocator = nullptr;

    // g_bumpPointerAllocator
    delete g_bumpPointerAllocator;
    g_bumpPointerAllocator = nullptr;

    inited = false;
}

} // namespace Escargot
