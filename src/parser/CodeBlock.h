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
typedef Object* (*NativeFunctionConstructor)(ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv);

struct NativeFunctionInfo {
    enum Flags {
        Strict = 1,
        Constructor = 1 << 1,
        ClassConstructor = 1 << 2,
    };
    bool m_isStrict;
    bool m_isConstructor;
    bool m_isClassConstructor;
    AtomicString m_name;
    NativeFunctionPointer m_nativeFunction;
    NativeFunctionConstructor m_nativeFunctionConstructor;
    size_t m_argumentCount;

    NativeFunctionInfo(AtomicString name, NativeFunctionPointer fn, size_t argc, NativeFunctionConstructor ctor = nullptr, int flags = Flags::Strict | Flags::Constructor)
        : m_isStrict(flags & Strict)
        , m_isConstructor(flags & Constructor)
        , m_isClassConstructor(flags & ClassConstructor)
        , m_name(name)
        , m_nativeFunction(fn)
        , m_nativeFunctionConstructor(ctor)
        , m_argumentCount(argc)
    {
        if (!m_isConstructor) {
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
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    SmallValue m_boundTargetFunction;
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

    // init for public api
    CodeBlock(Context* ctx, AtomicString name, size_t argc, bool isStrict, bool isCtor, CallNativeFunctionData* info);

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

    bool isConstructor() const
    {
        return m_isConstructor;
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

    bool hasEvalWithYield() const
    {
        return m_hasEval || m_hasWith || m_hasYield;
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

    bool isArrowFunctionExpression() const
    {
        return m_isArrowFunctionExpression;
    }

    bool isClassConstructor() const
    {
        return m_isClassConstructor;
    }

    bool needToLoadThisValue() const
    {
        return m_needToLoadThisValue;
    }

    void setNeedToLoadThisValue()
    {
        m_needToLoadThisValue = true;
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

    void setHasEval()
    {
        m_hasEval = true;
        m_canUseIndexedVariableStorage = false;
    }

    void setNeedsVirtualIDOperation()
    {
        ASSERT(isInterpretedCodeBlock());
        m_needsVirtualIDOperation = true;
    }

    bool needsVirtualIDOperation()
    {
        return m_needsVirtualIDOperation;
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

    bool m_isConstructor : 1;
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
    bool m_isArrowFunctionExpression : 1;
    bool m_isClassConstructor : 1;
    bool m_isInWithScope : 1;
    bool m_isEvalCodeInFunction : 1;
    bool m_isBindedFunction : 1;
    bool m_needsVirtualIDOperation : 1;
    bool m_needToLoadThisValue : 1;
    bool m_hasRestElement : 1;
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
    friend class ByteCodeInterpreter;

    friend int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr);

public:
    void computeVariables();
    void appendChildBlock(InterpretedCodeBlock* cb)
    {
        m_childBlocks.push_back(cb);
    }
    bool needToStoreThisValue();
    void captureThis();
    void captureArguments();
    bool tryCaptureIdentifiersFromChildCodeBlock(AtomicString name);
    void notifySelfOrChildHasEvalWithYield();

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

    bool inEvalWithYieldScope()
    {
        CodeBlock* cb = this;
        while (cb) {
            if (cb->hasEvalWithYield()) {
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

    bool shouldReparseArguments()
    {
        return m_shouldReparseArguments;
    }

    IndexedIdentifierInfo upperIndexedIdentifierInfo(const AtomicString& name)
    {
        size_t upperIndex = 1;
        IndexedIdentifierInfo info;

        ASSERT(this->parentCodeBlock());
        InterpretedCodeBlock* blk = this->parentCodeBlock();

        while (!blk->isGlobalScopeCodeBlock() && blk->canUseIndexedVariableStorage()) {
            size_t index = blk->findName(name);
            if (index != SIZE_MAX) {
                info.m_isResultSaved = true;
                info.m_isStackAllocated = blk->m_identifierInfos[index].m_needToAllocateOnStack;
                info.m_upperIndex = upperIndex;
                info.m_isMutable = blk->m_identifierInfos[index].m_isMutable;
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

    bool hasParameter(const AtomicString& name)
    {
        for (size_t i = 0; i < m_parametersInfomation.size(); i++) {
            if (m_parametersInfomation[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    const StringView& src()
    {
        return m_src;
    }

    const StringView& paramsSrc()
    {
        ASSERT(m_shouldReparseArguments);
        return m_paramsSrc;
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

    void clearCachedASTNode()
    {
        m_cachedASTNode = nullptr;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    // init global codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart);
    // init function codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart, InterpretedCodeBlock* parentBlock);

    Script* m_script;
    StringView m_paramsSrc; // function parameters elements src
    StringView m_src; // function source elements src
    ExtendedNodeLOC m_sourceElementStart;
    bool m_shouldReparseArguments : 1;

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
