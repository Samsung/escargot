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

#ifndef __EscargotByteCodeGenerator__
#define __EscargotByteCodeGenerator__

#include "parser/CodeBlock.h"
#include "runtime/String.h"
#include "runtime/Value.h"

namespace Escargot {

class Context;
class CodeBlock;
class ByteCodeBlock;
class Node;

struct ClassContextInformation {
    ClassContextInformation()
        : m_constructorIndex(SIZE_MAX)
        , m_prototypeIndex(SIZE_MAX)
        , m_superIndex(SIZE_MAX)
        , m_name(AtomicString())
        , m_src(String::emptyString)
    {
    }

    size_t m_constructorIndex;
    size_t m_prototypeIndex;
    size_t m_superIndex;
    AtomicString m_name;
    String* m_src;
};

struct ParserContextInformation {
    ParserContextInformation(bool isEvalCode = false, bool isForGlobalScope = false, bool isStrict = false, bool isWithScope = false)
        : m_isEvalCode(isEvalCode)
        , m_isForGlobalScope(isForGlobalScope)
        , m_isWithScope(isWithScope)
    {
        // NOTE: ES5 10.4.2.1
        if (isEvalCode && isStrict)
            m_isForGlobalScope = false;
    }

    bool m_isEvalCode : 1;
    bool m_isForGlobalScope : 1;
    bool m_isWithScope : 1;
};

struct ByteCodeGenerateError {
    size_t m_index;
    std::string m_message;
};

struct ByteCodeGenerateContext {
    ByteCodeGenerateContext(CodeBlock* codeBlock, ByteCodeBlock* byteCodeBlock, ParserContextInformation& parserContextInformation, NumeralLiteralVector* numeralLiteralData)
        : m_baseRegisterCount(0)
        , m_codeBlock(codeBlock)
        , m_byteCodeBlock(byteCodeBlock)
        , m_isGlobalScope(parserContextInformation.m_isForGlobalScope)
        , m_isEvalCode(parserContextInformation.m_isEvalCode)
        , m_isOutermostContext(true)
        , m_isWithScope(parserContextInformation.m_isWithScope)
        , m_isFunctionDeclarationBindingInitialization(false)
        , m_isVarDeclaredBindingInitialization(false)
        , m_isLexicallyDeclaredBindingInitialization(false)
        , m_canSkipCopyToRegister(true)
        , m_keepNumberalLiteralsInRegisterFile(numeralLiteralData)
        , m_shouldGenerateLOCData(false)
        , m_forInOfVarBinding(false)
        , m_isLeftBindingAffectedByRightExpression(false)
        , m_registerStack(new std::vector<ByteCodeRegisterIndex>())
        , m_lexicallyDeclaredNames(new std::vector<std::pair<size_t, AtomicString>>())
        , m_positionToContinue(0)
        , m_complexJumpBreakIgnoreCount(0)
        , m_complexJumpContinueIgnoreCount(0)
        , m_complexJumpLabeledBreakIgnoreCount(0)
        , m_complexJumpLabeledContinueIgnoreCount(0)
        , m_lexicalBlockIndex(0)
        , m_maxPauseStatementExtraDataLength(0)
        , m_numeralLiteralData(numeralLiteralData)
    {
        m_inCallingExpressionScope = false;
        m_isHeadOfMemberExpression = false;
        m_shouldGenerateByteCodeInstantly = true;
        m_classInfo = ClassContextInformation();
    }

    ByteCodeGenerateContext(const ByteCodeGenerateContext& contextBefore)
        : m_baseRegisterCount(contextBefore.m_baseRegisterCount)
        , m_codeBlock(contextBefore.m_codeBlock)
        , m_byteCodeBlock(contextBefore.m_byteCodeBlock)
        , m_isGlobalScope(contextBefore.m_isGlobalScope)
        , m_isEvalCode(contextBefore.m_isEvalCode)
        , m_isOutermostContext(false)
        , m_isWithScope(contextBefore.m_isWithScope)
        , m_isFunctionDeclarationBindingInitialization(contextBefore.m_isFunctionDeclarationBindingInitialization)
        , m_isVarDeclaredBindingInitialization(contextBefore.m_isVarDeclaredBindingInitialization)
        , m_isLexicallyDeclaredBindingInitialization(contextBefore.m_isLexicallyDeclaredBindingInitialization)
        , m_canSkipCopyToRegister(contextBefore.m_canSkipCopyToRegister)
        , m_keepNumberalLiteralsInRegisterFile(contextBefore.m_keepNumberalLiteralsInRegisterFile)
        , m_shouldGenerateByteCodeInstantly(contextBefore.m_shouldGenerateByteCodeInstantly)
        , m_inCallingExpressionScope(contextBefore.m_inCallingExpressionScope)
        , m_shouldGenerateLOCData(contextBefore.m_shouldGenerateLOCData)
        , m_forInOfVarBinding(contextBefore.m_forInOfVarBinding)
        , m_isLeftBindingAffectedByRightExpression(contextBefore.m_isLeftBindingAffectedByRightExpression)
        , m_registerStack(contextBefore.m_registerStack)
        , m_lexicallyDeclaredNames(contextBefore.m_lexicallyDeclaredNames)
        , m_positionToContinue(contextBefore.m_positionToContinue)
        , m_recursiveStatementStack(contextBefore.m_recursiveStatementStack)
        , m_complexJumpBreakIgnoreCount(contextBefore.m_complexJumpBreakIgnoreCount)
        , m_complexJumpContinueIgnoreCount(contextBefore.m_complexJumpContinueIgnoreCount)
        , m_complexJumpLabeledBreakIgnoreCount(contextBefore.m_complexJumpLabeledBreakIgnoreCount)
        , m_complexJumpLabeledContinueIgnoreCount(contextBefore.m_complexJumpLabeledContinueIgnoreCount)
        , m_lexicalBlockIndex(contextBefore.m_lexicalBlockIndex)
        , m_classInfo(contextBefore.m_classInfo)
        , m_maxPauseStatementExtraDataLength(contextBefore.m_maxPauseStatementExtraDataLength)
        , m_numeralLiteralData(contextBefore.m_numeralLiteralData)
    {
        m_isHeadOfMemberExpression = false;
    }


