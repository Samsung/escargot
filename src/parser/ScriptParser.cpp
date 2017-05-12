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

#include "Escargot.h"
#include "ScriptParser.h"
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"
#include "parser/esprima_cpp/esprima.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"

namespace Escargot {

ScriptParser::ScriptParser(Context* c)
    : m_context(c)
{
}

InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock)
{
    InterpretedCodeBlock* codeBlock;
    if (parentCodeBlock == nullptr) {
        // globalBlock
        codeBlock = new InterpretedCodeBlock(ctx, script, source, scopeCtx->m_isStrict, ExtendedNodeLOC(1, 1, 0), scopeCtx->m_names,
                                             (CodeBlock::CodeBlockInitFlag)((scopeCtx->m_hasEval ? CodeBlock::CodeBlockHasEval : 0)
                                                                            | (scopeCtx->m_hasWith ? CodeBlock::CodeBlockHasWith : 0)
                                                                            | (scopeCtx->m_hasCatch ? CodeBlock::CodeBlockHasCatch : 0)
                                                                            | (scopeCtx->m_hasYield ? CodeBlock::CodeBlockHasYield : 0)));
    } else {
        bool isFE = scopeCtx->m_nodeType == FunctionExpression;
        bool isFD = scopeCtx->m_nodeType == FunctionDeclaration;

        if (scopeCtx->m_needsSpecialInitialize)
            isFD = false;

        codeBlock = new InterpretedCodeBlock(ctx, script, StringView(source, scopeCtx->m_locStart.index, scopeCtx->m_locEnd.index),
                                             scopeCtx->m_locStart,
                                             scopeCtx->m_isStrict,
                                             scopeCtx->m_functionName, scopeCtx->m_parameters, scopeCtx->m_names, parentCodeBlock,
                                             (CodeBlock::CodeBlockInitFlag)((scopeCtx->m_hasEval ? CodeBlock::CodeBlockHasEval : 0)
                                                                            | (scopeCtx->m_hasWith ? CodeBlock::CodeBlockHasWith : 0)
                                                                            | (scopeCtx->m_hasCatch ? CodeBlock::CodeBlockHasCatch : 0)
                                                                            | (scopeCtx->m_hasYield ? CodeBlock::CodeBlockHasYield : 0)
                                                                            | (scopeCtx->m_inCatch ? CodeBlock::CodeBlockInCatch : 0)
                                                                            | (scopeCtx->m_inWith ? CodeBlock::CodeBlockInWith : 0)
                                                                            | (isFE ? CodeBlock::CodeBlockIsFunctionExpression : 0)
                                                                            | (isFD ? CodeBlock::CodeBlockIsFunctionDeclaration : 0)
                                                                            | (scopeCtx->m_needsSpecialInitialize ? CodeBlock::CodeBlockIsFunctionDeclarationWithSpecialBinding : 0)));
    }

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

        if (codeBlock->hasEvalWithCatchYield() || codeBlock->isFunctionDeclarationWithSpecialBinding()) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                c->notifySelfOrChildHasEvalWithCatchYield();
                c = c->parentCodeBlock();
            }
        }

        bool hasCapturedIdentifier = false;
        AtomicString arguments = ctx->staticStrings().arguments;
        for (size_t i = 0; i < scopeCtx->m_usingNames.size(); i++) {
            AtomicString uname = scopeCtx->m_usingNames[i];
            if (uname == arguments) {
                const InterpretedCodeBlock::FunctionParametersInfoVector& pv = codeBlock->parametersInfomation();
                bool hasArgumentsParameter = false;
                for (size_t i = 0; i < pv.size(); i++) {
                    if (pv[i].m_name == arguments) {
                        hasArgumentsParameter = true;
                        break;
                    }
                }
                if (!hasArgumentsParameter) {
                    codeBlock->m_usesArgumentsObject = true;
                    if (!codeBlock->hasName(arguments)) {
                        CodeBlock::IdentifierInfo info;
                        info.m_indexForIndexedStorage = SIZE_MAX;
                        info.m_name = arguments;
                        info.m_needToAllocateOnStack = true;
                        info.m_isMutable = true;
                        codeBlock->m_identifierInfos.pushBack(info);
                    }

                    InterpretedCodeBlock* b = codeBlock;
                    if (b->parameterCount()) {
                        b->m_canAllocateEnvironmentOnStack = false;
                        for (size_t j = 0; j < b->parametersInfomation().size(); j++) {
                            for (size_t k = 0; k < b->identifierInfos().size(); k++) {
                                if (b->identifierInfos()[k].m_name == b->parametersInfomation()[j].m_name) {
                                    b->m_identifierInfos[k].m_needToAllocateOnStack = false;
                                    break;
                                }
                            }
                        }
                    }
                }
            } else if (!codeBlock->hasName(uname)) {
                InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
                while (c) {
                    if (c->tryCaptureIdentifiersFromChildCodeBlock(uname)) {
                        hasCapturedIdentifier = true;
                        break;
                    }
                    c = c->parentCodeBlock();
                }
            }
        }

        if (hasCapturedIdentifier) {
            InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
            while (c && c->m_canAllocateEnvironmentOnStack) {
                c->m_canAllocateEnvironmentOnStack = false;
                c = c->parentCodeBlock();
            }
        }
    }

    codeBlock->m_childBlocks.resizeWithUninitializedValues(scopeCtx->m_childScopes.size());
    for (size_t i = 0; i < scopeCtx->m_childScopes.size(); i++) {
        codeBlock->m_childBlocks[i] = generateCodeBlockTreeFromASTWalker(ctx, source, script, scopeCtx->m_childScopes[i], codeBlock);
    }

    return codeBlock;
}

