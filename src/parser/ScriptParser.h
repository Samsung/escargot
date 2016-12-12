#ifndef __EscargotScriptParser__
#define __EscargotScriptParser__

#include "parser/Script.h"
#include "runtime/String.h"

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

    ScriptParserResult parse(String* script, String* fileName = String::emptyString)
    {
        return parse(StringView(script, 0, script->length()), fileName);
    }
    ScriptParserResult parse(StringView script, String* fileName = String::emptyString, CodeBlock* parentCodeBlock = nullptr);
    Node* parseFunction(CodeBlock* codeBlock);

protected:
    CodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program);
    CodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, CodeBlock* parentCodeBlock);

    Context* m_context;
};
}

#endif
