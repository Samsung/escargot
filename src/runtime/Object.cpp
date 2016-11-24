#include "Escargot.h"
#include "Object.h"
#include "ExecutionContext.h"
#include "Context.h"

namespace Escargot {

Object::Object(ExecutionState& state)
    : m_structure(state.context()->defaultStructureForObject())
    , m_rareData(nullptr)
{
    m_values.pushBack(Value());
}

Value Object::getPrototypeSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, state.context()->staticStrings().__proto__);
}

void Object::setPrototypeSlowCase(ExecutionState& state, const Value& value)
{
    defineOwnProperty(state, state.context()->staticStrings().__proto__, ObjectPropertyDescriptorForDefineOwnProperty(value));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-ordinarygetownproperty
Value Object::getOwnProperty(ExecutionState& state, String* P)
{
    size_t idx = m_structure->findProperty(state, P);
    if (LIKELY(idx != SIZE_MAX)) {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            if (LIKELY(!item.m_descriptor.isNativeAccessorProperty())) {
                return m_values[idx];
            } else {
                ObjectPropertyNativeGetterSetterData* data = item.m_descriptor.nativeGetterSetterData();
                return data->m_getter(state, this);
            }
        } else {
            // TODO
            // implement js getter, setter
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    return Value();
}

bool Object::defineOwnProperty(ExecutionState& state, String* P, const ObjectPropertyDescriptorForDefineOwnProperty& desc)
{
    if (isEverSetAsPrototypeObject()) {
        // TODO
        // implement bad time
        RELEASE_ASSERT_NOT_REACHED();
    }

    size_t oldIdx = m_structure->findProperty(state, P);
    if (oldIdx == SIZE_MAX) {
        if (UNLIKELY(!isExtensible()))
            return false;
        m_structure = m_structure->addProperty(state, P, desc.descriptor());
        // TODO implement JS getter setter
        ASSERT(desc.descriptor().isDataProperty());
        m_values.pushBack(desc.value());
        return true;
    } else {
        size_t idx = oldIdx;
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (!item.m_descriptor.isWritable())
            return false;

        if (item.m_descriptor != desc.descriptor()) {
            if (!item.m_descriptor.isConfigurable()) {
                return false;
            }

            m_structure = m_structure->removeProperty(state, idx);
            m_values.erase(idx);
            defineOwnProperty(state, P, desc);
            return true;
        }

        if (item.m_descriptor.isDataProperty()) {
            if (LIKELY(!item.m_descriptor.isNativeAccessorProperty())) {
                m_values[idx] = desc.value();
                return true;
            } else {
                ObjectPropertyNativeGetterSetterData* data = item.m_descriptor.nativeGetterSetterData();
                return data->m_setter(state, this, desc.value());
            }
        } else {
            // TODO
            // implement js getter, setter
            RELEASE_ASSERT_NOT_REACHED();
        }

        return true;
    }
}


}
