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

struct NativeFunctionInfo {
    enum Flags {
        Strict = 1,
        Constructor = 1 << 1,
    };

    bool m_isStrict : 1;
    bool m_isConstructor : 1;
    AtomicString m_name;
    NativeFunctionPointer m_nativeFunction;
    size_t m_argumentCount;

    NativeFunctionInfo(AtomicString name, NativeFunctionPointer fn, size_t argc, int flags = Flags::Strict | Flags::Constructor)
        : m_isStrict(flags & Strict)
        , m_isConstructor(flags & Constructor)
        , m_name(name)
        , m_nativeFunction(fn)
        , m_argumentCount(argc)
    {
    }
};

class CallNativeFunctionData : public gc {
public:
    NativeFunctionPointer m_fn;
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

    bool isGenerator() const
    {
        return m_isGenerator;
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

    void setAsClassConstructor()
    {
        m_isClassConstructor = true;
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
    bool m_hasSuper : 1;
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
    bool m_isGenerator : 1;
    bool m_isInWithScope : 1;
    bool m_isEvalCodeInFunction : 1;
    bool m_needsVirtualIDOperation : 1;
    bool m_needToLoadThisValue : 1;
    bool m_hasRestElement : 1;
    bool m_shouldReparseArguments : 1;
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
    bool tryCaptureIdentifiersFromChildCodeBlock(LexcialBlockIndex blockIndex, AtomicString name);
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

        enum DeclarationType {
            VarDeclared,
            LexicallyDeclared,
        };

        DeclarationType m_type : 1;

        size_t m_upperIndex;
        size_t m_index;

        IndexedIdentifierInfo()
            : m_isResultSaved(false)
        {
        }
    };


    struct BlockIdentifierInfo {
        bool m_needToAllocateOnStack : 1;
        bool m_isMutable : 1;
        size_t m_indexForIndexedStorage; // TODO reduce variable size into uint16_t.
        AtomicString m_name;
    };

    typedef TightVector<BlockIdentifierInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<BlockIdentifierInfo>> BlockIdentifierInfoVector;

    struct BlockInfo : public gc {
        bool m_canAllocateEnvironmentOnStack : 1;
        bool m_shouldAllocateEnvironment : 1;
        LexcialBlockIndex m_parentBlockIndex;
        LexcialBlockIndex m_blockIndex;
        BlockIdentifierInfoVector m_identifiers;

#ifndef NDEBUG
        ExtendedNodeLOC m_loc;
#endif
        BlockInfo(
#ifndef NDEBUG
            ExtendedNodeLOC loc
#endif
            )
            : m_canAllocateEnvironmentOnStack(false)
            , m_shouldAllocateEnvironment(false)
            , m_parentBlockIndex(LEXCIAL_BLOCK_INDEX_MAX)
            , m_blockIndex(LEXCIAL_BLOCK_INDEX_MAX)
#ifndef NDEBUG
            , m_loc(loc)
#endif
        {
        }


        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;
    };

    typedef TightVector<BlockInfo*, GCUtil::gc_malloc_ignore_off_page_allocator<BlockInfo*>> BlockInfoVector;

    struct IdentifierInfo {
        bool m_needToAllocateOnStack : 1;
        bool m_isMutable : 1;
        bool m_isExplicitlyDeclaredOrParameterName : 1;
        bool m_isVarDeclaration : 1;
        size_t m_indexForIndexedStorage; // TODO reduce variable size into uint16_t.
        AtomicString m_name;
    };

    typedef TightVector<IdentifierInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<IdentifierInfo>> IdentifierInfoVector;

    const IdentifierInfoVector& identifierInfos() const
    {
        return m_identifierInfos;
    }

    const BlockInfoVector& blockInfos() const
    {
        return m_blockInfos;
    }

    BlockInfo* blockInfo(LexcialBlockIndex blockIndex)
    {
        for (size_t i = 0; i < m_blockInfos.size(); i++) {
            if (m_blockInfos[i]->m_blockIndex == blockIndex) {
                return m_blockInfos[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    size_t identifierOnStackCount() const // var
    {
        return m_identifierOnStackCount;
    }

    size_t identifierOnHeapCount() const // var
    {
        return m_identifierOnHeapCount;
    }

    size_t lexicalBlockStackAllocatedIdentifierMaximumDepth() const // let
    {
        return m_lexicalBlockStackAllocatedIdentifierMaximumDepth;
    }

    size_t totalStackAllocatedVariableSize() const
    {
        return identifierOnStackCount() + lexicalBlockStackAllocatedIdentifierMaximumDepth();
    }

    LexcialBlockIndex lexicalBlockIndexFunctionLocatedIn() const
    {
        return m_lexicalBlockIndexFunctionLocatedIn;
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

    IndexedIdentifierInfo indexedIdentifierInfo(const AtomicString& name, LexcialBlockIndex blockIndex)
    {
        size_t upperIndex = 0;
        IndexedIdentifierInfo info;

        InterpretedCodeBlock* blk = this;
        while (blk && (blk->asInterpretedCodeBlock()->isGlobalScopeCodeBlock() || blk->canUseIndexedVariableStorage())) {
            // search block first.
            while (blockIndex != LEXCIAL_BLOCK_INDEX_MAX) {
                InterpretedCodeBlock::BlockInfo* bi = nullptr;
                for (size_t i = 0; i < blk->m_blockInfos.size(); i++) {
                    if (blk->m_blockInfos[i]->m_blockIndex == blockIndex) {
                        bi = blk->m_blockInfos[i];
                        break;
                    }
                }

                for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                    if (bi->m_identifiers[i].m_name == name) {
                        info.m_isResultSaved = true;
                        info.m_isStackAllocated = bi->m_identifiers[i].m_needToAllocateOnStack;
                        info.m_index = bi->m_identifiers[i].m_indexForIndexedStorage;
                        if (info.m_isStackAllocated) {
                            info.m_index += identifierOnStackCount();
                        }
                        info.m_upperIndex = upperIndex;
                        info.m_isMutable = bi->m_identifiers[i].m_isMutable;
                        info.m_type = IndexedIdentifierInfo::DeclarationType::LexicallyDeclared;
                        return info;
                    }
                }

                if (bi->m_shouldAllocateEnvironment) {
                    upperIndex++;
                }

                blockIndex = bi->m_parentBlockIndex;
            }
            if (!blk->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
                size_t index = blk->asInterpretedCodeBlock()->findVarName(name);
                if (index != SIZE_MAX) {
                    info.m_isResultSaved = true;
                    info.m_isStackAllocated = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_needToAllocateOnStack;
                    info.m_upperIndex = upperIndex;
                    info.m_isMutable = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_isMutable;
                    info.m_index = blk->asInterpretedCodeBlock()->m_identifierInfos[index].m_indexForIndexedStorage;
                    info.m_type = IndexedIdentifierInfo::DeclarationType::VarDeclared;
                    return info;
                }
            }

            upperIndex++;
            blockIndex = blk->lexicalBlockIndexFunctionLocatedIn();
            blk = blk->asInterpretedCodeBlock()->parentCodeBlock();
        }

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

    bool hasVarName(const AtomicString& name)
    {
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    bool hasName(LexcialBlockIndex blockIndex, const AtomicString& name)
    {
        if (std::get<0>(findNameWithinBlock(blockIndex, name))) {
            return true;
        }

        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == name) {
                return true;
            }
        }
        return false;
    }

    size_t findVarName(const AtomicString& name)
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
    ASTFunctionScopeContext* scopeContext()
    {
        return m_scopeContext;
    }
#endif

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

    void markHeapAllocatedEnvironmentFromHere(LexcialBlockIndex blockIndex = 0);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    // init global codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTFunctionScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart);
    // init function codeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTFunctionScopeContext* scopeCtx, ExtendedNodeLOC sourceElementStart, InterpretedCodeBlock* parentBlock);

    void computeBlockVariables(LexcialBlockIndex currentBlockIndex, size_t currentStackAllocatedVariableIndex, size_t& maxStackAllocatedVariableDepth);
    void initBlockScopeInformation(ASTFunctionScopeContext* scopeCtx);

    std::tuple<bool, size_t, size_t> findNameWithinBlock(LexcialBlockIndex blockIndex, AtomicString name)
    {
        BlockInfo* b = nullptr;
        size_t blockVectorIndex = SIZE_MAX;
        for (size_t i = 0; i < m_blockInfos.size(); i++) {
            if (m_blockInfos[i]->m_blockIndex == blockIndex) {
                b = m_blockInfos[i];
                blockVectorIndex = i;
                break;
            }
        }

        ASSERT(b != nullptr);
        while (true) {
            for (size_t i = 0; i < b->m_identifiers.size(); i++) {
                if (b->m_identifiers[i].m_name == name) {
                    return std::make_tuple(true, blockVectorIndex, i);
                }
            }

            if (b->m_parentBlockIndex == LEXCIAL_BLOCK_INDEX_MAX) {
                break;
            }

#ifndef NDEBUG
            bool finded = false;
#endif
            for (size_t i = 0; i < m_blockInfos.size(); i++) {
                if (m_blockInfos[i]->m_blockIndex == b->m_parentBlockIndex) {
                    b = m_blockInfos[i];
                    blockVectorIndex = i;
#ifndef NDEBUG
                    finded = true;
#endif
                    break;
                }
            }
            ASSERT(finded);
        }

        return std::make_tuple(false, SIZE_MAX, SIZE_MAX);
    }

    Script* m_script;
    StringView m_paramsSrc; // function parameters elements src
    StringView m_src; // function source elements src
    ExtendedNodeLOC m_sourceElementStart;

    FunctionParametersInfoVector m_parametersInfomation;
    uint16_t m_identifierOnStackCount; // this member variable only count `var`
    uint16_t m_identifierOnHeapCount; // this member variable only count `var`
    uint16_t m_lexicalBlockStackAllocatedIdentifierMaximumDepth; // this member variable only count `let`
    LexcialBlockIndex m_lexicalBlockIndexFunctionLocatedIn;
    IdentifierInfoVector m_identifierInfos;
    BlockInfoVector m_blockInfos;

    InterpretedCodeBlock* m_parentCodeBlock;
    CodeBlockVector m_childBlocks;

#ifndef NDEBUG
    ExtendedNodeLOC m_locStart;
    ExtendedNodeLOC m_locEnd;
    ASTFunctionScopeContext* m_scopeContext;
#endif
};
}

#endif
