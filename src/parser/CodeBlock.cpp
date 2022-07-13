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
#include "CodeBlock.h"
#include "parser/Script.h"
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

void* NativeCodeBlock::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(NativeCodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(NativeCodeBlock, m_context));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(NativeCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* InterpretedCodeBlock::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(InterpretedCodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_context));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_script));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_byteCodeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_parent));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_children));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_parameterNames));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_identifierInfos));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_blockInfos));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(InterpretedCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<InterpretedCodeBlock>().allocate(1);
#endif
}

void* InterpretedCodeBlockWithRareData::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(InterpretedCodeBlockWithRareData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_context));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_script));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_byteCodeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_parent));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_children));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_parameterNames));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_identifierInfos));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_blockInfos));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_rareData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(InterpretedCodeBlockWithRareData));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<InterpretedCodeBlockWithRareData>().allocate(1);
#endif
}

void* InterpretedCodeBlock::BlockInfo::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(InterpretedCodeBlock::BlockInfo)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock::BlockInfo, m_identifiers));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(InterpretedCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void InterpretedCodeBlock::initBlockScopeInformation(ASTScopeContext* scopeCtx)
{
    const ASTBlockContextVector& blockScopes = scopeCtx->m_childBlockScopes;
    m_blockInfos.resizeWithUninitializedValues(blockScopes.size());
    for (size_t i = 0; i < blockScopes.size(); i++) {
        BlockInfo* info = new BlockInfo(
#ifndef NDEBUG
            blockScopes[i]->m_loc
#endif
        );

        // if block comes from switch statement, we should allocate variables on heap.
        // because our we can track only heap variables are initialized by user
        // we can track that {let, const} variables are initialized by user only if variables are allocated in heap
        bool everyVariablesShouldUseHeapStorage = info->m_nodeType == ASTNodeType::SwitchStatement;
        m_canAllocateEnvironmentOnStack = m_canAllocateEnvironmentOnStack && !everyVariablesShouldUseHeapStorage;
        info->m_canAllocateEnvironmentOnStack = m_canAllocateEnvironmentOnStack;
        info->m_shouldAllocateEnvironment = true;
        info->m_nodeType = blockScopes[i]->m_nodeType;
        info->m_parentBlockIndex = blockScopes[i]->m_parentBlockIndex;
        info->m_blockIndex = blockScopes[i]->m_blockIndex;

        info->m_identifiers.resizeWithUninitializedValues(blockScopes[i]->m_names.size());
        for (size_t j = 0; j < blockScopes[i]->m_names.size(); j++) {
            BlockIdentifierInfo idInfo;
            idInfo.m_needToAllocateOnStack = m_canUseIndexedVariableStorage && !everyVariablesShouldUseHeapStorage;
            idInfo.m_isMutable = !blockScopes[i]->m_names[j].isConstBinding();
            idInfo.m_indexForIndexedStorage = SIZE_MAX;
            idInfo.m_name = blockScopes[i]->m_names[j].name();
            info->m_identifiers[j] = idInfo;
        }

        m_blockInfos[i] = info;
    }
}

InterpretedCodeBlock* InterpretedCodeBlock::createInterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction)
{
    if (UNLIKELY(scopeCtx->m_needRareData)) {
        return new InterpretedCodeBlockWithRareData(ctx, script, src, scopeCtx, isEvalCode, isEvalCodeInFunction);
    }

    return new InterpretedCodeBlock(ctx, script, src, scopeCtx, isEvalCode, isEvalCodeInFunction);
}

InterpretedCodeBlock* InterpretedCodeBlock::createInterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentBlock, bool isEvalCode, bool isEvalCodeInFunction)
{
    if (UNLIKELY(scopeCtx->m_needRareData)) {
        return new InterpretedCodeBlockWithRareData(ctx, script, src, scopeCtx, parentBlock, isEvalCode, isEvalCodeInFunction);
    }

    return new InterpretedCodeBlock(ctx, script, src, scopeCtx, parentBlock, isEvalCode, isEvalCodeInFunction);
}

// create an empty InterpretedCodeBlock
InterpretedCodeBlock* InterpretedCodeBlock::createInterpretedCodeBlock(Context* ctx, Script* script, bool needRareData)
{
    if (UNLIKELY(needRareData)) {
        return new InterpretedCodeBlockWithRareData(ctx, script);
    }

    return new InterpretedCodeBlock(ctx, script);
}

