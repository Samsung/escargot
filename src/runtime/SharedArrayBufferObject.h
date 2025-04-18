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

#include "runtime/ArrayBuffer.h"

namespace Escargot {

class SharedArrayBufferObject : public ArrayBuffer {
public:
    SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength);
    SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength, size_t maxByteLength);
    SharedArrayBufferObject(ExecutionState& state, Object* proto, BackingStore* backingStore);
    SharedArrayBufferObject(ExecutionState& state, Object* proto, SharedDataBlockInfo* sharedInfo);

    static SharedArrayBufferObject* allocateSharedArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength, Optional<uint64_t> maxByteLength);
    static SharedArrayBufferObject* allocateExternalSharedArrayBuffer(ExecutionState& state, void* dataBlock, size_t byteLength);

    virtual bool isSharedArrayBufferObject() const override
    {
        return true;
    }

    // thread-safe functions
    virtual void fillData(const uint8_t* newData, size_t length) override;
    virtual Value getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian = true) override;
    virtual void setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian = true) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    size_t byteLengthRMW(size_t newByteLength);
};
} // namespace Escargot

#endif
#endif
