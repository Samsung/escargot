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
    explicit ArrayBufferObject(ExecutionState& state, BackingStore* backingStore);

    static ArrayBufferObject* allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength,
                                                  Optional<uint64_t> maxByteLength = Optional<uint64_t>(), bool resizeable = true);
    static ArrayBufferObject* allocateExternalArrayBuffer(ExecutionState& state, void* dataBlock, size_t byteLength);
    static ArrayBufferObject* cloneArrayBuffer(ExecutionState& state, ArrayBuffer* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor);

    void allocateBuffer(ExecutionState& state, size_t bytelength);
    void allocateResizableBuffer(ExecutionState& state, size_t bytelength, size_t maxByteLength);
    void attachBuffer(BackingStore* backingStore);
    void detachArrayBuffer();

    virtual bool isArrayBufferObject() const override
    {
        return true;
    }

    virtual void fillData(const uint8_t* newData, size_t length) override
    {
        ASSERT(!isDetachedBuffer());
        memcpy(data(), newData, length);
    }

    virtual Value getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian = true) override;
    virtual void setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian = true) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

} // namespace Escargot

#endif
