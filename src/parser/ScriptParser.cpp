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
#include "Script.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
#include "parser/esprima_cpp/esprima.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "debugger/Debugger.h"
#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCache.h"
#endif

namespace Escargot {

ScriptParser::ScriptParser(Context* c)
    : m_context(c)
#if defined(ENABLE_CODE_CACHE)
    , m_codeBlockCacheInfo(nullptr)
#endif
{
}

ScriptParser::InitializeScriptResult::InitializeScriptResult()
    : parseErrorCode(ErrorCode::None)
    , parseErrorMessage(String::emptyString)
{
}

Script* ScriptParser::InitializeScriptResult::scriptThrowsExceptionIfParseError(ExecutionState& state)
{
    if (!script) {
        ErrorObject::throwBuiltinError(state, parseErrorCode, parseErrorMessage->toUTF8StringData().data());
    }

    return script.value();
}

InterpretedCodeBlock* ScriptParser::generateCodeBlockTreeFromASTWalker(Context* ctx, StringView source, Script* script, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentCodeBlock, bool isEvalCode, bool isEvalCodeInFunction)
{
    InterpretedCodeBlock* codeBlock;
    if (parentCodeBlock == nullptr) {
        // globalBlock
        codeBlock = InterpretedCodeBlock::createInterpretedCodeBlock(ctx, script, source, scopeCtx, isEvalCode, isEvalCodeInFunction);
    } else {
        codeBlock = InterpretedCodeBlock::createInterpretedCodeBlock(ctx, script, source, scopeCtx, parentCodeBlock, isEvalCode, isEvalCodeInFunction);
    }

#if defined(ENABLE_CODE_CACHE)
    // CodeBlock tree is generated in depth-first order
    // CodeCache stores each CodeBlock in depth-first order too
    if (m_codeBlockCacheInfo) {
        ASSERT(m_codeBlockCacheInfo->m_codeBlockIndex.find(codeBlock) == m_codeBlockCacheInfo->m_codeBlockIndex.end());
        m_codeBlockCacheInfo->m_codeBlockIndex.insert(std::make_pair(codeBlock, m_codeBlockCacheInfo->m_codeBlockCount));
        m_codeBlockCacheInfo->m_codeBlockCount++;
    }
#endif

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
                c = c->parent();
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
                    } else if ((c->isClassMethod() || c->isClassStaticMethod()) && scopeCtx->m_hasClassPrivateNameExpression) {
                        /*
                        // this covers case below
                        var C = class {
                          #f = 'Test262';

                          method() {
                            let arrowFunction = () => {
                              return this.#f; // private member access needs upper env's homeObject
                            }

                            return arrowFunction();
                          }
                        }
                        */
                        c->m_canAllocateEnvironmentOnStack = false;
                        break;
                    } else {
                        break;
                    }
                }
                c = c->parent();
            }
        }

        if (scopeCtx->m_hasSuperOrNewTarget) {
            InterpretedCodeBlock* c = codeBlock;
            while (c) {
                if (c->isKindOfFunction() && (!c->isArrowFunctionExpression() || c->isOneExpressionOnlyVirtualArrowFunctionExpression())) { // ThisEnvironment
                    c->m_canAllocateEnvironmentOnStack = false;
                    break;
                }
                c = c->parent();
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
                            argumentsObjectHolder = argumentsObjectHolder->parent();
                        }

                        if (hasKindOfArgumentsHolderOnAncestors && !argumentsObjectHolder->hasParameterName(arguments)) {
                            InterpretedCodeBlock* argumentsVariableHolder = nullptr;
                            InterpretedCodeBlock* c = codeBlock->parent();
                            while (c && !c->isGlobalCodeBlock()) {
                                if (c->hasParameterName(arguments)) {
                                    argumentsVariableHolder = c;
                                    break;
                                } else if (!c->isArrowFunctionExpression()) {
                                    argumentsVariableHolder = c;
                                    break;
                                }
                                c = c->parent();
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
                                    p = p->parent();
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

                if (codeBlock->canUseIndexedVariableStorage() && !codeBlock->hasName(blockIndex, uname)) {
                    auto parentBlockIndex = codeBlock->lexicalBlockIndexFunctionLocatedIn();
                    InterpretedCodeBlock* c = codeBlock->parent();
                    while (c && parentBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                        auto r = c->tryCaptureIdentifiersFromChildCodeBlock(parentBlockIndex, uname);

                        if (r.first) {
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
                        c = c->parent();
                    }
                }

                if (uname == codeBlock->functionName()) {
                    codeBlock->m_isFunctionNameUsedBySelf = true;
                }
            }
        }
    }

    size_t childCount = scopeCtx->childCount();
    if (childCount > 0) {
        InterpretedCodeBlockVector* codeBlockVector = new InterpretedCodeBlockVector();
        codeBlockVector->resizeWithUninitializedValues(childCount);

        ASTScopeContext* childScope = scopeCtx->firstChild();
        for (size_t i = 0; i < childCount; i++) {
            ASSERT(!!childScope);
            InterpretedCodeBlock* newBlock = generateCodeBlockTreeFromASTWalker(ctx, source, script, childScope, codeBlock, isEvalCode, isEvalCodeInFunction);
            (*codeBlockVector)[i] = newBlock;
            childScope = childScope->nextSibling();
        }

        ASSERT(!codeBlock->m_children);
        codeBlock->setChildren(codeBlockVector);
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
    if (cb->hasChildren()) {
        InterpretedCodeBlockVector& childrenVector = cb->children();
        for (size_t i = 0; i < childrenVector.size(); i++) {
            InterpretedCodeBlock* child = childrenVector[i];
            generateCodeBlockTreeFromASTWalkerPostProcess(child);
        }
    }

    cb->computeVariables();
    if (cb->m_identifierOnStackCount > VARIABLE_LIMIT || cb->m_identifierOnHeapCount > VARIABLE_LIMIT || cb->m_lexicalBlockStackAllocatedIdentifierMaximumDepth > VARIABLE_LIMIT) {
        auto err = new esprima::Error(new ASCIIString("variable limit exceeded"));
        err->errorCode = ErrorCode::SyntaxError;
        err->lineNumber = cb->m_functionStart.line;
        err->column = cb->m_functionStart.column;
        err->index = cb->m_functionStart.index;
        throw *err;
    }
}

#if defined(ENABLE_CODE_CACHE)
void ScriptParser::setCodeBlockCacheInfo(CodeBlockCacheInfo* info)
{
    ASSERT(!m_codeBlockCacheInfo);
    m_codeBlockCacheInfo = info;
}

void ScriptParser::deleteCodeBlockCacheInfo()
{
    delete m_codeBlockCacheInfo;
    m_codeBlockCacheInfo = nullptr;
}
#endif

ScriptParser::InitializeScriptResult ScriptParser::initializeScript(String* originSource, size_t originLineOffset, String* source, String* srcName, InterpretedCodeBlock* parentCodeBlock, bool isModule, bool isEvalMode, bool isEvalCodeInFunction, bool inWithOperation, bool strictFromOutside, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget, bool needByteCodeGeneration)
{
    ASSERT(m_context->astAllocator().isInitialized());

#if defined(ENABLE_CODE_CACHE)
    UNUSED_PARAMETER(originSource);

    size_t srcHash = 0;
    CodeCache* codeCache = m_context->vmInstance()->codeCache();
    bool cacheable = codeCache->enabled() && needByteCodeGeneration && !isModule && !isEvalMode && srcName->length() && source->length() > CODE_CACHE_MIN_SOURCE_LENGTH;

    // Load caching
    if (cacheable) {
        ASSERT(!parentCodeBlock);
        srcHash = source->hashValue();
        auto result = codeCache->searchCache(srcHash);
        if (result.first) {
            GC_disable();

            Script* script = new Script(srcName, source, nullptr, false, originLineOffset);
            CodeCacheEntry& entry = result.second;

            codeCache->prepareCacheLoading(m_context, srcHash, entry);
            // load CodeBlockTree
            InterpretedCodeBlock* topCodeBlock = codeCache->loadCodeBlockTree(m_context, script);
            // load global ByteCodeBlock
            ByteCodeBlock* topByteBlock = codeCache->loadByteCodeBlock(m_context, topCodeBlock);
            bool loadingDone = codeCache->postCacheLoading();
            cacheable = loadingDone;

            GC_enable();

            if (LIKELY(loadingDone)) {
                ASSERT(!!topCodeBlock && !!topByteBlock);
                script->m_topCodeBlock = topCodeBlock;
                topCodeBlock->m_byteCodeBlock = topByteBlock;

                ESCARGOT_LOG_INFO("[CodeCache] Load CodeCache Done (%s)\n", srcName->toUTF8StringData().data());

                ScriptParser::InitializeScriptResult result;
                result.script = script;
                return result;
            }
        } else {
            // prepare for caching
            setCodeBlockCacheInfo(new CodeBlockCacheInfo());
        }
    }
#endif

#ifdef ESCARGOT_DEBUGGER
    if (LIKELY(needByteCodeGeneration) && m_context->debuggerEnabled() && srcName->length() && !m_context->debugger()->skipSourceCode(srcName)) {
        return initializeScriptWithDebugger(originSource, originLineOffset, source, srcName, parentCodeBlock, isModule, isEvalMode, isEvalCodeInFunction, inWithOperation, strictFromOutside, allowSuperCall, allowSuperProperty, allowNewTarget);
    }
#endif /* ESCARGOT_DEBUGGER */

    GC_disable();

    bool inWith = (parentCodeBlock ? parentCodeBlock->inWith() : false) || inWithOperation;
    bool allowSC = (parentCodeBlock ? parentCodeBlock->allowSuperCall() : false) || allowSuperCall;
    bool allowSP = (parentCodeBlock ? parentCodeBlock->allowSuperProperty() : false) || allowSuperProperty;
    bool allowArguments = (parentCodeBlock ? parentCodeBlock->allowArguments() : true);

    InterpretedCodeBlock* topCodeBlock = nullptr;
    StringView sourceView(source, 0, source->length());
    ProgramNode* programNode = nullptr;
    Script* script = nullptr;

    // Parsing
    try {
        ASTClassInfo* outerClassInfo = esprima::generateClassInfoFrom(m_context, parentCodeBlock);

        programNode = esprima::parseProgram(m_context, sourceView, outerClassInfo,
                                            isModule, strictFromOutside, inWith, allowSC, allowSP, allowNewTarget, allowArguments);

        script = new Script(srcName, source, programNode->moduleData(), !parentCodeBlock, originLineOffset);
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
            programNode->scopeContext()->m_allowArguments = parentCodeBlock->allowArguments();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, sourceView, script, programNode->scopeContext(), parentCodeBlock, isEvalMode, isEvalCodeInFunction);
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, sourceView, script, programNode, isEvalMode, isEvalCodeInFunction);
        }

        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();
