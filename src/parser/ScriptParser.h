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

namespace Escargot {

struct ASTScopeContext;
class Script;
class CodeBlock;
class InterpretedCodeBlock;
class String;
class StringView;
class Context;
class ProgramNode;
class Node;

enum class ErrorCode : uint8_t;

namespace esprima {
    class Error;
};

#if defined(ENABLE_CODE_CACHE)
struct CodeBlockCacheInfo;
#endif

class ScriptParser : public gc {
public:
    explicit ScriptParser(Context* c);

    struct InitializeScriptResult {
        Optional<Script*> script;
        ErrorCode parseErrorCode;
        String* parseErrorMessage;

        InitializeScriptResult();
        Script* scriptThrowsExceptionIfParseError(ExecutionState& state);
    };

    InitializeScriptResult initializeScript(String* originSource, size_t originLineOffset, String* source, String* srcName, InterpretedCodeBlock* parentCodeBlock, bool isModule, bool isEvalMode = false, bool isEvalCodeInFunction = false, bool inWithOperation = false, bool strictFromOutside = false, bool allowSuperCall = false, bool allowSuperProperty = false, bool allowNewTarget = false, bool needByteCodeGeneration = true, size_t stackSizeRemain = SIZE_MAX);
    InitializeScriptResult initializeScript(String* source, String* srcName, bool isModule)
    {
        return initializeScript(nullptr, 0, source, srcName, nullptr, isModule);
    }

    Script* initializeJSONModule(String* source, String* srcName);

    Context* context() const { return m_context; }

    void generateFunctionByteCode(ExecutionState& state, InterpretedCodeBlock* codeBlock, size_t stackSizeRemain);

#if defined(ENABLE_CODE_CACHE)
    void setCodeBlockCacheInfo(CodeBlockCacheInfo* info);
    void deleteCodeBlockCacheInfo();
#endif

private:
    InterpretedCodeBlock* generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program, bool isEvalCode, bool isEvalCodeInFunction);
    InterpretedCodeBlock* generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock, bool isEvalCode, bool isEvalCodeInFunction);
    esprima::Error* generateCodeBlockTreeFromASTWalkerPostProcess(InterpretedCodeBlock* cb);
#ifndef NDEBUG
    void dumpCodeBlockTree(InterpretedCodeBlock* topCodeBlock);
#endif

#ifdef ESCARGOT_DEBUGGER
    void recursivelyGenerateChildrenByteCode(InterpretedCodeBlock* topCodeBlock);
    InitializeScriptResult initializeScriptWithDebugger(String* originSource, size_t originLineOffset, String* source, String* srcName, InterpretedCodeBlock* parentCodeBlock, bool isModule, bool isEvalMode, bool isEvalCodeInFunction, bool inWithOperation, bool strictFromOutside, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget);
#endif /* ESCARGOT_DEBUGGER */

    Context* m_context;

#if defined(ENABLE_CODE_CACHE)
    CodeBlockCacheInfo* m_codeBlockCacheInfo;
#endif
};
} // namespace Escargot

#endif