// generate code blocks from AST
InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromAST(Context* ctx, StringView source, Script* script, ProgramNode* program)
{
    return generateCodeBlockTreeFromASTWalker(ctx, source, script, program->scopeContext(), nullptr);
}

void ScriptParser::generateCodeBlockTreeFromASTWalkerPostProcess(InterpretedCodeBlock* cb)
{
    for (size_t i = 0; i < cb->m_childBlocks.size(); i++) {
        generateCodeBlockTreeFromASTWalkerPostProcess(cb->m_childBlocks[i]);
    }
    cb->computeVariables();
    if (cb->m_identifierOnStackCount > VARIABLE_LIMIT || cb->m_identifierOnHeapCount > VARIABLE_LIMIT) {
        auto err = new esprima::Error(new ASCIIString("variable limit exceeded"));
        err->errorCode = ErrorObject::SyntaxError;
        err->lineNumber = cb->m_sourceElementStart.line;
        err->column = cb->m_sourceElementStart.column;
        err->index = cb->m_sourceElementStart.index;
        throw err;
    }
}

ScriptParser::ScriptParserResult ScriptParser::parse(StringView scriptSource, String* fileName, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, size_t stackSizeRemain)
{
    Script* script = nullptr;
    ScriptParseError* error = nullptr;
    try {
        ProgramNode* program = esprima::parseProgram(m_context, scriptSource, nullptr, strictFromOutside, stackSizeRemain);

        script = new Script(fileName, new StringView(scriptSource));
        InterpretedCodeBlock* topCodeBlock;
        if (parentCodeBlock) {
            program->scopeContext()->m_hasEval = parentCodeBlock->hasEval();
            program->scopeContext()->m_hasWith = parentCodeBlock->hasWith();
            program->scopeContext()->m_hasCatch = parentCodeBlock->hasCatch();
            program->scopeContext()->m_hasYield = parentCodeBlock->hasYield();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, scriptSource, script, program->scopeContext(), parentCodeBlock);
            topCodeBlock->m_isEvalCodeInFunction = true;
            topCodeBlock->m_isInWithScope = parentCodeBlock->m_isInWithScope;
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, scriptSource, script, program);
        }

        topCodeBlock->m_isEvalCodeInFunction = isEvalCodeInFunction;

        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);

        topCodeBlock->m_cachedASTNode = program;
        script->m_topCodeBlock = topCodeBlock;

// dump Code Block
#ifndef NDEBUG
        if (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE"))) {
            std::function<void(InterpretedCodeBlock*, size_t depth)> fn = [&fn](InterpretedCodeBlock* cb, size_t depth) {

#define PRINT_TAB()                      \
    for (size_t i = 0; i < depth; i++) { \
        printf("  ");                    \
    }

                PRINT_TAB()
                printf("CodeBlock %s %s (%d:%d -> %d:%d)(%s, %s) (E:%d, W:%d, C:%d, Y:%d, A:%d)\n", cb->m_functionName.string()->toUTF8StringData().data(),
                       cb->m_isStrict ? "Strict" : "",
                       (int)cb->m_locStart.line,
                       (int)cb->m_locStart.column,
                       (int)cb->m_locEnd.line,
                       (int)cb->m_locEnd.column,
                       cb->m_canAllocateEnvironmentOnStack ? "Stack" : "Heap",
                       cb->m_canUseIndexedVariableStorage ? "Indexed" : "Named",
                       (int)cb->m_hasEval, (int)cb->m_hasWith, (int)cb->m_hasCatch, (int)cb->m_hasYield, (int)cb->m_usesArgumentsObject);

                PRINT_TAB()
                printf("Names: ");
                for (size_t i = 0; i < cb->m_identifierInfos.size(); i++) {
                    printf("%s(%s, %s, %d), ", cb->m_identifierInfos[i].m_name.string()->toUTF8StringData().data(),
                           cb->m_identifierInfos[i].m_needToAllocateOnStack ? "Stack" : "Heap",
                           cb->m_identifierInfos[i].m_isMutable ? "Mutable" : "Inmmutable",
                           (int)cb->m_identifierInfos[i].m_indexForIndexedStorage);
                }
                puts("");

                PRINT_TAB()
                printf("Using Names: ");
                for (size_t i = 0; i < cb->m_scopeContext->m_usingNames.size(); i++) {
                    printf("%s, ", cb->m_scopeContext->m_usingNames[i].string()->toUTF8StringData().data());
                }
                puts("");

                for (size_t i = 0; i < cb->m_childBlocks.size(); i++) {
                    fn(cb->m_childBlocks[i], depth + 1);
                }

            };
            fn(topCodeBlock, 0);
        }
#endif

    } catch (esprima::Error* orgError) {
        script = nullptr;
        error = new ScriptParseError();
        error->column = orgError->column;
        error->description = orgError->description;
        error->index = orgError->index;
        error->lineNumber = orgError->lineNumber;
        error->message = orgError->message;
        error->name = orgError->name;
        error->errorCode = orgError->errorCode;
        delete orgError;
    }

    ScriptParser::ScriptParserResult result(script, error);
    return result;
}

std::pair<Node*, ASTScopeContext*> ScriptParser::parseFunction(InterpretedCodeBlock* codeBlock, size_t stackSizeRemain, ExecutionState* state)
{
    try {
        std::pair<Node*, ASTScopeContext*> body = esprima::parseSingleFunction(m_context, codeBlock, stackSizeRemain);
        return body;
    } catch (esprima::Error* orgError) {
        ErrorObject::throwBuiltinError(*state, ErrorObject::SyntaxError, orgError->message->toUTF8StringData().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
}
}
