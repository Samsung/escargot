/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "ArrayObject.h"
#include "TypedArrayObject.h"
#include "ErrorObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "util/Util.h"

namespace Escargot {

ArrayObject::ArrayObject(ExecutionState& state)
    : ArrayObject(state, state.context()->globalObject()->arrayPrototype())
{
}

ArrayObject::ArrayObject(ExecutionState& state, Object* proto)
    : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
    , m_arrayLength(0)
    , m_fastModeData(nullptr)
{
    if (UNLIKELY(state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty())) {
        ensureRareData()->m_isFastModeArrayObject = false;
    }
}

ArrayObject::ArrayObject(ExecutionState& state, double length)
    : ArrayObject(state, state.context()->globalObject()->arrayPrototype(), length)
{
}

ArrayObject::ArrayObject(ExecutionState& state, Object* proto, double length)
    : ArrayObject(state, proto)
{
    // If length is -0, let length be +0.
    if (length == 0 && std::signbit(length)) {
        length = +0.0;
    }
    // If length>2^32-1, throw a RangeError exception.
    if (UNLIKELY(length > ((1LL << 32LL) - 1LL))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }

    setArrayLength(state, Value(length));
}

ArrayObject::ArrayObject(ExecutionState& state, const uint64_t& size)
    : ArrayObject(state, state.context()->globalObject()->arrayPrototype(), size)
{
}

ArrayObject::ArrayObject(ExecutionState& state, Object* proto, const uint64_t& size)
    : ArrayObject(state, proto)
{
    if (UNLIKELY(size > ((1LL << 32LL) - 1LL))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }

    setArrayLength(state, size, true);
}

ArrayObject::ArrayObject(ExecutionState& state, const Value* src, const uint64_t& size)
    : ArrayObject(state, state.context()->globalObject()->arrayPrototype(), src, size)
{
}

ArrayObject::ArrayObject(ExecutionState& state, Object* proto, const Value* src, const uint64_t& size)
    : ArrayObject(state, proto, size)
{
    // Let array be ! ArrayCreate(0).
    // Let n be 0.
    // For each element e of elements, do
    // Let status be CreateDataProperty(array, ! ToString(n), e).
    // Assert: status is true.
    // Increment n by 1.
    // Return array.

    if (isFastModeArray()) {
        for (size_t n = 0; n < size; n++) {
            setFastModeArrayValueWithoutExpanding(state, n, src[n]);
        }
    } else {
        for (size_t n = 0; n < size; n++) {
            defineOwnProperty(state, ObjectPropertyName(state, n), ObjectPropertyDescriptor(src[n], ObjectPropertyDescriptor::AllPresent));
        }
    }
}

ArrayObject* ArrayObject::createSpreadArray(ExecutionState& state)
{
    // SpreadArray is a Fixed Array which has no __proto__ property
    // Array.Prototype should not affect any SpreadArray operation
    ArrayObject* spreadArray = new ArrayObject(state);
    spreadArray->ensureRareData()->m_isFastModeArrayObject = true;
    spreadArray->rareData()->m_isSpreadArrayObject = true;
    spreadArray->rareData()->m_prototype = nullptr;

    return spreadArray;
}

ObjectHasPropertyResult ArrayObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ObjectGetResult v = getVirtualValue(state, P);
    if (LIKELY(v.hasValue())) {
        return ObjectHasPropertyResult(v);
    }

    return Object::hasProperty(state, P);
}


ObjectGetResult ArrayObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ObjectGetResult v = getVirtualValue(state, P);
    if (LIKELY(v.hasValue())) {
        return v;
    } else {
        return Object::getOwnProperty(state, P);
    }
}