    ~ByteCodeGenerateContext()
    {
    }

    void propagateInformationTo(ByteCodeGenerateContext& ctx)
    {
        ctx.m_breakStatementPositions.insert(ctx.m_breakStatementPositions.end(), m_breakStatementPositions.begin(), m_breakStatementPositions.end());
        ctx.m_continueStatementPositions.insert(ctx.m_continueStatementPositions.end(), m_continueStatementPositions.begin(), m_continueStatementPositions.end());
        ctx.m_labeledBreakStatmentPositions.insert(ctx.m_labeledBreakStatmentPositions.end(), m_labeledBreakStatmentPositions.begin(), m_labeledBreakStatmentPositions.end());
        ctx.m_labeledContinueStatmentPositions.insert(ctx.m_labeledContinueStatmentPositions.end(), m_labeledContinueStatmentPositions.begin(), m_labeledContinueStatmentPositions.end());
        ctx.m_complexCaseStatementPositions.insert(m_complexCaseStatementPositions.begin(), m_complexCaseStatementPositions.end());
        ctx.m_getObjectCodePositions.insert(ctx.m_getObjectCodePositions.end(), m_getObjectCodePositions.begin(), m_getObjectCodePositions.end());
        ctx.m_positionToContinue = m_positionToContinue;
        ctx.m_lexicalBlockIndex = m_lexicalBlockIndex;
        ctx.m_classInfo = m_classInfo;
        ctx.m_maxPauseStatementExtraDataLength = std::max(m_maxPauseStatementExtraDataLength, ctx.m_maxPauseStatementExtraDataLength);

        m_breakStatementPositions.clear();
        m_continueStatementPositions.clear();
        m_labeledBreakStatmentPositions.clear();
        m_labeledContinueStatmentPositions.clear();
        m_complexCaseStatementPositions.clear();

        ctx.m_recursiveStatementStack = std::move(m_recursiveStatementStack);
    }

    void pushBreakPositions(size_t pos)
    {
        m_breakStatementPositions.push_back(pos);
    }

    void pushLabeledBreakPositions(size_t pos, String* lbl)
    {
        m_labeledBreakStatmentPositions.push_back(std::make_pair(lbl, pos));
    }

    void pushContinuePositions(size_t pos)
    {
        m_continueStatementPositions.push_back(pos);
    }

    void pushLabeledContinuePositions(size_t pos, String* lbl)
    {
        m_labeledContinueStatmentPositions.push_back(std::make_pair(lbl, pos));
    }