#if defined(ENABLE_CODE_CACHE)
        if (cacheable) {
            deleteCodeBlockCacheInfo();
        }
#endif

        ScriptParser::InitializeScriptResult result;
        result.parseErrorCode = orgError->errorCode;
        result.parseErrorMessage = orgError->message;
        delete orgError;
        return result;
    }

    // dump Code Block
#ifndef NDEBUG
    {
        char* dumpCodeBlockTreeValue = getenv("DUMP_CODEBLOCK_TREE");
        if (dumpCodeBlockTreeValue && (strcmp(dumpCodeBlockTreeValue, "1") == 0)) {
            dumpCodeBlockTree(topCodeBlock);
        }
    }
#endif

    script->m_topCodeBlock = topCodeBlock;

    // Generate ByteCode
    if (LIKELY(needByteCodeGeneration)) {
        try {
#if defined(ENABLE_CODE_CACHE)
            // Store cache
            if (cacheable) {
                codeCache->prepareCacheWriting(srcHash);

                // For storing cache, CodeBlockTree is firstly saved
                codeCache->storeCodeBlockTree(topCodeBlock, m_codeBlockCacheInfo);

                // After CodeBlockTree, ByteCode and StringTable are stored sequentially
                topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, inWith, true);

                codeCache->postCacheWriting(srcHash);
                deleteCodeBlockCacheInfo();

                ESCARGOT_LOG_INFO("[CodeCache] Store CodeCache Done (%s)\n", srcName->toUTF8StringData().data());
            } else {
                topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, inWith, false);
            }
