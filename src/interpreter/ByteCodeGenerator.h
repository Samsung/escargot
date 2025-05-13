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

#ifndef __EscargotByteCodeGenerator__
#define __EscargotByteCodeGenerator__

#include "parser/ast/CommonASTData.h"
#include "debugger/Debugger.h"
#include "runtime/AtomicString.h"

namespace Escargot {

extern const uint8_t byteCodeLengths[];

class Context;
class CodeBlock;
class ByteCodeBlock;
class Node;
class InterpretedCodeBlock;

struct ClassContextInformation {
    ClassContextInformation()
        : m_constructorIndex(SIZE_MAX)
        , m_prototypeIndex(SIZE_MAX)
        , m_superIndex(SIZE_MAX)
        , m_name()
        , m_src(String::emptyString())
    {
    }

    size_t m_constructorIndex;
    size_t m_prototypeIndex;
    size_t m_superIndex;
    Optional<AtomicString> m_name;
    String* m_src;
};

#ifdef ESCARGOT_DEBUGGER
struct ByteCodeBreakpointContext {
    size_t m_lastBreakpointLineOffset; // cache breakpoint's calculated line offset
    size_t m_lastBreakpointIndexOffset; // cache breakpoint's calculated index offset
    size_t m_originSourceLineOffset; // original source's start line offset
    Debugger::BreakpointLocationsInfo* m_breakpointLocations;
    bool m_parsingEnabled;
    bool m_sharedWithDebugger;

    ByteCodeBreakpointContext(Debugger* debugger, InterpretedCodeBlock* codeBlock, bool addBreakpointLocationsInfoToDebugger = true);
    ~ByteCodeBreakpointContext();

    MAKE_STACK_ALLOCATED();
};
#endif /* ESCARGOT_DEBUGGER */

struct ByteCodeGenerateContext {
    ByteCodeGenerateContext(InterpretedCodeBlock* codeBlock, ByteCodeBlock* byteCodeBlock, bool isGlobalScope, bool isEvalCode,
                            bool isWithScope, void* numeralLiteralData);

    ByteCodeGenerateContext(const ByteCodeGenerateContext& contextBefore)
        : m_baseRegisterCount(contextBefore.m_baseRegisterCount)
        , m_codeBlock(contextBefore.m_codeBlock)
        , m_byteCodeBlock(contextBefore.m_byteCodeBlock)
        , m_locData(contextBefore.m_locData)
        , m_isGlobalScope(contextBefore.m_isGlobalScope)
        , m_isEvalCode(contextBefore.m_isEvalCode)
        , m_isOutermostContext(false)
        , m_isWithScope(contextBefore.m_isWithScope)
        , m_isFunctionDeclarationBindingInitialization(contextBefore.m_isFunctionDeclarationBindingInitialization)
        , m_isVarDeclaredBindingInitialization(contextBefore.m_isVarDeclaredBindingInitialization)
        , m_isLexicallyDeclaredBindingInitialization(contextBefore.m_isLexicallyDeclaredBindingInitialization)
        , m_canSkipCopyToRegister(contextBefore.m_canSkipCopyToRegister)
        , m_keepNumberalLiteralsInRegisterFile(contextBefore.m_keepNumberalLiteralsInRegisterFile)
        , m_inCallingExpressionScope(contextBefore.m_inCallingExpressionScope)
        , m_inObjectDestruction(contextBefore.m_inObjectDestruction)
        , m_inParameterInitialization(contextBefore.m_inParameterInitialization)
        , m_isHeadOfMemberExpression(false)
        , m_forInOfVarBinding(contextBefore.m_forInOfVarBinding)
        , m_isLeftBindingAffectedByRightExpression(contextBefore.m_isLeftBindingAffectedByRightExpression)
#if defined(ENABLE_TCO)
        , m_tcoDisabled(contextBefore.m_tcoDisabled)
#endif
        , m_needsExtendedExecutionState(contextBefore.m_needsExtendedExecutionState)
        , m_registerStack(contextBefore.m_registerStack)
        , m_lexicallyDeclaredNames(contextBefore.m_lexicallyDeclaredNames)
        , m_positionToContinue(contextBefore.m_positionToContinue)
        , m_recursiveStatementStack(contextBefore.m_recursiveStatementStack)
        , m_lexicalBlockIndex(contextBefore.m_lexicalBlockIndex)
        , m_classInfo(contextBefore.m_classInfo)
        , m_numeralLiteralData(contextBefore.m_numeralLiteralData) // should be NumeralLiteralVector
#if defined(ENABLE_TCO)
        , m_returnRegister(contextBefore.m_returnRegister)
#endif
#ifdef ESCARGOT_DEBUGGER
        , m_breakpointContext(contextBefore.m_breakpointContext)
#endif /* ESCARGOT_DEBUGGER */
    {
        checkStack();
    }

