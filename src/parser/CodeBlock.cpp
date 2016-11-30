#include "Escargot.h"
#include "CodeBlock.h"

namespace Escargot {

CodeBlock::CodeBlock(Context* ctx, StringView src, size_t astNodeStartIndex, const AtomicStringVector& innerIdentifiers)
    : m_context(ctx)
    , m_src(src)
    , m_astNodeStartIndex(astNodeStartIndex)
    , m_parentCodeBlock(nullptr)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_hasEval = false;
    m_hasWith = false;
    m_hasYield = false;
    m_canUseIndexedVariableStorage = false;
    m_canAllocateEnvironmentOnStack = false;

    for (size_t i = 0; i < innerIdentifiers.size(); i ++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i];
        info.m_needToAllocateOnStack = m_canUseIndexedVariableStorage;
        m_identifierInfos.push_back(info);
    }
}

CodeBlock::CodeBlock(Context* ctx, StringView src, size_t astNodeStartIndex, AtomicString functionName, const AtomicStringVector& parameterNames, const AtomicStringVector& innerIdentifiers,
    CodeBlock* parentBlock, CodeBlockInitFlag initFlags)
    : m_context(ctx)
    , m_src(src)
    , m_astNodeStartIndex(astNodeStartIndex)
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
        m_identifierInfos.push_back(info);
    }
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

}