bool ArrayObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (!P.isUIntType() && P.objectStructurePropertyName() == state.context()->staticStrings().length) {
        // Let newLen be ToUint32(Desc.[[Value]]).
        uint32_t newLen = 0;

        if (desc.isValuePresent()) {
            newLen = desc.value().toUint32(state);
            // If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
            if (newLen != desc.value().toNumber(state)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
            }
        }
        if (!isLengthPropertyWritable() && desc.isValuePresent() && m_arrayLength != newLen) {
            return false;
        }

        if (desc.isConfigurablePresent() && desc.isConfigurable()) {
            return false;
        }

        if (desc.isEnumerablePresent() && desc.isEnumerable()) {
            return false;
        }

        if (desc.isAccessorDescriptor()) {
            return false;
        }

        if (!isLengthPropertyWritable() && desc.isWritable()) {
            return false;
        }

        if (desc.isWritablePresent() && !desc.isWritable()) {
            ensureRareData()->m_isArrayObjectLengthWritable = false;
        }

        if (desc.isValuePresent() && m_arrayLength != newLen) {
            return setArrayLength(state, newLen);
        }

        return true;
    }

    uint64_t idx = P.tryToUseAsArrayIndex();
    if (LIKELY(isFastModeArray())) {
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = getArrayLength(state);
            if (len > idx && !m_fastModeData[idx].isEmpty()) {
                // Non-empty slot of fast-mode array always has {writable:true, enumerable:true, configurable:true}.
                // So, when new desciptor is not present, keep {w:true, e:true, c:true}
                if (UNLIKELY(!(desc.isValuePresentAlone() || desc.isDataWritableEnumerableConfigurable()))) {
                    convertIntoNonFastMode(state);
                    goto NonFastPath;
                }
            } else if (UNLIKELY(!desc.isDataWritableEnumerableConfigurable())) {
                // In case of empty slot property or over-lengthed property,
                // when new desciptor is not present, keep {w:false, e:false, c:false}
                convertIntoNonFastMode(state);
                goto NonFastPath;
            }

            if (!desc.isValuePresent()) {
                convertIntoNonFastMode(state);
                goto NonFastPath;
            }

            if (UNLIKELY(len <= idx)) {
                if (UNLIKELY(!isExtensible(state))) {
                    goto NonFastPath;
                }
                if (UNLIKELY(!setArrayLength(state, idx + 1)) || UNLIKELY(!isFastModeArray())) {
                    goto NonFastPath;
                }
            }
            m_fastModeData[idx] = desc.value();
            return true;
        }
    }

NonFastPath:

    uint32_t oldLen = getArrayLength(state);

    if (idx != Value::InvalidArrayIndexValue) {
        if ((idx >= oldLen) && !isLengthPropertyWritable())
            return false;
        bool succeeded = Object::defineOwnProperty(state, P, desc);
        if (!succeeded)
            return false;
        if (idx >= oldLen && ((idx + 1) <= Value::InvalidArrayIndexValue)) {
            return setArrayLength(state, idx + 1);
        }
        return true;
    }

    return Object::defineOwnProperty(state, P, desc);
}

bool ArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (!P.isUIntType() && P.toObjectStructurePropertyName(state) == state.context()->staticStrings().length) {
        return false;
    }

    if (LIKELY(isFastModeArray())) {
        uint64_t idx = P.tryToUseAsArrayIndex();
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint64_t len = getArrayLength(state);
            if (idx < len) {
                if (!m_fastModeData[idx].isEmpty()) {
                    m_fastModeData[idx] = Value(Value::EmptyValue);
                    ensureRareData()->m_shouldUpdateEnumerateObject = true;
                }
                return true;
            }
        }
    }

    return Object::deleteOwnProperty(state, P);
}

void ArrayObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        size_t len = getArrayLength(state);
        for (size_t i = 0; i < len; i++) {
            ASSERT(isFastModeArray());
            if (m_fastModeData[i].isEmpty())
                continue;
            if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent), data)) {
                return;
            }
        }
    }

    int attr = isLengthPropertyWritable() ? (int)ObjectStructurePropertyDescriptor::WritablePresent : 0;
    if (!callback(state, this, ObjectPropertyName(state.context()->staticStrings().length), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)attr), data)) {
        return;
    }

    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

void ArrayObject::sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    if (isFastModeArray()) {
        if (length) {
            uint32_t orgLength = length;
            size_t byteLength = sizeof(Value) * orgLength;
            bool canUseStack = byteLength <= 1024;
            Value* tempBuffer = canUseStack ? (Value*)alloca(byteLength) : CustomAllocator<Value>().allocate(orgLength);

            for (size_t i = 0; i < orgLength; i++) {
                tempBuffer[i] = m_fastModeData[i];
            }

            if (orgLength) {
                Value* tempSpace = canUseStack ? (Value*)alloca(byteLength) : CustomAllocator<Value>().allocate(orgLength);

                mergeSort(tempBuffer, orgLength, tempSpace, [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
                    *lessOrEqualp = comp(a, b);
                    return true;
                });

                if (!canUseStack) {
                    GC_FREE(tempSpace);
                }
            }

            if (getArrayLength(state) != orgLength) {
                setArrayLength(state, orgLength);
            }

            if (isFastModeArray()) {
                for (size_t i = 0; i < orgLength; i++) {
                    m_fastModeData[i] = tempBuffer[i];
                }
            }

            if (!canUseStack) {
                GC_FREE(tempBuffer);
            }
        }
        return;
    }
    Object::sort(state, length, comp);
}

