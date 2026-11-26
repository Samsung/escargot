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

#ifndef BinaryExpressionNotEqualNode_h
#define BinaryExpressionNotEqualNode_h

#include "BinaryExpressionNode.h"

namespace Escargot {

class BinaryExpressionNotEqualNode : public BinaryExpressionNode {
public:
    using BinaryExpressionNode::BinaryExpressionNode;

    virtual ASTNodeType type() override { return ASTNodeType::BinaryExpressionNotEqual; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool isSlow = !canUseDirectRegister(context, m_left, m_right);
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
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), src0, src1, false, false), context, this->m_loc.index);
        } else {
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), src0, src1, dstRegister, 1), context, this->m_loc.index);
        }

        context->m_canSkipCopyToRegister = directBefore;
    }
};
} // namespace Escargot

#endif
