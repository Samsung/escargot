/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "ErrorObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "util/Util.h"

namespace Escargot {

size_t g_arrayObjectTag;

ArrayObject::ArrayObject(ExecutionState& state, bool hasSpreadElement)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, true)
{
    m_structure = state.context()->defaultStructureForArrayObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(0);
    Object::setPrototype(state, state.context()->globalObject()->arrayPrototype());

    if (UNLIKELY(state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() || hasSpreadElement)) {
        ensureObjectRareData()->m_isFastModeArrayObject = false;
    }
}

ArrayObject::ArrayObject(ExecutionState& state, double length)
    : ArrayObject(state)
{
    // If length is -0, let length be +0.
    if (length == 0 && std::signbit(length)) {
        length = +0.0;
    }
    // If length>2^32-1, throw a RangeError exception.
    if (length > ((1LL << 32LL) - 1LL)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
    }
    defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().length), ObjectPropertyDescriptor(Value(length),
                                                                                                                          (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
}

ObjectGetResult ArrayObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ObjectGetResult v = getFastModeValue(state, P);
    if (LIKELY(v.hasValue())) {
        return v;
    } else {
        return Object::getOwnProperty(state, P);
    }
}

bool ArrayObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(setFastModeValue(state, P, desc))) {
        return true;
    }

    uint64_t idx = P.tryToUseAsArrayIndex();
    auto oldLenDesc = structure()->readProperty(state, (size_t)0);
    uint32_t oldLen = getArrayLength(state);

    if (idx != Value::InvalidArrayIndexValue) {
        if ((idx >= oldLen) && !oldLenDesc.m_descriptor.isWritable())
            return false;
        bool succeeded = Object::defineOwnProperty(state, P, desc);
        if (!succeeded)
            return false;
        if (idx >= oldLen && ((idx + 1) <= Value::InvalidArrayIndexValue)) {
            return setArrayLength(state, idx + 1);
        }
        return true;
    } else if (P.toPropertyName(state).equals(state.context()->staticStrings().length.string())) {
        // See 3.a ~ 3.n on http://www.ecma-international.org/ecma-262/5.1/#sec-15.4.5.1
        if (desc.isValuePresent()) {
            ObjectPropertyDescriptor newLenDesc(desc);

            uint32_t newLen = desc.value().toLength(state);
            bool newWritable = false;

            if (newLen != desc.value().toNumber(state)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
            }

            newLenDesc.setValue(Value(newLen));

            if (newLen >= oldLen) {
                return Object::defineOwnProperty(state, P, newLenDesc);
            }

            if (!(oldLenDesc.m_descriptor.isWritable())) {
                return false;
            }

            if (!(newLenDesc.isWritablePresent()) || newLenDesc.isWritable()) {
                newWritable = true;
            } else {
                // In this case, even if an element of this array cannot be deleted,
                // it should not throw any error.
                // Object::defineOwnProperty() below can make call to ArrayObject::setArrayLength()
                // so, we set this variable to give the information to ArrayObject::setArrayLength()
                // not to throw error.
                ensureObjectRareData()->m_isInArrayObjectDefineOwnProperty = true;
                newWritable = false;
                newLenDesc.setWritable(true);
            }

            if (!(Object::defineOwnProperty(state, P, newLenDesc))) {
                if (isInArrayObjectDefineOwnProperty()) {
                    ASSERT(rareData());
                    rareData()->m_isInArrayObjectDefineOwnProperty = false;
                }
                return false;
            }

            while (newLen < oldLen) {
                oldLen--;

                bool deleteSucceeded = Object::deleteOwnProperty(state, ObjectPropertyName(state, Value(oldLen).toString(state)));
                if (!deleteSucceeded) {
                    newLenDesc.setValue(Value(oldLen + 1));
                    if (!newWritable) {
                        newLenDesc.setWritable(false);
                    }
                    Object::defineOwnProperty(state, P, newLenDesc);
                    if (isInArrayObjectDefineOwnProperty()) {
                        ASSERT(rareData());
                        rareData()->m_isInArrayObjectDefineOwnProperty = false;
                    }
                    return false;
                }
            }

            if (!newWritable) {
                ObjectPropertyDescriptor tmpDesc(ObjectPropertyDescriptor::NonWritablePresent, true);
                Object::defineOwnProperty(state, P, tmpDesc);
            }

            if (isInArrayObjectDefineOwnProperty()) {
                ASSERT(rareData());
                rareData()->m_isInArrayObjectDefineOwnProperty = false;
            }
            return true;
        } else {
            return Object::defineOwnProperty(state, P, desc);
        }
    }

    return Object::defineOwnProperty(state, P, desc);
}

