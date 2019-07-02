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
#include "CodeBlock.h"
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

void* CodeBlock::operator new(size_t size)
{
#ifdef GC_DEBUG
    return CustomAllocator<CodeBlock>().allocate(1);
#else
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(CodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_context));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_byteCodeBlock));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(CodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

void* InterpretedCodeBlock::operator new(size_t size)
{
#ifdef GC_DEBUG
    return CustomAllocator<InterpretedCodeBlock>().allocate(1);
#else
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(InterpretedCodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_context));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_script));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_identifierInfos));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_parametersInfomation));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_parentCodeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_childBlocks));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_byteCodeBlock));
#ifndef NDEBUG
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(InterpretedCodeBlock, m_scopeContext));
#endif
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(InterpretedCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

CodeBlock::CodeBlock(Context* ctx, const NativeFunctionInfo& info)
    : m_context(ctx)
    , m_parameterCount(info.m_argumentCount)
    , m_functionName(info.m_name)
{
    m_scopeInfo.isConstructor = info.m_isConstructor;
    m_scopeInfo.isStrict = info.m_isStrict;
    m_scopeInfo.hasCallNativeFunctionCode = true;
    m_scopeInfo.isFunctionNameSaveOnHeap = false;
    m_scopeInfo.isFunctionNameExplicitlyDeclared = false;
    m_scopeInfo.canUseIndexedVariableStorage = true;
    m_scopeInfo.canAllocateEnvironmentOnStack = true;
    m_scopeInfo.needsComplexParameterCopy = false;
    m_scopeInfo.hasEval = false;
    m_scopeInfo.hasWith = false;
    m_scopeInfo.hasSuper = false;
    m_scopeInfo.hasCatch = false;
    m_scopeInfo.hasYield = false;
    m_scopeInfo.inCatch = false;
    m_scopeInfo.inWith = false;
    m_scopeInfo.usesArgumentsObject = false;
    m_scopeInfo.isFunctionExpression = false;
    m_scopeInfo.isFunctionDeclaration = false;
    m_scopeInfo.isFunctionDeclarationWithSpecialBinding = false;
    m_scopeInfo.isArrowFunctionExpression = false;
    m_scopeInfo.isClassConstructor = false;
    m_scopeInfo.isGenerator = false;
    m_scopeInfo.isInWithScope = false;
    m_scopeInfo.isEvalCodeInFunction = false;
    m_scopeInfo.needsVirtualIDOperation = false;
    m_scopeInfo.needToLoadThisValue = false;
    m_scopeInfo.hasRestElement = false;

    auto data = new (PointerFreeGC) CallNativeFunctionData();
    m_nativeFunctionData = (CallNativeFunctionData*)data;

    data->m_ctorFn = info.m_nativeFunctionConstructor;
    data->m_fn = info.m_nativeFunction;
}

CodeBlock::CodeBlock(Context* ctx, AtomicString name, size_t argc, bool isStrict, bool isCtor, CallNativeFunctionData* info)
    : m_context(ctx)
    , m_parameterCount(argc)
    , m_functionName(name)
    , m_nativeFunctionData(info)
{
    m_scopeInfo.isConstructor = isCtor;
    m_scopeInfo.isStrict = isStrict;
    m_scopeInfo.hasCallNativeFunctionCode = true;
    m_scopeInfo.isFunctionNameSaveOnHeap = false;
    m_scopeInfo.isFunctionNameExplicitlyDeclared = false;
    m_scopeInfo.canUseIndexedVariableStorage = true;
    m_scopeInfo.canAllocateEnvironmentOnStack = true;
    m_scopeInfo.needsComplexParameterCopy = false;
    m_scopeInfo.hasEval = false;
    m_scopeInfo.hasWith = false;
    m_scopeInfo.hasSuper = false;
    m_scopeInfo.hasCatch = false;
    m_scopeInfo.hasYield = false;
    m_scopeInfo.inCatch = false;
    m_scopeInfo.inWith = false;
    m_scopeInfo.usesArgumentsObject = false;
    m_scopeInfo.isFunctionExpression = false;
    m_scopeInfo.isFunctionDeclaration = false;
    m_scopeInfo.isFunctionDeclarationWithSpecialBinding = false;
    m_scopeInfo.isArrowFunctionExpression = false;
    m_scopeInfo.isClassConstructor = false;
    m_scopeInfo.isGenerator = false;
    m_scopeInfo.isInWithScope = false;
    m_scopeInfo.isEvalCodeInFunction = false;
    m_scopeInfo.needsVirtualIDOperation = false;
    m_scopeInfo.needToLoadThisValue = false;
    m_scopeInfo.hasRestElement = false;
}

InterpretedCodeBlock::InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart)
    : m_script(script)
    , m_src(src)
    , m_sourceElementStart(sourceElementStart)
    , m_shouldReparseArguments(false)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_parentCodeBlock(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_context = ctx;
    m_byteCodeBlock = nullptr;
    m_parameterCount = 0;

    m_scopeInfo.isConstructor = false;
    m_scopeInfo.isStrict = scopeCtx->m_isStrict;
    m_scopeInfo.hasCallNativeFunctionCode = false;
    m_scopeInfo.isFunctionNameSaveOnHeap = false;
    m_scopeInfo.isFunctionNameExplicitlyDeclared = false;
    m_scopeInfo.canUseIndexedVariableStorage = false;
    m_scopeInfo.canAllocateEnvironmentOnStack = false;
    m_scopeInfo.needsComplexParameterCopy = false;
    m_scopeInfo.hasEval = scopeCtx->m_hasEval;
    m_scopeInfo.hasWith = scopeCtx->m_hasWith;
    m_scopeInfo.hasSuper = false;
    m_scopeInfo.hasCatch = scopeCtx->m_hasCatch;
    m_scopeInfo.hasYield = scopeCtx->m_hasYield;
    m_scopeInfo.inCatch = false;
    m_scopeInfo.inWith = false;
    m_scopeInfo.usesArgumentsObject = false;
    m_scopeInfo.isFunctionExpression = false;
    m_scopeInfo.isFunctionDeclaration = false;
    m_scopeInfo.isFunctionDeclarationWithSpecialBinding = false;
    m_scopeInfo.isArrowFunctionExpression = false;
    m_scopeInfo.isClassConstructor = scopeCtx->m_isClassConstructor;
    m_scopeInfo.isGenerator = scopeCtx->m_isGenerator;
    m_scopeInfo.isInWithScope = false;
    m_scopeInfo.isEvalCodeInFunction = false;
    m_scopeInfo.needsVirtualIDOperation = false;
    m_scopeInfo.needToLoadThisValue = false;
    m_scopeInfo.hasRestElement = scopeCtx->m_hasRestElement;

    const ASTScopeContextNameInfoVector& innerIdentifiers = scopeCtx->m_names;

    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i].name();
        info.m_needToAllocateOnStack = false;
        info.m_isMutable = true;
        info.m_isExplicitlyDeclaredOrParameterName = innerIdentifiers[i].isExplicitlyDeclaredOrParameterName();
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }
}

