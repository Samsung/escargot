/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#include "parser/ast/CommonASTData.h"
#include "runtime/AtomicString.h"

namespace Escargot {

class ByteCodeBlock;
class InterpretedCodeBlock;
class NativeCodeBlock;
class Script;
class Value;
class Object;
struct ASTScopeContext;
struct ByteCodeGenerateContext;

typedef HashMap<AtomicString, StorePositiveNumberAsOddNumber, std::hash<AtomicString>, std::equal_to<AtomicString>,
                GCUtil::gc_malloc_allocator<std::pair<AtomicString const, StorePositiveNumberAsOddNumber>>>
    FunctionContextVarMap;

// length of argv is same with NativeFunctionInfo.m_argumentCount
// only in construct call, newTarget have Object*
typedef Value (*NativeFunctionPointer)(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

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


class CodeBlock : public gc {
public:
    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;

    virtual uint16_t functionLength() const = 0;
    virtual AtomicString functionName() const = 0;
    virtual void setFunctionName(AtomicString name) = 0;

    virtual bool isNativeCodeBlock() const
    {
        return false;
    }

    virtual bool isInterpretedCodeBlock() const
    {
        return false;
    }

    NativeCodeBlock* asNativeCodeBlock()
    {
        ASSERT(isNativeCodeBlock());
        return (NativeCodeBlock*)this;
    }

    InterpretedCodeBlock* asInterpretedCodeBlock()
    {
        ASSERT(isInterpretedCodeBlock());
        return (InterpretedCodeBlock*)this;
    }

    Context* context()
    {
        return m_context;
    }

protected:
    CodeBlock()
        : m_context(nullptr)
    {
    }

    CodeBlock(Context* ctx)
        : m_context(ctx)
    {
    }

    Context* m_context;
};

class NativeCodeBlock : public CodeBlock {
    friend int getValidValueInNativeCodeBlock(void* ptr, GC_mark_custom_result* arr);

public:
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    NativeCodeBlock(Context* ctx, const NativeFunctionInfo& info)
        : CodeBlock(ctx)
        , m_isNativeConstructor(info.m_isConstructor)
        , m_isStrict(info.m_isStrict)
        , m_functionLength(info.m_argumentCount)
        , m_functionName(info.m_name)
        , m_nativeFunction(info.m_nativeFunction)
    {
    }

    virtual bool isNativeCodeBlock() const override
    {
        return true;
    }

    virtual uint16_t functionLength() const override
    {
        return m_functionLength;
    }

    virtual AtomicString functionName() const override
    {
        return m_functionName;
    }

    virtual void setFunctionName(AtomicString name) override
    {
        m_functionName = name;
    }

    bool isNativeConstructor() const
    {
        return m_isNativeConstructor;
    }

    bool isStrict() const
    {
        return m_isStrict;
    }

    NativeFunctionPointer nativeFunction()
    {
        return m_nativeFunction;
    }

private:
    bool m_isNativeConstructor : 1;
    bool m_isStrict : 1;
    uint16_t m_functionLength;
    AtomicString m_functionName;
    NativeFunctionPointer m_nativeFunction;
};

struct InterpretedCodeBlockRareData : public gc {
    FunctionContextVarMap* m_identifierInfoMap;
    TightVector<Optional<ArrayObject*>, GCUtil::gc_malloc_allocator<Optional<ArrayObject*>>> m_taggedTemplateLiteralCache;
    AtomicStringTightVector* m_classPrivateNames;
#ifdef ESCARGOT_DEBUGGER
    size_t m_debuggerLineStart;
#endif /* ESCARGOT_DEBUGGER */

    InterpretedCodeBlockRareData()
        : m_identifierInfoMap(nullptr)
        , m_classPrivateNames(nullptr)
#ifdef ESCARGOT_DEBUGGER
        , m_debuggerLineStart(SIZE_MAX)
#endif /* ESCARGOT_DEBUGGER */
    {
    }

    InterpretedCodeBlockRareData(FunctionContextVarMap* map, AtomicStringTightVector* classPrivateNames)
        : m_identifierInfoMap(map)
        , m_classPrivateNames(classPrivateNames)
#ifdef ESCARGOT_DEBUGGER
        , m_debuggerLineStart(SIZE_MAX)
#endif /* ESCARGOT_DEBUGGER */
    {
    }
};

typedef TightVector<InterpretedCodeBlock*, GCUtil::gc_malloc_allocator<InterpretedCodeBlock*>> InterpretedCodeBlockVector;

class InterpretedCodeBlock : public CodeBlock {
    friend class ScriptParser;
    friend int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr);
#if defined(ENABLE_CODE_CACHE)
    friend class CodeCache;
    friend class CodeCacheWriter;
    friend class CodeCacheReader;
#endif

public:
    struct IndexedIdentifierInfo {
        bool m_isResultSaved : 1;
        bool m_isStackAllocated : 1;
        bool m_isMutable : 1;
        bool m_isUsing : 1;
        bool m_isGlobalLexicalVariable : 1;
        enum DeclarationType ENSURE_ENUM_UNSIGNED {
            VarDeclared,
            LexicallyDeclared,
        };

        DeclarationType m_type : 1;

        LexicalBlockIndex m_blockIndex : 16;

        size_t m_upperIndex;
        size_t m_index;

        IndexedIdentifierInfo()
            : m_isResultSaved(false)
            , m_isStackAllocated(false)
            , m_isMutable(false)
            , m_isUsing(false)
            , m_isGlobalLexicalVariable(false)
            , m_type(VarDeclared)
            , m_blockIndex(LEXICAL_BLOCK_INDEX_MAX)
            , m_upperIndex(SIZE_MAX)
            , m_index(SIZE_MAX)
        {
        }
    };

    struct BlockIdentifierInfo {
        bool m_needToAllocateOnStack : 1;
        bool m_isMutable : 1;
        bool m_isUsing : 1;
        size_t m_indexForIndexedStorage; // TODO reduce variable size into uint16_t.
        AtomicString m_name;
    };

    typedef TightVector<BlockIdentifierInfo, GCUtil::gc_malloc_atomic_allocator<BlockIdentifierInfo>> BlockIdentifierInfoVector;

    class BlockInfo : public gc {
    public:
        BlockInfo(bool isGenericBlockInfo = false,
                  bool canAllocateEnvironmentOnStack = false,
                  bool shouldAllocateEnvironment = false,
                  bool fromCatchClauseNode = false,
                  LexicalBlockIndex parentBlockIndex = LEXICAL_BLOCK_INDEX_MAX,
                  LexicalBlockIndex blockIndex = LEXICAL_BLOCK_INDEX_MAX)
            : m_isGenericBlockInfo(isGenericBlockInfo)
            , m_canAllocateEnvironmentOnStack(canAllocateEnvironmentOnStack)
            , m_shouldAllocateEnvironment(shouldAllocateEnvironment)
            , m_fromCatchClauseNode(fromCatchClauseNode)
            , m_parentBlockIndex(parentBlockIndex)
            , m_blockIndex(blockIndex)
        {
        }

        static size_t genericBlockInfoIndex(bool canAllocateEnvironmentOnStack, bool shouldAllocateEnvironment)
        {
            return (canAllocateEnvironmentOnStack ? 2 : 0) + (shouldAllocateEnvironment ? 1 : 0);
        }

        static BlockInfo* genericBlockInfo(bool canAllocateEnvironmentOnStack, bool shouldAllocateEnvironment)
        {
            static BlockInfo s_genericBlockInfo[4] = {
                BlockInfo(
                    true, false, false, false,
                    LEXICAL_BLOCK_INDEX_MAX, 0),
                BlockInfo(
                    true, false, true, false,
                    LEXICAL_BLOCK_INDEX_MAX, 0),
                BlockInfo(
                    true, true, false, false,
                    LEXICAL_BLOCK_INDEX_MAX, 0),
                BlockInfo(
                    true, true, true, false,
                    LEXICAL_BLOCK_INDEX_MAX, 0)
            };

            size_t idx = genericBlockInfoIndex(canAllocateEnvironmentOnStack, shouldAllocateEnvironment);
            return &s_genericBlockInfo[idx];
        }

        static BlockInfo** genericBlockInfoArray(bool canAllocateEnvironmentOnStack, bool shouldAllocateEnvironment)
        {
            static BlockInfo* s_genericBlockArray[4] = {
                genericBlockInfo(false, false),
                genericBlockInfo(false, true),
                genericBlockInfo(true, false),
                genericBlockInfo(true, true)
            };

            size_t idx = genericBlockInfoIndex(canAllocateEnvironmentOnStack, shouldAllocateEnvironment);
            return &s_genericBlockArray[idx];
        }

        static BlockInfo* create(bool canAllocateEnvironmentOnStack,
                                 bool shouldAllocateEnvironment,
                                 bool fromCatchClauseNode,
                                 LexicalBlockIndex parentBlockIndex,
                                 LexicalBlockIndex blockIndex,
                                 size_t identifierCount)
        {
            if (!fromCatchClauseNode && parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX && blockIndex == 0 && identifierCount == 0) {
                return genericBlockInfo(canAllocateEnvironmentOnStack, shouldAllocateEnvironment);
            }
            return new BlockInfo(
                false, canAllocateEnvironmentOnStack, shouldAllocateEnvironment,
                fromCatchClauseNode, parentBlockIndex, blockIndex);
        }

        bool isGenericBlockInfo() const
        {
            return m_isGenericBlockInfo;
        }

        bool canAllocateEnvironmentOnStack() const
        {
            return m_canAllocateEnvironmentOnStack;
        }

        void setCanAllocateEnvironmentOnStack(bool b)
        {
            ASSERT(!isGenericBlockInfo());
            m_canAllocateEnvironmentOnStack = b;
        }

        bool shouldAllocateEnvironment() const
        {
            return m_shouldAllocateEnvironment;
        }

        void setShouldAllocateEnvironment(bool b)
        {
            ASSERT(!isGenericBlockInfo());
            m_shouldAllocateEnvironment = b;
        }

        bool fromCatchClauseNode() const
        {
            return m_fromCatchClauseNode;
        }

        LexicalBlockIndex parentBlockIndex() const
        {
            return m_parentBlockIndex;
        }

        void setParentBlockIndex(LexicalBlockIndex b)
        {
            ASSERT(!isGenericBlockInfo());
            m_parentBlockIndex = b;
        }

        LexicalBlockIndex blockIndex() const
        {
            return m_blockIndex;
        }

        void setBlockIndex(LexicalBlockIndex b)
        {
            ASSERT(!isGenericBlockInfo());
            m_blockIndex = b;
        }

        BlockIdentifierInfoVector& identifiers()
        {
            return m_identifiers;
        }

        const BlockIdentifierInfoVector& identifiers() const
        {
            return m_identifiers;
        }

    private:
        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;

        bool m_isGenericBlockInfo : 1;
        bool m_canAllocateEnvironmentOnStack : 1;
        bool m_shouldAllocateEnvironment : 1;
        bool m_fromCatchClauseNode : 1;
        LexicalBlockIndex m_parentBlockIndex;
        LexicalBlockIndex m_blockIndex;
        BlockIdentifierInfoVector m_identifiers;
    };

    struct IdentifierInfo {
        bool m_needToAllocateOnStack : 1;
        bool m_isMutable : 1;
        bool m_isParameterName : 1;
        bool m_isExplicitlyDeclaredOrParameterName : 1;
        bool m_isVarDeclaration : 1;
        size_t m_indexForIndexedStorage; // TODO reduce variable size into uint16_t.
        AtomicString m_name;
    };

    typedef TightVector<IdentifierInfo, GCUtil::gc_malloc_atomic_allocator<IdentifierInfo>> IdentifierInfoVector;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    virtual bool isInterpretedCodeBlock() const override
    {
        return true;
    }

    virtual uint16_t functionLength() const override
    {
        return m_functionLength;
    }

    virtual AtomicString functionName() const override
    {
        return m_functionName;
    }

    virtual void setFunctionName(AtomicString name) override
    {
        // set function name is allowed only for dynamically created function except class constructor
        ASSERT(m_hasDynamicSourceCode && !m_isClassConstructor);
        m_functionName = name;
    }

    virtual bool hasRareData() const
    {
        return false;
    }

    virtual InterpretedCodeBlockRareData* rareData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual TightVector<Optional<ArrayObject*>, GCUtil::gc_malloc_allocator<Optional<ArrayObject*>>>& taggedTemplateLiteralCache()
    {
        RELEASE_ASSERT_NOT_REACHED();
        TightVector<Optional<ArrayObject*>, GCUtil::gc_malloc_allocator<Optional<ArrayObject*>>>* tempVector;
        return *tempVector;
    }

    virtual Optional<FunctionContextVarMap*> identifierInfoMap()
    {
        return nullptr;
    }

    virtual Optional<AtomicStringTightVector*> classPrivateNames()
    {
        return nullptr;
    }

    bool isKindOfFunction() const
    {
        return m_isFunctionDeclaration || m_isFunctionExpression || m_isArrowFunctionExpression || m_isClassConstructor || m_isObjectMethod || m_isClassMethod || m_isClassStaticMethod;
    }

    Script* script()
    {
        return m_script;
    }

    const StringView& src()
    {
        return m_src;
    }

    ByteCodeBlock* byteCodeBlock()
    {
        return m_byteCodeBlock;
    }

    void setByteCodeBlock(ByteCodeBlock* block)
    {
#ifndef NDEBUG
        if (!!m_byteCodeBlock) {
            ASSERT(!block);
        }
#endif
        m_byteCodeBlock = block;
    }

    InterpretedCodeBlock* parent()
    {
        return m_parent;
    }

    bool hasChildren() const
    {
        if (!!m_children) {
            ASSERT(m_children->size() > 0);
            return true;
        }
        return false;
    }

    InterpretedCodeBlockVector& children() const
    {
        ASSERT(!!m_children);
        return *m_children;
    }

    void setParent(InterpretedCodeBlock* parentCodeBlock)
    {
        m_parent = parentCodeBlock;
    }

    void setChildren(InterpretedCodeBlockVector* codeBlockVector)
    {
        ASSERT(!m_children);
        m_children = codeBlockVector;
    }

    InterpretedCodeBlock* childBlockAt(size_t idx)
    {
        ASSERT(!!m_children && idx < m_children->size());
        return (*m_children)[idx];
    }

    const AtomicStringTightVector& parameterNames() const
    {
        // return all parameter names vector including targets of patterns and rest element
        return m_parameterNames;
    }

    const IdentifierInfoVector& identifierInfos() const
    {
        return m_identifierInfos;
    }

    BlockInfo** blockInfos() const
    {
        return m_blockInfos;
    }

    size_t blockInfosLength() const
    {
        return m_blockInfosLength;
    }

    ExtendedNodeLOC functionStart()
    {
        return m_functionStart;
    }

#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    ExtendedNodeLOC bodyEndLOC()
    {
        return m_bodyEndLOC;
    }
#endif

    size_t parameterNamesCount() const
    {
        // return the number of all parameter names including targets of patterns and rest element
        return m_parameterNames.size();
    }

    size_t parameterCount() const
    {
        // return the number of parameter elements
        return m_parameterCount;
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

    LexicalBlockIndex functionBodyBlockIndex() const
    {
        return m_functionBodyBlockIndex;
    }

    LexicalBlockIndex lexicalBlockIndexFunctionLocatedIn() const
    {
        return m_lexicalBlockIndexFunctionLocatedIn;
    }

    /*
    - variable access rule -
    type                                         | canUseIndexedVariableStorage  | canAllocateVariablesOnStack | canAllocateEnvironmentOnStack                      | when access variable
    normal codes                                 | true                          | true                        | true when there is no captured variable by closure | use variable information on CodeBlock. if there is no variable information self or ancestor, use {Load, Store}GlobalVariable bytecode
    codes has eval, with...                      | false                         | false                       | false                                              | every variable should use {Load, Store}ByName bytecode
    ancestors of !canUseIndexedVariableStorage   | true                          | false                       | false                                              | same as normal code
    descendants of !canUseIndexedVariableStorage | true                          | true                        | true when there is no captured variable by closure | use variable information on CodeBlock. if there is no variable information self or ancestor, use {Load, Store}ByName bytecode
                           -                     |  -                            |  -                          | && every usingnames resolved on compile time       |     -
    isEvalCode                                   | false                         | false                       | false                                              | every variable should use {Load, Store}ByName bytecode
    */

    void setHasEval()
    {
        m_hasEval = true;
        m_canUseIndexedVariableStorage = false;
    }

    void setAsClassConstructor()
    {
        m_isClassConstructor = true;
    }

    void setAsDerivedClassConstructor()
    {
        m_isDerivedClassConstructor = true;
    }

    void setNeedsVirtualIDOperation()
    {
        ASSERT(isInterpretedCodeBlock());
        m_needsVirtualIDOperation = true;
    }

    bool shouldHaveMappedArguments() const
    {
        return !hasParameterOtherThanIdentifier() && !isStrict();
    }

    bool inWith()
    {
        return m_inWith;
    }

    bool hasEval() const
    {
        return m_hasEval;
    }

    bool hasWith() const
    {
        return m_hasWith;
    }

    bool isStrict() const
    {
        return m_isStrict;
    }

    bool canUseIndexedVariableStorage() const
    {
        return m_canUseIndexedVariableStorage;
    }

    bool canAllocateVariablesOnStack() const
    {
        return m_canAllocateVariablesOnStack;
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

    bool isArrowFunctionExpression() const
    {
        return m_isArrowFunctionExpression;
    }

    bool isOneExpressionOnlyVirtualArrowFunctionExpression() const
    {
        return m_isOneExpressionOnlyVirtualArrowFunctionExpression;
    }

    bool isFunctionBodyOnlyVirtualArrowFunctionExpression() const
    {
        return m_isFunctionBodyOnlyVirtualArrowFunctionExpression;
    }

    bool isClassConstructor() const
    {
        return m_isClassConstructor;
    }

    bool isDerivedClassConstructor() const
    {
        return m_isDerivedClassConstructor;
    }

    bool isObjectMethod() const
    {
        return m_isObjectMethod;
    }

    bool isClassMethod() const
    {
        return m_isClassMethod;
    }

    bool isClassStaticMethod() const
    {
        return m_isClassStaticMethod;
    }

    bool isGenerator() const
    {
        return m_isGenerator;
    }

    bool isAsync() const
    {
        return m_isAsync;
    }

    bool isAsyncOrGenerator() const
    {
        return m_isGenerator || m_isAsync;
    }

    // for TCO
    bool isTailCallTarget(size_t argc) const
    {
        // global scope cannot create a return statement, neither tail call
        ASSERT(!isGlobalScope());

        if (argc > TCO_ARGUMENT_COUNT_LIMIT) {
            return false;
        }

        // skip arrow functions because arrow functions are rarely invoked in tail call
        return m_canAllocateVariablesOnStack && !m_isArrowFunctionExpression && !m_isClassConstructor && !m_isDerivedClassConstructor && !m_isClassMethod && !m_isClassStaticMethod && !m_isGenerator && !m_isAsync && !m_usesArgumentsObject;
    }

    bool usesArgumentsObject() const
    {
        return m_usesArgumentsObject;
    }

    bool hasArrowParameterPlaceHolder() const
    {
        return m_hasArrowParameterPlaceHolder;
    }

    bool hasParameterOtherThanIdentifier() const
    {
        return m_hasParameterOtherThanIdentifier;
    }

    bool allowSuperCall() const
    {
        return m_allowSuperCall;
    }

    bool allowSuperProperty() const
    {
        return m_allowSuperProperty;
    }

    bool allowArguments() const
    {
        return m_allowArguments;
    }

    bool hasDynamicSourceCode() const
    {
        return m_hasDynamicSourceCode;
    }

    void setDynamicSourceCode()
    {
        m_hasDynamicSourceCode = true;
    }

#if defined(ENABLE_TCO)
    bool isTailRecursionDisabled() const
    {
        return m_isTailRecursionDisabled;
    }

    void disableTailRecursion()
    {
        m_isTailRecursionDisabled = true;
    }
#endif

#ifdef ESCARGOT_DEBUGGER
    bool markDebugging() const
    {
        return m_markDebugging;
    }
#endif

    bool isFunctionNameUsedBySelf() const
    {
        return m_isFunctionNameUsedBySelf;
    }

    bool isFunctionNameSaveOnHeap() const
    {
        return m_isFunctionNameSaveOnHeap;
    }

    bool isFunctionNameExplicitlyDeclared() const
    {
        return m_isFunctionNameExplicitlyDeclared;
    }

    bool isEvalCode() const
    {
        return m_isEvalCode;
    }

    bool isEvalCodeInFunction() const
    {
        return m_isEvalCodeInFunction;
    }

    bool needsVirtualIDOperation()
    {
        return m_needsVirtualIDOperation;
    }

    // You can use this function on ScriptParser only
    void computeVariables();
    // You can use this function on ScriptParser only
    void captureArguments(bool needToAllocateOnStack);

    // You can use this function on ScriptParser only
    /* capture ok, block vector index(if not block variable, returns SIZE_MAX) */
    std::pair<bool, size_t> tryCaptureIdentifiersFromChildCodeBlock(LexicalBlockIndex blockIndex, AtomicString name);

    BlockInfo* blockInfo(LexicalBlockIndex blockIndex)
    {
        for (size_t i = 0; i < m_blockInfosLength; i++) {
            if (m_blockInfos[i]->blockIndex() == blockIndex) {
                return m_blockInfos[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    size_t totalStackAllocatedVariableSize() const
    {
        return identifierOnStackCount() + lexicalBlockStackAllocatedIdentifierMaximumDepth();
    }


    bool needsToLoadThisBindingFromEnvironment();

    bool isGlobalCodeBlock() const
    {
        return m_parent == nullptr;
    }

    bool isGlobalScope() const
    {
        // For eval code which is not called inside a function
        // it is handled in the global scope
        return m_isEvalCode ? !m_isEvalCodeInFunction : isGlobalCodeBlock();
    }

    bool hasAncestorUsesNonIndexedVariableStorage()
    {
        auto ptr = m_parent;

        while (ptr) {
            if (!ptr->canUseIndexedVariableStorage()) {
                return true;
            }
            ptr = ptr->parent();
        }

        return false;
    }

    IndexedIdentifierInfo indexedIdentifierInfo(const AtomicString& name, ByteCodeGenerateContext* context);

    size_t findVarName(const AtomicString& name);

    bool hasName(LexicalBlockIndex blockIndex, const AtomicString& name)
    {
        if (std::get<0>(findNameWithinBlock(blockIndex, name))) {
            return true;
        }

        if (blockIndex < m_functionBodyBlockIndex) {
            return isParameterName(name);
        }

        return findVarName(name) != SIZE_MAX;
    }

    bool hasParameterName(const AtomicString& name)
    {
        for (size_t i = 0; i < parameterNamesCount(); i++) {
            if (m_parameterNames[i] == name) {
                return true;
            }
        }
        return false;
    }

    bool isParameterName(const AtomicString& name)
    {
        size_t r = findVarName(name);
        if (r != SIZE_MAX) {
            return m_identifierInfos[r].m_isParameterName;
        }

        return false;
    }

    void markHeapAllocatedEnvironmentFromHere(LexicalBlockIndex blockIndex = 0, InterpretedCodeBlock* to = nullptr);

    void setConstructedObjectPropertyCount(size_t s)
    {
        // overflow is ok.
        m_constructedObjectPropertyCount = s;
    }

    size_t constructedObjectPropertyCount() const
    {
        return m_constructedObjectPropertyCount;
    }

#ifndef NDEBUG
    ASTScopeContext* scopeContext()
    {
        ASSERT(!!m_scopeContext);
        return m_scopeContext;
    }
#endif

protected:
    Script* m_script;
    StringView m_src; // function source including parameters
    ByteCodeBlock* m_byteCodeBlock;

    InterpretedCodeBlock* m_parent;
    InterpretedCodeBlockVector* m_children;

    // all parameter names including targets of patterns and rest element
    AtomicStringTightVector m_parameterNames;
    IdentifierInfoVector m_identifierInfos;
    BlockInfo** m_blockInfos;
    static constexpr size_t maxBlockInfosLength = ((1 << 24) - 1);
    uint32_t m_blockInfosLength : 24;
    uint16_t m_constructedObjectPropertyCount : 8;

    AtomicString m_functionName;

    ExtendedNodeLOC m_functionStart; // point to the start position
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
    ExtendedNodeLOC m_bodyEndLOC;
#endif

    uint16_t m_functionLength : 16;
    uint16_t m_parameterCount : 16; // number of parameter elements

    uint16_t m_identifierOnStackCount : 16; // this member variable only count `var`
    uint16_t m_identifierOnHeapCount : 16; // this member variable only count `var`
    uint16_t m_lexicalBlockStackAllocatedIdentifierMaximumDepth : 16; // this member variable only count `let`

    LexicalBlockIndex m_functionBodyBlockIndex : 16;
    LexicalBlockIndex m_lexicalBlockIndexFunctionLocatedIn : 16;

    bool m_isFunctionNameUsedBySelf : 1;
    bool m_isFunctionNameSaveOnHeap : 1;
    bool m_isFunctionNameExplicitlyDeclared : 1;
    bool m_canUseIndexedVariableStorage : 1;
    bool m_canAllocateVariablesOnStack : 1;
    bool m_canAllocateEnvironmentOnStack : 1;
    bool m_hasDescendantUsesNonIndexedVariableStorage : 1;
    bool m_hasEval : 1;
    bool m_hasWith : 1;
    bool m_isStrict : 1;
    bool m_inWith : 1;
    bool m_isEvalCode : 1;
    bool m_isEvalCodeInFunction : 1;
    bool m_usesArgumentsObject : 1;
    bool m_isFunctionExpression : 1;
    bool m_isFunctionDeclaration : 1;
    bool m_isArrowFunctionExpression : 1;
    // one expression only arrow function only contains one expression in body(no param, no placeholder, no brace)
    bool m_isOneExpressionOnlyVirtualArrowFunctionExpression : 1;
    // one function body only arrow function only contains one code block(no param, no placeholder, starts with brace)
    bool m_isFunctionBodyOnlyVirtualArrowFunctionExpression : 1;
    bool m_isClassConstructor : 1;
    bool m_isDerivedClassConstructor : 1;
    bool m_isObjectMethod : 1;
    bool m_isClassMethod : 1;
    bool m_isClassStaticMethod : 1;
    bool m_isGenerator : 1;
    bool m_isAsync : 1;
    bool m_needsVirtualIDOperation : 1;
    bool m_hasArrowParameterPlaceHolder : 1;
    bool m_hasParameterOtherThanIdentifier : 1;
    bool m_allowSuperCall : 1;
    bool m_allowSuperProperty : 1;
    bool m_allowArguments : 1;
    // represent if its source code is created dynamically by createDynamicFunctionScript
    bool m_hasDynamicSourceCode : 1;
#if defined(ENABLE_TCO)
    bool m_isTailRecursionDisabled : 1;
#endif
#ifdef ESCARGOT_DEBUGGER
    // mark that this InterpretedCodeBlock should generate debugging bytecode (breakpoint)
    bool m_markDebugging : 1;
#endif

#ifndef NDEBUG
    ASTScopeContext* m_scopeContext;
#endif

    // init global CodeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction);
    // init function CodeBlock
    InterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentBlock, bool isEvalCode, bool isEvalCodeInFunction);
    // empty CodeBlock (for CodeCache loading)
    InterpretedCodeBlock(Context* ctx, Script* script);

    static InterpretedCodeBlock* createInterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction);
    static InterpretedCodeBlock* createInterpretedCodeBlock(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentBlock, bool isEvalCode, bool isEvalCodeInFunction);
    static InterpretedCodeBlock* createInterpretedCodeBlock(Context* ctx, Script* script, bool needRareData = false);

    void recordGlobalParsingInfo(ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction);
    void recordFunctionParsingInfo(ASTScopeContext* scopeCtxm, bool isEvalCode, bool isEvalCodeInFunction);

    void computeBlockVariables(LexicalBlockIndex currentBlockIndex, size_t currentStackAllocatedVariableIndex, size_t& maxStackAllocatedVariableDepth);
    void initBlockScopeInformation(ASTScopeContext* scopeCtx);

    // You can use this function on ScriptParser only
    std::tuple<bool, size_t, size_t> findNameWithinBlock(LexicalBlockIndex blockIndex, AtomicString name)
    {
        BlockInfo* b = nullptr;
        size_t blockVectorIndex = SIZE_MAX;
        size_t blockInfoSize = m_blockInfosLength;
        for (size_t i = 0; i < blockInfoSize; i++) {
            if (m_blockInfos[i]->blockIndex() == blockIndex) {
                b = m_blockInfos[i];
                blockVectorIndex = i;
                break;
            }
        }

        ASSERT(b != nullptr);
        while (true) {
            auto& v = b->identifiers();
            size_t idSize = v.size();
            for (size_t i = 0; i < idSize; i++) {
                if (v[i].m_name == name) {
                    return std::make_tuple(true, blockVectorIndex, i);
                }
            }

            if (b->parentBlockIndex() == LEXICAL_BLOCK_INDEX_MAX) {
                break;
            }

#ifndef NDEBUG
            bool finded = false;
#endif
            for (size_t i = 0; i < blockInfoSize; i++) {
                if (m_blockInfos[i]->blockIndex() == b->parentBlockIndex()) {
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
};

class InterpretedCodeBlockWithRareData : public InterpretedCodeBlock {
    friend class InterpretedCodeBlock;
    friend int getValidValueInInterpretedCodeBlockWithRareData(void* ptr, GC_mark_custom_result* arr);

public:
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    virtual bool hasRareData() const override
    {
        return true;
    }

    virtual InterpretedCodeBlockRareData* rareData() const override
    {
        ASSERT(!!m_rareData);
        return m_rareData;
    }

    virtual TightVector<Optional<ArrayObject*>, GCUtil::gc_malloc_allocator<Optional<ArrayObject*>>>& taggedTemplateLiteralCache() override
    {
        ASSERT(!!m_rareData);
        return m_rareData->m_taggedTemplateLiteralCache;
    }

    virtual Optional<FunctionContextVarMap*> identifierInfoMap() override
    {
        ASSERT(!!m_rareData);
        return m_rareData->m_identifierInfoMap;
    }

    virtual Optional<AtomicStringTightVector*> classPrivateNames() override
    {
        ASSERT(!!m_rareData);
        return m_rareData->m_classPrivateNames;
    }

private:
    InterpretedCodeBlockWithRareData(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, bool isEvalCode, bool isEvalCodeInFunction);

    InterpretedCodeBlockWithRareData(Context* ctx, Script* script, StringView src, ASTScopeContext* scopeCtx, InterpretedCodeBlock* parentBlock, bool isEvalCode, bool isEvalCodeInFunction);

    InterpretedCodeBlockWithRareData(Context* ctx, Script* script)
        : InterpretedCodeBlock(ctx, script)
        , m_rareData(new InterpretedCodeBlockRareData())
    {
    }

    InterpretedCodeBlockRareData* m_rareData;
};
} // namespace Escargot

#endif
