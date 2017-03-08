#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class ExecutionContext;
class Value;

class ControlFlowRecord;

typedef Vector<ControlFlowRecord*, GCUtil::gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    Vector<ControlFlowRecord*, GCUtil::gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>>* m_controlFlowRecord;

    ExecutionStateRareData()
    {
        m_controlFlowRecord = nullptr;
    }
};

class ExecutionState : public gc {
    friend class ByteCodeInterpreter;

public:
    ExecutionState(Context* context, ExecutionContext* executionContext = nullptr)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_rareData(nullptr)
    {
        volatile int sp;
        m_stackBase = (size_t)&sp;
    }

    ExecutionState(Context* context, ExecutionState* parent, ExecutionContext* executionContext)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_stackBase(parent->stackBase())
        , m_rareData(nullptr)
    {
    }

    ExecutionState(ExecutionState* parent, ExecutionContext* executionContext)
        : m_context(parent->context())
        , m_executionContext(executionContext)
        , m_stackBase(parent->stackBase())
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

    size_t stackBase()
    {
        return m_stackBase;
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
    size_t m_stackBase;
    ExecutionStateRareData* m_rareData;
};
}

#endif
