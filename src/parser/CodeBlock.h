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
class InterpretedCodeBlock;
class Script;

typedef TightVector<InterpretedCodeBlock*, GCUtil::gc_malloc_ignore_off_page_allocator<InterpretedCodeBlock*>> CodeBlockVector;

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

class CallNativeFunctionData : public gc {
public:
    NativeFunctionPointer m_fn;
    NativeFunctionConstructor m_ctorFn;
};

class CallBoundFunctionData : public CallNativeFunctionData {
public:
    SmallValue m_boundThis;
    SmallValue* m_boundArguments;
    size_t m_boundArgumentsCount;
};

class InterpretedCodeBlock;

class CodeBlock : public gc {
    friend class Script;
    friend class ScriptParser;
    friend class ByteCodeGenerator;
    friend class FunctionObject;
    friend class InterpretedCodeBlock;
    friend int getValidValueInCodeBlock(void* ptr, GC_mark_custom_result* arr);

public:
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    // init native CodeBlock
    CodeBlock(Context* ctx, const NativeFunctionInfo& info);

    // init bound CodeBlock
    CodeBlock(ExecutionState& state, FunctionObject* targetFunction, Value& boundThis, size_t boundArgc, Value* boundArgv);

    enum CodeBlockInitFlag {
        CodeBlockInitDefault = 0,
        CodeBlockHasEval = 1,
        CodeBlockHasWith = 1 << 1,
        CodeBlockHasCatch = 1 << 2,
        CodeBlockHasYield = 1 << 3,
        CodeBlockInCatch = 1 << 4,
        CodeBlockInWith = 1 << 5,
        CodeBlockIsFunctionDeclaration = 1 << 6,
        CodeBlockIsFunctionDeclarationWithSpecialBinding = 1 << 7,
        CodeBlockIsFunctionExpression = 1 << 8,
    };

    struct IdentifierInfo {
        bool m_needToAllocateOnStack;
        bool m_isMutable;
        bool m_isExplicitlyDeclaredOrParameterName;
        size_t m_indexForIndexedStorage;
        AtomicString m_name;
    };

    typedef Vector<IdentifierInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<IdentifierInfo>> IdentifierInfoVector;

    Context* context()
    {
        return m_context;
    }

    bool isConsturctor() const
    {
        return m_isConsturctor;
    }

    bool inCatchWith()
    {
        return m_inCatch || m_inWith;
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

    bool isFunctionDeclarationWithSpecialBinding() const
    {
        return m_isFunctionDeclarationWithSpecialBinding;
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

    AtomicString functionName() const
    {
        return m_functionName;
    }

    bool needsComplexParameterCopy() const
    {
        return m_needsComplexParameterCopy;
    }

    void setInWithScope()
    {
        m_isInWithScope = true;
    }

    void clearInWithScope()
    {
        m_isInWithScope = false;
    }

    bool isInWithScope() const
    {
        return m_isInWithScope;
    }

    bool isBindedFunction()
    {
        return m_isBindedFunction;
    }

    uint16_t parameterCount()
    {
        return m_parameterCount;
    }

    bool isInterpretedCodeBlock()
    {
        return !m_hasCallNativeFunctionCode;
    }

    InterpretedCodeBlock* asInterpretedCodeBlock()
    {
        ASSERT(!m_hasCallNativeFunctionCode);
        return (InterpretedCodeBlock*)this;
    }

    CallNativeFunctionData* nativeFunctionData()
    {
        return m_nativeFunctionData;
    }

    CallBoundFunctionData* boundFunctionInfo()
    {
        ASSERT(isBindedFunction());
        return (CallBoundFunctionData*)m_nativeFunctionData;
    }

protected:
    CodeBlock() {}
    Context* m_context;

    bool m_isConsturctor : 1;
    bool m_isStrict : 1;
    bool m_hasCallNativeFunctionCode : 1;
    bool m_isFunctionNameSaveOnHeap : 1;
    bool m_isFunctionNameExplicitlyDeclared : 1;
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
    bool m_isFunctionDeclarationWithSpecialBinding : 1;
    bool m_isInWithScope : 1;
    bool m_isEvalCodeInFunction : 1;
    bool m_isBindedFunction : 1;
    uint16_t m_parameterCount;

    AtomicString m_functionName;

    union {
        Node* m_cachedASTNode;
        ByteCodeBlock* m_byteCodeBlock;
        CallNativeFunctionData* m_nativeFunctionData;
    };
};

class InterpretedCodeBlock : public CodeBlock {
    friend class Script;
    friend class ScriptParser;
    friend class ByteCodeGenerator;
    friend class FunctionObject;

    friend int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr);

public:
    void computeVariables();
    void appendChildBlock(InterpretedCodeBlock* cb)
    {
        m_childBlocks.push_back(cb);
    }
    bool tryCaptureIdentifiersFromChildCodeBlock(AtomicString name);
    void notifySelfOrChildHasEvalWithCatchYield();

    struct FunctionParametersInfo {
        bool m_isHeapAllocated;
        bool m_isDuplicated;
        int32_t m_index;
        AtomicString m_name;
    };
    typedef TightVector<FunctionParametersInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<FunctionParametersInfo>> FunctionParametersInfoVector;
    const FunctionParametersInfoVector& parametersInfomation() const
    {
        return m_parametersInfomation;
    }

    struct IndexedIdentifierInfo {
        bool m_isResultSaved;
        bool m_isStackAllocated;
        bool m_isMutable;
        size_t m_upperIndex;
        size_t m_index;
    };

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

    Script* script()
    {
        return m_script;
    }

    bool inNotIndexedCodeBlockScope()
    {
        CodeBlock* cb = this;
        while (!cb->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
            if (!cb->canUseIndexedVariableStorage()) {
                return true;
            }
            cb = cb->asInterpretedCodeBlock()->parentCodeBlock();
        }
        return false;
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
            cb = cb->asInterpretedCodeBlock()->parentCodeBlock();
        }
        return false;
    }

