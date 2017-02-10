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

    virtual bool isStringObject() const
    {
        return true;
    }

    void setPrimitiveValue(ExecutionState& state, String* data)
    {
        m_primitiveValue = data;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual uint64_t length(ExecutionState& state)
    {
        return m_primitiveValue->length();
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "String";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;


protected:
    String* m_primitiveValue;
};
}

#endif
