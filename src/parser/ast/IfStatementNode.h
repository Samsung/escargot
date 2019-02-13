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

#ifndef IfStatementNode_h
#define IfStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class IfStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    IfStatementNode(Node* test, Node* consequente, Node* alternate)
        : StatementNode()
        , m_test((ExpressionNode*)test)
        , m_consequente((StatementNode*)consequente)
        , m_alternate((StatementNode*)alternate)
    {
    }

    virtual ~IfStatementNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::IfStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->getRegister(); // ExeuctionResult of m_consequente|m_alternate should not be overwritten by m_test
        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this);
        context->giveUpRegister();
        context->giveUpRegister();

        size_t jPos = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateStatementByteCode(codeBlock, context);
        size_t jPos2 = 0;
        if (m_alternate) {
            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index)), context, this);
            jPos2 = codeBlock->lastCodePosition<Jump>();
        }
        JumpIfFalse* j = codeBlock->peekCode<JumpIfFalse>(jPos);
        j->m_jumpPosition = codeBlock->currentCodeSize();

        if (!m_alternate) {
            if (context->m_isEvalCode || context->m_isGlobalScope) {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value()), context, this);
                context->giveUpRegister();
            }
        } else {
            m_alternate->generateStatementByteCode(codeBlock, context);
            Jump* j2 = codeBlock->peekCode<Jump>(jPos2);
            j2->m_jumpPosition = codeBlock->currentCodeSize();
        }
    }

private:
    RefPtr<ExpressionNode> m_test;
    RefPtr<StatementNode> m_consequente;
    RefPtr<StatementNode> m_alternate;
};
}

#endif
