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
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

ObjectHasPropertyResult TypedArrayObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return ObjectHasPropertyResult(integerIndexedElementGet(state, index));
        }
    }
    return Object::hasProperty(state, P);
}

ObjectGetResult TypedArrayObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return integerIndexedElementGet(state, index);
        }
    }
    return Object::getOwnProperty(state, P);
}

bool TypedArrayObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            if (buffer()->isDetachedBuffer() || !Value(Value::DoubleToIntConvertibleTestNeeds, index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength() || desc.isAccessorDescriptor()) {
                return false;
            }

            if (desc.isConfigurablePresent() && !desc.isConfigurable()) {
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

    return DerivedObject::defineOwnProperty(state, P, desc);
}

bool TypedArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            if (buffer()->isDetachedBuffer() || !Value(Value::DoubleToIntConvertibleTestNeeds, index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
                return true;
            }
            return false;
        }
    }
    return Object::deleteOwnProperty(state, P);
}

ObjectGetResult TypedArrayObject::get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            return integerIndexedElementGet(state, index);
        }
    }
    return Object::get(state, P, receiver);
}

bool TypedArrayObject::set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            integerIndexedElementSet(state, index, v);
            return true;
        }
    }
    return Object::set(state, P, v, receiver);
}

void TypedArrayObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    size_t len = arrayLength();
    for (size_t i = 0; i < len; i++) {
        if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)), data)) {
            return;
        }
    }
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

void TypedArrayObject::sort(ExecutionState& state, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    if (length) {
        Value* tempBuffer = (Value*)GC_MALLOC(sizeof(Value) * length);

        for (uint64_t i = 0; i < length; i++) {
            tempBuffer[i] = getIndexedProperty(state, Value(i)).value(state, this);
        }

        TightVector<Value, GCUtil::gc_malloc_allocator<Value>> tempSpace;
        tempSpace.resizeWithUninitializedValues(length);
        mergeSort(tempBuffer, length, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = comp(a, b);
            return true;
        });

        for (uint64_t i = 0; i < length; i++) {
            setIndexedProperty(state, Value(i), tempBuffer[i], this);
        }
        GC_FREE(tempBuffer);
    }
}

void TypedArrayObject::toSorted(ExecutionState& state, Object* target, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    ASSERT(target && (target->isArrayObject() || target->isTypedArrayObject()));

    if (length) {
        Value* tempBuffer = (Value*)GC_MALLOC(sizeof(Value) * length);

        for (uint64_t i = 0; i < length; i++) {
            tempBuffer[i] = getIndexedProperty(state, Value(i)).value(state, this);
        }

        TightVector<Value, GCUtil::gc_malloc_allocator<Value>> tempSpace;
        tempSpace.resizeWithUninitializedValues(length);
        mergeSort(tempBuffer, length, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = comp(a, b);
            return true;
        });

        for (uint64_t i = 0; i < length; i++) {
            target->setIndexedProperty(state, Value(i), tempBuffer[i], target);
        }
        GC_FREE(tempBuffer);
    }
}

ArrayBuffer* TypedArrayObject::validateTypedArray(ExecutionState& state, const Value& O)
{
    if (UNLIKELY(!O.isObject() || !O.asObject()->isTypedArrayObject())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotTypedArrayObject);
    }

    ArrayBuffer* buffer = O.asObject()->asTypedArrayObject()->buffer();
    buffer->throwTypeErrorIfDetached(state);
    return buffer;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementget
