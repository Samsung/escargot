#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class VMInstance;
class ExecutionContext;
class Value;

class ControlFlowRecord;

typedef Vector<ControlFlowRecord*, gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    Vector<ControlFlowRecord*, gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>>* m_controlFlowRecord;

    ExecutionStateRareData()
    {
        m_controlFlowRecord = nullptr;
    }
};

class ExecutionState : public gc {
    friend class ByteCodeInterpreter;
    MAKE_STACK_ALLOCATED();

public:
    ExecutionState(Context* context, ExecutionContext* executionContext = nullptr, Value* exeuctionResult = nullptr)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_exeuctionResult(exeuctionResult)
        , m_rareData(nullptr)
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

    ExecutionStateRareData* ensureRareData()
    {
        if (m_rareData == nullptr)
            m_rareData = new ExecutionStateRareData();
        return m_rareData;
    }

    ExecutionStateRareData* rareData()
    {
        return m_rareData;
    }

protected:
    Context* m_context;
    ExecutionContext* m_executionContext;
    Value* m_exeuctionResult;
    ExecutionStateRareData* m_rareData;
};
}

#endif
