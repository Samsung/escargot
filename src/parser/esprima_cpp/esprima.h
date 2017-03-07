#ifndef __EscargotEsprima__
#define __EscargotEsprima__

#include "parser/ast/Node.h"
#include "runtime/Context.h"
#include "runtime/String.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ProgramNode;
class CodeBlock;

namespace esprima {

// ECMA-262 11.2 White Space
ALWAYS_INLINE bool isWhiteSpace(char16_t ch)
{
    return (ch == 0x20) || (ch == 0x09) || (ch == 0x0B) || (ch == 0x0C) || (ch == 0xA0)
        || (ch >= 0x1680 && (ch == 0x1680 || ch == 0x180E || ch == 0x2000 || ch == 0x2001
                             || ch == 0x2002 || ch == 0x2003 || ch == 0x2004 || ch == 0x2005 || ch == 0x2006
                             || ch == 0x2007 || ch == 0x2008 || ch == 0x2009 || ch == 0x200A || ch == 0x202F
                             || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF));
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    return (ch == 0x0A) || (ch == 0x0D) || (ch == 0x2028) || (ch == 0x2029);
}

typedef std::function<void(::Escargot::Node*, NodeLOC start, NodeLOC end)> ParserASTNodeHandler;

struct Error : public gc {
    String* name;
    String* message;
    size_t index;
    size_t lineNumber;
    size_t column;
    String* description;
    ErrorObject::Code errorCode;

    Error(String* message)
    {
        this->name = String::emptyString;
        this->message = message;
        this->column = this->lineNumber = this->index = 0;
        this->description = String::emptyString;
        this->errorCode = ErrorObject::Code::SyntaxError;
    }
};

#define ESPRIMA_RECURSIVE_LIMIT 512

ProgramNode* parseProgram(::Escargot::Context* ctx, StringView source, ParserASTNodeHandler astHandler, bool strictFromOutside);
std::pair<Node*, ASTScopeContext*> parseSingleFunction(::Escargot::Context* ctx, CodeBlock* codeBlock);
}
}

#endif
