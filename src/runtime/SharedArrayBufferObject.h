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

#include "runtime/ArrayBufferObject.h"
#include "runtime/BackingStore.h"

namespace Escargot {

class SharedArrayBufferObjectBackingStoreData {
public:
    SharedArrayBufferObjectBackingStoreData(void* data, size_t byteLength)
        : m_data(data)
        , m_byteLength(byteLength)
        , m_refCount(1)
    {
    }

    void* data() const
    {
        return m_data;
    }

    size_t byteLength() const
    {
        return m_byteLength;
    }

    void ref()
    {
        m_refCount++;
    }

    void deref();

private:
    void* m_data;
    size_t m_byteLength;
    std::atomic<size_t> m_refCount;
};

class SharedArrayBufferObject : public ArrayBufferObject {
public:
    SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength);
    SharedArrayBufferObject(ExecutionState& state, Object* proto, SharedArrayBufferObjectBackingStoreData* data);

    static SharedArrayBufferObject* allocateSharedArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength);

    Optional<BackingStore*> backingStore()
    {
        return m_backingStore;
    }

    virtual bool isSharedArrayBufferObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};
} // namespace Escargot

#endif
#endif
