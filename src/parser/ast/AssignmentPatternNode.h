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

#ifndef AssignmentPatternNode_h
#define AssignmentPatternNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"

#include "MemberExpressionNode.h"
#include "LiteralNode.h"

namespace Escargot {

class AssignmentPatternNode : public ExpressionNode {
public:
    AssignmentPatternNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    AssignmentPatternNode(Node* left, Node* right, NodeLOC& loc)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
        m_loc = loc;
    }

    Node* left()
    {
        return m_left;
    }

    Node* right()
    {
        return m_right;
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        LiteralNode* undefinedNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value());
        size_t undefinedIndex = undefinedNode->getRegister(codeBlock, context);
        undefinedNode->generateExpressionByteCode(codeBlock, context, undefinedIndex);

        size_t cmpIndex = context->getRegister();
        codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), srcRegister, undefinedIndex, cmpIndex), context, this);

        context->giveUpRegister(); // for drop undefinedIndex

        codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
        size_t pos1 = codeBlock->lastCodePosition<JumpIfTrue>();
        context->giveUpRegister(); // for drop cmpIndex

        // not undefined case, set srcRegister
        m_left->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
        codeBlock->pushCode<Jump>(Jump(ByteCodeLOC(m_loc.index)), context, this);
        size_t pos2 = codeBlock->lastCodePosition<Jump>();

        // undefined case, set default node
        codeBlock->peekCode<JumpIfTrue>(pos1)->m_jumpPosition = codeBlock->currentCodeSize();
        size_t rightIndex = m_right->getRegister(codeBlock, context);
        m_right->generateExpressionByteCode(codeBlock, context, rightIndex);
        m_left->generateStoreByteCode(codeBlock, context, rightIndex, needToReferenceSelf);
        context->giveUpRegister(); // for drop rightIndex

        codeBlock->peekCode<Jump>(pos2)->m_jumpPosition = codeBlock->currentCodeSize();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_left->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_left) {
            m_left->iterateChildren(fn);
        }

        if (m_right) {
            m_right->iterateChildren(fn);
        }
    }

private:
    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
}

#endif