#else
            topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, inWith);
#endif
        } catch (const char* message) {
            m_context->astAllocator().reset();
            GC_enable();

            ScriptParser::InitializeScriptResult result;
            result.parseErrorCode = ErrorCode::SyntaxError;
            result.parseErrorMessage = String::fromASCII(message, strlen(message));
            return result;
        }
    }

    // reset ASTAllocator
    m_context->astAllocator().reset();

    GC_enable();

    ScriptParser::InitializeScriptResult result;
    result.script = script;
    return result;
}

void ScriptParser::generateFunctionByteCode(ExecutionState& state, InterpretedCodeBlock* codeBlock)
{
#ifdef ESCARGOT_DEBUGGER
    // When the debugger is enabled, lazy compilation is disabled, so the functions are compiled
    // during parsing, and this function is never called. However, implicit class constructors and dynamically generated functions
    // are still compiled later. These functions are ignored by the debugger.
    ASSERT(!m_context->debuggerEnabled() || !m_context->inDebuggingCodeMode());
#endif /* ESCARGOT_DEBUGGER */

    GC_disable();

    FunctionNode* functionNode;

    // Parsing
    try {
        functionNode = esprima::parseSingleFunction(m_context, codeBlock);
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();

        auto str = orgError->message->toUTF8StringData();
        delete orgError;
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, str.data());
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Generate ByteCode
    try {
        codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), codeBlock, functionNode);
    } catch (const char* message) {
        // reset ASTAllocator
        m_context->astAllocator().reset();
        GC_enable();
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, message);
        RELEASE_ASSERT_NOT_REACHED();
    }

    // reset ASTAllocator
    m_context->astAllocator().reset();
    GC_enable();
}

