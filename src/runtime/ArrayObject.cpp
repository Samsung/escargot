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
}

Value ArrayObject::getLengthSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().length)).value();
}

bool ArrayObject::setLengthSlowCase(ExecutionState& state, const Value& value)
{
    return defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().length), ObjectPropertyDescriptorForDefineOwnProperty(value));
}

Object::ObjectGetResult ArrayObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        uint32_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            ASSERT(m_fastModeData.size() == getLength(state));
            if (LIKELY(idx < m_fastModeData.size())) {
                Value v = m_fastModeData[idx];
                if (LIKELY(!v.isEmpty())) {
                    return Object::ObjectGetResult(v, true, true, true);
                }
                return Object::ObjectGetResult();
            }
        }
    }
    return Object::getOwnProperty(state, P);
}

bool ArrayObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        uint32_t idx;
        if (LIKELY(P.isUIntType())) {
            idx = P.uintValue();
        } else {
            idx = P.string(state)->tryToUseAsArrayIndex();
        }
        if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
            if (UNLIKELY(!desc.descriptor().isPlainDataWritableEnumerableConfigurable())) {
                convertIntoNonFastMode();
                goto NonFastMode;
            }
            uint32_t len = m_fastModeData.size();
            if (UNLIKELY(len <= idx)) {
                if (UNLIKELY(!setArrayLength(state, idx + 1))) {
                    goto NonFastMode;
                }
            }
            ASSERT(m_fastModeData.size() == getLength(state));
            m_fastModeData[idx] = desc.value();
            return true;
        }
    }
NonFastMode:
    return Object::defineOwnProperty(state, P, desc);
}

void ArrayObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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
            ASSERT(len == getLength(state));
            if (idx < len) {
                m_fastModeData[len] = Value(Value::EmptyValue);
                return;
            }
        }
    }
    return Object::deleteOwnProperty(state, P);
}

void ArrayObject::enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectPropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (LIKELY(isFastModeArray())) {
        size_t len = m_fastModeData.size();
        for (size_t i = 0; i < len; i++) {
            if (m_fastModeData[i].isEmpty())
                continue;
            if (!callback(ObjectPropertyName(state, Value(i)), ObjectPropertyDescriptor::createDataDescriptor(ObjectPropertyDescriptor::AllPresent))) {
                return;
            }
        }
    }
    Object::enumeration(state, callback);
}
}
