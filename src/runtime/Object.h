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
    Value getOwnProperty(ExecutionState& state, const AtomicString& P);
    size_t findProperty(ExecutionState& state, String* P);
    size_t findProperty(ExecutionState& state, const AtomicString& P);

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
    bool defineOwnProperty(ExecutionState& state, const AtomicString& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc);
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


    // 9.1.7 [[HasProperty]](P)

    // http://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-get-p-receiver
    struct ObjectGetResult {
        bool m_hasValue;
        Value m_value;
        ObjectGetResult()
            : m_hasValue(false)
            , m_value(Value(Value::ForceUninitialized))
        {

        }

        ObjectGetResult(const Value& v)
            : m_hasValue(true)
            , m_value(v)
        {

        }
    };

    ObjectGetResult get(ExecutionState& state, String* P)
    {
        return get(state, P, this);
    }

    ObjectGetResult get(ExecutionState& state, const AtomicString& P)
    {
        return get(state, P, this);
    }

    ObjectGetResult get(ExecutionState& state, String* P, Object* receiver);
    ObjectGetResult get(ExecutionState& state, const AtomicString& P, Object* receiver);

    bool set(ExecutionState& state, String* P, const Value& v)
    {
        return set(state, P, v, this);
    }
    bool set(ExecutionState& state, String* P, const Value& v, Object* receiver);
    bool set(ExecutionState& state, const AtomicString& P, const Value& v)
    {
        return set(state, P, v, this);
    }
    bool set(ExecutionState& state, const AtomicString& P, const Value& v, Object* receiver);
protected:
    ObjectStructure* m_structure;
    ObjectRareData* m_rareData;
    Vector<SmallValue, gc_allocator_ignore_off_page<SmallValue>> m_values;

    bool checkPropertyAlreadyDefinedWithNonWritableInPrototype(ExecutionState& state, const AtomicString& P);
    void ensureObjectRareData()
    {
        if (m_rareData == nullptr) {
            m_rareData = new ObjectRareData();
        }
    }

    Value uncheckedGetOwnPlainDataProperty(ExecutionState& state, size_t idx)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isPlainDataProperty());
        return m_values[idx];
    }

    void uncheckedSetOwnPlainDataProperty(ExecutionState& state, size_t idx, const Value& newValue)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isPlainDataProperty());
        m_values[idx] = newValue;
    }

    Value getOwnDataProperty(ExecutionState& state, size_t idx)
    {
        return getOwnDataProperty(state, idx, this);
    }

    Value getOwnDataProperty(ExecutionState& state, size_t idx, Object* receiver)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            return m_values[idx];
        } else {
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this);
        }
    }

    void setOwnDataProperty(ExecutionState& state, size_t idx, const Value& newValue)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        m_values[idx] = newValue;
    }

    Value getOwnProperty(ExecutionState& state, size_t idx, Object* receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            return getOwnDataProperty(state, idx, receiver);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
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
            return false;
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