void* ArrayObject::operator new(size_t size)
{
    return CustomAllocator<ArrayObject>().allocate(1);
}

void ArrayObject::iterateArrays(ExecutionState& state, HeapObjectIteratorCallback callback)
{
    iterateSpecificKindOfObject(state, HeapObjectKind::ArrayObjectKind, callback);
}

void ArrayObject::convertIntoNonFastMode(ExecutionState& state)
{
    if (!isFastModeArray())
        return;

    m_structure = structure()->convertToNonTransitionStructure();

    ensureRareData()->m_isFastModeArrayObject = false;

    auto length = getArrayLength(state);
    for (size_t i = 0; i < length; i++) {
        if (!m_fastModeData[i].isEmpty()) {
            defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, Value(i)), ObjectPropertyDescriptor(m_fastModeData[i], ObjectPropertyDescriptor::AllPresent));
        }
    }

    GC_FREE(m_fastModeData);
    m_fastModeData = nullptr;
}

bool ArrayObject::setArrayLength(ExecutionState& state, const Value& newLength)
{
    bool isPrimitiveValue;
    if (LIKELY(newLength.isPrimitive())) {
        isPrimitiveValue = true;
    } else {
        isPrimitiveValue = false;
    }
    // Let newLen be ToUint32(Desc.[[Value]]).
    uint32_t newLen = newLength.toUint32(state);
    // If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
    if (newLen != newLength.toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }

    bool ret;
    if (UNLIKELY(!isPrimitiveValue && !isLengthPropertyWritable())) {
        ret = false;
    } else {
        ret = setArrayLength(state, newLen, true);
    }
    return ret;
}

