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

#ifndef BinaryExpressionLogicalOrNode_h
#define BinaryExpressionLogicalOrNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionLogicalOrNode : public ExpressionNode {
public:
    BinaryExpressionLogicalOrNode(Node* left, Node* right)
        : ExpressionNode()
    {
        m_left = (ExpressionNode*)left;
        m_right = (ExpressionNode*)right;
    }

    virtual ~BinaryExpressionLogicalOrNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::BinaryExpressionLogicalOr; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool isSlow = !canUseDirectRegister(context, m_left.get(), m_right.get());
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }

        m_left->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), dstRegister), context, this);
        size_t pos = codeBlock->lastCodePosition<JumpIfTrue>();

        m_right->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->peekCode<JumpIfTrue>(pos)->m_jumpPosition = codeBlock->currentCodeSize();

        context->m_canSkipCopyToRegister = directBefore;
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_left->iterateChildrenIdentifier(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

private:
    RefPtr<ExpressionNode> m_left;
    RefPtr<ExpressionNode> m_right;
};
}

#endif
