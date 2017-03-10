/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotScriptParser__
#define __EscargotScriptParser__

#include "parser/Script.h"
#include "runtime/String.h"
#include "runtime/ErrorObject.h"

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

    ScriptParserResult parse(String* script, String* fileName = String::emptyString, bool strictFromOutside = false)
    {
        return parse(StringView(script, 0, script->length()), fileName, nullptr, strictFromOutside);
    }
    ScriptParserResult parse(StringView script, String* fileName = String::emptyString, CodeBlock* parentCodeBlock = nullptr, bool strictFromOutside = false);
    std::pair<Node*, ASTScopeContext*> parseFunction(CodeBlock* codeBlock);

protected:
    CodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program);
    CodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, CodeBlock* parentCodeBlock);
    void generateCodeBlockTreeFromASTWalkerPostProcess(CodeBlock* cb);

    Context* m_context;
};
}

#endif
