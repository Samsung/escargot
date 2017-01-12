#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "parser/CodeBlock.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class FunctionObject : public Object {
    friend class GlobalObject;
    enum ForBuiltin { __ForBuiltin__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin);
    void initFunctionObject(ExecutionState& state);
    FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin);

public:
    FunctionObject(ExecutionState& state, NativeFunctionInfo info, bool isConstructor = false);
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, bool isConstructor = true);

    Value getFunctionPrototype(ExecutionState& state)
    {
        ASSERT(m_isConstructor);
        return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER];
    }

    bool setFunctionPrototype(ExecutionState& state, const Value& v)
    {
        ASSERT(m_isConstructor);
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = v;
        return true;
    }

    bool isConstructor()
    {
        return m_isConstructor;
    }

    virtual bool isFunctionObject()
    {
        return true;
    }

    CodeBlock* codeBlock()
    {
        return m_codeBlock;
    }

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    Value call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false);
    ALWAYS_INLINE static Value call(ExecutionState& state, const Value& callee, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false)
    {
        if (LIKELY(callee.isFunction())) {
            return callee.asFunction()->call(state, receiver, argc, argv, isNewExpression);
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Function";
    }

protected:
    void generateBytecodeBlock(ExecutionState& state);
    bool m_isConstructor;
    CodeBlock* m_codeBlock;
    LexicalEnvironment* m_outerEnvironment;
};
}

#endif
