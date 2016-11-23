#ifndef __EscargotEsprima__
#define __EscargotEsprima__

#include "runtime/String.h"
#include "parser/ast/AST.h"

namespace Escargot {

namespace esprima {

typedef std::function<void (::Escargot::Node*, NodeLOC start, NodeLOC end)> ParserASTNodeHandler;

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

}


}

#endif
