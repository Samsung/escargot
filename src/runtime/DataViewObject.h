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
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayInlines.h"

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

    // https://www.ecma-international.org/ecma-262/#sec-getviewvalue
    Value getViewValue(ExecutionState& state, Value index, Value _isLittleEndian, TypedArrayType type)
    {
        double numberIndex = index.toIndex(state);
        if (numberIndex == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }

        bool isLittleEndian = _isLittleEndian.toBoolean(state);
        ArrayBufferObject* buffer = this->buffer();
        buffer->throwTypeErrorIfDetached(state);

        size_t viewOffset = byteOffset();
        size_t viewSize = byteLength();
        size_t elementSize = TypedArrayHelper::elementSize(type);

        if (numberIndex + elementSize > viewSize) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_RangeError);
        }

        size_t bufferIndex = numberIndex + viewOffset;
        return buffer->getValueFromBuffer(state, bufferIndex, type, isLittleEndian);
    }

    // https://www.ecma-international.org/ecma-262/#sec-setviewvalue
    Value setViewValue(ExecutionState& state, Value index, Value _isLittleEndian, TypedArrayType type, Value val)
    {
        double numberIndex = index.toIndex(state);
        if (numberIndex == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }

        auto numericValue = val.toNumeric(state);
        UNUSED_VARIABLE(numericValue);

        bool isLittleEndian = _isLittleEndian.toBoolean(state);
        ArrayBufferObject* buffer = this->buffer();
        buffer->throwTypeErrorIfDetached(state);

        size_t viewOffset = byteOffset();
        size_t viewSize = byteLength();
        size_t elementSize = TypedArrayHelper::elementSize(type);

        if (numberIndex + elementSize > viewSize) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_RangeError);
        }

        size_t bufferIndex = numberIndex + viewOffset;
        buffer->setValueInBuffer(state, bufferIndex, type, val, isLittleEndian);
        return Value();
    }
};
}

#endif
