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

#include "Escargot.h"
#include "ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
#include "parser/esprima_cpp/esprima.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "debugger/Debugger.h"

namespace Escargot {

ScriptParser::ScriptParser(Context* c)
    : m_context(c)
{
}

InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock, bool isEvalCode, bool isEvalCodeInFunction)
{
    InterpretedCodeBlock* codeBlock;
    if (parentCodeBlock == nullptr) {
        // globalBlock
        codeBlock = new InterpretedCodeBlock(ctx, script, source, scopeCtx, isEvalCode, isEvalCodeInFunction);
    } else {
        codeBlock = new InterpretedCodeBlock(ctx, script, source, scopeCtx, parentCodeBlock, isEvalCode, isEvalCodeInFunction);
    }

    // child scopes are don't need this
    isEvalCode = false;
    isEvalCodeInFunction = false;

#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    codeBlock->m_bodyEndLOC = scopeCtx->m_bodyEndLOC;
#endif

    if (parentCodeBlock) {
        if (!codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                c->m_hasDescendantUsesNonIndexedVariableStorage = true;
                c = c->parentCodeBlock();
            }
        }

        if (scopeCtx->m_hasThisExpression && scopeCtx->m_isArrowFunctionExpression) {
            // every arrow function should save this value of upper env.
            // except arrow function is localed on class constructor(class constructor needs test of this binding is valid)
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                if (c->isKindOfFunction()) {
                    if (c->isArrowFunctionExpression()) {
                        // pass
                    } else if (c->isClassConstructor()) {
                        c->m_canAllocateEnvironmentOnStack = false;
                        break;
                    } else {
                        break;
                    }
                }
                c = c->parentCodeBlock();
            }
        }

        if (scopeCtx->m_hasSuperOrNewTarget) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                if (c->isKindOfFunction() && !c->isArrowFunctionExpression()) { // ThisEnvironment
                    c->m_canAllocateEnvironmentOnStack = false;
                    break;
                }
                c = c->parentCodeBlock();
            }
        }

        AtomicString arguments = ctx->staticStrings().arguments;

        for (size_t i = 0; i < scopeCtx->m_childBlockScopes.size(); i++) {
            for (size_t j = 0; j < scopeCtx->m_childBlockScopes[i]->m_usingNames.size(); j++) {
                AtomicString uname = scopeCtx->m_childBlockScopes[i]->m_usingNames[j];
                auto blockIndex = scopeCtx->m_childBlockScopes[i]->m_blockIndex;
                if (uname == arguments) {
                    if (UNLIKELY(codeBlock->hasParameterName(arguments))) {
                        continue;
                    } else {
                        bool hasKindOfArgumentsHolderOnAncestors = false;

                        InterpretedCodeBlock* argumentsObjectHolder = codeBlock;
                        while (argumentsObjectHolder) {
                            if (argumentsObjectHolder->isKindOfFunction() && !argumentsObjectHolder->isArrowFunctionExpression()) {
                                hasKindOfArgumentsHolderOnAncestors = true;
                                break;
                            }
                            argumentsObjectHolder = argumentsObjectHolder->parentCodeBlock();
                        }

                        if (hasKindOfArgumentsHolderOnAncestors && !argumentsObjectHolder->hasParameterName(arguments)) {
                            InterpretedCodeBlock* argumentsVariableHolder = nullptr;
                            InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
                            while (c && !c->isGlobalScopeCodeBlock()) {
                                if (c->hasParameterName(arguments)) {
                                    argumentsVariableHolder = c;
                                    break;
                                } else if (!c->isArrowFunctionExpression()) {
                                    argumentsVariableHolder = c;
                                    break;
                                }
                                c = c->parentCodeBlock();
                            }

                            if (LIKELY(!codeBlock->isArrowFunctionExpression())) {
                                codeBlock->captureArguments();
                                continue;
                            } else {
                                InterpretedCodeBlock* p = codeBlock;
                                while (p) {
                                    if (!p->isArrowFunctionExpression()) {
                                        break;
                                    }
                                    p->m_usesArgumentsObject = true;
                                    p = p->parentCodeBlock();
                                }
                                if (argumentsVariableHolder) {
                                    argumentsVariableHolder->captureArguments();
                                }
                                if (!codeBlock->isKindOfFunction() || codeBlock->isArrowFunctionExpression()) {
                                    codeBlock->markHeapAllocatedEnvironmentFromHere(blockIndex, argumentsVariableHolder);
                                }
                            }
                        }
                    }
                }

                bool usingNameIsResolvedOnCompileTime = false;
                if (codeBlock->canUseIndexedVariableStorage()) {
                    if (codeBlock->hasName(blockIndex, uname)) {
                        usingNameIsResolvedOnCompileTime = true;
                    } else {
                        auto parentBlockIndex = codeBlock->lexicalBlockIndexFunctionLocatedIn();
                        InterpretedCodeBlock* c = codeBlock->parentCodeBlock();
                        while (c && parentBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                            auto r = c->tryCaptureIdentifiersFromChildCodeBlock(parentBlockIndex, uname);

                            if (r.first) {
                                usingNameIsResolvedOnCompileTime = true;
                                // if variable is global variable, we don't need to capture it
                                if (!codeBlock->hasAncestorUsesNonIndexedVariableStorage() && !c->isKindOfFunction() && (r.second == SIZE_MAX || c->blockInfos()[r.second]->m_parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX)) {
                                } else {
                                    if (r.second == SIZE_MAX) {
                                        // captured variable is `var` declared variable
                                        c->markHeapAllocatedEnvironmentFromHere(LEXICAL_BLOCK_INDEX_MAX, c);
                                    } else {
                                        // captured variable is `let` declared variable
                                        c->markHeapAllocatedEnvironmentFromHere(r.second, c);
                                    }
                                }
                                break;
                            }
                            parentBlockIndex = c->lexicalBlockIndexFunctionLocatedIn();
                            c = c->parentCodeBlock();
                        }
                    }
                }
            }
        }
    }

    ASTScopeContext* child = scopeCtx->firstChild();
    InterpretedCodeBlock* refer = nullptr;
    while (child) {
        InterpretedCodeBlock* newBlock = generateCodeBlockTreeFromASTWalker(ctx, source, script, child, codeBlock, isEvalCode, isEvalCodeInFunction);
        codeBlock->appendChild(newBlock, refer);
        refer = newBlock;
        child = child->nextSibling();
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
    InterpretedCodeBlock* child = cb->firstChild();
    while (child) {
        generateCodeBlockTreeFromASTWalkerPostProcess(child);
        child = child->nextSibling();
    }
    cb->computeVariables();
    if (cb->m_identifierOnStackCount > VARIABLE_LIMIT || cb->m_identifierOnHeapCount > VARIABLE_LIMIT || cb->m_lexicalBlockStackAllocatedIdentifierMaximumDepth > VARIABLE_LIMIT) {
        auto err = new esprima::Error(new ASCIIString("variable limit exceeded"));
        err->errorCode = ErrorObject::SyntaxError;
        err->lineNumber = cb->m_functionStart.line;
        err->column = cb->m_functionStart.column;
        err->index = cb->m_functionStart.index;
        throw * err;
    }
}