bool ArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        uint64_t idx = P.tryToUseAsArrayIndex();
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint64_t len = getArrayLength(state);
            if (idx < len) {
                m_fastModeData[idx] = Value(Value::EmptyValue);
                ensureObjectRareData()->m_shouldUpdateEnumerateObjectData = true;
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
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

void ArrayObject::sort(ExecutionState& state, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    if (isFastModeArray()) {
        if (getArrayLength(state)) {
            size_t orgLength = getArrayLength(state);
            Value* tempBuffer = (Value*)GC_MALLOC_IGNORE_OFF_PAGE(sizeof(Value) * orgLength);

            for (size_t i = 0; i < orgLength; i++) {
                tempBuffer[i] = m_fastModeData[i];
            }

            if (orgLength) {
                TightVector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> tempSpace;
                tempSpace.resizeWithUninitializedValues(orgLength);

                mergeSort(tempBuffer, orgLength, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
                    *lessOrEqualp = comp(a, b);
                    return true;
                });
            }

            if (getArrayLength(state) != orgLength) {
                setArrayLength(state, orgLength);
            }

            if (isFastModeArray()) {
                for (size_t i = 0; i < orgLength; i++) {
                    m_fastModeData[i] = tempBuffer[i];
                }
            }
            GC_FREE(tempBuffer);
        }
        return;
    }
    Object::sort(state, comp);
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

    if (!structure()->isStructureWithFastAccess()) {
        m_structure = structure()->convertToWithFastAccess(state);
    }

    ensureObjectRareData()->m_isFastModeArrayObject = false;

    auto length = getArrayLength(state);
    for (size_t i = 0; i < length; i++) {
        if (!m_fastModeData[i].isEmpty()) {
            defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, Value(i)), ObjectPropertyDescriptor(m_fastModeData[i], ObjectPropertyDescriptor::AllPresent));
        }
    }

    m_fastModeData.clear();
}

bool ArrayObject::setArrayLength(ExecutionState& state, const uint64_t newLength)
{
    if (UNLIKELY(isFastModeArray() && (newLength > ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE))) {
        uint32_t orgLength = getArrayLength(state);
        if (newLength > orgLength && (newLength - orgLength > ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP)) {
            convertIntoNonFastMode(state);
        }
    }

    if (LIKELY(isFastModeArray())) {
        auto oldSize = getArrayLength(state);
        auto oldLenDesc = structure()->readProperty(state, (size_t)0);
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(newLength);
        m_fastModeData.resize(oldSize, newLength, Value(Value::EmptyValue));

        if (UNLIKELY(!oldLenDesc.m_descriptor.isWritable())) {
            convertIntoNonFastMode(state);
        }
        return true;
    } else {
        if (!isInArrayObjectDefineOwnProperty()) {
            auto oldLenDesc = structure()->readProperty(state, (size_t)0);

            int64_t oldLen = length(state);
            int64_t newLen = newLength;

            while (newLen < oldLen) {
                oldLen--;
                ObjectPropertyName key(state, Value(oldLen));

                if (!getOwnProperty(state, key).hasValue()) {
                    double result;
                    Object::nextIndexBackward(state, this, oldLen, -1, false, result);
                    oldLen = result;

                    if (oldLen < newLen) {
                        break;
                    }

                    key = ObjectPropertyName(state, Value(oldLen));
                }

                bool deleteSucceeded = deleteOwnProperty(state, key);
                if (!deleteSucceeded) {
                    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(oldLen + 1);
                    return false;
                }
            }
        }
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(newLength);
        return true;
    }
}

ObjectGetResult ArrayObject::getFastModeValue(ExecutionState& state, const ObjectPropertyName& P)
{
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

bool ArrayObject::setFastModeValue(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    if (LIKELY(isFastModeArray())) {
        uint64_t idx = P.tryToUseAsArrayIndex();
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = getArrayLength(state);
            if (len > idx && !m_fastModeData[idx].isEmpty()) {
                // Non-empty slot of fast-mode array always has {writable:true, enumerable:true, configurable:true}.
                // So, when new desciptor is not present, keep {w:true, e:true, c:true}
                if (UNLIKELY(!(desc.isValuePresentAlone() || desc.isDataWritableEnumerableConfigurable()))) {
                    convertIntoNonFastMode(state);
                    return false;
                }
            } else if (UNLIKELY(!desc.isDataWritableEnumerableConfigurable())) {
                // In case of empty slot property or over-lengthed property,
                // when new desciptor is not present, keep {w:false, e:false, c:false}
                convertIntoNonFastMode(state);
                return false;
            }

            if (!desc.isValuePresent()) {
                convertIntoNonFastMode(state);
                return false;
            }

            if (UNLIKELY(len <= idx)) {
                if (UNLIKELY(!isExtensible(state))) {
                    return false;
                }
                if (UNLIKELY(!setArrayLength(state, idx + 1)) || UNLIKELY(!isFastModeArray())) {
                    return false;
                }
            }
            m_fastModeData[idx] = desc.value();
            return true;
        }
    }
    return false;
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
            return get(state, ObjectPropertyName(state, property));
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

bool ArrayObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    if (LIKELY(isFastModeArray())) {
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
            }
            m_fastModeData[idx] = value;
            return true;
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
    : IteratorObject(state)
    , m_array(a)
    , m_iteratorNextIndex(0)
    , m_type(type)
{
    Object::setPrototype(state, state.context()->globalObject()->arrayIteratorPrototype());
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

    // If a has a [[TypedArrayName]] internal slot, then
    // Let len be the value of a's [[ArrayLength]] internal slot.
    // Else,
    // Let len be ? ToLength(? Get(a, "length")).
    auto len = a->lengthES6(state);

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
    } else if (itemKind == Type::TypeValue) {
        return std::make_pair(Value(a->getIndexedProperty(state, Value(index)).value(state, a)), false);
    } else {
        ArrayObject* result = new ArrayObject(state);
        result->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(Value(index), ObjectPropertyDescriptor::AllPresent));
        result->defineOwnProperty(state, ObjectPropertyName(state, Value(1)), ObjectPropertyDescriptor(Value(a->getIndexedProperty(state, Value(index)).value(state, a)), ObjectPropertyDescriptor::AllPresent));
        return std::make_pair(result, false);
    }
}
}
