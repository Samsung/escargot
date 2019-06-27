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

#ifndef SwitchStatementNode_h
#define SwitchStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "SwitchCaseNode.h"

namespace Escargot {

class SwitchStatementNode : public StatementNode {
public:
    SwitchStatementNode(Node* discriminant, StatementContainer* casesA, Node* deflt, StatementContainer* casesB, bool lexical)
        : StatementNode()
        , m_discriminant((ExpressionNode*)discriminant)
        , m_casesA(casesA)
        , m_default((StatementNode*)deflt)
        , m_casesB(casesB)
        , m_lexical(lexical)
    {
    }

    virtual ~SwitchStatementNode()
    {
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ByteCodeGenerateContext newContext(*context);
        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by caseNode->m_test

        bool canSkipCopyToRegister = newContext.m_canSkipCopyToRegister;
        newContext.m_canSkipCopyToRegister = false;
        size_t rIndex0 = m_discriminant->getRegister(codeBlock, &newContext);
        m_discriminant->generateExpressionByteCode(codeBlock, &newContext, rIndex0);
        newContext.m_canSkipCopyToRegister = canSkipCopyToRegister;

        std::vector<size_t> jumpCodePerCaseNodePosition;
        StatementNode* nd = m_casesB->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            size_t refIndex = caseNode->m_test->getRegister(codeBlock, &newContext);
            caseNode->m_test->generateExpressionByteCode(codeBlock, &newContext, refIndex);
            size_t resultIndex = newContext.getRegister();
            codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this);
            jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), resultIndex), &newContext, this);
            newContext.giveUpRegister();
            newContext.giveUpRegister();
            nd = nd->nextSilbing();
        }

        ASSERT(rIndex0 == newContext.getLastRegisterIndex());
        nd = m_casesA->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            size_t refIndex = caseNode->m_test->getRegister(codeBlock, &newContext);
            caseNode->m_test->generateExpressionByteCode(codeBlock, &newContext, refIndex);
            size_t resultIndex = newContext.getRegister();
            codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this);
            jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), resultIndex), &newContext, this);
            newContext.giveUpRegister();
            newContext.giveUpRegister();
            nd = nd->nextSilbing();
        }

        newContext.giveUpRegister();
        newContext.giveUpRegister();

        size_t jmpToDefault = SIZE_MAX;
        jmpToDefault = codeBlock->currentCodeSize();
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), &newContext, this);
        size_t caseIdx = 0;
        nd = m_casesB->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            codeBlock->peekCode<JumpIfTrue>(jumpCodePerCaseNodePosition[caseIdx++])->m_jumpPosition = codeBlock->currentCodeSize();
            caseNode->generateStatementByteCode(codeBlock, &newContext);
            nd = nd->nextSilbing();
        }
        if (m_default) {
            codeBlock->peekCode<Jump>(jmpToDefault)->m_jumpPosition = codeBlock->currentCodeSize();
            m_default->generateStatementByteCode(codeBlock, &newContext);
        }
        nd = m_casesA->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            codeBlock->peekCode<JumpIfTrue>(jumpCodePerCaseNodePosition[caseIdx++])->m_jumpPosition = codeBlock->currentCodeSize();
            caseNode->generateStatementByteCode(codeBlock, &newContext);
            nd = nd->nextSilbing();
        }
        size_t breakPos = codeBlock->currentCodeSize();
        newContext.consumeBreakPositions(codeBlock, breakPos, context->m_tryStatementScopeCount);
        newContext.m_positionToContinue = context->m_positionToContinue;

        newContext.propagateInformationTo(*context);
        if (!m_default) {
            codeBlock->peekCode<Jump>(jmpToDefault)->m_jumpPosition = codeBlock->currentCodeSize();
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::SwitchStatement; }
private:
    ExpressionNode* m_discriminant;
    StatementContainer* m_casesA;
    StatementNode* m_default;
    StatementContainer* m_casesB;
    bool m_lexical : 1;
};
}

#endif
