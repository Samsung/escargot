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

#ifndef DefaultArgumentNode_h
#define DefaultArgumentNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "PatternNode.h"
#include "MemberExpressionNode.h"

namespace Escargot {

class DefaultArgumentNode : public ExpressionNode {
public:
    friend class ScriptParser;

    DefaultArgumentNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    virtual ~DefaultArgumentNode()
    {
    }

    void giveupChildren()
    {
        m_left = m_right = nullptr;
    }

    Node* left()
    {
        return m_left.get();
    }

    virtual ASTNodeType type() { return ASTNodeType::DefaultArgument; }
    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t undefinedIndex = context->getRegister();
        codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), undefinedIndex, PropertyName(codeBlock->m_codeBlock->context()->staticStrings().undefined)), context, this);

        size_t src0 = m_left->getRegister(codeBlock, context);
        m_left->generateExpressionByteCode(codeBlock, context, src0);

        context->giveUpRegister();

        size_t cmpIndex = context->getRegister();
        codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), src0, undefinedIndex, cmpIndex), context, this);

        context->giveUpRegister(); // for drop undefinedIndex

        codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
        size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();

        context->giveUpRegister(); // for drop cmpIndex

        RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(m_left.get(), m_right.get()));
        assign->m_loc = m_loc;
        assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);

        codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        // Note: This could only happen by destructuring a pattern
        RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(m_left.get(), m_right.get()));
        assign->m_loc = m_loc;
        assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
    }


private:
    RefPtr<Node> m_left; // left: Identifier;
    RefPtr<Node> m_right; // right: Expression;
};
}

#endif
