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

#ifndef ConditionalExpressionNode_h
#define ConditionalExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ConditionalExpressionNode : public ExpressionNode {
public:
    ConditionalExpressionNode(Node* test, Node* consequente, Node* alternate)
        : ExpressionNode()
        , m_test(test)
        , m_consequente(consequente)
        , m_alternate(alternate)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ConditionalExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this->m_loc.index);

        // give testReg
        context->giveUpRegister();

        size_t jumpPosForTestIsFalse = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateExpressionByteCode(codeBlock, context, dstRegister);
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this->m_loc.index);
        JumpIfFalse* jumpForTestIsFalse = codeBlock->peekCode<JumpIfFalse>(jumpPosForTestIsFalse);
        size_t jumpPosForEndOfConsequence = codeBlock->lastCodePosition<Jump>();

        jumpForTestIsFalse->m_jumpPosition = codeBlock->currentCodeSize();
        m_alternate->generateExpressionByteCode(codeBlock, context, dstRegister);

        Jump* jumpForEndOfConsequence = codeBlock->peekCode<Jump>(jumpPosForEndOfConsequence);
        jumpForEndOfConsequence->m_jumpPosition = codeBlock->currentCodeSize();
    }

#if defined(ENABLE_TCO)
    virtual void generateTCOExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister, bool& isTailCallForm) override
    {
        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this->m_loc.index);

        // give testReg
        context->giveUpRegister();

        size_t jumpPosForTestIsFalse = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateTCOExpressionByteCode(codeBlock, context, dstRegister, isTailCallForm);
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this->m_loc.index);
        JumpIfFalse* jumpForTestIsFalse = codeBlock->peekCode<JumpIfFalse>(jumpPosForTestIsFalse);
        size_t jumpPosForEndOfConsequence = codeBlock->lastCodePosition<Jump>();

        jumpForTestIsFalse->m_jumpPosition = codeBlock->currentCodeSize();
        m_alternate->generateTCOExpressionByteCode(codeBlock, context, dstRegister, isTailCallForm);

        Jump* jumpForEndOfConsequence = codeBlock->peekCode<Jump>(jumpPosForEndOfConsequence);
        jumpForEndOfConsequence->m_jumpPosition = codeBlock->currentCodeSize();
    }
#endif

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_test->iterateChildrenIdentifier(fn);
        m_consequente->iterateChildrenIdentifier(fn);
        m_alternate->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_test->iterateChildren(fn);
        m_consequente->iterateChildren(fn);
        m_alternate->iterateChildren(fn);
    }

private:
    Node* m_test;
    Node* m_consequente;
    Node* m_alternate;
};
} // namespace Escargot

#endif
