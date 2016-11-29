#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;
class LexicalEnvironment;
class EnvironmentRecord;

class ExecutionContext {
public:
    ExecutionContext(Context* context, LexicalEnvironment* lexicalEnvironment = nullptr)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
    {

    }

    Context* context()
    {
        return m_context;
    }

    LexicalEnvironment* lexicalEnvironment()
    {
        return m_lexicalEnvironment;
    }

private:
    Context* m_context;
    LexicalEnvironment* m_lexicalEnvironment;
};

}

#endif
