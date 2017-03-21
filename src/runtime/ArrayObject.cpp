/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

size_t g_arrayObjectTag;

ArrayObject::ArrayObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, true)
{
    m_structure = state.context()->defaultStructureForArrayObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(0);
    setPrototype(state, state.context()->globalObject()->arrayPrototype());

    if (UNLIKELY(state.context()->didSomePrototypeObjectDefineIndexedProperty())) {
        ensureObjectRareData()->m_isFastModeArrayObject = false;
    }
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

    uint64_t idx;
    if (LIKELY(P.isUIntType())) {
        idx = P.uintValue();
    } else {
        idx = P.string(state)->tryToUseAsArrayIndex();
    }
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
    } else if (P.string(state)->equals(state.context()->staticStrings().length.string())) {
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
                ObjectPropertyDescriptor tmpDesc(ObjectPropertyDescriptor::NonWritablePresent);
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
        uint64_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint64_t len = getArrayLength(state);
            if (idx < len) {
                m_fastModeData[idx] = Value(Value::EmptyValue);
                return true;
            }
        }
    }
    return Object::deleteOwnProperty(state, P);
}

void ArrayObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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
    Object::enumeration(state, callback, data);
}

void ArrayObject::sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp)
{
    if (isFastModeArray()) {
        if (getArrayLength(state)) {
            std::vector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> values(&m_fastModeData[0], m_fastModeData.data() + getArrayLength(state));
            std::sort(values.begin(), values.end(), comp);
            for (size_t i = 0; i < values.size(); i++) {
                m_fastModeData[i] = values[i];
            }
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

bool ArrayObject::setArrayLength(ExecutionState& state, const uint64_t& newLength)
{
    ASSERT(isExtensible() || newLength <= getArrayLength(state));

    if (UNLIKELY(isFastModeArray() && (newLength > ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE))) {
        uint32_t orgLength = getArrayLength(state);
        if (newLength > orgLength) {
            if ((newLength - orgLength > ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP)) {
                convertIntoNonFastMode(state);
            }
        }
    }

    if (LIKELY(isFastModeArray())) {
        auto oldSize = getArrayLength(state);
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(newLength);
        m_fastModeData.resize(oldSize, newLength, Value(Value::EmptyValue));
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
                    oldLen = Object::nextIndexBackward(state, this, oldLen, -1, false);

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

            if (!oldLenDesc.m_descriptor.isWritable()) {
                return false;
            }
        }
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(newLength);
        return true;
    }
}

ObjectGetResult ArrayObject::getFastModeValue(ExecutionState& state, const ObjectPropertyName& P)
{
    if (LIKELY(isFastModeArray())) {
        uint64_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            if (LIKELY(idx < getArrayLength(state))) {
                Value v = m_fastModeData[idx];
                if (LIKELY(!v.isEmpty())) {
                    return ObjectGetResult(v, true, true, true);
                }
                return ObjectGetResult();
            }
        }
    }
    return ObjectGetResult();
}

bool ArrayObject::setFastModeValue(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    if (LIKELY(isFastModeArray())) {
        uint64_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
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
                if (UNLIKELY(!isExtensible())) {
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
        uint64_t idx;
        if (LIKELY(property.isUInt32())) {
            idx = property.asUInt32();
        } else {
            idx = property.toString(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            if (LIKELY(idx < getArrayLength(state))) {
                Value v = m_fastModeData[idx];
                if (LIKELY(!v.isEmpty())) {
                    return ObjectGetResult(v, true, true, true);
                }
                return get(state, ObjectPropertyName(state, property));
            }
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

bool ArrayObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    if (LIKELY(isFastModeArray())) {
        uint64_t idx;
        if (LIKELY(property.isUInt32())) {
            idx = property.asUInt32();
        } else {
            idx = property.toString(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = getArrayLength(state);
            if (UNLIKELY(len <= idx)) {
                if (UNLIKELY(!isExtensible())) {
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
}
