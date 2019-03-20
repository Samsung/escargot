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

#ifndef AssignmentExpressionSimpleNode_h
#define AssignmentExpressionSimpleNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "PatternNode.h"
#include "MemberExpressionNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionSimpleNode : public ExpressionNode {
public:
    friend class ScriptParser;

    AssignmentExpressionSimpleNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    virtual ~AssignmentExpressionSimpleNode()
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

    virtual ASTNodeType type() { return ASTNodeType::AssignmentExpressionSimple; }
    static bool hasSlowAssigmentOperation(Node* left, Node* right)
    {
        std::vector<AtomicString> leftNames;
        left->iterateChildrenIdentifier([&leftNames](AtomicString name, bool isAssgnment) {
            leftNames.push_back(name);
        });

        bool isSlowMode = false;
        right->iterateChildrenIdentifier([&leftNames, &isSlowMode](AtomicString name, bool isAssgnment) {
            for (size_t i = 0; i < leftNames.size(); i++) {
                if (leftNames[i] == name) {
                    isSlowMode = true;
                }
            }
        });

        return isSlowMode;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        context->m_classInfo.isAssigmentTarget = true;
        bool isSlowMode = hasSlowAssigmentOperation(m_left.get(), m_right.get());

        bool isBase = context->m_registerStack->size() == 0;
        size_t rightRegister = dstRegister;
        if (isSlowMode) {
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_classInfo.isAssigmentTarget = false;
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        } else {
            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_classInfo.isAssigmentTarget = false;
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        }
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool isSlowMode = hasSlowAssigmentOperation(m_left.get(), m_right.get());
        context->m_classInfo.isAssigmentTarget = true;

        if (isSlowMode) {
            size_t rightRegister = m_right->getRegister(codeBlock, context);
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;
            context->m_classInfo.isAssigmentTarget = false;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        } else {
            auto rt = m_right->type();
            if (m_left->isIdentifier() && (rt != ASTNodeType::ArrayExpression && rt != ASTNodeType::ObjectExpression) && !context->m_isGlobalScope && !context->m_isEvalCode) {
                auto r = m_left->asIdentifier()->isAllocatedOnStack(context);
                context->m_classInfo.isAssigmentTarget = false;
                if (r.first) {
                    if (m_right->isLiteral()) {
                        auto r2 = m_right->getRegister(codeBlock, context);
                        if (r2 >= REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
                            context->giveUpRegister();
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r2, r.second), context, this);
                            return;
                        }
                    }
                    context->pushRegister(r.second);
                    m_right->generateExpressionByteCode(codeBlock, context, r.second);
                    context->giveUpRegister();
                    return;
                }
            }

            size_t rightRegister = m_right->getRegister(codeBlock, context);
            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_classInfo.isAssigmentTarget = false;
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        }
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
