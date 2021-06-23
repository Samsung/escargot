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

#ifndef __EscargotSharedArrayBufferObject__
#define __EscargotSharedArrayBufferObject__

#include "runtime/Context.h"
#include "runtime/BackingStore.h"

namespace Escargot {

class SharedArrayBufferObject : public Object {
public:
    SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength);

    static SharedArrayBufferObject* allocateSharedArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength);

    static const uint32_t maxArrayBufferSize = 210000000;

    Optional<BackingStore*> backingStore()
    {
        return m_backingStore;
    }

    virtual bool isSharedArrayBufferObject() const
    {
        return true;
    }

    ALWAYS_INLINE uint8_t* data() { return m_data; }
    ALWAYS_INLINE size_t byteLength() { return m_byteLength; }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    uint8_t* m_data; // Points backing stores data address
    size_t m_byteLength; // Indicates backing stores byte length
    Optional<BackingStore*> m_backingStore;
};
} // namespace Escargot

#endif
#endif
