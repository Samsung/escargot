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

#ifndef __EscargotDataViewObject__
#define __EscargotDataViewObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/ArrayBufferObject.h"

namespace Escargot {

class DataViewObject : public ArrayBufferView {
public:
    explicit DataViewObject(ExecutionState& state, Object* proto)
        : ArrayBufferView(state, proto)
    {
    }

    virtual bool isDataViewObject() const
    {
        return true;
    }

    // 24.2.1.1
    Value getViewValue(ExecutionState& state, Value index, Value _isLittleEndian, TypedArrayType type)
    {
        double numberIndex = index.toIndex(state);
        if (numberIndex == Value::InvalidIndexValue)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);

        bool isLittleEndian = _isLittleEndian.toBoolean(state);

        ArrayBufferObject* buffer = this->buffer();
        if (!buffer || buffer->isDetachedBuffer()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_DetachedBuffer);
            return Value();
        }

        unsigned viewOffset = byteOffset();
        unsigned viewSize = byteLength();

        unsigned elementSize = ArrayBufferView::getElementSize(type);

        if (numberIndex + elementSize > viewSize)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_RangeError);

        unsigned bufferIndex = numberIndex + viewOffset;
        switch (type) {
        case TypedArrayType::Float32:
            return buffer->getValueFromBuffer<float>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Float64:
            return buffer->getValueFromBuffer<double>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Int8:
            return buffer->getValueFromBuffer<int8_t>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Int16:
            return buffer->getValueFromBuffer<int16_t>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Int32:
            return buffer->getValueFromBuffer<int32_t>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Uint8:
            return buffer->getValueFromBuffer<uint8_t>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Uint16:
            return buffer->getValueFromBuffer<uint16_t>(state, bufferIndex, isLittleEndian);
        case TypedArrayType::Uint32:
            return buffer->getValueFromBuffer<uint32_t>(state, bufferIndex, isLittleEndian);
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    // 24.2.1.2
    Value setViewValue(ExecutionState& state, Value index, Value _isLittleEndian, TypedArrayType type, Value val)
    {
        double numberIndex = index.toIndex(state);
        if (numberIndex == Value::InvalidIndexValue)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);

        double numberValue = val.toNumber(state);
        bool isLittleEndian = _isLittleEndian.toBoolean(state);
        ArrayBufferObject* buffer = this->buffer();
        if (!buffer || buffer->isDetachedBuffer()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_DetachedBuffer);
            return Value();
        }

        unsigned viewOffset = byteOffset();
        unsigned viewSize = byteLength();

        unsigned elementSize = this->getElementSize(type);

        if (numberIndex + elementSize > viewSize)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_RangeError);

        unsigned bufferIndex = numberIndex + viewOffset;
        switch (type) {
        case TypedArrayType::Float32:
            buffer->setValueInBuffer<Float32Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Float64:
            buffer->setValueInBuffer<Float64Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Int8:
            buffer->setValueInBuffer<Int8Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Int16:
            buffer->setValueInBuffer<Int16Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Int32:
            buffer->setValueInBuffer<Int32Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Uint8:
            buffer->setValueInBuffer<Uint8Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Uint16:
            buffer->setValueInBuffer<Uint16Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        case TypedArrayType::Uint32:
            buffer->setValueInBuffer<Uint32Adaptor>(state, bufferIndex, val, isLittleEndian);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        return Value();
    }

private:
};
}

#endif