    ~ByteCodeGenerateContext()
    {
    }

    void propagateInformationTo(ByteCodeGenerateContext& ctx)
    {
        ctx.m_breakStatementPositions.insert(ctx.m_breakStatementPositions.end(), m_breakStatementPositions.begin(), m_breakStatementPositions.end());
        ctx.m_continueStatementPositions.insert(ctx.m_continueStatementPositions.end(), m_continueStatementPositions.begin(), m_continueStatementPositions.end());
        ctx.m_labelledBreakStatmentPositions.insert(ctx.m_labelledBreakStatmentPositions.end(), m_labelledBreakStatmentPositions.begin(), m_labelledBreakStatmentPositions.end());
        ctx.m_labelledContinueStatmentPositions.insert(ctx.m_labelledContinueStatmentPositions.end(), m_labelledContinueStatmentPositions.begin(), m_labelledContinueStatmentPositions.end());
        ctx.m_complexCaseStatementPositions.insert(m_complexCaseStatementPositions.begin(), m_complexCaseStatementPositions.end());
        ctx.m_positionToContinue = m_positionToContinue;
        ctx.m_lexicalBlockIndex = m_lexicalBlockIndex;
        ctx.m_classInfo = m_classInfo;
        ctx.m_needsExtendedExecutionState = ctx.m_needsExtendedExecutionState | m_needsExtendedExecutionState;

        m_breakStatementPositions.clear();
        m_continueStatementPositions.clear();
        m_labelledBreakStatmentPositions.clear();
        m_labelledContinueStatmentPositions.clear();
        m_complexCaseStatementPositions.clear();

        ctx.m_recursiveStatementStack = std::move(m_recursiveStatementStack);
    }

    void pushBreakPositions(size_t pos)
    {
        m_breakStatementPositions.push_back(pos);
    }

    void pushLabelledBreakPositions(size_t pos, String* lbl)
    {
        m_labelledBreakStatmentPositions.push_back(std::make_pair(lbl, pos));
    }

    void pushContinuePositions(size_t pos)
    {
        m_continueStatementPositions.push_back(pos);
    }

    void pushLabelledContinuePositions(size_t pos, String* lbl)
    {
        m_labelledContinueStatmentPositions.push_back(std::make_pair(lbl, pos));
    }

    void addOptionalChainingJumpPosition(size_t pos)
    {
        ASSERT(m_optionalChainingJumpPositionLists.size());
        m_optionalChainingJumpPositionLists.back().push_back(pos);
    }

    void pushOptionalChainingJumpPositionList()
    {
        m_optionalChainingJumpPositionLists.push_back(std::vector<size_t>());
    }

    void popOptionalChainingJumpPositionList()
    {
        ASSERT(m_optionalChainingJumpPositionLists.size());
        m_optionalChainingJumpPositionLists.pop_back();
    }

    void linkOptionalChainingJumpPosition(ByteCodeBlock* cb, size_t jumpToPosition);

