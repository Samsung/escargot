#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class VMInstance;
class ExecutionContext;
class Value;

class ExecutionState : public gc {
    MAKE_STACK_ALLOCATED();
public:
    ExecutionState(Context* context, ExecutionContext* executionContext = nullptr, Value* exeuctionResult = nullptr)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_exeuctionResult(exeuctionResult)
    {

    }

    Context* context()
    {
        return m_context;
    }

    ExecutionContext* executionContext()
    {
        return m_executionContext;
    }

    Value* exeuctionResult()
    {
        return m_exeuctionResult;
    }

    bool inStrictMode()
    {
        return m_executionContext->inStrictMode();
    }

    void throwException(const Value& e);

protected:
    Context* m_context;
    ExecutionContext* m_executionContext;
    Value* m_exeuctionResult;
};

}

#endif
