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

#if defined(ENABLE_THREADING)

#ifndef __EscargotSpinLock__
#define __EscargotSpinLock__

#include <atomic>

namespace Escargot {

class SpinLock {
    std::atomic_flag m_locked;

public:
    SpinLock()
        : m_locked(ATOMIC_FLAG_INIT)
    {
    }

    void lock()
    {
        while (m_locked.test_and_set(std::memory_order_acquire)) {
        }
    }

    void unlock()
    {
        m_locked.clear(std::memory_order_release);
    }
};

} // namespace Escargot

#endif

#endif
