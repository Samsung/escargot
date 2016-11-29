#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class CodeBlock;
class Context;

class Script : public gc {
public:
    Script(CodeBlock* topCodeBlock)
        : m_topCodeBlock(topCodeBlock)
    {
    }

    struct ScriptExecuteResult {
        MAKE_STACK_ALLOCATED();
        Value result;
    };
    ScriptExecuteResult execute(Context* ctx);
protected:
    CodeBlock* m_topCodeBlock;
};

}

#endif
