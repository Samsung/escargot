/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __EscargotEsprima__
#define __EscargotEsprima__

#include "parser/ast/Node.h"
#include "parser/ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/String.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ProgramNode;
class CodeBlock;

namespace esprima {

#define ESPRIMA_START_IDENT 1
#define ESPRIMA_IS_IDENT 2
#define ESPRIMA_IS_WHITESPACE 4
#define ESPRIMA_IS_LINE_TERMINATOR 8

extern char g_asciiRangeCharMap[128];

// ECMA-262 11.2 White Space
NEVER_INLINE bool isWhiteSpaceSlowCase(char16_t ch);
ALWAYS_INLINE bool isWhiteSpace(char16_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & ESPRIMA_IS_WHITESPACE;
    }
    return isWhiteSpaceSlowCase(ch);
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & ESPRIMA_IS_LINE_TERMINATOR;
    }
    return UNLIKELY(ch == 0x2028 || ch == 0x2029);
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

#define ESPRIMA_RECURSIVE_LIMIT 1024

RefPtr<ProgramNode> parseProgram(::Escargot::Context* ctx, StringView source, bool strictFromOutside, size_t stackRemain);
std::tuple<RefPtr<Node>, ASTScopeContext*> parseSingleFunction(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock, size_t stackRemain);
}
}

#endif
