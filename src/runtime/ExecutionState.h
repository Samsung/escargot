#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class VMInstance;

class ExecutionState : public gc {
    MAKE_STACK_ALLOCATED();
public:
    ExecutionState(Context* context)
        : m_context(context)
    {
    }

    Context* context()
    {
        return m_context;
    }

    bool inStrictMode()
    {
        // TODO
        return false;
    }
protected:
    Context* m_context;
};

}

#endif