    bool inEvalWithScope()
    {
        CodeBlock* cb = this;
        while (cb) {
            if (cb->hasEval() || cb->hasWith()) {
                return true;
            }
            cb = cb->asInterpretedCodeBlock()->parentCodeBlock();
        }
        return false;
    }

    IndexedIdentifierInfo indexedIdentifierInfo(const AtomicString& name)
    {
        size_t upperIndex = 0;
        IndexedIdentifierInfo info;

        CodeBlock* blk = this;
        while (!blk->asInterpretedCodeBlock()->isGlobalScopeCodeBlock() && blk->canUseIndexedVariableStorage()) {
            size_t index = blk->asInterpretedCodeBlock()->findName(name);
            if (index != SIZE_MAX) {
                info.m_isResultSaved = true;
                info.m_isStackAllocated = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_needToAllocateOnStack;
                info.m_upperIndex = upperIndex;
                info.m_isMutable = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_isMutable;
                if (blk->canUseIndexedVariableStorage()) {
                    info.m_index = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_indexForIndexedStorage;
                } else {
                    info.m_index = index;
                }
                return info;
            }
            upperIndex++;
            blk = blk->asInterpretedCodeBlock()->parentCodeBlock();
        }

        info.m_isResultSaved = false;
        return info;
    }

    void updateSourceElementStart(size_t line, size_t column)
    {
        m_sourceElementStart.line = line;
        m_sourceElementStart.column = column;
    }

    InterpretedCodeBlock* parentCodeBlock()
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

    const StringView& src()
    {
        return m_src;
    }

    ExtendedNodeLOC sourceElementStart()
    {
        return m_sourceElementStart;
    }

#ifndef NDEBUG
    ASTScopeContext* scopeContext()
    {
        return m_scopeContext;
    }
#endif

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

    Node* cachedASTNode()
    {
        return m_cachedASTNode;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    // init global codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, bool isStrict, ExtendedNodeLOC sourceElementStart, const ASTScopeContextNameInfoVector& innerIdentifiers, CodeBlockInitFlag initFlags);
    // init function codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ExtendedNodeLOC sourceElementStart, bool isStrict, AtomicString functionName, const AtomicStringTightVector& parameterNames, const ASTScopeContextNameInfoVector& innerIdentifiers, InterpretedCodeBlock* parentBlock, CodeBlockInitFlag initFlags);

    Script* m_script;
    StringView m_src; // function source elements src
    ExtendedNodeLOC m_sourceElementStart;

    FunctionParametersInfoVector m_parametersInfomation;
    uint16_t m_identifierOnStackCount;
    uint16_t m_identifierOnHeapCount;
    IdentifierInfoVector m_identifierInfos;

    InterpretedCodeBlock* m_parentCodeBlock;
    CodeBlockVector m_childBlocks;

#ifndef NDEBUG
    ExtendedNodeLOC m_locStart;
    ExtendedNodeLOC m_locEnd;
    ASTScopeContext* m_scopeContext;
#endif
};
}

#endif
