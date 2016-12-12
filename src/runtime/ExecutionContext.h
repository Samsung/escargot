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
    friend class SandBox;
    friend class Script;

public:
    ExecutionContext(Context* context, ExecutionContext* parent = nullptr, LexicalEnvironment* lexicalEnvironment = nullptr, bool inStrictMode = false)
        : m_inStrictMode(inStrictMode)
        , m_programCounter(nullptr)
        , m_context(context)
        , m_parent(parent)
        , m_stackStorage(nullptr)
        , m_lexicalEnvironment(lexicalEnvironment)
    {
    }

    Context* context()
    {
        return m_context;
    }

    ExecutionContext* parent()
    {
        return m_parent;
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

    size_t programCounter()
    {
        if (m_programCounter)
            return *m_programCounter;
        else
            return 0;
    }

    bool m_inStrictMode;
    size_t* m_programCounter;
    Context* m_context;
    ExecutionContext* m_parent;
    Value* m_stackStorage;
    LexicalEnvironment* m_lexicalEnvironment;
};
}

#endif
