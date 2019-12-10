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

#ifndef AssignmentExpressionMinusNode_h
#define AssignmentExpressionMinusNode_h

#include "ExpressionNode.h"
#include "AssignmentExpressionSimpleNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionMinusNode : public ExpressionNode {
public:
    AssignmentExpressionMinusNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentExpressionMinus; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool slowMode = AssignmentExpressionSimpleNode::isLeftReferenceExpressionRelatedWithRightExpression(m_left, m_right);
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
        codeBlock->pushCode(BinaryMinus(ByteCodeLOC(m_loc.index), src0, src1, dstRegister), context, this);
        m_left->generateStoreByteCode(codeBlock, context, dstRegister, false);

        if (slowMode) {
            context->m_canSkipCopyToRegister = flagBefore;
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        return m_left->getRegister(codeBlock, context);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_left->iterateChildrenIdentifierAssigmentCase(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
    }

private:
    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
}

#endif
