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

#ifndef __EscargotScriptParser__
#define __EscargotScriptParser__

#include "parser/Script.h"
#include "runtime/String.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ASTScopeContext;
class CodeBlock;
class InterpretedCodeBlock;
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
        ErrorObject::Code errorCode;
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

    ScriptParserResult parse(String* script, String* fileName = String::emptyString, bool strictFromOutside = false, bool isEvalCodeInFunction = false, size_t stackSizeRemain = SIZE_MAX)
    {
        return parse(StringView(script, 0, script->length()), fileName, nullptr, strictFromOutside, isEvalCodeInFunction, stackSizeRemain);
    }
    ScriptParserResult parse(StringView script, String* fileName = String::emptyString, InterpretedCodeBlock* parentCodeBlock = nullptr, bool strictFromOutside = false, bool isEvalCodeInFunction = false, size_t stackSizeRemain = SIZE_MAX);
    std::pair<Node*, ASTScopeContext*> parseFunction(InterpretedCodeBlock* codeBlock, size_t stackSizeRemain, ExecutionState* state = nullptr);

protected:
    InterpretedCodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program);
    InterpretedCodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock);
    void generateCodeBlockTreeFromASTWalkerPostProcess(InterpretedCodeBlock* cb);

    Context* m_context;
};
}

#endif
