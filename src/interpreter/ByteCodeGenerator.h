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

struct ByteCodeGenerateContext {
    ByteCodeGenerateContext(CodeBlock* codeBlock, ByteCodeBlock* byteCodeBlock, ParserContextInformation& parserContextInformation, Vector<Value, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<Value>>* numeralLiteralData)
        : m_baseRegisterCount(0)
        , m_codeBlock(codeBlock)
        , m_byteCodeBlock(byteCodeBlock)
        , m_isGlobalScope(parserContextInformation.m_isForGlobalScope)
        , m_isEvalCode(parserContextInformation.m_isEvalCode)
        , m_isOutermostContext(true)
        , m_isWithScope(parserContextInformation.m_isWithScope)
        , m_canSkipCopyToRegister(true)
        , m_keepNumberalLiteralsInRegisterFile(numeralLiteralData)
        , m_catchScopeCount(0)
        , m_shouldGenerateLOCData(true)
        , m_registerStack(new std::vector<ByteCodeRegisterIndex>())
        , m_offsetToBasePointer(0)
        , m_positionToContinue(0)
        , m_tryStatementScopeCount(0)
        , m_feCounter(0)
        , m_numeralLiteralData(numeralLiteralData)
    {
        m_inCallingExpressionScope = false;
        m_isHeadOfMemberExpression = false;
        m_shouldGenerateByteCodeInstantly = true;
    }

    ByteCodeGenerateContext(const ByteCodeGenerateContext& contextBefore)
        : m_baseRegisterCount(contextBefore.m_baseRegisterCount)
        , m_codeBlock(contextBefore.m_codeBlock)
        , m_byteCodeBlock(contextBefore.m_byteCodeBlock)
        , m_isGlobalScope(contextBefore.m_isGlobalScope)
        , m_isEvalCode(contextBefore.m_isEvalCode)
        , m_isOutermostContext(false)
        , m_isWithScope(false)
        , m_canSkipCopyToRegister(contextBefore.m_canSkipCopyToRegister)
        , m_keepNumberalLiteralsInRegisterFile(contextBefore.m_keepNumberalLiteralsInRegisterFile)
        , m_catchScopeCount(contextBefore.m_catchScopeCount)
        , m_shouldGenerateByteCodeInstantly(contextBefore.m_shouldGenerateByteCodeInstantly)
        , m_inCallingExpressionScope(contextBefore.m_inCallingExpressionScope)
        , m_shouldGenerateLOCData(contextBefore.m_shouldGenerateLOCData)
        , m_registerStack(contextBefore.m_registerStack)
        , m_offsetToBasePointer(contextBefore.m_offsetToBasePointer)
        , m_positionToContinue(contextBefore.m_positionToContinue)
        , m_tryStatementScopeCount(contextBefore.m_tryStatementScopeCount)
        , m_feCounter(contextBefore.m_feCounter)
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
        ctx.m_offsetToBasePointer = m_offsetToBasePointer;
        ctx.m_positionToContinue = m_positionToContinue;
        ctx.m_feCounter = m_feCounter;

        m_breakStatementPositions.clear();
        m_continueStatementPositions.clear();
        m_labeledBreakStatmentPositions.clear();
        m_labeledContinueStatmentPositions.clear();
        m_complexCaseStatementPositions.clear();
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
        ASSERT(m_tryStatementScopeCount);
        for (unsigned i = 0; i < m_breakStatementPositions.size(); i++) {
            if (m_breakStatementPositions[i] > (unsigned long)frontlimit) {
                if (m_complexCaseStatementPositions.find(m_breakStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_breakStatementPositions[i], m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0; i < m_continueStatementPositions.size(); i++) {
            if (m_continueStatementPositions[i] > (unsigned long)frontlimit) {
                if (m_complexCaseStatementPositions.find(m_continueStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_continueStatementPositions[i], m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0; i < m_labeledBreakStatmentPositions.size(); i++) {
            if (m_labeledBreakStatmentPositions[i].second > (unsigned long)frontlimit) {
                if (m_complexCaseStatementPositions.find(m_labeledBreakStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledBreakStatmentPositions[i].second, m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0; i < m_labeledContinueStatmentPositions.size(); i++) {
            if (m_labeledContinueStatmentPositions[i].second > (unsigned long)frontlimit) {
                if (m_complexCaseStatementPositions.find(m_labeledContinueStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledContinueStatmentPositions[i].second, m_tryStatementScopeCount));
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

    void consumeBreakPositions(ByteCodeBlock* cb, size_t position);
    void consumeLabeledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl);
    void consumeContinuePositions(ByteCodeBlock* cb, size_t position);
    void consumeLabeledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl);
    void morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos);
    bool isGlobalScope()
    {
        return m_isGlobalScope;
    }

    // NOTE this is counter! not index!!!!!!
    size_t m_baseRegisterCount;

    CodeBlock* m_codeBlock;
    ByteCodeBlock* m_byteCodeBlock;
    bool m_isGlobalScope;
    bool m_isEvalCode;
    bool m_isOutermostContext;
    bool m_isWithScope;
    bool m_canSkipCopyToRegister;
    bool m_keepNumberalLiteralsInRegisterFile;

    size_t m_catchScopeCount;

    AtomicString m_lastCatchVariableName;

    bool m_shouldGenerateByteCodeInstantly;
    bool m_inCallingExpressionScope;
    bool m_isHeadOfMemberExpression;
    bool m_shouldGenerateLOCData;

    std::shared_ptr<std::vector<ByteCodeRegisterIndex>> m_registerStack;
    std::vector<size_t> m_breakStatementPositions;
    std::vector<size_t> m_continueStatementPositions;
    std::vector<std::pair<String*, size_t>> m_labeledBreakStatmentPositions;
    std::vector<std::pair<String*, size_t>> m_labeledContinueStatmentPositions;
    std::vector<size_t> m_getObjectCodePositions;
    // For For In Statement
    size_t m_offsetToBasePointer;
    // For Label Statement
    size_t m_positionToContinue;
    // code position, tryStatement count
    int m_tryStatementScopeCount;
    size_t m_feCounter;
    std::map<size_t, size_t> m_complexCaseStatementPositions;
    Vector<Value, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<Value>>* m_numeralLiteralData;
};

class ByteCodeGenerator {
    MAKE_STACK_ALLOCATED();

public:
    ByteCodeGenerator()
    {
    }

    ByteCodeBlock* generateByteCode(Context* c, CodeBlock* codeBlock, Node* ast, ASTScopeContext* scopeCtx, bool isEvalMode = false, bool isOnGlobal = false, bool shouldGenerateLOCData = false);
};
}

#endif