ObjectGetResult TypedArrayObject::integerIndexedElementGet(ExecutionState& state, double index)
{
    if (UNLIKELY(buffer()->isDetachedBuffer() || !Value(Value::DoubleToIntConvertibleTestNeeds, index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength())) {
        return ObjectGetResult();
    }

    size_t indexedPosition = (index * elementSize()) + byteOffset();
    // Return GetValueFromBuffer(buffer, indexedPosition, elementType, true, "Unordered").
    return ObjectGetResult(buffer()->getValueFromBuffer(state, indexedPosition, typedArrayType()), true, true, true);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementset
bool TypedArrayObject::integerIndexedElementSet(ExecutionState& state, double index, const Value& value)
{
    Value numValue(Value::ForceUninitialized);
    // If arrayTypeName is "BigUint64Array" or "BigInt64Array", let numValue be ? ToBigInt(value).
    auto type = typedArrayType();
    bool isBigIntArray = type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64;
    if (UNLIKELY(isBigIntArray)) {
        numValue = value.toBigInt(state);
    } else {
        // Otherwise, let numValue be ? ToNumber(value).
        numValue = Value(Value::DoubleToIntConvertibleTestNeeds, value.toNumber(state));
    }

    if (buffer()->isDetachedBuffer() || !Value(Value::DoubleToIntConvertibleTestNeeds, index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
        return false;
    }

    size_t indexedPosition = (index * elementSize()) + byteOffset();
    // Perform SetValueInBuffer(buffer, indexedPosition, elementType, numValue, true, "Unordered").
    buffer()->setValueInBuffer(state, indexedPosition, typedArrayType(), numValue);
    return true;
}

#define DECLARE_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                           \
    TypedArrayObject* TYPE##ArrayObject::allocateTypedArray(ExecutionState& state, Object* newTarget, size_t length)                              \
    {                                                                                                                                             \
        ASSERT(!!newTarget);                                                                                                                      \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget, [](ExecutionState& state, Context* constructorRealm) -> Object* {   \
            return constructorRealm->globalObject()->type##ArrayPrototype();                                                                      \
        });                                                                                                                                       \
        TypedArrayObject* obj = new TYPE##ArrayObject(state, proto);                                                                              \
        if (length == std::numeric_limits<size_t>::max()) {                                                                                       \
            obj->setBuffer(nullptr, 0, 0, 0);                                                                                                     \
        } else {                                                                                                                                  \
            auto buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), length * siz);            \
            obj->setBuffer(buffer, 0, length * siz, length);                                                                                      \
        }                                                                                                                                         \
        return obj;                                                                                                                               \
    }                                                                                                                                             \
                                                                                                                                                  \
    template <const bool isLittleEndian>                                                                                                          \
    Value TYPE##ArrayObject::getDirectValueFromBuffer(ExecutionState& state, size_t byteindex)                                                    \
    {                                                                                                                                             \
        typedef typename TYPE##Adaptor::Type Type;                                                                                                \
        ASSERT(byteLength());                                                                                                                     \
        size_t elementSize = sizeof(Type);                                                                                                        \
        ASSERT(byteindex + elementSize <= byteLength());                                                                                          \
        auto bufferAddress = rawBuffer();                                                                                                         \
        if (UNLIKELY(bufferAddress == nullptr)) {                                                                                                 \
            return Value();                                                                                                                       \
        }                                                                                                                                         \
        uint8_t* rawStart = bufferAddress + byteindex;                                                                                            \
        Type res;                                                                                                                                 \
        if (isLittleEndian) {                                                                                                                     \
            res = *((Type*)rawStart);                                                                                                             \
        } else {                                                                                                                                  \
            for (size_t i = 0; i < elementSize; i++) {                                                                                            \
                ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];                                                                              \
            }                                                                                                                                     \
        }                                                                                                                                         \
        if (std::is_same<int64_t, nativeType>::value) {                                                                                           \
            return Value(new BigInt((int64_t)res));                                                                                               \
        } else if (std::is_same<uint64_t, nativeType>::value) {                                                                                   \
            return Value(new BigInt((uint64_t)res));                                                                                              \
        } else if (std::is_same<uint8_t, nativeType>::value) {                                                                                    \
            return Value((uint8_t)res);                                                                                                           \
        } else if (std::is_same<uint16_t, nativeType>::value) {                                                                                   \
            return Value((uint16_t)res);                                                                                                          \
        } else if (std::is_same<uint32_t, nativeType>::value) {                                                                                   \
            return Value((uint32_t)res);                                                                                                          \
        } else if (std::is_same<int8_t, nativeType>::value) {                                                                                     \
            return Value((int8_t)res);                                                                                                            \
        } else if (std::is_same<int16_t, nativeType>::value) {                                                                                    \
            return Value((int16_t)res);                                                                                                           \
        } else if (std::is_same<int32_t, nativeType>::value) {                                                                                    \
            return Value((int32_t)res);                                                                                                           \
        }                                                                                                                                         \
        return Value(Value::DoubleToIntConvertibleTestNeeds, res);                                                                                \
    }                                                                                                                                             \
    template <const bool isLittleEndian>                                                                                                          \
    void TYPE##ArrayObject::setDirectValueInBuffer(ExecutionState& state, size_t byteindex, const Value& val)                                     \
    {                                                                                                                                             \
        typedef typename TYPE##Adaptor::Type Type;                                                                                                \
        Type littleEndianVal = TYPE##Adaptor::toNative(state, val);                                                                               \
        ASSERT(byteLength());                                                                                                                     \
        size_t elementSize = siz;                                                                                                                 \
        ASSERT(byteindex + elementSize <= byteLength());                                                                                          \
        auto bufferAddress = rawBuffer();                                                                                                         \
        uint8_t* rawStart = bufferAddress + byteindex;                                                                                            \
                                                                                                                                                  \
        if (isLittleEndian) {                                                                                                                     \
            *((Type*)rawStart) = littleEndianVal;                                                                                                 \
        } else {                                                                                                                                  \
            for (size_t i = 0; i < elementSize; i++) {                                                                                            \
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];                                                                  \
            }                                                                                                                                     \
        }                                                                                                                                         \
    }                                                                                                                                             \
                                                                                                                                                  \
    ObjectGetResult TYPE##ArrayObject::getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver)                    \
    {                                                                                                                                             \
        if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength())) {                                                         \
            if (UNLIKELY(buffer()->isDetachedBuffer())) {                                                                                         \
                return ObjectGetResult();                                                                                                         \
            }                                                                                                                                     \
            size_t indexedPosition = property.asUInt32() * siz;                                                                                   \
            return ObjectGetResult(getDirectValueFromBuffer(state, indexedPosition), true, true, false);                                          \
        }                                                                                                                                         \
        return get(state, ObjectPropertyName(state, property), receiver);                                                                         \
    }                                                                                                                                             \
                                                                                                                                                  \
    Value TYPE##ArrayObject::getIndexedPropertyValue(ExecutionState& state, const Value& property, const Value& receiver)                         \
    {                                                                                                                                             \
        if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength())) {                                                         \
            if (UNLIKELY(buffer()->isDetachedBuffer())) {                                                                                         \
                return Value();                                                                                                                   \
            }                                                                                                                                     \
            size_t indexedPosition = property.asUInt32() * siz;                                                                                   \
            return getDirectValueFromBuffer(state, indexedPosition);                                                                              \
        }                                                                                                                                         \
        return get(state, ObjectPropertyName(state, property), receiver).value(state, receiver);                                                  \
    }                                                                                                                                             \
                                                                                                                                                  \
    bool TYPE##ArrayObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver)           \
    {                                                                                                                                             \
        if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength() && !buffer()->isDetachedBuffer() && value.isPrimitive())) { \
            size_t indexedPosition = property.asUInt32() * siz;                                                                                   \
            setDirectValueInBuffer(state, indexedPosition, value);                                                                                \
            return true;                                                                                                                          \
        }                                                                                                                                         \
        return set(state, ObjectPropertyName(state, property), value, receiver);                                                                  \
    }

FOR_EACH_TYPEDARRAY_TYPES(DECLARE_TYPEDARRAY)
#undef DECLARE_TYPEDARRAY
} // namespace Escargot
