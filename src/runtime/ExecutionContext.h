#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;

class ExcutionContext {
    MAKE_STACK_ALLOCATED();
public:
    ExcutionContext(Context* context)
        : m_context(context)
    {
    }

private:
    Context* m_context;
};

}

#endif