ScriptParser::InitializeScriptResult ScriptParser::initializeScript(StringView scriptSource, String* fileName, bool isModule, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, size_t stackSizeRemain, bool needByteCodeGeneration, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget)
{
#ifdef ESCARGOT_DEBUGGER
    if (LIKELY(needByteCodeGeneration) && m_context->debugger() != NULL && m_context->debugger()->enabled()) {
        return initializeScriptWithDebugger(scriptSource, fileName, isModule, parentCodeBlock, strictFromOutside, isEvalCodeInFunction, isEvalMode, inWithOperation, allowSuperCall, allowSuperProperty, allowNewTarget);
    }
#endif /* ESCARGOT_DEBUGGER */

    GC_disable();

    bool inWith = (parentCodeBlock ? parentCodeBlock->inWith() : false) || inWithOperation;
    bool allowSC = (parentCodeBlock ? parentCodeBlock->allowSuperCall() : false) || allowSuperCall;
    bool allowSP = (parentCodeBlock ? parentCodeBlock->allowSuperProperty() : false) || allowSuperProperty;

    // Parsing
    try {
        InterpretedCodeBlock* topCodeBlock = nullptr;
        ProgramNode* programNode = esprima::parseProgram(m_context, scriptSource, isModule, strictFromOutside, inWith, stackSizeRemain, allowSC, allowSP, allowNewTarget);

        Script* script = new Script(fileName, new StringView(scriptSource), programNode->moduleData(), !parentCodeBlock);
        if (parentCodeBlock) {
            programNode->scopeContext()->m_hasEval = parentCodeBlock->hasEval();
            programNode->scopeContext()->m_hasWith = parentCodeBlock->hasWith();
            programNode->scopeContext()->m_isClassConstructor = parentCodeBlock->isClassConstructor();
            programNode->scopeContext()->m_isDerivedClassConstructor = parentCodeBlock->isDerivedClassConstructor();
            programNode->scopeContext()->m_isObjectMethod = parentCodeBlock->isObjectMethod();
            programNode->scopeContext()->m_isClassMethod = parentCodeBlock->isClassMethod();
            programNode->scopeContext()->m_isClassStaticMethod = parentCodeBlock->isClassStaticMethod();
            programNode->scopeContext()->m_allowSuperCall = parentCodeBlock->allowSuperCall();
            programNode->scopeContext()->m_allowSuperProperty = parentCodeBlock->allowSuperProperty();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, scriptSource, script, programNode->scopeContext(), parentCodeBlock, isEvalMode, isEvalCodeInFunction);
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, scriptSource, script, programNode, isEvalMode, isEvalCodeInFunction);
        }
        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);

        script->m_topCodeBlock = topCodeBlock;

