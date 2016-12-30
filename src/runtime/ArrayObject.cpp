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

    uint32_t idx = P.toValue(state).toArrayIndex(state);
    if (idx != Value::InvalidArrayIndexValue) {
        setArrayLength(state, idx + 1);
    }

    return Object::defineOwnProperty(state, P, desc);
}

bool ArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        uint32_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            uint32_t len = m_fastModeData.size();
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
}
