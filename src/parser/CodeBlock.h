#ifndef __EscargotCodeBlock__
#define __EscargotCodeBlock__

#include "runtime/String.h"
#include "runtime/AtomicString.h"
#include "parser/ast/Node.h"

namespace Escargot {

class Node;
class ByteCodeBlock;

class CodeBlock : public gc {
public:
    friend class ScriptParser;
    friend class ByteCodeGenerator;
    enum CodeBlockInitFlag {
        CodeBlockInitDefault = 0,
        CodeBlockHasEval = 1,
        CodeBlockHasWith = 1 << 1,
        CodeBlockHasYield = 1 << 2,
    };

    bool isGlobalScopeCodeBlock()
    {
        return m_parentCodeBlock == nullptr;
    }

    bool hasEvalWithYield()
    {
        return m_hasEval || m_hasWith || m_hasYield;
    }

    CodeBlock* parentCodeBlock()
    {
        return m_parentCodeBlock;
    }

    bool hasName(AtomicString name)
    {
        for (size_t i = 0; i < m_identifierInfos.size(); i ++) {
            if (m_identifierInfos[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    Node* cachedASTNode()
    {
        return m_cachedASTNode;
    }

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

protected:
    CodeBlock(Context* ctx, StringView src, const AtomicStringVector& innerIdentifiers, CodeBlock* parentBlock, CodeBlockInitFlag initFlags);
    void appendChildBlock(CodeBlock* cb)
    {
        m_childBlocks.push_back(cb);
    }
    bool tryCaptureIdentifiersFromChildCodeBlock(AtomicString name);
    void notifySelfOrChildHasEvalWithYield();
    void appendIdentifier(AtomicString name, bool onStack = false)
    {
        ASSERT(!hasName(name));
        IdentifierInfo info;
        info.m_name = name;
        info.m_needToAllocateOnStack = onStack;
        m_identifierInfos.push_back(info);
    }

    bool m_hasEval;
    bool m_hasWith;
    bool m_hasYield;
    bool m_canUseIndexedVariableStorage;
    bool m_canAllocateEnvironmentOnStack;
    Context* m_context;
    StringView m_src;

    struct IdentifierInfo {
        bool m_needToAllocateOnStack;
        AtomicString m_name;
    };
    Vector<IdentifierInfo, gc_malloc_ignore_off_page_allocator<IdentifierInfo>> m_identifierInfos;

    CodeBlock* m_parentCodeBlock;
    Vector<CodeBlock*, gc_malloc_ignore_off_page_allocator<CodeBlock*>> m_childBlocks;

    Node* m_cachedASTNode;
    ByteCodeBlock* m_byteCodeBlock;

#ifndef NDEBUG
    NodeLOC m_locStart;
    NodeLOC m_locEnd;
    ASTScopeContext* m_scopeContext;
#endif
};

}

#endif
