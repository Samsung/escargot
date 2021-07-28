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

#include "Escargot.h"
#include "runtime/VMInstance.h"
#include "runtime/BackingStore.h"
#include "runtime/ArrayBuffer.h"
#include "runtime/TypedArrayInlines.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/SharedArrayBufferObject.h"

namespace Escargot {

unsigned TypedArrayHelper::elementSizeTable[11] = { 1, 2, 4, 1, 2, 4, 1, 4, 8, 8, 8 };

ArrayBuffer::ArrayBuffer(ExecutionState& state, Object* proto)
    : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
    , m_mayPointsSharedBackingStore(false)
{
}

// $24.1.1.6
Value ArrayBuffer::getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());
    uint8_t* rawStart = data() + byteindex;
    if (LIKELY(isLittleEndian)) {
        return TypedArrayHelper::rawBytesToNumber(state, type, rawStart);
    } else {
        uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes[elemSize - i - 1] = rawStart[i];
        }
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
    }
}

// $24.1.1.8
void ArrayBuffer::setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());
    uint8_t* rawStart = data() + byteindex;
    uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, val, rawBytes);
    if (LIKELY(isLittleEndian)) {
        memcpy(rawStart, rawBytes, elemSize);
    } else {
        for (size_t i = 0; i < elemSize; i++) {
            rawStart[i] = rawBytes[elemSize - i - 1];
        }
    }
}
} // namespace Escargot