Script* ScriptParser::initializeJSONModule(String* source, String* srcName)
{
    Script::ModuleData* moduleData = new Script::ModuleData();

    Script::ExportEntry entry;
    entry.m_exportName = m_context->staticStrings().stringDefault;
    entry.m_localName = m_context->staticStrings().stringStarDefaultStar;

    moduleData->m_localExportEntries.push_back(entry);

    Script* script = new Script(srcName, source, moduleData, false, 0);

    ModuleEnvironmentRecord* moduleRecord = new ModuleEnvironmentRecord(script);
    moduleData->m_moduleRecord = moduleRecord;

    moduleData->m_cycleRoot = script;
    moduleData->m_status = Script::ModuleData::Linked;
    return script;
}

#ifdef ESCARGOT_DEBUGGER

void ScriptParser::recursivelyGenerateChildrenByteCode(InterpretedCodeBlock* parent)
{
    ASSERT(GC_is_disabled());

    if (!parent->hasChildren()) {
        return;
    }

    InterpretedCodeBlockVector& childrenVector = parent->children();
    for (size_t i = 0; i < childrenVector.size(); i++) {
        InterpretedCodeBlock* codeBlock = childrenVector[i];

        // Errors caught by the caller.
        FunctionNode* functionNode = esprima::parseSingleFunction(m_context, codeBlock);
        codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, codeBlock, functionNode);

        m_context->astAllocator().reset();
    }

    for (size_t i = 0; i < childrenVector.size(); i++) {
        recursivelyGenerateChildrenByteCode(childrenVector[i]);
    }
}

