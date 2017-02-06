#ifndef __EscargotArgumentsObject__
#define __EscargotArgumentsObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ExecutionContext;
class FunctionEnvironmentRecord;

extern size_t g_argumentsObjectTag;

class ArgumentsObject : public Object {
public:
    ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec);
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data);
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value);
    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Arguments";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    Vector<std::pair<SmallValue, ObjectStructurePropertyDescriptor>, gc_malloc_ignore_off_page_allocator<std::pair<SmallValue, ObjectStructurePropertyDescriptor>>> m_argumentPropertyInfo;
};
}

#endif
