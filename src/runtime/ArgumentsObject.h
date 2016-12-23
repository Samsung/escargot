#ifndef __EscargotArgumentsObject__
#define __EscargotArgumentsObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ExecutionContext;
class FunctionEnvironmentRecord;

class ArgumentsObject : public Object {
public:
    ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec);
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Arguments";
    }

protected:
};
}

#endif
