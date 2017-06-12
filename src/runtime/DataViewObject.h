/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotDataViewObject__
#define __EscargotDataViewObject__

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"

namespace Escargot {

class DataViewObject : public ArrayBufferView {
public:
    DataViewObject(ExecutionState& state)
        : ArrayBufferView(state)
    {
    }

    virtual TypedArrayType typedArrayType()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool isDataViewObject() const
    {
        return true;
    }

    virtual const char* internalClassProperty()
    {
        return "DataView";
    }

    // 24.2.1.1
    Value getViewValue(ExecutionState& state, Value index, Value _isLittleEndian, TypedArrayType type)
    {
        double numberIndex = index.toNumber(state);
        double getIndex = Value(numberIndex).toInteger(state);

        if (numberIndex != getIndex || getIndex < 0)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);

        bool isLittleEndian = _isLittleEndian.toBoolean(state);

        ArrayBufferObject* buffer = this->buffer();
        if (buffer->isDetachedBuffer())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_DetachedBuffer);

        unsigned viewOffset = byteoffset();
        unsigned viewSize = bytelength();

        unsigned elementSize = ArrayBufferView::getElementSize(type);

        if (getIndex + elementSize > viewSize)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_RangeError);

        unsigned bufferIndex = getIndex + viewOffset;
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
        double numberIndex = index.toNumber(state);
        double getIndex = Value(numberIndex).toInteger(state);

        if (numberIndex != getIndex || getIndex < 0)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);

        bool isLittleEndian = _isLittleEndian.toBoolean(state);

        ArrayBufferObject* buffer = this->buffer();
        if (buffer->isDetachedBuffer())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_DetachedBuffer);

        unsigned viewOffset = byteoffset();
        unsigned viewSize = bytelength();

        unsigned elementSize = this->getElementSize(type);

        if (getIndex + elementSize > viewSize)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, errorMessage_GlobalObject_RangeError);

        unsigned bufferIndex = getIndex + viewOffset;
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

#endif
