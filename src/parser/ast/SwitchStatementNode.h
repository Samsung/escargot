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

#ifndef SwitchStatementNode_h
#define SwitchStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "SwitchCaseNode.h"

namespace Escargot {

class SwitchStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    SwitchStatementNode(Node* discriminant, StatementNodeVector&& casesA, Node* deflt, StatementNodeVector&& casesB, bool lexical)
        : StatementNode()
    {
        m_discriminant = (ExpressionNode*)discriminant;
        m_casesA = casesA;
        m_default = (StatementNode*)deflt;
        m_casesB = casesB;
        m_lexical = lexical;
    }

    virtual ~SwitchStatementNode()
    {
        delete m_discriminant;
        for (unsigned i = 0; i < m_casesA.size(); i++) {
            delete m_casesA[i];
        }
        for (unsigned i = 0; i < m_casesB.size(); i++) {
            delete m_casesB[i];
        }
        delete m_default;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by caseNode->m_test

        size_t rIndex0 = m_discriminant->getRegister(codeBlock, &newContext);
        m_discriminant->generateExpressionByteCode(codeBlock, &newContext, rIndex0);

        std::vector<size_t> jumpCodePerCaseNodePosition;
        for (unsigned i = 0; i < m_casesB.size(); i++) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)m_casesB[i];
            size_t refIndex = caseNode->m_test->getRegister(codeBlock, &newContext);
            caseNode->m_test->generateExpressionByteCode(codeBlock, &newContext, refIndex);
            size_t resultIndex = newContext.getRegister();
            codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this);
            jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), resultIndex), &newContext, this);
            newContext.giveUpRegister();
            newContext.giveUpRegister();
        }

        ASSERT(rIndex0 == newContext.getLastRegisterIndex());

        for (unsigned i = 0; i < m_casesA.size(); i++) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)m_casesA[i];
            size_t refIndex = caseNode->m_test->getRegister(codeBlock, &newContext);
            caseNode->m_test->generateExpressionByteCode(codeBlock, &newContext, refIndex);
            size_t resultIndex = newContext.getRegister();
            codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this);
            jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), resultIndex), &newContext, this);
            newContext.giveUpRegister();
            newContext.giveUpRegister();
        }

        newContext.giveUpRegister();
        newContext.giveUpRegister();

        size_t jmpToDefault = SIZE_MAX;
        jmpToDefault = codeBlock->currentCodeSize();
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), &newContext, this);
        size_t caseIdx = 0;
        for (unsigned i = 0; i < m_casesB.size(); i++) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)m_casesB[i];
            codeBlock->peekCode<JumpIfTrue>(jumpCodePerCaseNodePosition[caseIdx++])->m_jumpPosition = codeBlock->currentCodeSize();
            caseNode->generateStatementByteCode(codeBlock, &newContext);
        }
        if (m_default) {
            codeBlock->peekCode<Jump>(jmpToDefault)->m_jumpPosition = codeBlock->currentCodeSize();
            m_default->generateStatementByteCode(codeBlock, &newContext);
        }
        for (unsigned i = 0; i < m_casesA.size(); i++) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)m_casesA[i];
            codeBlock->peekCode<JumpIfTrue>(jumpCodePerCaseNodePosition[caseIdx++])->m_jumpPosition = codeBlock->currentCodeSize();
            caseNode->generateStatementByteCode(codeBlock, &newContext);
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
protected:
    ExpressionNode* m_discriminant;
    StatementNodeVector m_casesA;
    StatementNode* m_default;
    StatementNodeVector m_casesB;
    bool m_lexical;
};
}

#endif
