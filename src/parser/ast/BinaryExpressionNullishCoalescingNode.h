/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef BinaryExpressionNullishCoalescingNode_h
#define BinaryExpressionNullishCoalescingNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionNullishCoalescingNode : public ExpressionNode {
public:
    BinaryExpressionNullishCoalescingNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::BinaryExpressionNullishCoalescing; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool isSlow = !canUseDirectRegister(context, m_left, m_right);
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }

        m_left->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), true, dstRegister), context, this->m_loc.index);
        size_t pos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();

        m_right->generateExpressionByteCode(codeBlock, context, dstRegister);
        codeBlock->peekCode<JumpIfUndefinedOrNull>(pos)->m_jumpPosition = codeBlock->currentCodeSize();

        context->m_canSkipCopyToRegister = directBefore;
    }

#if defined(ENABLE_TCO)
    virtual void generateTCOExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister, bool& isTailCall) override
    {
        bool isSlow = !canUseDirectRegister(context, m_left, m_right);
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }

        m_left->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), true, dstRegister), context, this->m_loc.index);
        size_t pos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();

        // try TCO only for right hand-side because we need to check the result of left hand-side expression
        m_right->generateTCOExpressionByteCode(codeBlock, context, dstRegister, isTailCall);
        codeBlock->peekCode<JumpIfUndefinedOrNull>(pos)->m_jumpPosition = codeBlock->currentCodeSize();

        context->m_canSkipCopyToRegister = directBefore;
    }
#endif

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_left->iterateChildrenIdentifier(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
    }


private:
    Node* m_left;
    Node* m_right;
};
} // namespace Escargot

#endif
