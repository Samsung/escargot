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

#include "Escargot.h"
#include "ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
#include "parser/esprima_cpp/esprima.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"

namespace Escargot {

ScriptParser::ScriptParser(Context* c)
    : m_context(c)
{
}

InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTFunctionScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock, bool isEvalCode, bool isEvalCodeInFunction)
{
    InterpretedCodeBlock* codeBlock;
    if (parentCodeBlock == nullptr) {
        // globalBlock
        codeBlock = new InterpretedCodeBlock(ctx, script, source, scopeCtx, ExtendedNodeLOC(1, 1, 0), isEvalCode, isEvalCodeInFunction);
    } else {
        codeBlock = new InterpretedCodeBlock(ctx, script, source, scopeCtx, scopeCtx->m_locStart, parentCodeBlock, isEvalCode, isEvalCodeInFunction);
    }

    // child scopes are don't need this
    isEvalCode = false;
    isEvalCodeInFunction = false;

#ifndef NDEBUG
    codeBlock->m_locStart = scopeCtx->m_locStart;
    codeBlock->m_locEnd = scopeCtx->m_locEnd;
    codeBlock->m_scopeContext = scopeCtx;
#endif

    if (parentCodeBlock) {
        if (scopeCtx->m_hasEvaluateBindingId) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                c->m_canAllocateEnvironmentOnStack = false;
                c = c->parentCodeBlock();
            }
        }

        if (!codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                c->m_hasDescendantUsesNonIndexedVariableStorage = true;
                c = c->parentCodeBlock();
            }
        }

        AtomicString arguments = ctx->staticStrings().arguments;

        for (size_t i = 0; i < scopeCtx->m_childBlockScopes.size(); i++) {
            for (size_t j = 0; j < scopeCtx->m_childBlockScopes[i]->m_usingNames.size(); j++) {
                AtomicString uname = scopeCtx->m_childBlockScopes[i]->m_usingNames[j];

                if (uname == arguments) {
                    if (UNLIKELY(codeBlock->hasParameter(arguments))) {
                        continue;
                    } else {
                        if (LIKELY(!codeBlock->isArrowFunctionExpression())) {
                            codeBlock->captureArguments();
                            codeBlock->markHeapAllocatedEnvironmentFromHere();
                            continue;
                        } else {
                            InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
                            while (c && !c->isGlobalScopeCodeBlock()) {
                                if (c->hasParameter(arguments)) {
                                    break;
                                } else if (!c->isArrowFunctionExpression()) {
                                    c->captureArguments();
                                    c->markHeapAllocatedEnvironmentFromHere();
                                    break;
                                }
                                c = c->parentCodeBlock();
                            }
                        }
                    }
                }

                bool usingNameIsResolvedOnCompileTime = false;
                if (codeBlock->canUseIndexedVariableStorage()) {
                    if (codeBlock->hasName(scopeCtx->m_childBlockScopes[i]->m_blockIndex, uname)) {
                        usingNameIsResolvedOnCompileTime = true;
                    } else {
                        auto parentBlockIndex = codeBlock->lexicalBlockIndexFunctionLocatedIn();
                        InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
                        while (c && parentBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                            if (c->tryCaptureIdentifiersFromChildCodeBlock(parentBlockIndex, uname)) {
                                codeBlock->markHeapAllocatedEnvironmentFromHere(scopeCtx->m_childBlockScopes[i]->m_blockIndex);
                                usingNameIsResolvedOnCompileTime = true;
                                break;
                            }
                            parentBlockIndex = c->lexicalBlockIndexFunctionLocatedIn();
                            c = c->parentCodeBlock();
                        }
                    }
                    if (!usingNameIsResolvedOnCompileTime && codeBlock->hasAncestorUsesNonIndexedVariableStorage()) {
                        // {Load, Store}ByName needs this
                        codeBlock->markHeapAllocatedEnvironmentFromHere(scopeCtx->m_childBlockScopes[i]->m_blockIndex);
                    }
                }
            }
        }
    }

    codeBlock->m_childBlocks.resizeWithUninitializedValues(scopeCtx->m_childScopes.size());
    for (size_t i = 0; i < scopeCtx->m_childScopes.size(); i++) {
        codeBlock->m_childBlocks[i] = generateCodeBlockTreeFromASTWalker(ctx, source, script, scopeCtx->m_childScopes[i], codeBlock, isEvalCode, isEvalCodeInFunction);
    }

    return codeBlock;
}

