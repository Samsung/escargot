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
            if (buffer()->isDetachedBuffer() || !Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength() || desc.isAccessorDescriptor()) {
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

    return Object::defineOwnProperty(state, P, desc);
}

bool TypedArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(P.isStringType())) {
        double index = P.canonicalNumericIndexString(state);
        if (LIKELY(index != Value::UndefinedIndex)) {
            if (buffer()->isDetachedBuffer() || !Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
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

void TypedArrayObject::sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
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

ArrayBufferObject* TypedArrayObject::validateTypedArray(ExecutionState& state, const Value& O)
{
    if (!O.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    Object* thisObject = O.asObject();
    if (!thisObject->isTypedArrayObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotTypedArrayObject);
    }

    auto wrapper = thisObject->asTypedArrayObject();
    ArrayBufferObject* buffer = wrapper->buffer();
    buffer->throwTypeErrorIfDetached(state);
    return buffer;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementget
ObjectGetResult TypedArrayObject::integerIndexedElementGet(ExecutionState& state, double index)
{
    if (buffer()->isDetachedBuffer() || !Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
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
        numValue = Value(value.toNumber(state));
    }

    if (buffer()->isDetachedBuffer() || !Value(index).isInteger(state) || index == Value::MinusZeroIndex || index < 0 || index >= arrayLength()) {
        return false;
    }

    size_t indexedPosition = (index * elementSize()) + byteOffset();
    // Perform SetValueInBuffer(buffer, indexedPosition, elementType, numValue, true, "Unordered").
    buffer()->setValueInBuffer(state, indexedPosition, typedArrayType(), numValue);
    return true;
}

#define DECLARE_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                         \
    TypedArrayObject* TYPE##ArrayObject::allocateTypedArray(ExecutionState& state, Object* newTarget, size_t length)                            \
    {                                                                                                                                           \
        ASSERT(!!newTarget);                                                                                                                    \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget, [](ExecutionState& state, Context* constructorRealm) -> Object* { \
            return constructorRealm->globalObject()->type##ArrayPrototype();                                                                    \
        });                                                                                                                                     \
        TypedArrayObject* obj = new TYPE##ArrayObject(state, proto);                                                                            \
        if (length == std::numeric_limits<size_t>::max()) {                                                                                     \
            obj->setBuffer(nullptr, 0, 0, 0);                                                                                                   \
        } else {                                                                                                                                \
            auto buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), length * siz);          \
            obj->setBuffer(buffer, 0, length* siz, length);                                                                                     \
        }                                                                                                                                       \
        return obj;                                                                                                                             \
    }                                                                                                                                           \
                                                                                                                                                \
    Value TYPE##ArrayObject::getDirectValueFromBuffer(ExecutionState& state, size_t byteindex, bool isLittleEndian)                             \
    {                                                                                                                                           \
        typedef typename TYPE##Adaptor::Type Type;                                                                                              \
        ASSERT(byteLength());                                                                                                                   \
        size_t elementSize = sizeof(Type);                                                                                                      \
        ASSERT(byteindex + elementSize <= byteLength());                                                                                        \
        uint8_t* rawStart = rawBuffer() + byteindex;                                                                                            \
        Type res;                                                                                                                               \
        if (LIKELY(isLittleEndian)) {                                                                                                           \
            res = *((Type*)rawStart);                                                                                                           \
        } else {                                                                                                                                \
            for (size_t i = 0; i < elementSize; i++) {                                                                                          \
                ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];                                                                            \
            }                                                                                                                                   \
        }                                                                                                                                       \
        if (std::is_same<int64_t, nativeType>::value) {                                                                                         \
            return Value(new BigInt((int64_t)res));                                                                                             \
        } else if (std::is_same<uint64_t, nativeType>::value) {                                                                                 \
            return Value(new BigInt((uint64_t)res));                                                                                            \
        }                                                                                                                                       \
        return Value(res);                                                                                                                      \
    }                                                                                                                                           \
                                                                                                                                                \
    void TYPE##ArrayObject::setDirectValueInBuffer(ExecutionState& state, size_t byteindex, const Value& val, bool isLittleEndian)              \
    {                                                                                                                                           \
        typedef typename TYPE##Adaptor::Type Type;                                                                                              \
        ASSERT(byteLength());                                                                                                                   \
        size_t elementSize = sizeof(Type);                                                                                                      \
        ASSERT(byteindex + elementSize <= byteLength());                                                                                        \
        uint8_t* rawStart = rawBuffer() + byteindex;                                                                                            \
        Type littleEndianVal = TYPE##Adaptor::toNative(state, val);                                                                             \
                                                                                                                                                \
        if (LIKELY(isLittleEndian)) {                                                                                                           \
            *((Type*)rawStart) = littleEndianVal;                                                                                               \
        } else {                                                                                                                                \
            for (size_t i = 0; i < elementSize; i++) {                                                                                          \
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];                                                                \
            }                                                                                                                                   \
        }                                                                                                                                       \
    }                                                                                                                                           \
                                                                                                                                                \
    ObjectGetResult TYPE##ArrayObject::getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver)                  \
    {                                                                                                                                           \
        if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength())) {                                                       \
            if (buffer()->isDetachedBuffer()) {                                                                                                 \
                return ObjectGetResult();                                                                                                       \
            }                                                                                                                                   \
            size_t indexedPosition = property.asUInt32() * elementSize();                                                                       \
            return ObjectGetResult(getDirectValueFromBuffer(state, indexedPosition), true, true, false);                                        \
        }                                                                                                                                       \
        return get(state, ObjectPropertyName(state, property), receiver);                                                                       \
    }                                                                                                                                           \
                                                                                                                                                \
    bool TYPE##ArrayObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver)         \
    {                                                                                                                                           \
        if (LIKELY(property.isUInt32() && (size_t)property.asUInt32() < arrayLength() && !buffer()->isDetachedBuffer())) {                      \
            size_t indexedPosition = property.asUInt32() * elementSize();                                                                       \
            setDirectValueInBuffer(state, indexedPosition, value);                                                                              \
            return true;                                                                                                                        \
        }                                                                                                                                       \
        return set(state, ObjectPropertyName(state, property), value, receiver);                                                                \
    }

FOR_EACH_TYPEDARRAY_TYPES(DECLARE_TYPEDARRAY)
#undef DECLARE_TYPEDARRAY
} // namespace Escargot