InterpretedCodeBlock::InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction)
    : CodeBlock(ctx)
    , m_script(script)
    , m_src(src)
    , m_byteCodeBlock(nullptr)
    , m_parent(nullptr)
    , m_children(nullptr)
    , m_functionStart(1, 1, 0)
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    , m_bodyEndLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#endif
    , m_functionLength(0)
    , m_parameterCount(0)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_lexicalBlockStackAllocatedIdentifierMaximumDepth(0)
    , m_functionBodyBlockIndex(0)
    , m_lexicalBlockIndexFunctionLocatedIn(0)
    , m_isFunctionNameUsedBySelf(false)
    , m_isFunctionNameSaveOnHeap(false)
    , m_isFunctionNameExplicitlyDeclared(false)
    , m_canUseIndexedVariableStorage(false)
    , m_canAllocateVariablesOnStack(false)
    , m_canAllocateEnvironmentOnStack(false)
    , m_hasDescendantUsesNonIndexedVariableStorage(false)
    , m_hasEval(false)
    , m_hasWith(false)
    , m_isStrict(false)
    , m_inWith(false)
    , m_isEvalCode(false)
    , m_isEvalCodeInFunction(false)
    , m_usesArgumentsObject(false)
    , m_isFunctionExpression(false)
    , m_isFunctionDeclaration(false)
    , m_isArrowFunctionExpression(false)
    , m_isOneExpressionOnlyVirtualArrowFunctionExpression(false)
    , m_isFunctionBodyOnlyVirtualArrowFunctionExpression(false)
    , m_isClassConstructor(false)
    , m_isDerivedClassConstructor(false)
    , m_isObjectMethod(false)
    , m_isClassMethod(false)
    , m_isClassStaticMethod(false)
    , m_isGenerator(false)
    , m_isAsync(false)
    , m_needsVirtualIDOperation(false)
    , m_hasArrowParameterPlaceHolder(false)
    , m_hasParameterOtherThanIdentifier(false)
    , m_allowSuperCall(false)
    , m_allowSuperProperty(false)
    , m_allowArguments(false)
    , m_hasDynamicSourceCode(false)
#ifdef ESCARGOT_DEBUGGER
    , m_markDebugging(ctx->inDebuggingCodeMode())
#endif
#ifndef NDEBUG
    , m_scopeContext(scopeCtx)
#endif
{
    recordGlobalParsingInfo(scopeCtx, isEvalCode, isEvalCodeInFunction);
}

InterpretedCodeBlock::InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentBlock, bool isEvalCode, bool isEvalCodeInFunction)
    : CodeBlock(ctx)
    , m_script(script)
    , m_src(StringView(src, scopeCtx->m_functionStartLOC.index, scopeCtx->m_bodyEndLOC.index))
    , m_byteCodeBlock(nullptr)
    , m_parent(parentBlock)
    , m_children(nullptr)
    , m_functionName(scopeCtx->m_functionName)
    , m_functionStart(scopeCtx->m_functionStartLOC)
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    , m_bodyEndLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#endif
    , m_functionLength(scopeCtx->m_functionLength)
    , m_parameterCount(scopeCtx->m_parameterCount)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_lexicalBlockStackAllocatedIdentifierMaximumDepth(0)
    , m_functionBodyBlockIndex(0)
    , m_lexicalBlockIndexFunctionLocatedIn(scopeCtx->m_lexicalBlockIndexFunctionLocatedIn)
    , m_isFunctionNameUsedBySelf(false)
    , m_isFunctionNameSaveOnHeap(false)
    , m_isFunctionNameExplicitlyDeclared(false)
    , m_canUseIndexedVariableStorage(false)
    , m_canAllocateVariablesOnStack(false)
    , m_canAllocateEnvironmentOnStack(false)
    , m_hasDescendantUsesNonIndexedVariableStorage(false)
    , m_hasEval(false)
    , m_hasWith(false)
    , m_isStrict(false)
    , m_inWith(false)
    , m_isEvalCode(false)
    , m_isEvalCodeInFunction(false)
    , m_usesArgumentsObject(false)
    , m_isFunctionExpression(false)
    , m_isFunctionDeclaration(false)
    , m_isArrowFunctionExpression(false)
    , m_isOneExpressionOnlyVirtualArrowFunctionExpression(false)
    , m_isFunctionBodyOnlyVirtualArrowFunctionExpression(false)
    , m_isClassConstructor(false)
    , m_isDerivedClassConstructor(false)
    , m_isObjectMethod(false)
    , m_isClassMethod(false)
    , m_isClassStaticMethod(false)
    , m_isGenerator(false)
    , m_isAsync(false)
    , m_needsVirtualIDOperation(false)
    , m_hasArrowParameterPlaceHolder(false)
    , m_hasParameterOtherThanIdentifier(false)
    , m_allowSuperCall(false)
    , m_allowSuperProperty(false)
    , m_allowArguments(false)
    , m_hasDynamicSourceCode(false)
