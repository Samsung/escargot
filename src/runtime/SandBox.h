#ifndef __EscargotSandBox__
#define __EscargotSandBox__

#include "parser/ast/Node.h"
#include "runtime/Context.h"

namespace Escargot {

class SandBox {
    MAKE_STACK_ALLOCATED();
    friend class ByteCodeInterpreter;

public:
    SandBox(Context* s)
    {
        m_context = s;
        m_context->m_sandBoxStack.pushBack(this);
    }

    ~SandBox()
    {
        ASSERT(m_context->m_sandBoxStack.back() == this);
        m_context->m_sandBoxStack.pop_back();
    }

    struct StackTraceData : public gc {
        String* fileName;
        ExtendedNodeLOC loc;
        StackTraceData()
            : fileName(String::emptyString)
            , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        {
        }
    };

    struct SandBoxResult {
        Value result;
        Value error;
        String* msgStr;
        Vector<StackTraceData, gc_malloc_allocator<StackTraceData>> stackTraceData;
        SandBoxResult()
            : result(Value::EmptyValue)
            , error(Value::EmptyValue)
            , msgStr(String::emptyString)
        {
        }
    };

    SandBoxResult run(const std::function<Value()>& scriptRunner); // for capsule script executing with try-catch
    void throwException(ExecutionState& state, Value exception);

protected:
    Context* m_context;
    Vector<std::pair<ExecutionContext*, StackTraceData>, gc_malloc_allocator<std::pair<ExecutionContext*, StackTraceData>>> m_stackTraceData;
    Value m_exception; // To avoid accidential GC of exception value
};
}

#endif