// dump Code Block
#ifndef NDEBUG
        if (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE"))) {
            dumpCodeBlockTree(topCodeBlock);
        }
#endif

        ASSERT(script->m_topCodeBlock == topCodeBlock);

        // Generate ByteCode
        if (LIKELY(needByteCodeGeneration)) {
            topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, isEvalMode, !isEvalCodeInFunction, inWith);
        }

        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.script = script;
        return result;

    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.parseErrorCode = orgError->errorCode;
        result.parseErrorMessage = orgError->message;
        delete orgError;
        return result;
    }
}

void ScriptParser::generateFunctionByteCode(ExecutionState& state, InterpretedCodeBlock* codeBlock, size_t stackSizeRemain)
{
#ifdef ESCARGOT_DEBUGGER
    ASSERT(m_context->debugger() == NULL || !m_context->debugger()->enabled());
#endif /* ESCARGOT_DEBUGGER */

    GC_disable();

    FunctionNode* functionNode;

    // Parsing
    try {
        functionNode = esprima::parseSingleFunction(m_context, codeBlock, stackSizeRemain);
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();

        auto str = orgError->message->toUTF8StringData();
        delete orgError;
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, str.data());
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Generate ByteCode
    codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), codeBlock, functionNode, false, false, false, false);

    // reset ASTAllocator
    m_context->astAllocator().reset();
    GC_enable();
}

#ifdef ESCARGOT_DEBUGGER