    void registerJumpPositionsToComplexCase(size_t frontlimit)
    {
        ASSERT(tryCatchWithBlockStatementCount());
        for (unsigned i = 0; i < m_breakStatementPositions.size(); i++) {
            if (m_breakStatementPositions[i] > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_breakStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                m_complexCaseStatementPositions.insert(std::make_pair(m_breakStatementPositions[i], tryCatchWithBlockStatementCount()));
            }
        }

        for (unsigned i = 0; i < m_continueStatementPositions.size(); i++) {
            if (m_continueStatementPositions[i] > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_continueStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                m_complexCaseStatementPositions.insert(std::make_pair(m_continueStatementPositions[i], tryCatchWithBlockStatementCount()));
            }
        }

        for (unsigned i = 0; i < m_labelledBreakStatmentPositions.size(); i++) {
            if (m_labelledBreakStatmentPositions[i].second > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_labelledBreakStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                m_complexCaseStatementPositions.insert(std::make_pair(m_labelledBreakStatmentPositions[i].second, tryCatchWithBlockStatementCount()));
            }
        }

        for (unsigned i = 0; i < m_labelledContinueStatmentPositions.size(); i++) {
            if (m_labelledContinueStatmentPositions[i].second > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_labelledContinueStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                m_complexCaseStatementPositions.insert(std::make_pair(m_labelledContinueStatmentPositions[i].second, tryCatchWithBlockStatementCount()));
            }
        }
    }

    size_t getLastRegisterIndex()
    {
        return m_registerStack->back();
    }

    size_t getLastRegisterIndex(size_t idx)
    {
        return *(m_registerStack->end() - idx - 1);
    }

    size_t getRegister()
    {
        if (UNLIKELY(m_baseRegisterCount >= REGULAR_REGISTER_LIMIT)) {
            throw "register limit exceed while generate byte code";
        }
        checkStack();
        m_registerStack->push_back(m_baseRegisterCount);
        m_baseRegisterCount++;
        return m_registerStack->back();
    }

    void pushRegister(ByteCodeRegisterIndex index)
    {
        if (index == m_baseRegisterCount) {
            m_baseRegisterCount++;
        }
        m_registerStack->push_back(index);
    }

    void giveUpRegister()
    {
        ASSERT(m_registerStack->size());
        if (m_registerStack->back() == (m_baseRegisterCount - 1)) {
            m_baseRegisterCount--;
        }
        m_registerStack->pop_back();
    }

    void consumeBreakPositions(ByteCodeBlock* cb, size_t position, int outerLimitCount);
    void consumeLabelledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount);
    void consumeContinuePositions(ByteCodeBlock* cb, size_t position, int outerLimitCount);
    void consumeLabelledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount);
    void morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos, size_t outerLimitCount = SIZE_MAX);

    bool shouldCareScriptExecutionResult() const
    {
        return (m_isGlobalScope || m_isEvalCode) && m_baseRegisterCount == 0;
    }

    void addLexicallyDeclaredNames(AtomicString name)
    {
        bool find = false;
        auto iter = m_lexicallyDeclaredNames->begin();
        while (iter != m_lexicallyDeclaredNames->end()) {
            if (iter->first == m_lexicalBlockIndex && iter->second == name) {
                find = true;
                break;
            }
            iter++;
        }

        if (!find) {
            m_lexicallyDeclaredNames->push_back(std::make_pair(m_lexicalBlockIndex, name));
        }
    }

    void addInitializedParameterNames(AtomicString name)
    {
        bool find = false;
        for (size_t i = 0; i < m_initializedParameterNames.size(); i++) {
            if (m_initializedParameterNames[i] == name) {
                find = true;
                break;
            }
        }

        if (!find) {
            m_initializedParameterNames.push_back(name);
        }
    }

    int tryCatchWithBlockStatementCount()
    {
        return m_recursiveStatementStack.size();
    }

#if defined(ENABLE_TCO)
    void setReturnRegister(size_t dstRegister)
    {
        ASSERT(m_returnRegister != dstRegister);
        m_returnRegister = dstRegister;
    }
#endif

#ifndef NDEBUG
    void checkAllDataUsed()
    {
        // below member vectors should be all consumed during bytecode generation
        // this check cannot be embedded in ~ByteCodeGenerateContext() because
        // an exception could be ocurred during bytecode generation
        // so, this check function is invoked after bytecode generation completed normally
        ASSERT(m_breakStatementPositions.empty());
        ASSERT(m_continueStatementPositions.empty());
        ASSERT(m_labelledBreakStatmentPositions.empty());
        ASSERT(m_labelledContinueStatmentPositions.empty());
        ASSERT(m_optionalChainingJumpPositionLists.empty());
        ASSERT(m_recursiveStatementStack.empty());
    }
