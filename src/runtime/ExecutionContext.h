#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;

class ExecutionContext {
public:
    ExecutionContext(Context* context)
        : m_context(context)
    {
    }

    Context* context()
    {
        return m_context;
    }

private:
    Context* m_context;
};

}

#endif
