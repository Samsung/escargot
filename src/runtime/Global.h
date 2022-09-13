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
class Context;
class Platform;
class Object;

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
    struct Waiter;
    struct WaiterItem {
        WaiterItem(Context* context, Waiter* waiter, Optional<Object*> promise = nullptr)
            : m_context(context)
            , m_waiter(waiter)
            , m_promise(promise)
        {
        }

        Context* m_context;
        Waiter* m_waiter;
        Optional<Object*> m_promise;
    };

    struct Waiter {
        void* m_blockAddress;
        std::mutex m_mutex;
        std::condition_variable m_waiter;
        std::mutex m_conditionVariableMutex;
        std::vector<std::shared_ptr<WaiterItem>> m_waiterList;
    };

    static std::mutex g_waiterMutex;
    static std::vector<Waiter*> g_waiter;
    static Waiter* waiter(void* blockAddress);
#endif
};

} // namespace Escargot

#endif
