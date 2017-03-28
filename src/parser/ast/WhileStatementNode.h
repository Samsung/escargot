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

#ifndef WhileStatementNode_h
#define WhileStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class WhileStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    WhileStatementNode(Node *test, Node *body)
        : StatementNode()
    {
        m_test = (ExpressionNode *)test;
        m_body = (StatementNode *)body;
    }

    virtual ~WhileStatementNode()
    {
        delete m_test;
        delete m_body;
    }

    virtual ASTNodeType type() { return ASTNodeType::WhileStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test

        size_t whileStart = codeBlock->currentCodeSize();
        size_t testPos = SIZE_MAX;
        ExecutionState stateForTest(codeBlock->m_codeBlock->context());
        if (m_test->isLiteral() && m_test->asLiteral()->value().isPrimitive() && m_test->asLiteral()->value().toBoolean(stateForTest)) {
            // skip generate code
        } else {
            ByteCodeRegisterIndex testR = m_test->getRegister(codeBlock, &newContext);
            m_test->generateExpressionByteCode(codeBlock, &newContext, testR);
            codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testR), &newContext, this);
            testPos = codeBlock->lastCodePosition<JumpIfFalse>();
            newContext.giveUpRegister();
        }

        newContext.giveUpRegister();

        m_body->generateStatementByteCode(codeBlock, &newContext);

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), whileStart), &newContext, this);
        newContext.consumeContinuePositions(codeBlock, whileStart, context->m_tryStatementScopeCount);
        size_t whileEnd = codeBlock->currentCodeSize();
        newContext.consumeBreakPositions(codeBlock, whileEnd, context->m_tryStatementScopeCount);
        if (testPos != SIZE_MAX)
            codeBlock->peekCode<JumpIfFalse>(testPos)->m_jumpPosition = whileEnd;
        newContext.m_positionToContinue = context->m_positionToContinue;
        newContext.propagateInformationTo(*context);
    }

protected:
    ExpressionNode *m_test;
    StatementNode *m_body;
};
}

#endif