InterpretedCodeBlock::InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart, InterpretedCodeBlock* parentBlock)
    : m_script(script)
    , m_paramsSrc(scopeCtx->m_hasNonIdentArgument ? StringView(src, scopeCtx->m_paramsStart.index, scopeCtx->m_locStart.index) : StringView())
    , m_src(StringView(src, scopeCtx->m_locStart.index, scopeCtx->m_locEnd.index))
    , m_sourceElementStart(sourceElementStart)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_parentCodeBlock(parentBlock)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    bool isFE = scopeCtx->m_nodeType == FunctionExpression || scopeCtx->m_nodeType == ArrowFunctionExpression;
    bool isFD = scopeCtx->m_nodeType == FunctionDeclaration;

    if (scopeCtx->m_needsSpecialInitialize)
        isFD = false;

    const AtomicStringTightVector& parameterNames = scopeCtx->m_parameters;

    m_context = ctx;
    m_byteCodeBlock = nullptr;
    m_functionName = scopeCtx->m_functionName;
    m_parameterCount = scopeCtx->m_hasRestElement ? parameterNames.size() - 1 : parameterNames.size();
    m_shouldReparseArguments = scopeCtx->m_hasNonIdentArgument;

    m_scopeInfo.isStrict = scopeCtx->m_isStrict;
    m_scopeInfo.hasCallNativeFunctionCode = false;
    m_scopeInfo.isFunctionNameSaveOnHeap = false;
    m_scopeInfo.isFunctionNameExplicitlyDeclared = false;
    m_scopeInfo.canUseIndexedVariableStorage = false;
    m_scopeInfo.canAllocateEnvironmentOnStack = false;
    m_scopeInfo.needsComplexParameterCopy = false;
    m_scopeInfo.hasEval = scopeCtx->m_hasEval;
    m_scopeInfo.hasWith = scopeCtx->m_hasWith;
    m_scopeInfo.hasSuper = false;
    m_scopeInfo.hasCatch = scopeCtx->m_hasCatch;
    m_scopeInfo.hasYield = scopeCtx->m_hasYield;
    m_scopeInfo.inCatch = scopeCtx->m_inCatch;
    m_scopeInfo.inWith = scopeCtx->m_inWith;
    m_scopeInfo.usesArgumentsObject = false;
    m_scopeInfo.isFunctionExpression = isFE;
    m_scopeInfo.isFunctionDeclaration = isFD;
    m_scopeInfo.isFunctionDeclarationWithSpecialBinding = scopeCtx->m_needsSpecialInitialize;
    m_scopeInfo.isArrowFunctionExpression = scopeCtx->m_isArrowFunctionExpression;
    m_scopeInfo.isConstructor = !scopeCtx->m_isArrowFunctionExpression;
    m_scopeInfo.isClassConstructor = scopeCtx->m_isClassConstructor;
    m_scopeInfo.isGenerator = scopeCtx->m_isGenerator;
    m_scopeInfo.isInWithScope = false;
    m_scopeInfo.isEvalCodeInFunction = false;
    m_scopeInfo.needsVirtualIDOperation = false;
    m_scopeInfo.needToLoadThisValue = false;
    m_scopeInfo.hasRestElement = scopeCtx->m_hasRestElement;

    m_parametersInfomation.resizeWithUninitializedValues(parameterNames.size());
    for (size_t i = 0; i < parameterNames.size(); i++) {
        m_parametersInfomation[i].m_name = parameterNames[i];
        m_parametersInfomation[i].m_isDuplicated = false;
    }

    m_scopeInfo.canUseIndexedVariableStorage = !hasEvalWithYield() && !m_scopeInfo.inCatch && !m_scopeInfo.inWith && !scopeCtx->m_hasArrowSuper && !m_shouldReparseArguments && !m_scopeInfo.isGenerator;
    m_scopeInfo.canAllocateEnvironmentOnStack = m_scopeInfo.canUseIndexedVariableStorage;

    const ASTScopeContextNameInfoVector& innerIdentifiers = scopeCtx->m_names;
    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i].name();
        info.m_needToAllocateOnStack = m_scopeInfo.canUseIndexedVariableStorage;
        info.m_isMutable = true;
        info.m_isExplicitlyDeclaredOrParameterName = innerIdentifiers[i].isExplicitlyDeclaredOrParameterName();
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }
}

