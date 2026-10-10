/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifndef __EscargotScopedGCBuffer__
#define __EscargotScopedGCBuffer__

#include "GCUtil.h"
#include "util/Optional.h"

namespace Escargot {

// Owns an exclusively held temporary GC allocation. Stack storage and
// allocations whose ownership escapes must not be passed to this owner.
class ScopedGCBuffer {
public:
    explicit ScopedGCBuffer(Optional<void*> buffer)
        : m_buffer(buffer)
    {
    }

    ~ScopedGCBuffer()
    {
        if (m_buffer) {
            GC_FREE(m_buffer.value());
        }
    }

    ScopedGCBuffer(const ScopedGCBuffer&) = delete;
    ScopedGCBuffer& operator=(const ScopedGCBuffer&) = delete;
    MAKE_STACK_ALLOCATED();

private:
    Optional<void*> m_buffer;
};

} // namespace Escargot

#endif
