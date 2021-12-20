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

#ifndef __EscargotGlobal__
#define __EscargotGlobal__

#if defined(ENABLE_THREADING)
#include <mutex>
#endif

#if defined(ENABLE_THREADING) && !defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
#define ENABLE_ATOMICS_GLOBAL_LOCK
#endif

#if defined(ENABLE_ATOMICS_GLOBAL_LOCK)
#include "util/SpinLock.h"
#endif

namespace Escargot {

class Platform;

// Global is a global interface used by all threads
class Global {
    static bool inited;
    static Platform* g_platform;
#if defined(ENABLE_ATOMICS_GLOBAL_LOCK)
    static SpinLock g_atomicsLock;
#endif
public:
    static void initialize(Platform* platform);
    static void finalize();

    static Platform* platform();
#if defined(ENABLE_ATOMICS_GLOBAL_LOCK)
    static SpinLock& atomicsLock()
    {
        return g_atomicsLock;
    }
#endif

#if defined(ENABLE_THREADING)
    struct Waiter {
        void* m_blockAddress;
        std::mutex m_mutex;
        std::condition_variable m_waiter;
        std::mutex m_conditionVariableMutex;
        std::atomic_uint m_waiterCount;
    };

    static std::mutex g_waiterMutex;
    static std::vector<Waiter*> g_waiter;
    static Waiter* waiter(void* blockAddress);
#endif

    struct HeapProfile {
        HeapProfile()
            : gcCount(0)
            , compCount(0)
            , decompCount(0)
            , lastGCMarkTime(0)
            , gcTime(0)
            , compTime(0)
            , decompTime(0)
        {
        }

        size_t gcCount;
        size_t compCount;
        size_t decompCount;
        uint64_t lastGCMarkTime;
        uint64_t gcTime;
        uint64_t compTime;
        uint64_t decompTime;

        void printResult();
    };

    static HeapProfile* g_heapProfile;
    static HeapProfile* heapProfile();

    // Heap Profile Functions
    static void increaseHeapProfileGCCount() { g_heapProfile->gcCount++; }
    static void increaseHeapProfileCompCount() { g_heapProfile->compCount++; }
    static void increaseHeapProfileDecompCount() { g_heapProfile->decompCount++; }
    static void markHeapProfileGCMarkStartTime(uint64_t time)
    {
        ASSERT(g_heapProfile->lastGCMarkTime == 0);
        g_heapProfile->lastGCMarkTime = time;
    }
    static void markHeapProfileGCReclaimEndTime(uint64_t time)
    {
        ASSERT(time >= g_heapProfile->lastGCMarkTime);
        g_heapProfile->gcTime += (time - g_heapProfile->lastGCMarkTime);
        g_heapProfile->lastGCMarkTime = 0;
    }
    static void addHeapProfileCompTime(uint64_t time) { g_heapProfile->compTime += time; }
    static void addHeapProfileDecompTime(uint64_t time) { g_heapProfile->decompTime += time; }
};

} // namespace Escargot

#endif
