#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "runtime/Object.h"
#include "parser/CodeBlock.h"

namespace Escargot {

class FunctionObject : public Object {
    friend class GlobalObject;
    enum ForBuiltin { __ForBuiltin__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin);
    void initFunctionObject(ExecutionState& state);
public:
    FunctionObject(ExecutionState& state, NativeFunctionInfo info, bool isConstructor = true);
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, bool isConstructor = true);

    Value getFunctionPrototype(ExecutionState& state)
    {
        ASSERT(m_isConstructor);
        if (LIKELY(isPlainObject())) {
            return m_values[1];
        } else {
            return getFunctionPrototypeSlowCase(state);
        }
    }

    bool setFunctionPrototype(ExecutionState& state, const Value& v)
    {
        ASSERT(m_isConstructor);
        if (LIKELY(isPlainObject())) {
            m_values[1] = v;
            return true;
        } else {
            return setFunctionPrototypeSlowCase(state, v);
        }
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

    Value call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false);
    static Value call(const Value& callee, ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false);
protected:
    Value getFunctionPrototypeSlowCase(ExecutionState& state);
    bool setFunctionPrototypeSlowCase(ExecutionState& state, const Value& v);
    bool m_isConstructor;
    CodeBlock* m_codeBlock;
};

}

#endif
