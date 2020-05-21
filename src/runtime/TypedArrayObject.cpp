/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "runtime/TypedArrayObject.h"

namespace Escargot {

template <typename T>
ObjectHasPropertyResult TypedArrayObject<T>::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return ObjectHasPropertyResult(integerIndexedElementGet(state, index));
        }
    }
    return Object::hasProperty(state, P);
}

template <typename T>
ObjectGetResult TypedArrayObject<T>::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return integerIndexedElementGet(state, index);
        }
    }
    return Object::getOwnProperty(state, P);
}

template <typename T>
bool TypedArrayObject<T>::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            if (!Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength() || desc.isAccessorDescriptor()) {
                return false;
            }

            if (desc.isConfigurablePresent() && desc.isConfigurable()) {
                return false;
            }
            if (desc.isEnumerablePresent() && !desc.isEnumerable()) {
                return false;
            }
            if (desc.isWritablePresent() && !desc.isWritable()) {
                return false;
            }
            if (desc.isValuePresent()) {
                return integerIndexedElementSet(state, index, desc.value());
            }
            return true;
        }
    }

    return Object::defineOwnProperty(state, P, desc);
}

template <typename T>
ObjectGetResult TypedArrayObject<T>::get(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return integerIndexedElementGet(state, index);
        }
    }
    return Object::get(state, P);
}

template <typename T>
bool TypedArrayObject<T>::set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return integerIndexedElementSet(state, index, v);
        }
    }
    return Object::set(state, P, v, receiver);
}

template <typename T>
ObjectGetResult TypedArrayObject<T>::getIndexedProperty(ExecutionState& state, const Value& property)
{
    if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength())) {
        buffer()->throwTypeErrorIfDetached(state);

        size_t indexedPosition = property.asUInt32() * elementSize();
        return ObjectGetResult(getDirectValueFromBuffer(state, indexedPosition), true, true, false);
    }
    return get(state, ObjectPropertyName(state, property));
}

template <typename T>
bool TypedArrayObject<T>::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength())) {
        double numValue = value.toNumber(state);
        buffer()->throwTypeErrorIfDetached(state);

        size_t indexedPosition = property.asUInt32() * elementSize();
        setDirectValueInBuffer(state, indexedPosition, numValue);
        return true;
    }
    return set(state, ObjectPropertyName(state, property), value, this);
}

template <typename T>
void TypedArrayObject<T>::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    size_t len = arrayLength();
    for (size_t i = 0; i < len; i++) {
        if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)), data)) {
            return;
        }
    }
    Object::enumeration(state, callback, data);
}

template <typename T>
void TypedArrayObject<T>::sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    if (length) {
        Value* tempBuffer = (Value*)GC_MALLOC(sizeof(Value) * length);

        for (int64_t i = 0; i < length; i++) {
            tempBuffer[i] = integerIndexedElementGet(state, i).value(state, this);
        }

        TightVector<Value, GCUtil::gc_malloc_allocator<Value>> tempSpace;
        tempSpace.resizeWithUninitializedValues(length);
        mergeSort(tempBuffer, length, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = comp(a, b);
            return true;
        });

        for (int64_t i = 0; i < length; i++) {
            integerIndexedElementSet(state, i, tempBuffer[i]);
        }
        GC_FREE(tempBuffer);

        return;
    }
}

template <typename T>
Value TypedArrayObject<T>::getDirectValueFromBuffer(ExecutionState& state, size_t byteindex, bool isLittleEndian)
{
    typedef typename T::Type Type;
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elementSize = sizeof(Type);
    ASSERT(byteindex + elementSize <= byteLength());
    uint8_t* rawStart = rawBuffer() + byteindex;
    Type res;
    if (isLittleEndian != 1) { // bigEndian
        for (size_t i = 0; i < elementSize; i++) {
            ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];
        }
    } else { // littleEndian
        res = *((Type*)rawStart);
    }
    return Value(res);
}

template <typename T>
void TypedArrayObject<T>::setDirectValueInBuffer(ExecutionState& state, size_t byteindex, double val, bool isLittleEndian)
{
    typedef typename T::Type Type;
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elementSize = sizeof(Type);
    ASSERT(byteindex + elementSize <= byteLength());
    uint8_t* rawStart = rawBuffer() + byteindex;
    Type littleEndianVal = T::toNative(state, Value(val));

    if (isLittleEndian != 1) {
        for (size_t i = 0; i < elementSize; i++) {
            rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
        }
    } else {
        *((Type*)rawStart) = littleEndianVal;
    }
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementget
template <typename T>
ObjectGetResult TypedArrayObject<T>::integerIndexedElementGet(ExecutionState& state, double index)
{
    buffer()->throwTypeErrorIfDetached(state);

    if (!Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
        return ObjectGetResult();
    }

    size_t indexedPosition = index * elementSize();
    // Return GetValueFromBuffer(buffer, indexedPosition, elementType, true, "Unordered").
    return ObjectGetResult(getDirectValueFromBuffer(state, indexedPosition), true, true, false);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementset
template <typename T>
bool TypedArrayObject<T>::integerIndexedElementSet(ExecutionState& state, double index, const Value& value)
{
    double numValue = value.toNumber(state);
    buffer()->throwTypeErrorIfDetached(state);

    if (!Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
        return false;
    }

    size_t indexedPosition = index * elementSize();
    // Perform SetValueInBuffer(buffer, indexedPosition, elementType, numValue, true, "Unordered").
    setDirectValueInBuffer(state, indexedPosition, numValue);
    return true;
}

template class TypedArrayObject<Int8Adaptor>;
template class TypedArrayObject<Int16Adaptor>;
template class TypedArrayObject<Int32Adaptor>;
template class TypedArrayObject<Uint8Adaptor>;
template class TypedArrayObject<Uint8ClampedAdaptor>;
template class TypedArrayObject<Uint16Adaptor>;
template class TypedArrayObject<Uint32Adaptor>;
template class TypedArrayObject<Float32Adaptor>;
template class TypedArrayObject<Float64Adaptor>;
}