#ifdef ESCARGOT_DEBUGGER
    , m_markDebugging(ctx->inDebuggingCodeMode())
#endif
#ifndef NDEBUG
    , m_scopeContext(scopeCtx)
#endif
{
    recordFunctionParsingInfo(scopeCtx, isEvalCode, isEvalCodeInFunction);
}

InterpretedCodeBlock::InterpretedCodeBlock(Context* ctx, Script* script)
    : CodeBlock(ctx)
    , m_script(script)
    , m_src()
    , m_byteCodeBlock(nullptr)
    , m_parent(nullptr)
    , m_children(nullptr)
    , m_functionName()
    , m_functionStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    , m_bodyEndLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#endif
    , m_functionLength(0)
    , m_parameterCount(0)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_lexicalBlockStackAllocatedIdentifierMaximumDepth(0)
    , m_functionBodyBlockIndex(0)
    , m_lexicalBlockIndexFunctionLocatedIn(0)
    , m_isFunctionNameUsedBySelf(false)
    , m_isFunctionNameSaveOnHeap(false)
    , m_isFunctionNameExplicitlyDeclared(false)
    , m_canUseIndexedVariableStorage(false)
    , m_canAllocateVariablesOnStack(false)
    , m_canAllocateEnvironmentOnStack(false)
    , m_hasDescendantUsesNonIndexedVariableStorage(false)
    , m_hasEval(false)
    , m_hasWith(false)
    , m_isStrict(false)
    , m_inWith(false)
    , m_isEvalCode(false)
    , m_isEvalCodeInFunction(false)
    , m_usesArgumentsObject(false)
    , m_isFunctionExpression(false)
    , m_isFunctionDeclaration(false)
    , m_isArrowFunctionExpression(false)
    , m_isOneExpressionOnlyVirtualArrowFunctionExpression(false)
    , m_isFunctionBodyOnlyVirtualArrowFunctionExpression(false)
    , m_isClassConstructor(false)
    , m_isDerivedClassConstructor(false)
    , m_isObjectMethod(false)
    , m_isClassMethod(false)
    , m_isClassStaticMethod(false)
    , m_isGenerator(false)
    , m_isAsync(false)
    , m_needsVirtualIDOperation(false)
    , m_hasArrowParameterPlaceHolder(false)
    , m_hasParameterOtherThanIdentifier(false)
    , m_allowSuperCall(false)
    , m_allowSuperProperty(false)
    , m_allowArguments(false)
    , m_hasDynamicSourceCode(false)
#ifdef ESCARGOT_DEBUGGER
    , m_markDebugging(ctx->inDebuggingCodeMode())
#endif
#ifndef NDEBUG
    , m_scopeContext(nullptr)
#endif
{
}

void InterpretedCodeBlock::recordGlobalParsingInfo(ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction)
{
    m_isStrict = scopeCtx->m_isStrict;
    m_isClassConstructor = scopeCtx->m_isClassConstructor;
    m_isDerivedClassConstructor = scopeCtx->m_isDerivedClassConstructor;
    m_isObjectMethod = scopeCtx->m_isObjectMethod;
    m_isClassMethod = scopeCtx->m_isClassMethod;
    m_isClassStaticMethod = scopeCtx->m_isClassStaticMethod;
    m_isGenerator = scopeCtx->m_isGenerator;
    m_isAsync = scopeCtx->m_isAsync;

    m_hasEval = scopeCtx->m_hasEval;
    m_hasWith = scopeCtx->m_hasWith;
    m_inWith = scopeCtx->m_inWith;

    m_isEvalCode = isEvalCode;
    m_isEvalCodeInFunction = isEvalCodeInFunction;

    m_canUseIndexedVariableStorage = !m_hasEval && !m_isEvalCode && !m_hasWith;
    m_canAllocateEnvironmentOnStack = m_canUseIndexedVariableStorage;
    m_canAllocateVariablesOnStack = m_canAllocateEnvironmentOnStack;

    m_allowSuperCall = scopeCtx->m_allowSuperCall;
    m_allowSuperProperty = scopeCtx->m_allowSuperProperty;
    m_allowArguments = scopeCtx->m_allowArguments;

    const ASTScopeContextNameInfoVector& innerIdentifiers = scopeCtx->m_varNames;
    m_identifierInfos.resize(innerIdentifiers.size());

    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_needToAllocateOnStack = false;
        info.m_isMutable = true;
        info.m_isParameterName = innerIdentifiers[i].isParameterName();
        info.m_isExplicitlyDeclaredOrParameterName = innerIdentifiers[i].isExplicitlyDeclaredOrParameterName();
        info.m_isVarDeclaration = innerIdentifiers[i].isVarDeclaration();
        info.m_indexForIndexedStorage = SIZE_MAX;
        info.m_name = innerIdentifiers[i].name();
        m_identifierInfos[i] = info;
    }

    initBlockScopeInformation(scopeCtx);
}

