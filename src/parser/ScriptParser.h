/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

struct ASTScopeContext;
class CodeBlock;
class InterpretedCodeBlock;
class Context;
class ProgramNode;
class Node;

class ScriptParser : public gc {
public:
    explicit ScriptParser(Context* c);

    struct InitializeScriptResult {
        Optional<Script*> script;
        ErrorObject::Code parseErrorCode;
        String* parseErrorMessage;

        InitializeScriptResult()
            : parseErrorCode(ErrorObject::Code::None)
            , parseErrorMessage(String::emptyString)
        {
        }

        Script* scriptThrowsExceptionIfParseError(ExecutionState& state)
        {
            if (!script) {
                ErrorObject::throwBuiltinError(state, parseErrorCode, parseErrorMessage->toUTF8StringData().data());
            }

            return script.value();
        }
    };

    InitializeScriptResult initializeScript(StringView scriptSource, String* fileName, bool isModule, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, size_t stackSizeRemain, bool needByteCodeGeneration, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget);
    InitializeScriptResult initializeScript(String* scriptSource, String* fileName, bool isModule, bool strictFromOutside = false, bool isRunningEvalOnFunction = false, bool isEvalMode = false, size_t stackSizeRemain = SIZE_MAX)
    {
        return initializeScript(StringView(scriptSource, 0, scriptSource->length()), fileName, isModule, nullptr, strictFromOutside, isRunningEvalOnFunction, isEvalMode, false, stackSizeRemain, true, false, false, false);
    }

    void generateFunctionByteCode(ExecutionState& state, InterpretedCodeBlock* codeBlock, size_t stackSizeRemain);

private:
    InterpretedCodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program, bool isEvalCode, bool isEvalCodeInFunction);
    InterpretedCodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock, bool isEvalCode, bool isEvalCodeInFunction);
    void generateCodeBlockTreeFromASTWalkerPostProcess(InterpretedCodeBlock* cb);
#ifndef NDEBUG
    void dumpCodeBlockTree(InterpretedCodeBlock* topCodeBlock);
#endif

#ifdef ESCARGOT_DEBUGGER
    void recursivelyGenerateByteCode(InterpretedCodeBlock* topCodeBlock);
    InitializeScriptResult initializeScriptWithDebugger(StringView scriptSource, String* fileName, bool isModule, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget);
#endif /* ESCARGOT_DEBUGGER */

    Context* m_context;
};
}

#endif