bool ArrayObject::setArrayLength(ExecutionState& state, const uint32_t newLength, bool useFitStorage)
{
    bool isFastMode = isFastModeArray();
    if (UNLIKELY(isFastMode && (newLength > ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE))) {
        uint32_t orgLength = getArrayLength(state);
        constexpr uint32_t maxSize = std::numeric_limits<uint32_t>::max() / 2;
        if (newLength > orgLength && ((newLength - orgLength > ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP) || newLength >= maxSize)) {
            convertIntoNonFastMode(state);
            isFastMode = false;
        }
    }

    if (LIKELY(isFastMode)) {
        auto oldLength = getArrayLength(state);
        if (LIKELY(oldLength != newLength)) {
            m_arrayLength = newLength;
            if (useFitStorage || oldLength == 0 || newLength <= 128) {
                bool hasRD = hasRareData();
                size_t oldCapacity = hasRD ? (size_t)rareData()->m_arrayObjectFastModeBufferCapacity : 0;
                if (oldCapacity) {
                    if (newLength > oldCapacity) {
                        m_fastModeData = (SmallValue*)GC_REALLOC(m_fastModeData, sizeof(SmallValue) * newLength);

                        for (size_t i = oldLength; i < newLength; i++) {
                            m_fastModeData[i] = SmallValue(SmallValue::EmptyValue);
                        }

                    } else {
                        m_fastModeData = (SmallValue*)GC_REALLOC(m_fastModeData, sizeof(SmallValue) * newLength);
                    }
                } else {
                    m_fastModeData = (SmallValue*)GC_REALLOC(m_fastModeData, sizeof(SmallValue) * newLength);

                    for (size_t i = oldLength; i < newLength; i++) {
                        m_fastModeData[i] = SmallValue(SmallValue::EmptyValue);
                    }
                }
                if (hasRD) {
                    rareData()->m_arrayObjectFastModeBufferCapacity = 0;
                }
            } else {
                const size_t minExpandCountForUsingLog2Function = 3;
                bool hasRD = hasRareData();
                size_t oldCapacity = hasRD ? (size_t)rareData()->m_arrayObjectFastModeBufferCapacity : oldLength;

                if (newLength) {
                    auto rd = ensureRareData();
                    if (newLength > oldCapacity) {
                        size_t newCapacity;
                        if (rd->m_arrayObjectFastModeBufferExpandCount >= minExpandCountForUsingLog2Function) {
                            ComputeReservedCapacityFunctionWithLog2<> f;
                            newCapacity = f(newLength);
                        } else {
                            ComputeReservedCapacityFunctionWithPercent<130> f;
                            newCapacity = f(newLength);
                        }
                        auto newFastModeData = (SmallValue*)GC_MALLOC(sizeof(SmallValue) * newCapacity);
                        memcpy(newFastModeData, m_fastModeData, sizeof(SmallValue) * oldLength);
                        GC_FREE(m_fastModeData);
                        m_fastModeData = newFastModeData;

                        for (size_t i = oldLength; i < newLength; i++) {
                            m_fastModeData[i] = SmallValue(SmallValue::EmptyValue);
                        }

                        rd->m_arrayObjectFastModeBufferCapacity = newCapacity;
                        if (rd->m_arrayObjectFastModeBufferExpandCount < minExpandCountForUsingLog2Function) {
                            rd->m_arrayObjectFastModeBufferExpandCount++;
                        }
                    } else {
                        for (size_t i = oldLength; i < newLength; i++) {
                            m_fastModeData[i] = SmallValue(SmallValue::EmptyValue);
                        }
                        rd->m_arrayObjectFastModeBufferCapacity = oldCapacity;
                    }
                } else {
                    GC_FREE(m_fastModeData);
                    m_fastModeData = nullptr;

                    if (hasRD) {
                        rareData()->m_arrayObjectFastModeBufferCapacity = 0;
                    }
                }
            }

            if (UNLIKELY(!isLengthPropertyWritable())) {
                convertIntoNonFastMode(state);
            }
        }
        return true;
    } else {
        int64_t oldLen = length(state);
        int64_t newLen = newLength;

        while (newLen < oldLen) {
            oldLen--;
            ObjectPropertyName key(state, oldLen);

            if (!getOwnProperty(state, key).hasValue()) {
                int64_t result;
                Object::nextIndexBackward(state, this, oldLen, -1, result);
                oldLen = result;

                if (oldLen < newLen) {
                    break;
                }

                key = ObjectPropertyName(state, Value(oldLen));
            }

            bool deleteSucceeded = deleteOwnProperty(state, key);
            if (!deleteSucceeded) {
                m_arrayLength = oldLen + 1;
                return false;
            }
        }
        m_arrayLength = newLength;
        return true;
    }
}

ObjectGetResult ArrayObject::getVirtualValue(ExecutionState& state, const ObjectPropertyName& P)
{
    if (!P.isUIntType() && P.objectStructurePropertyName() == state.context()->staticStrings().length) {
        return ObjectGetResult(Value(m_arrayLength), isLengthPropertyWritable(), false, false);
    }
    if (LIKELY(isFastModeArray())) {
        uint64_t idx = P.tryToUseAsArrayIndex();
        if (LIKELY(idx != Value::InvalidArrayIndexValue) && LIKELY(idx < getArrayLength(state))) {
            Value v = m_fastModeData[idx];
            if (LIKELY(!v.isEmpty())) {
                return ObjectGetResult(v, true, true, true);
            }
            return ObjectGetResult();
        }
    }
    return ObjectGetResult();
}

ObjectHasPropertyResult ArrayObject::hasIndexedProperty(ExecutionState& state, const Value& propertyName)
{
    if (LIKELY(isFastModeArray())) {
        uint32_t idx = propertyName.tryToUseAsArrayIndex(state);
        if (LIKELY(idx != Value::InvalidArrayIndexValue) && LIKELY(idx < getArrayLength(state))) {
            Value v = m_fastModeData[idx];
            if (LIKELY(!v.isEmpty())) {
                return ObjectHasPropertyResult(ObjectGetResult(v, true, true, true));
            }
        }
    }
    return hasProperty(state, ObjectPropertyName(state, propertyName));
}

