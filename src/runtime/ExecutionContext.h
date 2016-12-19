#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;
class LexicalEnvironment;
class EnvironmentRecord;
class Value;
class ControlFlowRecord;

struct ExecutionContextRareData : public gc {
    Vector<ControlFlowRecord*, gc_malloc_ignore_off_page_allocator<ExecutionContextRareData*>> m_controlFlowRecord;
};

class ExecutionContext : public gc {
    friend class FunctionObject;
    friend class ByteCodeInterpreter;
    friend class SandBox;
    friend class Script;

public:
    ExecutionContext(Context* context, ExecutionContext* parent = nullptr, LexicalEnvironment* lexicalEnvironment = nullptr, bool inStrictMode = false)
        : m_inStrictMode(inStrictMode)
        , m_context(context)
        , m_parent(parent)
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
    bool m_inStrictMode;
    Context* m_context;
    ExecutionContext* m_parent;
    LexicalEnvironment* m_lexicalEnvironment;
    ExecutionContextRareData* m_rareData;
};
}

#endif