bool InterpretedCodeBlock::needToStoreThisValue()
{
    return hasName(m_context->staticStrings().stringThis);
}

void InterpretedCodeBlock::captureThis()
{
    ASSERT(!isGlobalScopeCodeBlock());

    if (hasName(m_context->staticStrings().stringThis)) {
        return;
    }

    m_scopeInfo.canAllocateEnvironmentOnStack = false;

    IdentifierInfo info;
    info.m_name = m_context->staticStrings().stringThis;
    info.m_needToAllocateOnStack = false;
    info.m_isMutable = true;
    info.m_isExplicitlyDeclaredOrParameterName = false;
    info.m_indexForIndexedStorage = SIZE_MAX;
    m_identifierInfos.push_back(info);
}

void InterpretedCodeBlock::captureArguments()
{
    AtomicString arguments = m_context->staticStrings().arguments;
    ASSERT(!hasParameter(arguments));
    ASSERT(!isGlobalScopeCodeBlock() && !isArrowFunctionExpression());

    if (m_scopeInfo.usesArgumentsObject) {
        return;
    }

    m_scopeInfo.usesArgumentsObject = true;
    if (!hasName(arguments)) {
        IdentifierInfo info;
        info.m_indexForIndexedStorage = SIZE_MAX;
        info.m_name = arguments;
        info.m_needToAllocateOnStack = true;
        info.m_isMutable = true;
        m_identifierInfos.pushBack(info);
    }
    if (m_parameterCount) {
        m_scopeInfo.canAllocateEnvironmentOnStack = false;
        for (size_t j = 0; j < m_parametersInfomation.size(); j++) {
            for (size_t k = 0; k < m_identifierInfos.size(); k++) {
                if (m_identifierInfos[k].m_name == m_parametersInfomation[j].m_name) {
                    m_identifierInfos[k].m_needToAllocateOnStack = false;
                    break;
                }
            }
        }
    }
}