void InterpretedCodeBlock::recordFunctionParsingInfo(ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction)
{
    m_isStrict = scopeCtx->m_isStrict;
    m_hasEval = scopeCtx->m_hasEval;
    m_hasWith = scopeCtx->m_hasWith;
    m_inWith = scopeCtx->m_inWith;
    m_hasArrowParameterPlaceHolder = scopeCtx->m_hasArrowParameterPlaceHolder;
    m_hasParameterOtherThanIdentifier = scopeCtx->m_hasParameterOtherThanIdentifier;
    m_functionBodyBlockIndex = scopeCtx->m_functionBodyBlockIndex;

    m_isEvalCode = isEvalCode;
    m_isEvalCodeInFunction = isEvalCodeInFunction;

    bool isFE = scopeCtx->m_nodeType == FunctionExpression || scopeCtx->m_nodeType == ArrowFunctionExpression;
    bool isFD = scopeCtx->m_nodeType == FunctionDeclaration;
    m_isFunctionDeclaration = isFD;
    m_isFunctionExpression = isFE;

    m_isArrowFunctionExpression = scopeCtx->m_isArrowFunctionExpression;
    m_isOneExpressionOnlyVirtualArrowFunctionExpression = scopeCtx->m_isOneExpressionOnlyVirtualArrowFunctionExpression;
    m_isFunctionBodyOnlyVirtualArrowFunctionExpression = scopeCtx->m_isFunctionBodyOnlyVirtualArrowFunctionExpression;
    m_isClassConstructor = scopeCtx->m_isClassConstructor;
    m_isDerivedClassConstructor = scopeCtx->m_isDerivedClassConstructor;
    m_isObjectMethod = scopeCtx->m_isObjectMethod;
    m_isClassMethod = scopeCtx->m_isClassMethod;
    m_isClassStaticMethod = scopeCtx->m_isClassStaticMethod;
    m_isGenerator = scopeCtx->m_isGenerator;
    m_isAsync = scopeCtx->m_isAsync;

    m_allowSuperCall = scopeCtx->m_allowSuperCall;
    m_allowSuperProperty = scopeCtx->m_allowSuperProperty;
    m_allowArguments = scopeCtx->m_allowArguments;

    const AtomicStringTightVector& parameterNames = scopeCtx->m_parameters;
    if (parameterNames.size() > 0) {
        size_t parameterNamesCount = parameterNames.size();
        m_parameterNames.resizeWithUninitializedValues(parameterNamesCount);
        for (size_t i = 0; i < parameterNamesCount; i++) {
            m_parameterNames[i] = parameterNames[i];
        }
    }

    m_canUseIndexedVariableStorage = !m_hasEval && !m_isEvalCode && !m_hasWith;
    m_canAllocateEnvironmentOnStack = m_canUseIndexedVariableStorage && !m_isGenerator && !m_isAsync;
    m_canAllocateVariablesOnStack = true;
    m_hasDescendantUsesNonIndexedVariableStorage = false;

    const ASTScopeContextNameInfoVector& innerIdentifiers = scopeCtx->m_varNames;
    m_identifierInfos.resizeWithUninitializedValues(innerIdentifiers.size());
    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_needToAllocateOnStack = m_canUseIndexedVariableStorage;
        info.m_isMutable = true;
        info.m_isParameterName = innerIdentifiers[i].isParameterName();
        info.m_isExplicitlyDeclaredOrParameterName = innerIdentifiers[i].isExplicitlyDeclaredOrParameterName();
        info.m_isVarDeclaration = innerIdentifiers[i].isVarDeclaration();
        info.m_indexForIndexedStorage = SIZE_MAX;
        info.m_name = innerIdentifiers[i].name();
        m_identifierInfos[i] = info;
    }

    initBlockScopeInformation(scopeCtx);
}

