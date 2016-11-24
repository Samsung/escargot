#ifndef __EscargotObject__
#define __EscargotObject__

#include "runtime/PointerValue.h"
#include "util/Vector.h"
#include "runtime/SmallValue.h"
#include "runtime/ObjectStructure.h"

namespace Escargot {

struct ObjectRareData {
    bool m_isExtensible;
    bool m_isPlainObject; // tells it has __proto__ at first index
    bool m_isEverSetAsPrototypeObject;
    ObjectRareData()
    {
        m_isExtensible = true;
        m_isPlainObject = true;
        m_isEverSetAsPrototypeObject = false;
    }
};

class Object : public PointerValue {
    friend class Context;
public:
    Object(ExecutionState& state);

    virtual Type type()
    {
        return ObjectType;
    }

    virtual bool isObject()
    {
        return true;
    }

    Value getPrototype(ExecutionState& state)
    {
        if (LIKELY(isPlainObject())) {
            return m_values[0];
        } else {
            return getPrototypeSlowCase(state);
        }
    }

    void setPrototype(ExecutionState& state, const Value& value)
    {
        if (LIKELY(isPlainObject())) {
            m_values[0] = value;
        } else {
           setPrototypeSlowCase(state, value);
        }
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-isextensible
    bool isExtensible()
    {
        return m_rareData == nullptr ? true : m_rareData->m_isExtensible;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-preventextensions
    void preventExtensions()
    {
        ensureObjectRareData();
        m_rareData->m_isExtensible = true;
    }

    Value getOwnProperty(ExecutionState& state, String* P);

    class ObjectPropertyDescriptorForDefineOwnProperty {
    public:
        // for plain data property
        ObjectPropertyDescriptorForDefineOwnProperty(const Value& value, ObjectPropertyDescriptor::PresentAttribute attribute = ObjectPropertyDescriptor::AllPresent)
            : m_descriptor(ObjectPropertyDescriptor::createDataDescriptor(attribute))
            , m_value(value)
        {
        }

        // TODO
        // for native acc. data property
        // for js acc. property

        const Value& value() const
        {
            ASSERT(m_descriptor.isDataProperty());
            return m_value;
        }

        const ObjectPropertyDescriptor& descriptor() const
        {
            return m_descriptor;
        }
    protected:
        MAKE_STACK_ALLOCATED();
        ObjectPropertyDescriptor m_descriptor;
        Value m_value;
    };
    bool defineOwnProperty(ExecutionState& state, String* P, const ObjectPropertyDescriptorForDefineOwnProperty& desc);
    void defineOwnPropertyThrowsException(ExecutionState& state, String* P, const ObjectPropertyDescriptorForDefineOwnProperty& desc)
    {
        if (!defineOwnProperty(state, P, desc)) {
            // TODO throw exception
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void defineOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, String* P, const ObjectPropertyDescriptorForDefineOwnProperty& desc)
    {
        if (!defineOwnProperty(state, P, desc) && state.inStrictMode()) {
            // TODO throw exception
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

protected:
    ObjectStructure* m_structure;
    ObjectRareData* m_rareData;
    Vector<SmallValue, gc_allocator_ignore_off_page<SmallValue>> m_values;

    void ensureObjectRareData()
    {
        if (m_rareData == nullptr) {
            m_rareData = new ObjectRareData();
        }
    }

    Value uncheckedGetOwnProperty(size_t idx)
    {
        return m_values[idx];
    }

    void uncheckedSetOwnProperty(size_t idx, const Value& newValue)
    {
        m_values[idx] = newValue;
    }

    bool isPlainObject() const
    {
        if (LIKELY(m_rareData == nullptr)) {
            return true;
        } else {
            return m_rareData->m_isPlainObject;
        }
    }

    bool isEverSetAsPrototypeObject() const
    {
        if (LIKELY(m_rareData == nullptr)) {
            return true;
        } else {
            return m_rareData->m_isEverSetAsPrototypeObject;
        }
    }

    void markAsPrototypeObject()
    {
        ensureObjectRareData();
        m_rareData->m_isEverSetAsPrototypeObject = true;
    }

    Value getPrototypeSlowCase(ExecutionState& state);
    void setPrototypeSlowCase(ExecutionState& state, const Value& value);
};

}

#endif
