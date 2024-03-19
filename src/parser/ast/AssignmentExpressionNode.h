/*
 * Copyright (c) 2023-present Samsung Electronics Co., Ltd
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

#ifndef AssignmentExpressionNode_h
#define AssignmentExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "ArrayPatternNode.h"
#include "MemberExpressionNode.h"
#include "UnaryExpressionDeleteNode.h"
#include "CallExpressionNode.h"

namespace Escargot {

class AssignmentExpressionNode : public ExpressionNode {
public:
    AssignmentExpressionNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    Node* left()
    {
        return m_left;
    }

    Node* right()
    {
        return m_right;
    }

    virtual ASTNodeType type() override = 0;
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override = 0;

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

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        return m_left->getRegister(codeBlock, context);
    }

protected:
    bool isLeftReferenceExpressionRelatedWithRightExpression()
    {
        std::vector<AtomicString> leftNames;
        m_left->iterateChildrenIdentifier([&leftNames](AtomicString name, bool isAssignment) {
            leftNames.push_back(name);
        });

        bool isSlowMode = false;
        m_right->iterateChildrenIdentifier([&leftNames, &isSlowMode](AtomicString name, bool isAssignment) {
            for (size_t i = 0; i < leftNames.size(); i++) {
                if (leftNames[i] == name) {
                    isSlowMode = true;
                    break;
                }
            }
        });

        return isSlowMode;
    }

    bool isLeftBindingAffectedByRightExpression()
    {
        if (m_left->isIdentifier()) {
            AtomicString leftName = m_left->asIdentifier()->name();
            bool result = false;
            m_right->iterateChildren([&](Node* node) {
                if (node->type() == ASTNodeType::CallExpression) {
                    CallExpressionNode* call = (CallExpressionNode*)node;
                    if (call->callee()->isIdentifier() && call->callee()->asIdentifier()->name().string()->equals("eval")) {
                        // x = (eval("var x;"), 1);
                        result = true;
                    }
                }
            });
            return result;
        }
        return false;
    }

    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
} // namespace Escargot

#endif
