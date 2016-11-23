#include "Escargot.h"
#include "CodeBlock.h"

namespace Escargot {

CodeBlock::CodeBlock(Context* ctx, StringView src, const AtomicStringVector& innerIdentifiers, CodeBlock* parentBlock, CodeBlockInitFlag initFlags)
    : m_context(ctx)
    , m_src(src)
    , m_parentCodeBlock(parentBlock)
    , m_cachedASTNode(nullptr)
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

    m_canAllocateEnvironmentOnStack = true;
    m_canUseVectorStorage = !m_hasEval && !m_hasWith && !m_hasYield && parentBlock != nullptr;

    if (m_canUseVectorStorage) {
        for (size_t i = 0; i < innerIdentifiers.size(); i ++) {
            IdentifierInfo info;
            info.m_name = innerIdentifiers[i];
            info.m_needToAllocateOnStack = true;
            m_identifierInfos.push_back(info);
        }
    } else {
        m_canAllocateEnvironmentOnStack = false;
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
    m_canAllocateEnvironmentOnStack = true;
    m_canUseVectorStorage = false;
    m_identifierInfos.clear();
}

}
