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

#ifndef __EscargotCodeBlock__
#define __EscargotCodeBlock__

#include "parser/ast/Node.h"
#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"
#include "runtime/String.h"

namespace Escargot {

class Node;
class ByteCodeBlock;
class LexicalEnvironment;
class CodeBlock;
class Script;

typedef TightVector<CodeBlock*, GCUtil::gc_malloc_ignore_off_page_allocator<CodeBlock*>> CodeBlockVector;

// length of argv is same with NativeFunctionInfo.m_argumentCount
typedef Value (*NativeFunctionPointer)(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
typedef Object* (*NativeFunctionConstructor)(ExecutionState& state, size_t argc, Value* argv);


struct NativeFunctionInfo {
    enum Flags {
        Strict = 1,
        Consturctor = 1 << 1,
    };
    bool m_isStrict;
    bool m_isConsturctor;
    AtomicString m_name;
    NativeFunctionPointer m_nativeFunction;
    NativeFunctionConstructor m_nativeFunctionConstructor;
    size_t m_argumentCount;

    NativeFunctionInfo(AtomicString name, NativeFunctionPointer fn, size_t argc, NativeFunctionConstructor ctor = nullptr, int flags = Flags::Strict | Flags::Consturctor)
        : m_isStrict(flags & Strict)
        , m_isConsturctor(flags & Consturctor)
        , m_name(name)
        , m_nativeFunction(fn)
        , m_nativeFunctionConstructor(ctor)
        , m_argumentCount(argc)
    {
        if (!m_isConsturctor) {
            ASSERT(ctor == nullptr);
        } else {
            ASSERT(ctor);
        }
    }
};


class CodeBlock : public gc {
    friend class Script;
    friend class ScriptParser;
    friend class ByteCodeGenerator;
    friend class FunctionObject;
    friend int getValidValueInCodeBlock(void* ptr, GC_mark_custom_result* arr);

public:
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    // init native CodeBlock
    CodeBlock(Context* ctx, const NativeFunctionInfo& info);

    // init bound CodeBlock
    CodeBlock(Context* ctx, FunctionObject* targetFunction, Value& boundThis, size_t boundArgc, Value* boundArgv);

    enum CodeBlockInitFlag {
        CodeBlockInitDefault = 0,
        CodeBlockHasEval = 1,
        CodeBlockHasWith = 1 << 1,
        CodeBlockHasCatch = 1 << 2,
        CodeBlockHasYield = 1 << 3,
        CodeBlockInCatch = 1 << 4,
        CodeBlockInWith = 1 << 5,
        CodeBlockIsFunctionDeclaration = 1 << 6,
        CodeBlockIsFunctionExpression = 1 << 7,
    };

    struct IdentifierInfo {
        bool m_needToAllocateOnStack;
        size_t m_indexForIndexedStorage;
        AtomicString m_name;
    };

    typedef Vector<IdentifierInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<IdentifierInfo>> IdentifierInfoVector;

    Context* context()
    {
        return m_context;
    }

    Script* script()
    {
        return m_script;
    }

    bool isConsturctor() const
    {
        return m_isConsturctor;
    }

    bool isGlobalScopeCodeBlock() const
    {
        return m_parentCodeBlock == nullptr;
    }

    bool inEvalWithCatchYieldScope()
    {
        CodeBlock* cb = this;
        while (cb) {
            if (cb->hasEvalWithCatchYield()) {
                return true;
            }
            cb = cb->parentCodeBlock();
        }
        return false;
    }

    bool inEvalScope()
    {
        CodeBlock* cb = this;
        while (cb) {
            if (cb->hasEval()) {
                return true;
            }
            cb = cb->parentCodeBlock();
        }
        return false;
    }

    bool inCatchWith()
    {
        return m_inCatch || m_inWith;
    }

    bool inNotIndexedCodeBlockScope()
    {
        CodeBlock* cb = this;
        while (!cb->isGlobalScopeCodeBlock()) {
            if (!cb->canUseIndexedVariableStorage()) {
                return true;
            }
            cb = cb->parentCodeBlock();
        }
        return false;
    }

    bool hasEval() const
    {
        return m_hasEval;
    }

    bool hasWith() const
    {
        return m_hasWith;
    }

    bool hasCatch() const
    {
        return m_hasCatch;
    }

    bool hasYield() const
    {
        return m_hasYield;
    }

    bool hasEvalWithCatchYield() const
    {
        return m_hasEval || m_hasWith || m_hasCatch || m_hasYield;
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
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    size_t findName(const AtomicString& name)
    {
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
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

    const StringView& src()
    {
        return m_src;
    }

    ExtendedNodeLOC sourceElementStart()
    {
        return m_sourceElementStart;
    }

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

    NativeFunctionConstructor nativeFunctionConstructor();

    bool isStrict() const
    {
        return m_isStrict;
    }

    bool canUseIndexedVariableStorage() const
    {
        return m_canUseIndexedVariableStorage;
    }

    bool canAllocateEnvironmentOnStack() const
    {
        return m_canAllocateEnvironmentOnStack;
    }

    bool isFunctionDeclaration() const
    {
        return m_isFunctionDeclaration;
    }

    bool isFunctionExpression() const
    {
        return m_isFunctionExpression;
    }

    bool hasCallNativeFunctionCode() const
    {
        return m_hasCallNativeFunctionCode;
    }

    bool usesArgumentsObject() const
    {
        return m_usesArgumentsObject;
    }

    const IdentifierInfoVector& identifierInfos() const
    {
        return m_identifierInfos;
    }

    size_t identifierOnStackCount() const
    {
        return m_identifierOnStackCount;
    }

    size_t identifierOnHeapCount() const
    {
        return m_identifierOnHeapCount;
    }

    AtomicString functionName() const
    {
        // check function
        return m_functionName;
    }

    const AtomicStringTightVector& functionParameters() const
    {
        // check function
        return m_parameterNames;
    }

    bool needsComplexParameterCopy() const
    {
        return m_needsComplexParameterCopy;
    }

    void setInWithScope()
    {
        m_isInWithScope = true;
    }

    bool isInWithScope() const
    {
        return m_isInWithScope;
    }

    struct FunctionParametersInfo {
        bool m_isHeapAllocated;
        int32_t m_index;
        AtomicString m_name;
    };
    typedef TightVector<FunctionParametersInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<FunctionParametersInfo>> FunctionParametersInfoVector;
    const FunctionParametersInfoVector& parametersInfomation() const
    {
        return m_parametersInfomation;
    }

    const AtomicStringTightVector& parameterNames() const
    {
        return m_parameterNames;
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
        while (!blk->isGlobalScopeCodeBlock()) {
            size_t index = blk->findName(name);
            if (index != SIZE_MAX) {
                info.m_isResultSaved = true;
                info.m_isStackAllocated = blk->m_identifierInfos[index].m_needToAllocateOnStack;
                info.m_upperIndex = upperIndex;
                if (blk->canUseIndexedVariableStorage()) {
                    info.m_index = blk->m_identifierInfos[index].m_indexForIndexedStorage;
                } else {
                    info.m_index = index;
                }
                return info;
            }
            upperIndex++;
            blk = blk->parentCodeBlock();
        }

        info.m_isResultSaved = false;
        return info;
    }

#ifndef NDEBUG
    ASTScopeContext* scopeContext()
    {
        return m_scopeContext;
    }
#endif
protected:
    // init global codeBlock
    CodeBlock(Context* ctx, Script* script, StringView src, bool isStrict, ExtendedNodeLOC sourceElementStart, const AtomicStringVector& innerIdentifiers, CodeBlockInitFlag initFlags);

    // init function codeBlock
    CodeBlock(Context* ctx, Script* script, StringView src, ExtendedNodeLOC sourceElementStart, bool isStrict, AtomicString functionName, const AtomicStringVector& parameterNames, const AtomicStringVector& innerIdentifiers, CodeBlock* parentBlock, CodeBlockInitFlag initFlags);

    void computeVariables();
    void appendChildBlock(CodeBlock* cb)
    {
        m_childBlocks.push_back(cb);
    }
    bool tryCaptureIdentifiersFromChildCodeBlock(AtomicString name);
    void notifySelfOrChildHasEvalWithCatchYield();
    void appendIdentifier(AtomicString name, bool onStack = false)
    {
        ASSERT(!hasName(name));
        IdentifierInfo info;
        info.m_name = name;
        info.m_needToAllocateOnStack = onStack;
        m_identifierInfos.push_back(info);
    }

    Context* m_context;

    bool m_isConsturctor : 1;
    bool m_isStrict : 1;
    bool m_hasCallNativeFunctionCode : 1;
    bool m_isFunctionNameSaveOnHeap : 1;
    bool m_canUseIndexedVariableStorage : 1;
    bool m_canAllocateEnvironmentOnStack : 1;
    bool m_needsComplexParameterCopy : 1;
    bool m_hasEval : 1;
    bool m_hasWith : 1;
    bool m_hasCatch : 1;
    bool m_hasYield : 1;
    bool m_inCatch : 1;
    bool m_inWith : 1;
    bool m_usesArgumentsObject : 1;
    bool m_isFunctionExpression : 1;
    bool m_isFunctionDeclaration : 1;
    bool m_isInWithScope : 1;
    bool m_isEvalCodeInFunction : 1;

    Script* m_script;
    StringView m_src; // function source elements src
    ExtendedNodeLOC m_sourceElementStart;

    uint16_t m_identifierOnStackCount;
    uint16_t m_identifierOnHeapCount;
    IdentifierInfoVector m_identifierInfos;

    // function info
    AtomicString m_functionName;
    AtomicStringTightVector m_parameterNames;
    FunctionParametersInfoVector m_parametersInfomation;

    CodeBlock* m_parentCodeBlock;
    CodeBlockVector m_childBlocks;

    union {
        Node* m_cachedASTNode;
        ByteCodeBlock* m_byteCodeBlock;
    };

#ifndef NDEBUG
    ExtendedNodeLOC m_locStart;
    ExtendedNodeLOC m_locEnd;
    ASTScopeContext* m_scopeContext;
#endif
};
}

#endif