void ScriptParser::recursivelyGenerateByteCode(InterpretedCodeBlock* topCodeBlock)
{
    InterpretedCodeBlock* codeBlock = topCodeBlock;
    bool directionDown = true;

    if (codeBlock->m_firstChild == NULL) {
        return;
    }

    while (true) {
        if (directionDown) {
            if (codeBlock->m_firstChild != NULL) {
                codeBlock = codeBlock->m_firstChild;
            } else if (codeBlock->m_nextSibling != NULL) {
                codeBlock = codeBlock->m_nextSibling;
            } else {
                directionDown = false;
                codeBlock = codeBlock->m_parentCodeBlock;

                if (codeBlock == topCodeBlock) {
                    return;
                }
                continue;
            }
        } else if (codeBlock->m_nextSibling != NULL) {
            codeBlock = codeBlock->m_nextSibling;
            directionDown = true;
        } else {
            codeBlock = codeBlock->m_parentCodeBlock;

            if (codeBlock == topCodeBlock) {
                return;
            }
            continue;
        }

        // Errors caught by the caller.
        FunctionNode* functionNode = esprima::parseSingleFunction(m_context, codeBlock, SIZE_MAX);
        codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, codeBlock, functionNode, false, false, false, false);

        if (m_context->debugger() != NULL && m_context->debugger()->enabled()) {
            String* functionName = codeBlock->functionName().string();
            if (functionName->length() > 0) {
                StringView* functionNameView = new StringView(functionName);
                m_context->debugger()->sendString(Debugger::ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT, functionNameView);
            }

            if (m_context->debugger()->enabled()) {
                m_context->debugger()->sendFunctionInfo(codeBlock);
            }
        }

        m_context->astAllocator().reset();
    }
}

ScriptParser::InitializeScriptResult ScriptParser::initializeScriptWithDebugger(StringView scriptSource, String* fileName, bool isModule, InterpretedCodeBlock* parentCodeBlock, bool strictFromOutside, bool isEvalCodeInFunction, bool isEvalMode, bool inWithOperation, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget)
{
    GC_disable();

    bool inWith = (parentCodeBlock ? parentCodeBlock->inWith() : false) || inWithOperation;
    bool allowSC = (parentCodeBlock ? parentCodeBlock->allowSuperCall() : false) || allowSuperCall;
    bool allowSP = (parentCodeBlock ? parentCodeBlock->allowSuperProperty() : false) || allowSuperProperty;

    // Parsing
    try {
        InterpretedCodeBlock* topCodeBlock = nullptr;
        ProgramNode* programNode = esprima::parseProgram(m_context, scriptSource, isModule, strictFromOutside, inWith, SIZE_MAX, allowSC, allowSP, allowNewTarget);

        StringView* scriptSourceView = new StringView(scriptSource);

        if (m_context->debugger() != nullptr && m_context->debugger()->enabled()) {
            m_context->debugger()->sendString(Debugger::ESCARGOT_MESSAGE_SOURCE_8BIT, scriptSourceView);

            if (m_context->debugger()->enabled()) {
                StringView* fileNameView = new StringView(fileName);
                m_context->debugger()->sendString(Debugger::ESCARGOT_MESSAGE_FILE_NAME_8BIT, fileNameView);
            }
        }

        Script* script = new Script(fileName, scriptSourceView, programNode->moduleData(), !parentCodeBlock);
        if (parentCodeBlock) {
            programNode->scopeContext()->m_hasEval = parentCodeBlock->hasEval();
            programNode->scopeContext()->m_hasWith = parentCodeBlock->hasWith();
            programNode->scopeContext()->m_isClassConstructor = parentCodeBlock->isClassConstructor();
            programNode->scopeContext()->m_isDerivedClassConstructor = parentCodeBlock->isDerivedClassConstructor();
            programNode->scopeContext()->m_isClassMethod = parentCodeBlock->isClassMethod();
            programNode->scopeContext()->m_isClassStaticMethod = parentCodeBlock->isClassStaticMethod();
            programNode->scopeContext()->m_allowSuperCall = parentCodeBlock->allowSuperCall();
            programNode->scopeContext()->m_allowSuperProperty = parentCodeBlock->allowSuperProperty();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, scriptSource, script, programNode->scopeContext(), parentCodeBlock, isEvalMode, isEvalCodeInFunction);
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, scriptSource, script, programNode, isEvalMode, isEvalCodeInFunction);
        }
        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);

        script->m_topCodeBlock = topCodeBlock;