// generate code blocks from AST
InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program, bool isEvalCode, bool isEvalCodeInFunction)
{
    return generateCodeBlockTreeFromASTWalker(ctx, source, script, program->scopeContext(), nullptr, isEvalCode, isEvalCodeInFunction);
}

void ScriptParser::generateCodeBlockTreeFromASTWalkerPostProcess(InterpretedCodeBlock* cb)
{
    for (size_t i = 0; i < cb->m_childBlocks.size(); i++) {
        generateCodeBlockTreeFromASTWalkerPostProcess(cb->m_childBlocks[i]);
    }
    cb->computeVariables();
    if (cb->m_identifierOnStackCount > VARIABLE_LIMIT || cb->m_identifierOnHeapCount > VARIABLE_LIMIT || cb->m_lexicalBlockStackAllocatedIdentifierMaximumDepth > VARIABLE_LIMIT) {
        auto err = new esprima::Error(new ASCIIString("variable limit exceeded"));
        err->errorCode = ErrorObject::SyntaxError;
        err->lineNumber = cb->m_sourceElementStart.line;
        err->column = cb->m_sourceElementStart.column;
        err->index = cb->m_sourceElementStart.index;
        throw * err;
    }
}

Script* ScriptParser::initializeScript(ExecutionState& state, StringView scriptSource, String* fileName, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, size_t stackSizeRemain, bool needByteCodeGeneration)
{
    Script* script = new Script(fileName, new StringView(scriptSource));
    generateProgramCodeBlock(state, scriptSource, script, parentCodeBlock, strictFromOutside, isEvalCodeInFunction, isEvalMode, inWithOperation, stackSizeRemain, needByteCodeGeneration);

    return script;
}

void ScriptParser::generateProgramCodeBlock(ExecutionState& state, StringView scriptSource, Script* script, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, size_t stackSizeRemain, bool needByteCodeGeneration)
{
    ASSERT(script != nullptr);

    InterpretedCodeBlock* topCodeBlock = nullptr;
    RefPtr<ProgramNode> programNode;

    GC_disable();

    bool inWith = (parentCodeBlock ? parentCodeBlock->inWith() : false) || inWithOperation;

    // Parsing
    try {
        m_context->vmInstance()->m_parsedSourceCodes.push_back(scriptSource.string());
        programNode = esprima::parseProgram(m_context, scriptSource, strictFromOutside, inWith, stackSizeRemain);

        if (parentCodeBlock) {
            programNode->scopeContext()->m_hasEval = parentCodeBlock->hasEval();
            programNode->scopeContext()->m_hasWith = parentCodeBlock->hasWith();
            programNode->scopeContext()->m_hasYield = parentCodeBlock->hasYield();
            programNode->scopeContext()->m_isClassConstructor = parentCodeBlock->isClassConstructor();
            programNode->scopeContext()->m_isDerivedClassConstructor = parentCodeBlock->isDerivedClassConstructor();
            programNode->scopeContext()->m_isClassMethod = parentCodeBlock->isClassMethod();
            programNode->scopeContext()->m_isClassStaticMethod = parentCodeBlock->isClassStaticMethod();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, scriptSource, script, programNode->scopeContext(), parentCodeBlock, isEvalMode, isEvalCodeInFunction);
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, scriptSource, script, programNode.get(), isEvalMode, isEvalCodeInFunction);
        }
        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);

        script->m_topCodeBlock = topCodeBlock;

// dump Code Block
#ifndef NDEBUG
        if (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE"))) {
            dumpCodeBlockTree(topCodeBlock);
        }
#endif
    } catch (esprima::Error& orgError) {
        GC_enable();
        ErrorObject::throwBuiltinError(state, orgError.errorCode, orgError.message->toUTF8StringData().data());
        RELEASE_ASSERT_NOT_REACHED();
    }

    GC_enable();
    ASSERT(script->m_topCodeBlock == topCodeBlock);

    // Generate ByteCode
    if (LIKELY(needByteCodeGeneration)) {
        topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), topCodeBlock, programNode.get(), ((ProgramNode*)programNode.get())->scopeContext(), isEvalMode, !isEvalCodeInFunction, inWith);
    }
}

void ScriptParser::generateFunctionByteCode(ExecutionState& state, InterpretedCodeBlock* codeBlock, size_t stackSizeRemain)
{
    RefPtr<Node> body;
    ASTFunctionScopeContext* scopeContext = nullptr;

    // Parsing
    try {
        body = esprima::parseSingleFunction(m_context, codeBlock, scopeContext, stackSizeRemain);
    } catch (esprima::Error& orgError) {
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, orgError.message->toUTF8StringData().data());
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Generate ByteCode
    codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), codeBlock, body.get(), scopeContext, false, false, false, false);
}