void InterpretedCodeBlock::captureArguments()
{
    AtomicString arguments = m_context->staticStrings().arguments;
    ASSERT(!hasParameterName(arguments));
    ASSERT(!isGlobalCodeBlock() && !isArrowFunctionExpression());

    if (m_usesArgumentsObject) {
        return;
    }

    m_usesArgumentsObject = true;
    if (findVarName(arguments) == SIZE_MAX) {
        IdentifierInfo info;
        info.m_needToAllocateOnStack = true;
        info.m_isMutable = true;
        info.m_isParameterName = false;
        info.m_isExplicitlyDeclaredOrParameterName = false;
        info.m_isVarDeclaration = false;
        info.m_indexForIndexedStorage = SIZE_MAX;
        info.m_name = arguments;
        m_identifierInfos.pushBack(info);

        auto idMap = identifierInfoMap();
        if (idMap) {
            idMap->insert(std::make_pair(arguments, m_identifierInfos.size() - 1));
        }
    }
    if (m_functionLength && shouldHaveMappedArguments()) {
        ASSERT(m_functionLength == m_parameterNames.size());
        // Unmapped arguments object doesn't connect arguments object property with arguments variable
        m_canAllocateEnvironmentOnStack = false;
        for (size_t j = 0; j < m_parameterNames.size(); j++) {
            for (size_t k = 0; k < m_identifierInfos.size(); k++) {
                if (m_identifierInfos[k].m_name == m_parameterNames[j]) {
                    m_identifierInfos[k].m_needToAllocateOnStack = false;
                    break;
                }
            }
        }
    }
}

std::pair<bool, size_t> InterpretedCodeBlock::tryCaptureIdentifiersFromChildCodeBlock(LexicalBlockIndex blockIndex, AtomicString name)
{
    auto r = findNameWithinBlock(blockIndex, name);

    if (std::get<0>(r)) {
        auto& id = m_blockInfos[std::get<1>(r)]->m_identifiers[std::get<2>(r)];
        ASSERT(id.m_name == name);
        id.m_needToAllocateOnStack = false;
        return std::make_pair(true, std::get<1>(r));
    }


    if (blockIndex < m_functionBodyBlockIndex) {
        if (!isParameterName(name)) {
            return std::make_pair(false, SIZE_MAX);
        }
    }

    size_t idx = findVarName(name);
    if (idx != SIZE_MAX) {
        m_identifierInfos[idx].m_needToAllocateOnStack = false;
        return std::make_pair(true, SIZE_MAX);
    }
    return std::make_pair(false, SIZE_MAX);
}

void InterpretedCodeBlock::markHeapAllocatedEnvironmentFromHere(LexicalBlockIndex blockIndex, InterpretedCodeBlock* to)
{
    InterpretedCodeBlock* c = this;

    while (c) {
        InterpretedCodeBlock::BlockInfo* bi = nullptr;
        for (size_t i = 0; i < c->m_blockInfos.size(); i++) {
            if (c->m_blockInfos[i]->m_blockIndex == blockIndex) {
                bi = c->m_blockInfos[i];
                break;
            }
        }

        while (bi && bi->m_canAllocateEnvironmentOnStack) {
            bi->m_canAllocateEnvironmentOnStack = false;

            if (bi->m_parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX) {
                break;
            }

            for (size_t i = 0; i < c->m_blockInfos.size(); i++) {
                if (c->m_blockInfos[i]->m_blockIndex == bi->m_parentBlockIndex) {
                    bi = c->m_blockInfos[i];
                    break;
                }
            }
        }

        c->m_canAllocateEnvironmentOnStack = false;
        blockIndex = c->lexicalBlockIndexFunctionLocatedIn();

        if (c == to) {
            return;
        }

        c = c->parent();
    }
}

