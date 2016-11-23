#ifndef __EscargotScriptParser__
#define __EscargotScriptParser__

#include "runtime/String.h"
#include "parser/Script.h"

namespace Escargot {

class ASTScopeContext;
class CodeBlock;
class Context;
class ProgramNode;
class Node;


class ScriptParser : public gc {
public:
    ScriptParser(Context* c);

    struct ScriptParseError : public gc {
        String* name;
        String* message;
        size_t index;
        size_t lineNumber;
        size_t column;
        String* description;
    };
    struct ScriptParserResult : public gc {
        ScriptParserResult(Script* script, ScriptParseError* error)
            : m_script(script)
            , m_error(error)
        {
        }

        Script* m_script;
        ScriptParseError* m_error;
    };

    ScriptParserResult parse(String* script)
    {
        return parse(StringView(script, 0, script->length()));
    }
    ScriptParserResult parse(StringView script);

protected:
    CodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, ProgramNode* program);
    CodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, ASTScopeContext* scopeCtx, CodeBlock* parentCodeBlock);

    Context* m_context;
};

}

#endif
