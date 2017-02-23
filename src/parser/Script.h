#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class CodeBlock;
class Context;

class Script : public gc {
    friend class ScriptParser;
    friend class GlobalObject;
    Script(String* fileName, String* src)
        : m_fileName(fileName)
        , m_src(src)
        , m_topCodeBlock(nullptr)
    {
    }

public:
    struct ScriptSandboxExecuteResult {
        MAKE_STACK_ALLOCATED();
        Value result;
        String* msgStr;
        struct Error {
            Value errorValue;
            struct StackTrace {
                String* fileName;
                size_t line;
                size_t column;
            };
            Vector<StackTrace, GCUtil::gc_malloc_ignore_off_page_allocator<StackTrace>> stackTrace;
        } error;
    };
    Value execute(ExecutionState& state, bool isEvalMode = false, bool needNewEnv = false, bool isOnGlobal = false);
    ScriptSandboxExecuteResult sandboxExecute(Context* ctx); // execute using sandbox
    String* fileName()
    {
        return m_fileName;
    }

    CodeBlock* topCodeBlock()
    {
        return m_topCodeBlock;
    }

protected:
    Value executeLocal(ExecutionState& state, Value thisValue, bool isEvalMode = false, bool needNewEnv = false);
    String* m_fileName;
    String* m_src;
    CodeBlock* m_topCodeBlock;
};
}

#endif