void InterpretedCodeBlock::computeBlockVariables(LexicalBlockIndex currentBlockIndex, size_t currentStackAllocatedVariableIndex, size_t& maxStackAllocatedVariableDepth)
{
    InterpretedCodeBlock::BlockInfo* bi = nullptr;
    for (size_t i = 0; i < m_blockInfos.size(); i++) {
        if (m_blockInfos[i]->m_blockIndex == currentBlockIndex) {
            bi = m_blockInfos[i];
            break;
        }
    }

    ASSERT(bi != nullptr);

    size_t heapIndex = 0;
    size_t stackSize = 0;
    for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
        if (!m_canAllocateVariablesOnStack) {
            bi->m_identifiers[i].m_needToAllocateOnStack = false;
        }

        if (bi->m_identifiers[i].m_needToAllocateOnStack) {
            bi->m_identifiers[i].m_indexForIndexedStorage = currentStackAllocatedVariableIndex;
            stackSize++;
            currentStackAllocatedVariableIndex++;
            maxStackAllocatedVariableDepth = std::max(maxStackAllocatedVariableDepth, currentStackAllocatedVariableIndex);
        } else {
            bi->m_identifiers[i].m_indexForIndexedStorage = heapIndex++;
            bi->m_canAllocateEnvironmentOnStack = false;
        }
    }

    // if there is no heap indexed variable, we can skip allocate env
    bool isThereHeapVariable = false;
    for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
        if (!bi->m_identifiers[i].m_needToAllocateOnStack) {
            isThereHeapVariable = true;
        }
    }

    bi->m_shouldAllocateEnvironment = isThereHeapVariable;

    for (size_t i = 0; i < m_blockInfos.size(); i++) {
        if (m_blockInfos[i]->m_parentBlockIndex == currentBlockIndex) {
            computeBlockVariables(m_blockInfos[i]->m_blockIndex, currentStackAllocatedVariableIndex, maxStackAllocatedVariableDepth);
        }
    }
}

// global block id map
// [this, ..]
// function block id map
// [this, functionName, arg0, arg1...]