#endif

    ALWAYS_INLINE void checkStack()
    {
#ifdef STACK_GROWS_DOWN
        if (UNLIKELY(ThreadLocal::stackLimit() > (size_t)currentStackPointer())) {
#else
        if (UNLIKELY(ThreadLocal::stackLimit() < (size_t)currentStackPointer())) {
#endif
            throw "native stack limit exceed while generate byte code";
        }
    }

#ifdef ESCARGOT_DEBUGGER
    void calculateBreakpointLocation(size_t index, ExtendedNodeLOC sourceElementStart);
    void insertBreakpoint(size_t index, Node* node);
    void insertBreakpointAt(size_t line, Node* node);
#endif /* ESCARGOT_DEBUGGER */

    // NOTE this is counter! not index!!!!!!
    size_t m_baseRegisterCount;

    InterpretedCodeBlock* m_codeBlock;
    ByteCodeBlock* m_byteCodeBlock;
    std::vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>>* m_locData; // used only for calculating location info

    bool m_isGlobalScope : 1;
    bool m_isEvalCode : 1;
    bool m_isOutermostContext : 1;
    bool m_isWithScope : 1;
    bool m_isFunctionDeclarationBindingInitialization : 1;
    bool m_isVarDeclaredBindingInitialization : 1;
    bool m_isLexicallyDeclaredBindingInitialization : 1;
    bool m_canSkipCopyToRegister : 1;
    bool m_keepNumberalLiteralsInRegisterFile : 1;
    bool m_inCallingExpressionScope : 1;
    bool m_inObjectDestruction : 1;
    bool m_inParameterInitialization : 1;
    bool m_isHeadOfMemberExpression : 1;
    bool m_forInOfVarBinding : 1;
    bool m_isLeftBindingAffectedByRightExpression : 1; // x = delete x; or x = eval("var x"), 1;
#if defined(ENABLE_TCO)
    bool m_tcoDisabled : 1; // disable tail call optimizaiton (TCO) for some conditions
#endif
    bool m_needsExtendedExecutionState : 1;

    std::shared_ptr<std::vector<ByteCodeRegisterIndex>> m_registerStack;
    std::shared_ptr<std::vector<std::pair<size_t, AtomicString>>> m_lexicallyDeclaredNames;
    std::vector<AtomicString> m_initializedParameterNames;
    std::vector<size_t> m_breakStatementPositions;
    std::vector<size_t> m_continueStatementPositions;
    std::vector<std::pair<String*, size_t>> m_labelledBreakStatmentPositions;
    std::vector<std::pair<String*, size_t>> m_labelledContinueStatmentPositions;

    // For recording optional chaing jump positions when target is undefined or null
    std::vector<std::vector<size_t>> m_optionalChainingJumpPositionLists;

    // For Label Statement
    size_t m_positionToContinue;
    // code position, special Statement count
    enum RecursiveStatementKind : size_t {
        Try,
        Catch,
        Finally,
        OpenEnv,
        Block
    };
    std::vector<std::pair<RecursiveStatementKind, size_t>> m_recursiveStatementStack;
    size_t m_lexicalBlockIndex;
    ClassContextInformation m_classInfo;
    std::map<size_t, size_t> m_complexCaseStatementPositions;
    void* m_numeralLiteralData; // should be NumeralLiteralVector

#if defined(ENABLE_TCO)
    size_t m_returnRegister; // for tail call optimizaiton (TCO)
#endif

#ifdef ESCARGOT_DEBUGGER
    ByteCodeBreakpointContext* m_breakpointContext;
#endif /* ESCARGOT_DEBUGGER */
};

class ByteCodeGenerator {
public:
    static ByteCodeBlock* generateByteCode(Context* context, InterpretedCodeBlock* codeBlock, Node* ast, bool inWithFromRuntime = false, bool cacheByteCode = false);
    static void collectByteCodeLOCData(Context* context, InterpretedCodeBlock* codeBlock, std::vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>>* locData);
    static void relocateByteCode(ByteCodeBlock* block);

#ifndef NDEBUG
    static void printByteCode(Context* context, ByteCodeBlock* block);
#endif
};
} // namespace Escargot

#endif