// dump Code Block
#ifndef NDEBUG
        if (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE"))) {
            dumpCodeBlockTree(topCodeBlock);
        }
#endif

        ASSERT(script->m_topCodeBlock == topCodeBlock);

        // Generate ByteCode
        topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, isEvalMode, !isEvalCodeInFunction, inWith);

        if (m_context->debugger() != nullptr && m_context->debugger()->enabled()) {
            m_context->debugger()->sendFunctionInfo(topCodeBlock);
        }

        // reset ASTAllocator
        m_context->astAllocator().reset();

        if (m_context->debugger() != nullptr && m_context->debugger()->enabled()) {
            recursivelyGenerateByteCode(topCodeBlock);
            m_context->debugger()->sendType(Debugger::ESCARGOT_MESSAGE_PARSE_DONE);
            if (m_context->debugger()->pendingWait()) {
                m_context->debugger()->waitForResolvingPendingBreakpoints();
            }
        }

        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.script = script;
        return result;

    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();

        if (m_context->debugger() != nullptr && m_context->debugger()->enabled()) {
            m_context->debugger()->sendType(Debugger::ESCARGOT_MESSAGE_PARSE_ERROR);
            StringView* errorView = new StringView(orgError->message, 0, orgError->message->length());
            m_context->debugger()->sendString(Debugger::ESCARGOT_MESSAGE_STRING_8BIT, errorView);
        }

        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.parseErrorCode = orgError->errorCode;
        result.parseErrorMessage = orgError->message;
        return result;
    }
}

#endif /* ESCARGOT_DEBUGGER */

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
        printf("CodeBlock %p %s %s%s%s(%d:%d -> %d:%d, block %d, body %d)(%s, %s) (E:%d, W:%d, A:%d)\n", cb, cb->m_functionName.string()->toUTF8StringData().data(),
               cb->m_isStrict ? "Strict " : "",
               cb->m_isGenerator ? "Generator " : "",
               cb->m_isAsync ? "Async" : "",
               (int)cb->m_functionStart.line,
               (int)cb->m_functionStart.column,
               (int)cb->m_bodyEndLOC.line,
               (int)cb->m_bodyEndLOC.column,
               (int)cb->lexicalBlockIndexFunctionLocatedIn(),
               (int)cb->functionBodyBlockIndex(),
               cb->m_canAllocateEnvironmentOnStack ? "Stack" : "Heap",
               cb->m_canUseIndexedVariableStorage ? "Indexed" : "Named",
               (int)cb->m_hasEval, (int)cb->m_hasWith, (int)cb->m_usesArgumentsObject);

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
        ASSERT(cb->scopeContext()->m_childBlockScopes.size() == cb->m_blockInfos.size());
        for (size_t i = 0; i < cb->m_blockInfos.size(); i++) {
            puts("");
            PRINT_TAB_BLOCK()
            printf("Block %p %d:%d [%d parent(%d)]: %s, %s", cb->m_blockInfos[i], (int)cb->scopeContext()->m_childBlockScopes[i]->m_loc.line, (int)cb->scopeContext()->m_childBlockScopes[i]->m_loc.column, (int)cb->m_blockInfos[i]->m_blockIndex, (int)cb->m_blockInfos[i]->m_parentBlockIndex, cb->m_blockInfos[i]->m_canAllocateEnvironmentOnStack ? "Stack" : "Heap", cb->m_blockInfos[i]->m_shouldAllocateEnvironment ? "Allocated" : "Skipped");

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
            for (size_t j = 0; j < cb->scopeContext()->m_childBlockScopes[i]->m_usingNames.size(); j++) {
                printf("%s, ", cb->scopeContext()->m_childBlockScopes[i]->m_usingNames[j].string()->toUTF8StringData().data());
            }
        }

        puts("");

        InterpretedCodeBlock* child = cb->firstChild();
        while (child) {
            fn(child, depth + 1);
            child = child->nextSibling();
        }

    };
    fn(topCodeBlock, 0);
}
#endif
}