ObjectGetResult ArrayObject::getIndexedProperty(ExecutionState& state, const Value& property)
{
    if (LIKELY(isFastModeArray())) {
        uint32_t idx = property.tryToUseAsArrayIndex(state);
        if (LIKELY(idx != Value::InvalidArrayIndexValue) && LIKELY(idx < getArrayLength(state))) {
            Value v = m_fastModeData[idx];
            if (LIKELY(!v.isEmpty())) {
                return ObjectGetResult(v, true, true, true);
            }
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

bool ArrayObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    // checking isUint32 to prevent invoke toString on property more than once while calling setIndexedProperty
    if (LIKELY(isFastModeArray() && property.isUInt32())) {
        uint32_t idx = property.tryToUseAsArrayIndex(state);
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = getArrayLength(state);
            if (UNLIKELY(len <= idx)) {
                if (UNLIKELY(!isExtensible(state))) {
                    return false;
                }
                if (UNLIKELY(!setArrayLength(state, idx + 1)) || UNLIKELY(!isFastModeArray())) {
                    return set(state, ObjectPropertyName(state, property), value, this);
                }
                // fast, non-fast mode can be changed while changing length
                if (LIKELY(isFastModeArray())) {
                    m_fastModeData[idx] = value;
                    return true;
                }
            } else {
                m_fastModeData[idx] = value;
                return true;
            }
        }
    }
    return set(state, ObjectPropertyName(state, property), value, this);
}

bool ArrayObject::preventExtensions(ExecutionState& state)
{
    // first, convert to non-fast-mode.
    // then, set preventExtensions
    convertIntoNonFastMode(state);
    return Object::preventExtensions(state);
}

ArrayIteratorObject::ArrayIteratorObject(ExecutionState& state, Object* a, Type type)
    : IteratorObject(state, state.context()->globalObject()->arrayIteratorPrototype())
    , m_array(a)
    , m_iteratorNextIndex(0)
    , m_type(type)
{
}

void* ArrayIteratorObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayIteratorObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayIteratorObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayIteratorObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayIteratorObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayIteratorObject, m_array));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> ArrayIteratorObject::advance(ExecutionState& state)
{
    // Let a be the value of the [[IteratedObject]] internal slot of O.
    Object* a = m_array;
    // If a is undefined, return CreateIterResultObject(undefined, true).
    if (a == nullptr) {
        return std::make_pair(Value(), true);
    }
    // Let index be the value of the [[ArrayIteratorNextIndex]] internal slot of O.
    size_t index = m_iteratorNextIndex;
    // Let itemKind be the value of the [[ArrayIterationKind]] internal slot of O.
    Type itemKind = m_type;

    size_t len;
    // If a has a [[TypedArrayName]] internal slot, then
    if (a->isTypedArrayObject()) {
        // If IsDetachedBuffer(a.[[ViewedArrayBuffer]]) is true, throw a TypeError exception.
        if (a->asArrayBufferView()->buffer()->isDetachedBuffer()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayIterator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_DetachedBuffer);
            return std::make_pair(Value(), false);
        }
        // Let len be a.[[ArrayLength]].
        len = a->asArrayBufferView()->arrayLength();
    } else {
        // Let len be ? ToLength(? Get(a, "length")).
        len = a->lengthES6(state);
    }

    // If index ≥ len, then
    if (index >= len) {
        // Set the value of the [[IteratedObject]] internal slot of O to undefined.
        m_array = nullptr;
        // Return CreateIterResultObject(undefined, true).
        return std::make_pair(Value(), true);
    }

    // Set the value of the [[ArrayIteratorNextIndex]] internal slot of O to index+1.
    m_iteratorNextIndex = index + 1;

    // If itemKind is "key", return CreateIterResultObject(index, false).
    if (itemKind == Type::TypeKey) {
        return std::make_pair(Value(index), false);
    } else {
        Value elementValue = a->getIndexedProperty(state, Value(index)).value(state, a);
        if (itemKind == Type::TypeValue) {
            return std::make_pair(elementValue, false);
        } else {
            ASSERT(itemKind == Type::TypeKeyValue);
            Value v[2] = { Value(index), elementValue };
            Value resultValue = Object::createArrayFromList(state, 2, v);
            return std::make_pair(resultValue, false);
        }
    }
}

ArrayPrototypeObject::ArrayPrototypeObject(ExecutionState& state)
    : ArrayObject(state, state.context()->globalObject()->objectPrototype())
{
}
}
