#include "Escargot.h"
#include "FunctionObject.h"
#include "ExecutionContext.h"
#include "Context.h"

namespace Escargot {

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, bool isConstructor)
    : Object(state)
    , m_isConstructor(isConstructor)
    , m_codeBlock(codeBlock)
{
    if (m_isConstructor) {
        m_structure = state.context()->defaultStructureForFunctionObject();
        m_values.pushBack(Value(new Object(state)));
        m_values.pushBack(Value(codeBlock->functionName().string()));
        m_values.pushBack(Value(codeBlock->functionParameters().size()));
    } else {
        m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
        m_values.pushBack(Value(codeBlock->functionName().string()));
        m_values.pushBack(Value(codeBlock->functionParameters().size()));
    }
}

Value FunctionObject::getFunctionPrototypeSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, state.context()->staticStrings().prototype);
}

}
