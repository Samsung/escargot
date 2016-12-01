#include "Escargot.h"
#include "CodeBlock.h"
#include "runtime/Context.h"

namespace Escargot {

CodeBlock::CodeBlock(Context* ctx, StringView src, bool isStrict, size_t astNodeStartIndex, const AtomicStringVector& innerIdentifiers)
    : m_context(ctx)
    , m_src(src)
    , m_sourceElementStart(1, 0, 0)
    , m_astNodeStartIndex(astNodeStartIndex)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_parentCodeBlock(nullptr)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_isStrict = isStrict;
    m_hasEval = false;
    m_hasWith = false;
    m_hasYield = false;
    m_canUseIndexedVariableStorage = false;
    m_canAllocateEnvironmentOnStack = false;
    m_needsComplexParameterCopy = false;

    for (size_t i = 0; i < innerIdentifiers.size(); i ++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i];
        info.m_needToAllocateOnStack = false;
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }
}

CodeBlock::CodeBlock(Context* ctx, StringView src, NodeLOC sourceElementStart, bool isStrict, size_t astNodeStartIndex, AtomicString functionName, const AtomicStringVector& parameterNames, const AtomicStringVector& innerIdentifiers,
    CodeBlock* parentBlock, CodeBlockInitFlag initFlags)
    : m_context(ctx)
    , m_src(src)
    , m_sourceElementStart(sourceElementStart)
    , m_astNodeStartIndex(astNodeStartIndex)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_functionName(functionName)
    , m_parameterNames(parameterNames)
    , m_parentCodeBlock(parentBlock)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_isStrict = isStrict;
    if (initFlags & CodeBlockInitFlag::CodeBlockHasEval) {
        m_hasEval = true;
    } else {
        m_hasEval = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasWith) {
        m_hasWith = true;
    } else {
        m_hasWith = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasYield) {
        m_hasYield = true;
    } else {
        m_hasYield = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockIsFunctionDeclaration) {
        m_isFunctionDeclaration = true;
    } else {
        m_isFunctionDeclaration = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockIsFunctionExpression) {
        m_isFunctionExpression = true;
    } else {
        m_isFunctionExpression = false;
    }

    m_canUseIndexedVariableStorage = !m_hasEval && !m_hasWith && !m_hasYield && parentBlock != nullptr;

    if (m_canUseIndexedVariableStorage) {
        m_canAllocateEnvironmentOnStack = true;
    } else {
        m_canAllocateEnvironmentOnStack = false;
    }

    for (size_t i = 0; i < innerIdentifiers.size(); i ++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i];
        info.m_needToAllocateOnStack = m_canUseIndexedVariableStorage;
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }

    m_needsComplexParameterCopy = false;
}

bool CodeBlock::tryCaptureIdentifiersFromChildCodeBlock(AtomicString name)
{
    for (size_t i = 0; i < m_identifierInfos.size(); i ++) {
        if (m_identifierInfos[i].m_name == name) {
            m_canAllocateEnvironmentOnStack = false;
            m_identifierInfos[i].m_needToAllocateOnStack = false;
            return true;
        }
    }
    return false;
}

void CodeBlock::notifySelfOrChildHasEvalWithYield()
{
    m_canAllocateEnvironmentOnStack = false;
    m_canUseIndexedVariableStorage = false;
}

bool CodeBlock::hasNonConfiguableNameOnGlobal(const AtomicString& name)
{
    ASSERT(canUseIndexedVariableStorage());
    CodeBlock* top = this;
    while (top->parentCodeBlock()) {
        top = top->parentCodeBlock();
    }

    if (top->hasName(name)) {
        return true;
    }

    ExecutionState state(m_context);
    size_t idx = m_context->globalObject()->findOwnProperty(state, PropertyName(state, name));
    if (idx != SIZE_MAX) {
        ObjectPropertyDescriptor desc = m_context->globalObject()->readPropertyDescriptor(state, idx);
        if (!desc.isConfigurable()) {
            return true;
        }
    }
    return false;
}

void CodeBlock::computeVariables()
{
    if (canUseIndexedVariableStorage()) {
        size_t s = 0;
        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i ++) {
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
    }

    size_t siz = m_parameterNames.size();
    m_parametersInfomation.resize(siz);
    size_t heapCount = 0;
    size_t stackCount = 0;
    for (size_t i = 0; i < siz; i ++) {
        bool isHeap = !m_identifierInfos[i].m_needToAllocateOnStack;
        m_parametersInfomation[i].m_isHeapAllocated = isHeap;
        if (isHeap) {
            m_parametersInfomation[i].m_index = i - stackCount;
            m_needsComplexParameterCopy = true;
            heapCount++;
        } else {
            m_parametersInfomation[i].m_index = i - heapCount;
            stackCount++;
        }
    }
}

}
