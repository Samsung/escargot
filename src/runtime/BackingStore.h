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

#ifndef __EscargotBackingStore__
#define __EscargotBackingStore__

namespace Escargot {

using BackingStoreDeleterCallback = void (*)(void* data, size_t length,
                                             void* deleterData);

class BackingStore : public gc {
    friend class ArrayBufferObject;

public:
    BackingStore(size_t byteLength);
    BackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback,
                 void* callbackData, bool isShared = false);
    ~BackingStore();

    void* data() const
    {
        return m_data;
    }

    size_t byteLength() const
    {
        return m_byteLength;
    }

    bool isShared() const
    {
        return m_isShared;
    }

    void* deleterData() const
    {
        return m_deleterData;
    }

    void reallocate(size_t newByteLength);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    // Indicates whether the backing store was created for SharedArrayBuffer
    bool m_isShared;
    bool m_isAllocatedByPlatformAllocator;
    void* m_data;
    size_t m_byteLength;
    BackingStoreDeleterCallback m_deleter;
    void* m_deleterData;
};

} // namespace Escargot

#endif
