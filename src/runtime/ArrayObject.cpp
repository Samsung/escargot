#include "Escargot.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

ArrayObject::ArrayObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, true)
{
    m_structure = state.context()->defaultStructureForArrayObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(0);
    setPrototype(state, state.context()->globalObject()->arrayPrototype());

    if (UNLIKELY(state.context()->didSomePrototypeObjectDefineIndexedProperty())) {
        ensureObjectRareData()->m_isFastModeArrayObject = false;
        ASSERT(m_fastModeData.size() == 0);
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

    uint64_t idx = P.toValue(state).toArrayIndex(state);
    if (idx != Value::InvalidArrayIndexValue) {
        auto oldLenDesc = structure()->readProperty(state, (size_t)0);
        uint32_t oldLen = getArrayLength(state);
        if ((idx >= oldLen) && !oldLenDesc.m_descriptor.isWritable())
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
    if (LIKELY(isFastModeArray())) {
        uint64_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint64_t len = m_fastModeData.size();
            ASSERT(len == getArrayLength(state));
            if (idx < len) {
                m_fastModeData[idx] = Value(Value::EmptyValue);
                return true;
            }
        }
    }
    return Object::deleteOwnProperty(state, P);
}

void ArrayObject::enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        size_t len = m_fastModeData.size();
        for (size_t i = 0; i < len; i++) {
            ASSERT(isFastModeArray());
            if (m_fastModeData[i].isEmpty())
                continue;
            if (!callback(ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent))) {
                return;
            }
        }
    }
    Object::enumeration(state, callback);
}

void ArrayObject::sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp)
{
    if (isFastModeArray()) {
        if (m_fastModeData.size()) {
            std::vector<Value, gc_malloc_ignore_off_page_allocator<Value>> values(&m_fastModeData[0], m_fastModeData.data() + m_fastModeData.size());
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

    for (size_t i = 0; i < m_fastModeData.size(); i++) {
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
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(newLength);
        m_fastModeData.resize(newLength, Value(Value::EmptyValue));
        return true;
    } else {
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
            ASSERT(m_fastModeData.size() == getArrayLength(state));
            if (LIKELY(idx < m_fastModeData.size())) {
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
            idx = P.toValue(state).toArrayIndex(state);
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = m_fastModeData.size();
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
            ASSERT(m_fastModeData.size() == getArrayLength(state));
            m_fastModeData[idx] = desc.value();
            return true;
        }
    }
    return false;
}
}
