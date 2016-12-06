#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class CodeBlock;
class Context;

class Script : public gc {
    friend class ScriptParser;
    Script(String* fileName)
        : m_fileName(fileName)
        , m_topCodeBlock(nullptr)
    {
    }
public:
    struct ScriptExecuteResult {
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
    ScriptExecuteResult execute(Context* ctx);
    String* fileName()
    {
        return m_fileName;
    }
protected:
    String* m_fileName;
    CodeBlock* m_topCodeBlock;
};

}

#endif
