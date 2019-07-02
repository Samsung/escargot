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
    };

    bool m_isStrict : 1;
    bool m_isConstructor : 1;
    AtomicString m_name;
    NativeFunctionPointer m_nativeFunction;
    NativeFunctionConstructor m_nativeFunctionConstructor;
    size_t m_argumentCount;

    NativeFunctionInfo(AtomicString name, NativeFunctionPointer fn, size_t argc, NativeFunctionConstructor ctor = nullptr, int flags = Flags::Strict | Flags::Constructor)
        : m_isStrict(flags & Strict)
        , m_isConstructor(flags & Constructor)
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

typedef struct ScopeInfo {
    bool isConstructor : 1;
    bool isStrict : 1;
    bool hasCallNativeFunctionCode : 1;
    bool isFunctionNameSaveOnHeap : 1;
    bool isFunctionNameExplicitlyDeclared : 1;
    bool canUseIndexedVariableStorage : 1;
    bool canAllocateEnvironmentOnStack : 1;
    bool needsComplexParameterCopy : 1;
    bool hasEval : 1;
    bool hasWith : 1;
    bool hasSuper : 1;
    bool hasCatch : 1;
    bool hasYield : 1;
    bool inCatch : 1;
    bool inWith : 1;
    bool usesArgumentsObject : 1;
    bool isFunctionExpression : 1;
    bool isFunctionDeclaration : 1;
    bool isFunctionDeclarationWithSpecialBinding : 1;
    bool isArrowFunctionExpression : 1;
    bool isClassConstructor : 1;
    bool isGenerator : 1;
    bool isInWithScope : 1;
    bool isEvalCodeInFunction : 1;
    bool needsVirtualIDOperation : 1;
    bool needToLoadThisValue : 1;
    bool hasRestElement : 1;
} ScopeInfo;

class CallNativeFunctionData : public gc {
public:
    NativeFunctionPointer m_fn;
    NativeFunctionConstructor m_ctorFn;
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

    // init for public api
    CodeBlock(Context* ctx, AtomicString name, size_t argc, bool isStrict, bool isCtor, CallNativeFunctionData* info);

    struct IdentifierInfo {
        bool m_needToAllocateOnStack : 1;
        bool m_isMutable : 1;
        bool m_isExplicitlyDeclaredOrParameterName : 1;
        size_t m_indexForIndexedStorage;
        AtomicString m_name;
    };

    typedef Vector<IdentifierInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<IdentifierInfo>> IdentifierInfoVector;

    Context* context()
    {
        return m_context;
    }

    ScopeInfo descriptor()
    {
        return m_scopeInfo;
    }

    bool isConstructor() const
    {
        return m_scopeInfo.isConstructor;
    }

    bool inCatchWith()
    {
        return m_scopeInfo.inCatch || m_scopeInfo.inWith;
    }

    bool hasEval() const
    {
        return m_scopeInfo.hasEval;
    }

    bool hasWith() const
    {
        return m_scopeInfo.hasWith;
    }

    bool hasCatch() const
    {
        return m_scopeInfo.hasCatch;
    }

    bool hasYield() const
    {
        return m_scopeInfo.hasYield;
    }

    bool hasEvalWithYield() const
    {
        return m_scopeInfo.hasEval || m_scopeInfo.hasWith || m_scopeInfo.hasYield;
    }

    bool isStrict() const
    {
        return m_scopeInfo.isStrict;
    }

    bool canUseIndexedVariableStorage() const
    {
        return m_scopeInfo.canUseIndexedVariableStorage;
    }

    bool canAllocateEnvironmentOnStack() const
    {
        return m_scopeInfo.canAllocateEnvironmentOnStack;
    }

    bool isFunctionDeclaration() const
    {
        return m_scopeInfo.isFunctionDeclaration;
    }

    bool isFunctionDeclarationWithSpecialBinding() const
    {
        return m_scopeInfo.isFunctionDeclarationWithSpecialBinding;
    }

    bool isFunctionExpression() const
    {
        return m_scopeInfo.isFunctionExpression;
    }

    bool isArrowFunctionExpression() const
    {
        return m_scopeInfo.isArrowFunctionExpression;
    }

    bool isClassConstructor() const
    {
        return m_scopeInfo.isClassConstructor;
    }

    bool isGenerator() const
    {
        return m_scopeInfo.isGenerator;
    }

    bool needToLoadThisValue() const
    {
        return m_scopeInfo.needToLoadThisValue;
    }

    void setNeedToLoadThisValue()
    {
        m_scopeInfo.needToLoadThisValue = true;
    }

    bool hasCallNativeFunctionCode() const
    {
        return m_scopeInfo.hasCallNativeFunctionCode;
    }

    bool usesArgumentsObject() const
    {
        return m_scopeInfo.usesArgumentsObject;
    }

    AtomicString functionName() const
    {
        return m_functionName;
    }

    bool needsComplexParameterCopy() const
    {
        return m_scopeInfo.needsComplexParameterCopy;
    }

    void setInWithScope()
    {
        m_scopeInfo.isInWithScope = true;
    }

    void clearInWithScope()
    {
        m_scopeInfo.isInWithScope = false;
    }

    bool isInWithScope() const
    {
        return m_scopeInfo.isInWithScope;
    }

    void setHasEval()
    {
        m_scopeInfo.hasEval = true;
        m_scopeInfo.canUseIndexedVariableStorage = false;
    }

    void setAsClassConstructor()
    {
        m_scopeInfo.isClassConstructor = true;
    }

    void setNeedsVirtualIDOperation()
    {
        ASSERT(isInterpretedCodeBlock());
        m_scopeInfo.needsVirtualIDOperation = true;
    }

    bool needsVirtualIDOperation()
    {
        return m_scopeInfo.needsVirtualIDOperation;
    }

    uint16_t parameterCount()
    {
        return m_parameterCount;
    }

    bool isInterpretedCodeBlock()
    {
        return !m_scopeInfo.hasCallNativeFunctionCode;
    }

    InterpretedCodeBlock* asInterpretedCodeBlock()
    {
        ASSERT(!m_scopeInfo.hasCallNativeFunctionCode);
        return (InterpretedCodeBlock*)this;
    }

    CallNativeFunctionData* nativeFunctionData()
    {
        return m_nativeFunctionData;
    }

protected:
    CodeBlock() {}
    Context* m_context;

    ScopeInfo m_scopeInfo;
    uint16_t m_parameterCount;
    AtomicString m_functionName;

    union {
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
        bool m_isHeapAllocated : 1;
        bool m_isDuplicated : 1;
        int32_t m_index;
        AtomicString m_name;
    };
    typedef TightVector<FunctionParametersInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<FunctionParametersInfo>> FunctionParametersInfoVector;
    const FunctionParametersInfoVector& parametersInfomation() const
    {
        return m_parametersInfomation;
    }

    struct IndexedIdentifierInfo {
        bool m_isResultSaved : 1;
        bool m_isStackAllocated : 1;
        bool m_isMutable : 1;
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
