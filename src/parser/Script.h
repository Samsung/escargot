#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class CodeBlock;
class Context;

class Script : public gc {
    friend class ScriptParser;
    friend class GlobalObject;
    Script(String* fileName)
        : m_fileName(fileName)
        , m_topCodeBlock(nullptr)
    {
    }

public:
    struct ScriptSandboxExecuteResult {
        MAKE_STACK_ALLOCATED();
        Value result;
        struct Error {
            Value errorValue;
            struct StackTrace {
                String* fileName;
                size_t line;
                size_t column;
            };
            Vector<StackTrace, gc_malloc_ignore_off_page_allocator<StackTrace>> stackTrace;
        } error;
    };
    Value execute(Context* ctx);
    ScriptSandboxExecuteResult sandboxExecute(Context* ctx); // execute using sandbox
    String* fileName()
    {
        return m_fileName;
    }

protected:
    Value executeLocal(ExecutionState& state);
    String* m_fileName;
    CodeBlock* m_topCodeBlock;
};
}

#endif
