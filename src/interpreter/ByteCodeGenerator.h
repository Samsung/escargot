#ifndef __EscargotByteCodeGenerator__
#define __EscargotByteCodeGenerator__

#include "runtime/Value.h"
#include "runtime/String.h"
#include "parser/CodeBlock.h"

namespace Escargot {

class Context;
class CodeBlock;
class ByteCodeBlock;
class Node;

struct ParserContextInformation {
    ParserContextInformation(bool isEvalCode = false, bool isForGlobalScope = false)
        : m_isEvalCode(isEvalCode)
        , m_isForGlobalScope(isForGlobalScope)
    {
    }

    bool m_isEvalCode : 1;
    bool m_isForGlobalScope : 1;
};

struct ByteCodeGenerateContext {
    ByteCodeGenerateContext(CodeBlock* codeBlock, ByteCodeBlock* byteCodeBlock, ParserContextInformation& parserContextInformation)
        : m_baseRegisterCount(0)
        , m_codeBlock(codeBlock)
        , m_byteCodeBlock(byteCodeBlock)
        , m_isGlobalScope(parserContextInformation.m_isForGlobalScope)
        , m_isEvalCode(parserContextInformation.m_isEvalCode)
        , m_isOutermostContext(true)
        , m_offsetToBasePointer(0)
        , m_positionToContinue(0)
        , m_tryStatementScopeCount(0)
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
        , m_shouldGenerateByteCodeInstantly(contextBefore.m_shouldGenerateByteCodeInstantly)
        , m_inCallingExpressionScope(contextBefore.m_inCallingExpressionScope)
        , m_offsetToBasePointer(contextBefore.m_offsetToBasePointer)
        , m_positionToContinue(contextBefore.m_positionToContinue)
        , m_tryStatementScopeCount(contextBefore.m_tryStatementScopeCount)
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
        ctx.m_offsetToBasePointer = m_offsetToBasePointer;
        ctx.m_positionToContinue = m_positionToContinue;

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
        for (unsigned i = 0 ; i < m_breakStatementPositions.size() ; i ++) {
            if (m_breakStatementPositions[i] > (unsigned long) frontlimit) {
                if (m_complexCaseStatementPositions.find(m_breakStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_breakStatementPositions[i], m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0 ; i < m_continueStatementPositions.size() ; i ++) {
            if (m_continueStatementPositions[i] > (unsigned long) frontlimit) {
                if (m_complexCaseStatementPositions.find(m_continueStatementPositions[i]) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_continueStatementPositions[i], m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0 ; i < m_labeledBreakStatmentPositions.size() ; i ++) {
            if (m_labeledBreakStatmentPositions[i].second > (unsigned long) frontlimit) {
                if (m_complexCaseStatementPositions.find(m_labeledBreakStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledBreakStatmentPositions[i].second, m_tryStatementScopeCount));
                }
            }
        }

        for (unsigned i = 0 ; i < m_labeledContinueStatmentPositions.size() ; i ++) {
            if (m_labeledContinueStatmentPositions[i].second > (unsigned long) frontlimit) {
                if (m_complexCaseStatementPositions.find(m_labeledContinueStatmentPositions[i].second) == m_complexCaseStatementPositions.end()) {
                    m_complexCaseStatementPositions.insert(std::make_pair(m_labeledContinueStatmentPositions[i].second, m_tryStatementScopeCount));
                }
            }
        }

    }

    size_t getLastRegisterIndex()
    {
        ASSERT(m_baseRegisterCount > 0);
        return m_baseRegisterCount - 1;
    }

    size_t getRegister()
    {
        return m_baseRegisterCount++;
    }

    void giveUpRegister()
    {
        ASSERT(m_baseRegisterCount > 0);
        m_baseRegisterCount--;
    }

    void consumeBreakPositions(ByteCodeBlock* cb, size_t position);
    void consumeLabeledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl);
    void consumeContinuePositions(ByteCodeBlock* cb, size_t position);
    void consumeLabeledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl);
    void morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos);

    // NOTE this is counter! not index!!!!!!
    size_t m_baseRegisterCount;

    CodeBlock* m_codeBlock;
    ByteCodeBlock* m_byteCodeBlock;
    bool m_isGlobalScope;
    bool m_isEvalCode;
    bool m_isOutermostContext;

    bool m_shouldGenerateByteCodeInstantly;
    bool m_inCallingExpressionScope;
    bool m_isHeadOfMemberExpression;
    // bool m_hasArgumentsBinding;

    std::vector<size_t> m_breakStatementPositions;
    std::vector<size_t> m_continueStatementPositions;
    std::vector<std::pair<String*, size_t> > m_labeledBreakStatmentPositions;
    std::vector<std::pair<String*, size_t> > m_labeledContinueStatmentPositions;
    // For For In Statement
    size_t m_offsetToBasePointer;
    // For Label Statement
    size_t m_positionToContinue;
    // code position, tryStatement count
    int m_tryStatementScopeCount;
    std::map<size_t, size_t> m_complexCaseStatementPositions;
};

class ByteCodeGenerator {
    MAKE_STACK_ALLOCATED();
public:
    ByteCodeGenerator()
    {
    }

    void generateByteCode(Context* c, CodeBlock* codeBlock, Node* ast);
};

}

#endif
