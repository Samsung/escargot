#ifndef __EscargotStringObject__
#define __EscargotStringObject__

#include "runtime/Object.h"

namespace Escargot {

class StringObject : public Object {
public:
    StringObject(ExecutionState& state, String* value = String::emptyString);

    String* primitiveValue()
    {
        return m_primitiveValue;
    }

    virtual bool isStringObject()
    {
        return true;
    }

    void setPrimitiveValue(ExecutionState& state, String* data)
    {
        m_primitiveValue = data;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint32_t length(ExecutionState& state)
    {
        return m_primitiveValue->length();
    }

protected:
    String* m_primitiveValue;
};
}

#endif
