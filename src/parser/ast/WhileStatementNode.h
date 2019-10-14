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

#ifndef WhileStatementNode_h
#define WhileStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class WhileStatementNode : public StatementNode {
public:
    WhileStatementNode(Node* test, Node* body)
        : StatementNode()
        , m_test(test)
        , m_body(body)
    {
    }

    virtual ~WhileStatementNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::WhileStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test

        size_t whileStart = codeBlock->currentCodeSize();
        size_t testPos = SIZE_MAX;
        ExecutionState stateForTest(codeBlock->m_codeBlock->context());
        if (m_test->isLiteral() && m_test->asLiteral()->value().isPrimitive() && m_test->asLiteral()->value().toBoolean(stateForTest)) {
            // skip generate code
        } else {
            if (m_test->isRelationOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfRelation>();
            } else if (m_test->isEqualityOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfEqual>();
            } else {
                ByteCodeRegisterIndex testR = m_test->getRegister(codeBlock, &newContext);
                m_test->generateExpressionByteCode(codeBlock, &newContext, testR);
                codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testR), &newContext, this);
                testPos = codeBlock->lastCodePosition<JumpIfFalse>();
                newContext.giveUpRegister();
            }
        }

        newContext.giveUpRegister();

        m_body->generateStatementByteCode(codeBlock, &newContext);

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), whileStart), &newContext, this);
        newContext.consumeContinuePositions(codeBlock, whileStart, context->tryCatchWithBlockStatementCount());
        size_t whileEnd = codeBlock->currentCodeSize();
        newContext.consumeBreakPositions(codeBlock, whileEnd, context->tryCatchWithBlockStatementCount());
        if (testPos != SIZE_MAX)
            codeBlock->peekCode<JumpByteCode>(testPos)->m_jumpPosition = whileEnd;
        newContext.m_positionToContinue = context->m_positionToContinue;
        newContext.propagateInformationTo(*context);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_test->iterateChildren(fn);
        m_body->iterateChildren(fn);
    }

private:
    Node* m_test;
    Node* m_body;
};
}

#endif