bool InterpretedCodeBlock::tryCaptureIdentifiersFromChildCodeBlock(AtomicString name)
{
    for (size_t i = 0; i < m_identifierInfos.size(); i++) {
        if (m_identifierInfos[i].m_name == name) {
            m_scopeInfo.canAllocateEnvironmentOnStack = false;
            m_identifierInfos[i].m_needToAllocateOnStack = false;
            return true;
        }
    }
    return false;
}

void InterpretedCodeBlock::notifySelfOrChildHasEvalWithYield()
{
    m_scopeInfo.canAllocateEnvironmentOnStack = false;
    m_scopeInfo.canUseIndexedVariableStorage = false;

    for (size_t i = 0; i < m_identifierInfos.size(); i++) {
        m_identifierInfos[i].m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos[i].m_needToAllocateOnStack = false;
    }
}

// global block id map
// [this, ..]
// function block id map
// [this, functionName, arg0, arg1...]

void InterpretedCodeBlock::computeVariables()
{
    if (m_scopeInfo.usesArgumentsObject) {
        m_scopeInfo.canAllocateEnvironmentOnStack = false;
    }

    if (inEvalWithYieldScope() || inNotIndexedCodeBlockScope() || hasCatch()) {
        m_scopeInfo.canAllocateEnvironmentOnStack = false;
    }

    if (!m_scopeInfo.canAllocateEnvironmentOnStack) {
        CodeBlock* cb = parentCodeBlock();
        while (cb && cb->isInterpretedCodeBlock()) {
            cb->m_scopeInfo.canAllocateEnvironmentOnStack = false;
            cb = cb->asInterpretedCodeBlock()->parentCodeBlock();
        }
    }

    if (m_functionName.string()->length()) {
        if (m_scopeInfo.isFunctionExpression) {
            // name of function expression is immuable
            for (size_t i = 0; i < m_identifierInfos.size(); i++) {
                if (m_identifierInfos[i].m_name == m_functionName && !m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_identifierInfos[i].m_isMutable = false;
                    m_scopeInfo.needsComplexParameterCopy = true;
                    break;
                }
            }
        }

        for (size_t i = 0; i < m_parametersInfomation.size(); i++) {
            AtomicString name = m_parametersInfomation[i].m_name;
            if (name == m_functionName) {
                m_scopeInfo.needsComplexParameterCopy = true;
                break;
            }
        }
    }

    if (canUseIndexedVariableStorage()) {
        size_t s = isGlobalScopeCodeBlock() ? 1 : 2;
        size_t h = 0;

        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == m_functionName) {
                m_scopeInfo.needsComplexParameterCopy = true;
                if (m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_scopeInfo.isFunctionNameExplicitlyDeclared = true;
                }

                if (!m_identifierInfos[i].m_needToAllocateOnStack) {
                    m_scopeInfo.isFunctionNameSaveOnHeap = true;
                    m_identifierInfos[i].m_indexForIndexedStorage = h;
                    h++;
                } else {
                    if (m_scopeInfo.isFunctionNameExplicitlyDeclared) {
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

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;

        size_t siz = m_parametersInfomation.size();

        std::vector<std::pair<AtomicString, size_t>> computedNameIndex;

        for (size_t i = 0; i < siz; i++) {
            AtomicString name = m_parametersInfomation[i].m_name;
            size_t idIndex = findName(name);
            bool isHeap = !m_identifierInfos[idIndex].m_needToAllocateOnStack;
            size_t indexInIdInfo = m_identifierInfos[idIndex].m_indexForIndexedStorage;
            if (!isHeap) {
                indexInIdInfo -= 2;
            }
            m_parametersInfomation[i].m_isHeapAllocated = isHeap;
            m_parametersInfomation[i].m_name = name;

            if (name == m_functionName) {
                if (m_identifierInfos[idIndex].m_needToAllocateOnStack) {
                    m_parametersInfomation[i].m_index = -1;
                    m_parametersInfomation[i].m_isHeapAllocated = false;
                } else {
                    m_parametersInfomation[i].m_index = indexInIdInfo;
                    m_parametersInfomation[i].m_isHeapAllocated = true;
                }
                continue;
            }

            bool computed = false;
            size_t computedIndex = SIZE_MAX;
            for (size_t j = 0; j < computedNameIndex.size(); j++) {
                if (computedNameIndex[j].first == name) {
                    // found dup parameter name!
                    m_scopeInfo.needsComplexParameterCopy = true;
                    computed = true;
                    computedIndex = computedNameIndex[j].second;

                    for (size_t k = i - 1;; k--) {
                        if (m_parametersInfomation[k].m_name == name) {
                            m_parametersInfomation[k].m_isDuplicated = true;
                            break;
                        }
                    }

                    break;
                }
            }

            if (!computed) {
                if (isHeap) {
                    m_parametersInfomation[i].m_index = indexInIdInfo;
                    m_scopeInfo.needsComplexParameterCopy = true;
                    computedNameIndex.push_back(std::make_pair(name, indexInIdInfo));
                } else {
                    m_parametersInfomation[i].m_index = indexInIdInfo;
                    computedNameIndex.push_back(std::make_pair(name, indexInIdInfo));
                }
            } else {
                m_parametersInfomation[i].m_index = computedNameIndex[computedIndex].second;
            }
        }
    } else {
        m_scopeInfo.needsComplexParameterCopy = true;

        if (m_scopeInfo.isEvalCodeInFunction) {
            AtomicString arguments = m_context->staticStrings().arguments;
            for (size_t i = 0; i < m_identifierInfos.size(); i++) {
                if (m_identifierInfos[i].m_name == arguments) {
                    m_identifierInfos.erase(i);
                    break;
                }
            }
        }

        size_t s = isGlobalScopeCodeBlock() ? 1 : 2;
        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            m_identifierInfos[i].m_needToAllocateOnStack = false;

            if (m_identifierInfos[i].m_name == m_functionName) {
                if (m_identifierInfos[i].m_isExplicitlyDeclaredOrParameterName) {
                    m_scopeInfo.isFunctionNameExplicitlyDeclared = true;
                }
                m_scopeInfo.isFunctionNameSaveOnHeap = true;
                m_identifierInfos[i].m_indexForIndexedStorage = h;
                h++;
                continue;
            }
            m_identifierInfos[i].m_indexForIndexedStorage = h;
            h++;
        }

        size_t siz = m_parametersInfomation.size();
        for (size_t i = 0; i < siz; i++) {
            m_parametersInfomation[i].m_isHeapAllocated = true;
            m_parametersInfomation[i].m_index = std::numeric_limits<int32_t>::max();
        }

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;
    }
}
}
