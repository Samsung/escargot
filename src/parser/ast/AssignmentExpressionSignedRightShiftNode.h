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

#ifndef AssignmentExpressionSignedRightShiftNode_h
#define AssignmentExpressionSignedRightShiftNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "PatternNode.h"
#include "AssignmentExpressionSimpleNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionSignedRightShiftNode : public ExpressionNode {
public:
    friend class ScriptParser;

    AssignmentExpressionSignedRightShiftNode(Node* left, Node* right)
        : ExpressionNode()
    {
        m_left = left;
        m_right = right;
    }

    virtual ~AssignmentExpressionSignedRightShiftNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::AssignmentExpressionSignedRightShift; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool slowMode = AssignmentExpressionSimpleNode::hasSlowAssigmentOperation(m_left.get(), m_right.get());
        ;
        bool flagBefore;
        if (slowMode) {
            flagBefore = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;
        }

        m_left->generateResolveAddressByteCode(codeBlock, context);
        m_left->generateReferenceResolvedAddressByteCode(codeBlock, context);
        size_t src0 = context->getLastRegisterIndex();
        size_t src1 = m_right->getRegister(codeBlock, context);
        m_right->generateExpressionByteCode(codeBlock, context, src1);
        context->giveUpRegister();
        context->giveUpRegister();
        codeBlock->pushCode(BinarySignedRightShift(ByteCodeLOC(m_loc.index), src0, src1, dstRegister), context, this);
        m_left->generateStoreByteCode(codeBlock, context, dstRegister, false);

        if (slowMode) {
            context->m_canSkipCopyToRegister = flagBefore;
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        return m_left->getRegister(codeBlock, context);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_left->iterateChildrenIdentifierAssigmentCase(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

private:
    RefPtr<Node> m_left; // left: Pattern;
    RefPtr<Node> m_right; // right: Expression;
};
}

#endif
