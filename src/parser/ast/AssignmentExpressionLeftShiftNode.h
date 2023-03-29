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

#ifndef AssignmentExpressionLeftShiftNode_h
#define AssignmentExpressionLeftShiftNode_h

#include "AssignmentExpressionNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionLeftShiftNode : public AssignmentExpressionNode {
public:
    AssignmentExpressionLeftShiftNode(Node* left, Node* right)
        : AssignmentExpressionNode(left, right)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentExpressionLeftShift; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool slowMode = isLeftReferenceExpressionRelatedWithRightExpression();
        bool flagBefore = context->m_canSkipCopyToRegister;
        if (slowMode) {
            context->m_canSkipCopyToRegister = false;
        }

        m_left->generateResolveAddressByteCode(codeBlock, context);
        m_left->generateReferenceResolvedAddressByteCode(codeBlock, context);
        size_t src0 = context->getLastRegisterIndex();
        size_t src1 = m_right->getRegister(codeBlock, context);
        m_right->generateExpressionByteCode(codeBlock, context, src1);
        context->giveUpRegister();
        context->giveUpRegister();
        codeBlock->pushCode(BinaryLeftShift(ByteCodeLOC(m_loc.index), src0, src1, dstRegister), context, this->m_loc.index);
        m_left->generateStoreByteCode(codeBlock, context, dstRegister, false);

        if (slowMode) {
            context->m_canSkipCopyToRegister = flagBefore;
        }
    }
};
} // namespace Escargot

#endif