void InterpretedCodeBlock::computeVariables()
{
    // we should check m_inWith
    // because CallFunctionInWithScope needs LoadByName
    m_canAllocateVariablesOnStack = !m_isEvalCode && !m_hasDescendantUsesNonIndexedVariableStorage && m_canUseIndexedVariableStorage && !m_inWith;

    if (m_canAllocateEnvironmentOnStack) {
        // we should check m_inWith
        // because CallFunctionInWithScope needs LoadByName
        m_canAllocateEnvironmentOnStack = !m_isEvalCode && !m_hasDescendantUsesNonIndexedVariableStorage && m_canUseIndexedVariableStorage && !m_inWith;
    }

    if (m_functionName.string()->length()) {
        if (m_isFunctionExpression) {
            // name of function expression is immutable
            for (size_t i = 0; i < m_identifierInfos.size(); i++) {
                if (m_identifierInfos[i].m_name == m_functionName && !m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_identifierInfos[i].m_isMutable = false;
                    break;
                }
            }
        }
    }

    if (canUseIndexedVariableStorage() && !isGlobalCodeBlock()) {
        size_t s = isGlobalCodeBlock() ? 1 : 2;
        size_t h = 0;

        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (!m_canAllocateVariablesOnStack) {
                m_identifierInfos[i].m_needToAllocateOnStack = false;
            }
            if (m_identifierInfos[i].m_name == m_functionName) {
                if (m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_isFunctionNameExplicitlyDeclared = true;
                }

                if (!m_identifierInfos[i].m_needToAllocateOnStack) {
                    m_isFunctionNameSaveOnHeap = true;
                    m_identifierInfos[i].m_indexForIndexedStorage = h;
                    h++;
                } else {
                    if (m_isFunctionNameExplicitlyDeclared) {
                        s++;
                    }
                    m_identifierInfos[i].m_indexForIndexedStorage = 1;
                }
                continue;
            }
            if (m_identifierInfos[i].m_needToAllocateOnStack) {
                m_identifierInfos[i].m_indexForIndexedStorage = s;
                s++;
            } else {
                m_identifierInfos[i].m_indexForIndexedStorage = h;
                h++;
            }
        }

        size_t currentStackAllocatedVariableIndex = 0;
        size_t maxStackAllocatedVariableDepth = 0;
        computeBlockVariables(0, currentStackAllocatedVariableIndex, maxStackAllocatedVariableDepth);
        m_lexicalBlockStackAllocatedIdentifierMaximumDepth = maxStackAllocatedVariableDepth;

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;
    } else if (isGlobalCodeBlock() && script()->isModule()) {
        size_t s = 1;
        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            m_identifierInfos[i].m_needToAllocateOnStack = false;
            m_identifierInfos[i].m_indexForIndexedStorage = h;
            h++;
        }

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;

        m_lexicalBlockStackAllocatedIdentifierMaximumDepth = 0;

        if (!canUseIndexedVariableStorage()) {
            for (size_t i = 0; i < m_blockInfos.size(); i++) {
                m_blockInfos[i]->m_canAllocateEnvironmentOnStack = false;
                m_blockInfos[i]->m_shouldAllocateEnvironment = m_blockInfos[i]->m_identifiers.size();
                for (size_t j = 0; j < m_blockInfos[i]->m_identifiers.size(); j++) {
                    m_blockInfos[i]->m_identifiers[j].m_indexForIndexedStorage = SIZE_MAX;
                    m_blockInfos[i]->m_identifiers[j].m_needToAllocateOnStack = false;
                }
            }
        } else {
            ASSERT(isGlobalCodeBlock());

            size_t currentStackAllocatedVariableIndex = 0;
            size_t maxStackAllocatedVariableDepth = 0;
            computeBlockVariables(0, currentStackAllocatedVariableIndex, maxStackAllocatedVariableDepth);
            m_lexicalBlockStackAllocatedIdentifierMaximumDepth = maxStackAllocatedVariableDepth;
        }

        InterpretedCodeBlock::BlockInfo* bi = nullptr;
        for (size_t i = 0; i < m_blockInfos.size(); i++) {
            if (m_blockInfos[i]->m_blockIndex == 0) {
                bi = m_blockInfos[i];
                break;
            }
        }
        ASSERT(!!bi);

        // global {let, const} declaration should be processed on ModuleEnvironmentRecord
        for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
            bi->m_identifiers[i].m_indexForIndexedStorage = i + identifierInfos().size();
            bi->m_identifiers[i].m_needToAllocateOnStack = false;
        }

        bi->m_shouldAllocateEnvironment = false;
    } else {
        if (m_isEvalCodeInFunction) {
            AtomicString arguments = m_context->staticStrings().arguments;
            for (size_t i = 0; i < m_identifierInfos.size(); i++) {
                if (m_identifierInfos[i].m_name == arguments) {
                    m_identifierInfos.erase(i);
                    break;
                }
            }
        }

        size_t s = isGlobalCodeBlock() ? 1 : 2;
        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            m_identifierInfos[i].m_needToAllocateOnStack = false;

            if (m_identifierInfos[i].m_name == m_functionName) {
                if (m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_isFunctionNameExplicitlyDeclared = true;
                }
                m_isFunctionNameSaveOnHeap = true;
                m_identifierInfos[i].m_indexForIndexedStorage = h;
                h++;
                continue;
            }
            m_identifierInfos[i].m_indexForIndexedStorage = h;
            h++;
        }

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;

        m_lexicalBlockStackAllocatedIdentifierMaximumDepth = 0;

        if (!canUseIndexedVariableStorage()) {
            for (size_t i = 0; i < m_blockInfos.size(); i++) {
                m_blockInfos[i]->m_canAllocateEnvironmentOnStack = false;
                m_blockInfos[i]->m_shouldAllocateEnvironment = m_blockInfos[i]->m_identifiers.size();
                for (size_t j = 0; j < m_blockInfos[i]->m_identifiers.size(); j++) {
                    m_blockInfos[i]->m_identifiers[j].m_indexForIndexedStorage = SIZE_MAX;
                    m_blockInfos[i]->m_identifiers[j].m_needToAllocateOnStack = false;
                }
            }
        } else {
            ASSERT(isGlobalCodeBlock());

            size_t currentStackAllocatedVariableIndex = 0;
            size_t maxStackAllocatedVariableDepth = 0;
            computeBlockVariables(0, currentStackAllocatedVariableIndex, maxStackAllocatedVariableDepth);
            m_lexicalBlockStackAllocatedIdentifierMaximumDepth = maxStackAllocatedVariableDepth;
        }

        if (isGlobalCodeBlock()) {
            InterpretedCodeBlock::BlockInfo* bi = nullptr;
            for (size_t i = 0; i < m_blockInfos.size(); i++) {
                if (m_blockInfos[i]->m_blockIndex == 0) {
                    bi = m_blockInfos[i];
                    break;
                }
            }
            ASSERT(!!bi);

            // global {let, const} declaration should be processed on GlobalEnvironmentRecord
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                bi->m_identifiers[i].m_indexForIndexedStorage = SIZE_MAX;
                bi->m_identifiers[i].m_needToAllocateOnStack = false;
            }

            bi->m_shouldAllocateEnvironment = false;
        }
    }
}

