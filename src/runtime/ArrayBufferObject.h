/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotArrayBufferObject__
#define __EscargotArrayBufferObject__

#include "runtime/ArrayBuffer.h"

namespace Escargot {

class ArrayBufferObject : public ArrayBuffer {
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr);

public:
    explicit ArrayBufferObject(ExecutionState& state);
    explicit ArrayBufferObject(ExecutionState& state, Object* proto);

    static ArrayBufferObject* allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength);
    static ArrayBufferObject* cloneArrayBuffer(ExecutionState& state, ArrayBuffer* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor);

    void allocateBuffer(ExecutionState& state, size_t bytelength);
    void attachBuffer(BackingStore* backingStore);
    void detachArrayBuffer();

    virtual bool isArrayBufferObject() const
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

} // namespace Escargot

#endif