#ifndef NDEBUG
void ScriptParser::dumpCodeBlockTree(InterpretedCodeBlock* topCodeBlock)
{
    std::function<void(InterpretedCodeBlock*, size_t depth)> fn = [&fn](InterpretedCodeBlock* cb, size_t depth) {

#define PRINT_TAB()                      \
    for (size_t i = 0; i < depth; i++) { \
        printf("  ");                    \
    }

#define PRINT_TAB_BLOCK()                \
    for (size_t i = 0; i < depth; i++) { \
        printf("  ");                    \
    }                                    \
    printf(" ");

        PRINT_TAB()
        printf("CodeBlock %s %s (%d:%d -> %d:%d, block %d)(%s, %s) (E:%d, W:%d, Y:%d, A:%d)\n", cb->m_functionName.string()->toUTF8StringData().data(),
               cb->m_isStrict ? "Strict" : "",
               (int)cb->m_locStart.line,
               (int)cb->m_locStart.column,
               (int)cb->m_locEnd.line,
               (int)cb->m_locEnd.column,
               (int)cb->lexicalBlockIndexFunctionLocatedIn(),
               cb->m_canAllocateEnvironmentOnStack ? "Stack" : "Heap",
               cb->m_canUseIndexedVariableStorage ? "Indexed" : "Named",
               (int)cb->m_hasEval, (int)cb->m_hasWith, (int)cb->m_hasYield, (int)cb->m_usesArgumentsObject);

        PRINT_TAB()
        printf("Names(var): ");
        for (size_t i = 0; i < cb->m_identifierInfos.size(); i++) {
            printf("%s(%s, %s, %d), ", cb->m_identifierInfos[i].m_name.string()->toUTF8StringData().data(),
                   cb->m_identifierInfos[i].m_needToAllocateOnStack ? "Stack" : "Heap",
                   cb->m_identifierInfos[i].m_isMutable ? "Mutable" : "Inmmutable",
                   (int)cb->m_identifierInfos[i].m_indexForIndexedStorage);
        }
        puts("");

        PRINT_TAB_BLOCK()
        printf("Block information: ");
        ASSERT(cb->m_scopeContext->m_childBlockScopes.size() == cb->m_blockInfos.size());
        for (size_t i = 0; i < cb->m_blockInfos.size(); i++) {
            puts("");
            PRINT_TAB_BLOCK()
            printf("Block %p %d:%d [%d parent(%d)]: %s, %s", cb->m_blockInfos[i], (int)cb->m_scopeContext->m_childBlockScopes[i]->m_loc.line, (int)cb->m_scopeContext->m_childBlockScopes[i]->m_loc.column, (int)cb->m_blockInfos[i]->m_blockIndex, (int)cb->m_blockInfos[i]->m_parentBlockIndex, cb->m_blockInfos[i]->m_canAllocateEnvironmentOnStack ? "Stack" : "Heap", cb->m_blockInfos[i]->m_shouldAllocateEnvironment ? "Allocated" : "Skipped");

            puts("");
            PRINT_TAB_BLOCK()
            printf("Names : ");

            for (size_t j = 0; j < cb->m_blockInfos[i]->m_identifiers.size(); j++) {
                printf("%s(%s, %s, %d), ", cb->m_blockInfos[i]->m_identifiers[j].m_name.string()->toUTF8StringData().data(),
                       cb->m_blockInfos[i]->m_identifiers[j].m_needToAllocateOnStack ? "Stack" : "Heap",
                       cb->m_blockInfos[i]->m_identifiers[j].m_isMutable ? "Mutable" : "Inmmutable",
                       (int)cb->m_blockInfos[i]->m_identifiers[j].m_indexForIndexedStorage);
            }

            puts("");
            PRINT_TAB_BLOCK()
            printf("Using names : ");
            for (size_t j = 0; j < cb->m_scopeContext->m_childBlockScopes[i]->m_usingNames.size(); j++) {
                printf("%s, ", cb->m_scopeContext->m_childBlockScopes[i]->m_usingNames[j].string()->toUTF8StringData().data());
            }
        }

        puts("");

        for (size_t i = 0; i < cb->m_childBlocks.size(); i++) {
            fn(cb->m_childBlocks[i], depth + 1);
        }

    };
    fn(topCodeBlock, 0);
}
#endif
}