bool InterpretedCodeBlock::needsToLoadThisBindingFromEnvironment()
{
    if (isClassConstructor()) {
        return true;
    }
    if (isArrowFunctionExpression()) {
        InterpretedCodeBlock* cb = m_parent;
        while (cb) {
            if (cb->isArrowFunctionExpression()) {
                // pass through
            } else if (cb->isClassConstructor()) {
                return true;
            } else {
                break;
            }

            cb = cb->m_parent;
        }
        return false;
    }
    return false;
}

InterpretedCodeBlock::IndexedIdentifierInfo InterpretedCodeBlock::indexedIdentifierInfo(const AtomicString& name, ByteCodeGenerateContext* generatorContext)
{
    size_t upperIndex = 0;
    IndexedIdentifierInfo info;
    InterpretedCodeBlock* blk = this;
    LexicalBlockIndex blockIndex = generatorContext->m_lexicalBlockIndex;
    LexicalBlockIndex startBlockIndex = blockIndex;
    while (blk && blk->canUseIndexedVariableStorage()) {
        // search block first.
        while (true) {
            InterpretedCodeBlock::BlockInfo* bi = nullptr;
            for (size_t i = 0; i < blk->m_blockInfos.size(); i++) {
                if (blk->m_blockInfos[i]->m_blockIndex == blockIndex) {
                    bi = blk->m_blockInfos[i];
                    break;
                }
            }

            ASSERT(bi);

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                if (bi->m_identifiers[i].m_name == name) {
                    info.m_isResultSaved = true;
                    info.m_isStackAllocated = bi->m_identifiers[i].m_needToAllocateOnStack;
                    info.m_index = bi->m_identifiers[i].m_indexForIndexedStorage;
                    if (info.m_isStackAllocated) {
                        info.m_index += identifierOnStackCount();
                    }
                    info.m_upperIndex = upperIndex + generatorContext->m_openedNonBlockEnvCount;
                    info.m_isMutable = bi->m_identifiers[i].m_isMutable;
                    info.m_type = IndexedIdentifierInfo::DeclarationType::LexicallyDeclared;
                    info.m_blockIndex = bi->m_blockIndex;

                    if (blk->isGlobalCodeBlock() && !blk->script()->isModule() && bi->m_parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX) {
                        info.m_isGlobalLexicalVariable = true;
                    } else {
                        info.m_isGlobalLexicalVariable = false;
                        ASSERT(info.m_index != SIZE_MAX);
                    }
                    return info;
                }
            }

            if (bi->m_shouldAllocateEnvironment) {
                upperIndex++;
            }

            if (bi->m_parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX) {
                break;
            }
            blockIndex = bi->m_parentBlockIndex;
        }

        if (blk->isGlobalCodeBlock() && !blk->script()->isModule()) {
            break;
        }

        bool canUseVarName;
        if (startBlockIndex < blk->functionBodyBlockIndex()) {
            canUseVarName = blk->isParameterName(name) || name == blk->functionName() || name == context()->staticStrings().arguments;
        } else {
            canUseVarName = true;
        }

        if (canUseVarName) {
            size_t index = blk->findVarName(name);
            if (index != SIZE_MAX) {
                ASSERT(blk->m_identifierInfos[index].m_indexForIndexedStorage != SIZE_MAX);
                info.m_isResultSaved = true;
                info.m_isGlobalLexicalVariable = false;
                info.m_isStackAllocated = blk->m_identifierInfos[index].m_needToAllocateOnStack;
                info.m_upperIndex = upperIndex + generatorContext->m_openedNonBlockEnvCount;
                info.m_isMutable = blk->m_identifierInfos[index].m_isMutable;
                info.m_index = blk->m_identifierInfos[index].m_indexForIndexedStorage;
                info.m_type = IndexedIdentifierInfo::DeclarationType::VarDeclared;
                return info;
            }
        }

        if (blk == this) {
            upperIndex += 1;
        } else {
            upperIndex += !blk->canAllocateEnvironmentOnStack();
        }

        startBlockIndex = blockIndex = blk->lexicalBlockIndexFunctionLocatedIn();
        blk = blk->parent();
    }

    return info;
}
} // namespace Escargot