ScriptParser::InitializeScriptResult ScriptParser::initializeScriptWithDebugger(String* originSource, size_t originLineOffset, String* source, String* srcName, InterpretedCodeBlock* parentCodeBlock, bool isModule, bool isEvalMode, bool isEvalCodeInFunction, bool inWithOperation, bool strictFromOutside, bool allowSuperCall, bool allowSuperProperty, bool allowNewTarget)
{
    // src name should have valid string
    ASSERT(srcName && srcName->length());

    GC_disable();
    if (m_context->debuggerEnabled()) {
        m_context->debugger()->setInDebuggingCodeMode(true);
    }

    bool inWith = (parentCodeBlock ? parentCodeBlock->inWith() : false) || inWithOperation;
    bool allowSC = (parentCodeBlock ? parentCodeBlock->allowSuperCall() : false) || allowSuperCall;
    bool allowSP = (parentCodeBlock ? parentCodeBlock->allowSuperProperty() : false) || allowSuperProperty;
    bool allowArguments = (parentCodeBlock ? parentCodeBlock->allowArguments() : true);

    InterpretedCodeBlock* topCodeBlock = nullptr;
    StringView sourceView(source, 0, source->length());
    ProgramNode* programNode = nullptr;
    Script* script = nullptr;

    // Parsing
    try {
        ASTClassInfo* outerClassInfo = esprima::generateClassInfoFrom(m_context, parentCodeBlock);

        programNode = esprima::parseProgram(m_context, sourceView, outerClassInfo, isModule, strictFromOutside, inWith, allowSC, allowSP, allowNewTarget, allowArguments);

        script = new Script(srcName, source, programNode->moduleData(), !parentCodeBlock, originLineOffset);
        if (parentCodeBlock) {
            programNode->scopeContext()->m_hasEval = parentCodeBlock->hasEval();
            programNode->scopeContext()->m_hasWith = parentCodeBlock->hasWith();
            programNode->scopeContext()->m_isClassConstructor = parentCodeBlock->isClassConstructor();
            programNode->scopeContext()->m_isDerivedClassConstructor = parentCodeBlock->isDerivedClassConstructor();
            programNode->scopeContext()->m_isClassMethod = parentCodeBlock->isClassMethod();
            programNode->scopeContext()->m_isClassStaticMethod = parentCodeBlock->isClassStaticMethod();
            programNode->scopeContext()->m_allowSuperCall = parentCodeBlock->allowSuperCall();
            programNode->scopeContext()->m_allowSuperProperty = parentCodeBlock->allowSuperProperty();
            topCodeBlock = generateCodeBlockTreeFromASTWalker(m_context, sourceView, script, programNode->scopeContext(), parentCodeBlock, isEvalMode, isEvalCodeInFunction);
        } else {
            topCodeBlock = generateCodeBlockTreeFromAST(m_context, sourceView, script, programNode, isEvalMode, isEvalCodeInFunction);
        }

        generateCodeBlockTreeFromASTWalkerPostProcess(topCodeBlock);
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        m_context->astAllocator().reset();

        if (m_context->debuggerEnabled()) {
            m_context->debugger()->parseCompleted(originSource ? originSource : source, srcName, originLineOffset, orgError->message);
            m_context->debugger()->clearParsingData();
            m_context->debugger()->setInDebuggingCodeMode(false);
        }

        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.parseErrorCode = orgError->errorCode;
        result.parseErrorMessage = orgError->message;
        return result;
    }

    // dump Code Block
#ifndef NDEBUG
    {
        char* dumpCodeBlockTreeValue = getenv("DUMP_CODEBLOCK_TREE");
        if (dumpCodeBlockTreeValue && (strcmp(dumpCodeBlockTreeValue, "1") == 0)) {
            dumpCodeBlockTree(topCodeBlock);
        }
    }
#endif

    script->m_topCodeBlock = topCodeBlock;

    // Generate ByteCode
    try {
        topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(m_context, topCodeBlock, programNode, inWith);

        // reset ASTAllocator
        m_context->astAllocator().reset();

        Debugger* debugger = m_context->debugger();
        if (debugger != nullptr) {
            recursivelyGenerateChildrenByteCode(topCodeBlock);

            debugger->parseCompleted(originSource ? originSource : source, srcName, originLineOffset);
            debugger->clearParsingData();
            debugger->setInDebuggingCodeMode(false);
        }

        GC_enable();
    } catch (const char* message) {
        m_context->astAllocator().reset();

        String* msg = String::fromASCII(message, strlen(message));

        if (m_context->debuggerEnabled()) {
            m_context->debugger()->parseCompleted(originSource ? originSource : source, srcName, originLineOffset, msg);
            m_context->debugger()->clearParsingData();
            m_context->debugger()->setInDebuggingCodeMode(false);
        }

        GC_enable();

        ScriptParser::InitializeScriptResult result;
        result.parseErrorCode = ErrorCode::SyntaxError;
        result.parseErrorMessage = msg;
        return result;
    }

    ScriptParser::InitializeScriptResult result;
    result.script = script;
    return result;
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

        if (cb->hasChildren()) {
            InterpretedCodeBlockVector& childrenVector = cb->children();
            for (size_t i = 0; i < childrenVector.size(); i++) {
                InterpretedCodeBlock* child = childrenVector[i];
                fn(child, depth + 1);
            }
        }
    };
    fn(topCodeBlock, 0);
}
#endif
} // namespace Escargot