    void registerJumpPositionsToComplexCase(size_t frontlimit)
    {
        ASSERT(tryCatchWithBlockStatementCount());
        for (unsigned i = 0; i < m_breakStatementPositions.size(); i++) {
            if (m_breakStatementPositions[i] > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_breakStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                if (tryCatchWithBlockStatementCount() - m_complexJumpBreakIgnoreCount > 0) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_breakStatementPositions[i], tryCatchWithBlockStatementCount() - m_complexJumpBreakIgnoreCount));
                }
            }
        }

        for (unsigned i = 0; i < m_continueStatementPositions.size(); i++) {
            if (m_continueStatementPositions[i] > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_continueStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                if (tryCatchWithBlockStatementCount() - m_complexJumpContinueIgnoreCount > 0) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_continueStatementPositions[i], tryCatchWithBlockStatementCount() - m_complexJumpContinueIgnoreCount));
                }
            }
        }

        for (unsigned i = 0; i < m_labeledBreakStatmentPositions.size(); i++) {
            if (m_labeledBreakStatmentPositions[i].second > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_labeledBreakStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                if (tryCatchWithBlockStatementCount() - m_complexJumpLabeledBreakIgnoreCount > 0) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledBreakStatmentPositions[i].second, tryCatchWithBlockStatementCount() - m_complexJumpLabeledBreakIgnoreCount));
                }
            }
        }

        for (unsigned i = 0; i < m_labeledContinueStatmentPositions.size(); i++) {
            if (m_labeledContinueStatmentPositions[i].second > (unsigned long)frontlimit && m_complexCaseStatementPositions.find(m_labeledContinueStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                if (tryCatchWithBlockStatementCount() - m_complexJumpLabeledContinueIgnoreCount > 0) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledContinueStatmentPositions[i].second, tryCatchWithBlockStatementCount() - m_complexJumpLabeledContinueIgnoreCount));
                }
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
        if ((m_baseRegisterCount + 1) > REGULAR_REGISTER_LIMIT) {
            throw "register limit exceed";
        }
        RELEASE_ASSERT(m_baseRegisterCount + 1 < REGULAR_REGISTER_LIMIT);
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
    void consumeLabeledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount);
    void consumeContinuePositions(ByteCodeBlock* cb, size_t position, int outerLimitCount);
    void consumeLabeledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount);
    void morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos, size_t outerLimitCount = SIZE_MAX);
    bool isGlobalScope()
    {
        return m_isGlobalScope;
    }

    void addLexicallyDeclaredNames(AtomicString name)
    {
        bool finded = false;
        auto iter = m_lexicallyDeclaredNames->begin();
        while (iter != m_lexicallyDeclaredNames->end()) {
            if (iter->first == m_lexicalBlockIndex && iter->second == name) {
                finded = true;
                break;
            }
            iter++;
        }

        if (!finded) {
            m_lexicallyDeclaredNames->push_back(std::make_pair(m_lexicalBlockIndex, name));
        }
    }

    int tryCatchWithBlockStatementCount()
    {
        return m_recursiveStatementStack.size();
    }

    bool inTryStatement()
    {
        for (size_t i = 0; i < m_recursiveStatementStack.size(); i++) {
            if (m_recursiveStatementStack[i].first == Try) {
                return true;
            }
        }
        return false;
    }

    // NOTE this is counter! not index!!!!!!
    size_t m_baseRegisterCount;

    CodeBlock* m_codeBlock;
    ByteCodeBlock* m_byteCodeBlock;
    bool m_isGlobalScope : 1;
    bool m_isEvalCode : 1;
    bool m_isOutermostContext : 1;
    bool m_isWithScope : 1;
    bool m_isFunctionDeclarationBindingInitialization : 1;
    bool m_isVarDeclaredBindingInitialization : 1;
    bool m_isLexicallyDeclaredBindingInitialization : 1;
    bool m_canSkipCopyToRegister : 1;
    bool m_keepNumberalLiteralsInRegisterFile : 1;
    bool m_shouldGenerateByteCodeInstantly : 1;
    bool m_inCallingExpressionScope : 1;
    bool m_isHeadOfMemberExpression : 1;
    bool m_shouldGenerateLOCData : 1;
    bool m_forInOfVarBinding : 1;
    bool m_isLeftBindingAffectedByRightExpression : 1; // x = delete x; or x = eval("var x"), 1;

    std::shared_ptr<std::vector<ByteCodeRegisterIndex>> m_registerStack;
    std::shared_ptr<std::vector<std::pair<size_t, AtomicString>>> m_lexicallyDeclaredNames;
    std::vector<size_t> m_breakStatementPositions;
    std::vector<size_t> m_continueStatementPositions;
    std::vector<std::pair<String*, size_t>> m_labeledBreakStatmentPositions;
    std::vector<std::pair<String*, size_t>> m_labeledContinueStatmentPositions;
    std::vector<size_t> m_getObjectCodePositions;
    // For Label Statement
    size_t m_positionToContinue;
    // code position, special Statement count
    enum RecursiveStatementKind : size_t {
        Try,
        Catch,
        Finally,
        With,
        Block
    };
    std::vector<std::pair<RecursiveStatementKind, size_t>> m_recursiveStatementStack;
    int m_complexJumpBreakIgnoreCount;
    int m_complexJumpContinueIgnoreCount;
    int m_complexJumpLabeledBreakIgnoreCount;
    int m_complexJumpLabeledContinueIgnoreCount;
    size_t m_lexicalBlockIndex;
    ClassContextInformation m_classInfo;
    std::map<size_t, size_t> m_complexCaseStatementPositions;
    size_t m_maxPauseStatementExtraDataLength;
    NumeralLiteralVector* m_numeralLiteralData;
};

class ByteCodeGenerator {
public:
    static ByteCodeBlock* generateByteCode(Context* c, InterpretedCodeBlock* codeBlock, Node* ast, ASTFunctionScopeContext* scopeCtx, bool isEvalMode = false, bool isOnGlobal = false, bool inWithFromRuntime = false, bool shouldGenerateLOCData = false);
};
}

#endif
