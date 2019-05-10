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

#ifndef BinaryExpressionEqualNode_h
#define BinaryExpressionEqualNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionEqualNode : public ExpressionNode {
public:
    friend class ScriptParser;

    BinaryExpressionEqualNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left((ExpressionNode*)left)
        , m_right((ExpressionNode*)right)
    {
    }

    virtual ~BinaryExpressionEqualNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::BinaryExpressionEqual; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool isSlow = !canUseDirectRegister(context, m_left.get(), m_right.get());
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }
        size_t src0 = m_left->getRegister(codeBlock, context);
        size_t src1 = m_right->getRegister(codeBlock, context);
        m_left->generateExpressionByteCode(codeBlock, context, src0);
        m_right->generateExpressionByteCode(codeBlock, context, src1);

        context->giveUpRegister();
        context->giveUpRegister();

        if (dstRegister == REGISTER_LIMIT) {
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), src0, src1, false, false), context, this);
        } else {
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), src0, src1, dstRegister), context, this);
        }

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
