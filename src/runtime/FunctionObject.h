#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "runtime/Object.h"
#include "parser/CodeBlock.h"

namespace Escargot {

class CodeBlock;

class FunctionObject : public Object {
public:
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

    Value call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv);
protected:
    Value getFunctionPrototypeSlowCase(ExecutionState& state);
    bool m_isConstructor;
    CodeBlock* m_codeBlock;
};

}

#endif
