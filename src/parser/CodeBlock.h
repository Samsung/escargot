#ifndef __EscargotCodeBlock__
#define __EscargotCodeBlock__

#include "runtime/String.h"
#include "runtime/AtomicString.h"
#include "parser/ast/Node.h"
#include "runtime/ExecutionState.h"

namespace Escargot {

class Node;
class ByteCodeBlock;
class LexicalEnvironment;
class CodeBlock;

typedef Vector<CodeBlock*, gc_malloc_ignore_off_page_allocator<CodeBlock*>> CodeBlockVector;

// length of argv is same with NativeFunctionInfo.m_argumentCount
typedef Value (*NativeFunctionPointer)(ExecutionState& state, Value thisValue, Value* argv, bool isNewExpression);

struct NativeFunctionInfo {
    bool m_isStrict;
    AtomicString m_name;
    NativeFunctionPointer m_nativeFunction;
    size_t m_argumentCount;

    NativeFunctionInfo(AtomicString name, NativeFunctionPointer fn, size_t argc, bool isStrict = true)
        : m_isStrict(isStrict)
        , m_name(name)
        , m_nativeFunction(fn)
        , m_argumentCount(argc)
    {
    }
};


class CodeBlock : public gc {
    friend class ScriptParser;
    friend class ByteCodeGenerator;
public:
    // init native CodeBlock
    CodeBlock(Context* ctx, const NativeFunctionInfo& info);

    enum CodeBlockInitFlag {
        CodeBlockInitDefault = 0,
        CodeBlockHasEval = 1,
        CodeBlockHasWith = 1 << 1,
        CodeBlockHasYield = 1 << 2,
        CodeBlockIsFunctionDeclaration = 1 << 3,
        CodeBlockIsFunctionExpression = 1 << 4,
    };

    struct IdentifierInfo {
        bool m_needToAllocateOnStack;
        size_t m_indexForIndexedStorage;
        AtomicString m_name;
    };

    typedef Vector<IdentifierInfo, gc_malloc_ignore_off_page_allocator<IdentifierInfo>> IdentifierInfoVector;

    Context* context()
    {
        return m_context;
    }

    bool isGlobalScopeCodeBlock()
    {
        return m_parentCodeBlock == nullptr;
    }

    bool inEvalWithYieldScope()
    {
        CodeBlock* cb = this;
        while (cb) {
            if (hasEvalWithYield()) {
                return true;
            }
            cb = cb->parentCodeBlock();
        }
        return false;
    }

    bool hasEvalWithYield()
    {
        return m_hasEval || m_hasWith || m_hasYield;
    }

    CodeBlock* parentCodeBlock()
    {
        return m_parentCodeBlock;
    }

    const CodeBlockVector& childBlocks()
    {
        return m_childBlocks;
    }

    bool hasName(const AtomicString& name)
    {
        for (size_t i = 0; i < m_identifierInfos.size(); i ++) {
            if (m_identifierInfos[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    size_t findName(const AtomicString& name)
    {
        for (size_t i = 0; i < m_identifierInfos.size(); i ++) {
            if (m_identifierInfos[i].m_name == name) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    Node* cachedASTNode()
    {
        return m_cachedASTNode;
    }

    StringView src()
    {
        return m_src;
    }

    NodeLOC sourceElementStart()
    {
        return m_sourceElementStart;
    }

    size_t astNodeStartIndex()
    {
        return m_astNodeStartIndex;
    }

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

    bool isStrict()
    {
        return m_isStrict;
    }

    bool canUseIndexedVariableStorage()
    {
        return m_canUseIndexedVariableStorage;
    }

    bool canAllocateEnvironmentOnStack()
    {
        return m_canAllocateEnvironmentOnStack;
    }

    bool isFunctionDeclaration()
    {
        return m_isFunctionDeclaration;
    }

    bool isFunctionExpression()
    {
        return m_isFunctionExpression;
    }


    const IdentifierInfoVector& identifierInfos()
    {
        return m_identifierInfos;
    }

    size_t identifierOnStackCount()
    {
        return m_identifierOnStackCount;
    }

    size_t identifierOnHeapCount()
    {
        return m_identifierOnHeapCount;
    }

    AtomicString functionName()
    {
        // check function
        ASSERT(m_parentCodeBlock || m_isNativeFunction);
        return m_functionName;
    }

    const AtomicStringVector& functionParameters()
    {
        // check function
        ASSERT(m_parentCodeBlock || m_isNativeFunction);
        return m_parameterNames;
    }

    bool needsComplexParameterCopy()
    {
        return m_needsComplexParameterCopy;
    }

    struct FunctionParametersInfo {
        bool m_isHeapAllocated;
        size_t m_index;
    };
    typedef Vector<FunctionParametersInfo, gc_malloc_atomic_ignore_off_page_allocator<FunctionParametersInfo>> FunctionParametersInfoVector;
    const FunctionParametersInfoVector& parametersInfomation()
    {
        return m_parametersInfomation;
    }

    struct IndexedIdentifierInfo {
        bool m_isResultSaved;
        bool m_isStackAllocated;
        size_t m_upperIndex;
        size_t m_index;
    };

    IndexedIdentifierInfo indexedIdentifierInfo(const AtomicString& name)
    {
        size_t upperIndex = 0;
        IndexedIdentifierInfo info;

        CodeBlock* blk = this;
        while (blk) {
            if (blk->canUseIndexedVariableStorage()) {
                size_t index = blk->findName(name);
                if (index != SIZE_MAX) {
                    info.m_isResultSaved = true;
                    info.m_isStackAllocated = blk->m_identifierInfos[index].m_needToAllocateOnStack;
                    info.m_upperIndex = upperIndex;
                    info.m_index = blk->m_identifierInfos[index].m_indexForIndexedStorage;
                    return info;
                }
            } else {
                break;
            }
            upperIndex++;
            blk = blk->parentCodeBlock();
        }

        info.m_isResultSaved = false;
        return info;
    }

    bool hasNonConfiguableNameOnGlobal(const AtomicString& name);

protected:
    // init global codeBlock
    CodeBlock(Context* ctx, StringView src, bool isStrict, size_t astNodeStartIndex, const AtomicStringVector& innerIdentifiers);

    // init function codeBlock
    CodeBlock(Context* ctx, StringView src, NodeLOC sourceElementStart, bool isStrict, size_t astNodeStartIndex, AtomicString functionName, const AtomicStringVector& parameterNames, const AtomicStringVector& innerIdentifiers, CodeBlock* parentBlock, CodeBlockInitFlag initFlags);

    void computeVariables();
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

    bool m_isNativeFunction;
    bool m_isStrict;
    bool m_hasEval;
    bool m_hasWith;
    bool m_hasYield;
    bool m_canUseIndexedVariableStorage;
    bool m_canAllocateEnvironmentOnStack;
    bool m_isFunctionExpression;
    bool m_isFunctionDeclaration;
    bool m_needsComplexParameterCopy;

    Context* m_context;
    StringView m_src; // function source elements src
    NodeLOC m_sourceElementStart;
    size_t m_astNodeStartIndex;

    size_t m_identifierOnStackCount;
    size_t m_identifierOnHeapCount;
    Vector<IdentifierInfo, gc_malloc_ignore_off_page_allocator<IdentifierInfo>> m_identifierInfos;

    // function info
    AtomicString m_functionName;
    AtomicStringVector m_parameterNames;
    FunctionParametersInfoVector m_parametersInfomation;

    CodeBlock* m_parentCodeBlock;
    CodeBlockVector m_childBlocks;

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
