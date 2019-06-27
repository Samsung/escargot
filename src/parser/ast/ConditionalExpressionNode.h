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

#ifndef ConditionalExpressionNode_h
#define ConditionalExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ConditionalExpressionNode : public ExpressionNode {
public:
    ConditionalExpressionNode(Node* test, Node* consequente, Node* alternate)
        : ExpressionNode()
        , m_test((ExpressionNode*)test)
        , m_consequente((ExpressionNode*)consequente)
        , m_alternate((ExpressionNode*)alternate)
    {
    }
    virtual ~ConditionalExpressionNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ConditionalExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t resultRegisterExpected = dstRegister;

        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this);

        // give testReg
        context->giveUpRegister();

        size_t jumpPosForTestIsFalse = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateExpressionByteCode(codeBlock, context, dstRegister);
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
        JumpIfFalse* jumpForTestIsFalse = codeBlock->peekCode<JumpIfFalse>(jumpPosForTestIsFalse);
        size_t jumpPosForEndOfConsequence = codeBlock->lastCodePosition<Jump>();

        jumpForTestIsFalse->m_jumpPosition = codeBlock->currentCodeSize();
        m_alternate->generateExpressionByteCode(codeBlock, context, dstRegister);

        Jump* jumpForEndOfConsequence = codeBlock->peekCode<Jump>(jumpPosForEndOfConsequence);
        jumpForEndOfConsequence->m_jumpPosition = codeBlock->currentCodeSize();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_test->iterateChildrenIdentifier(fn);
        m_consequente->iterateChildrenIdentifier(fn);
        m_alternate->iterateChildrenIdentifier(fn);
    }

private:
    ExpressionNode* m_test;
    ExpressionNode* m_consequente;
    ExpressionNode* m_alternate;
};
}

#endif
