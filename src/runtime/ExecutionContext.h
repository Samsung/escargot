#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;
class LexicalEnvironment;
class EnvironmentRecord;
class Value;

class ExecutionContext : public gc {
    friend class FunctionObject;
    friend class ByteCodeInterpreter;
public:
    ExecutionContext(Context* context, LexicalEnvironment* lexicalEnvironment = nullptr, bool inStrictMode = false)
        : m_inStrictMode(inStrictMode)
        , m_context(context)
        , m_stackStorage(nullptr)
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

    bool inStrictMode()
    {
        return m_inStrictMode;
    }

private:
    Value* stackStorage()
    {
        return m_stackStorage;
    }

    void giveStackStorage(Value* storage)
    {
        ASSERT(m_stackStorage == nullptr);
        m_stackStorage = storage;
    }

    bool m_inStrictMode;
    Context* m_context;
    Value* m_stackStorage;
    LexicalEnvironment* m_lexicalEnvironment;
};

}

#endif
