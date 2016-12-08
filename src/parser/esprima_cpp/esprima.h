#ifndef __EscargotEsprima__
#define __EscargotEsprima__

#include "parser/ast/Node.h"
#include "runtime/Context.h"
#include "runtime/String.h"

namespace Escargot {

class ProgramNode;
class CodeBlock;

namespace esprima {

typedef std::function<void(::Escargot::Node*, NodeLOC start, NodeLOC end)> ParserASTNodeHandler;

struct Error : public gc {
    String* name;
    String* message;
    size_t index;
    size_t lineNumber;
    size_t column;
    String* description;

    Error(String* message)
    {
        this->name = String::emptyString;
        this->message = message;
        this->column = this->lineNumber = this->index = 0;
        this->description = String::emptyString;
    }
};

ProgramNode* parseProgram(::Escargot::Context* ctx, StringView source, ParserASTNodeHandler astHandler);
Node* parseSingleFunction(::Escargot::Context* ctx, CodeBlock* codeBlock);
}
}

#endif
