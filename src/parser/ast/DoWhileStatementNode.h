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

#ifndef DoWhileStatementNode_h
#define DoWhileStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class DoWhileStatementNode : public StatementNode {
public:
    DoWhileStatementNode(Node* test, Node* body)
        : StatementNode()
        , m_test(test)
        , m_body(body)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::DoWhileStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        ByteCodeGenerateContext newContext(*context);

        size_t doStart = codeBlock->currentCodeSize();
        if (context->shouldCareScriptExecutionResult()) {
            // IterationStatement : do Statement while ( Expression ) ;
            // 1. Let V = undefined.
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), 0, Value()), &newContext, this);
        }

#ifdef ESCARGOT_DEBUGGER
        insertEmptyStatementBreakpoint(context, m_body);
#endif /* ESCARGOT_DEBUGGER */
        m_body->generateStatementByteCode(codeBlock, &newContext);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test
        size_t testPos = codeBlock->currentCodeSize();
        size_t testReg = m_test->getRegister(codeBlock, &newContext);
        m_test->generateExpressionByteCode(codeBlock, &newContext, testReg);
        codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), testReg, doStart), &newContext, this);

        newContext.giveUpRegister();
        newContext.giveUpRegister();

        size_t doEnd = codeBlock->currentCodeSize();
        newContext.consumeContinuePositions(codeBlock, testPos, context->tryCatchWithBlockStatementCount());
        newContext.consumeBreakPositions(codeBlock, doEnd, context->tryCatchWithBlockStatementCount());
        newContext.m_positionToContinue = testPos;
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
} // namespace Escargot

#endif
