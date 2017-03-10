/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    {
        m_left = left;
        m_right = right;
    }

    virtual ~AssignmentExpressionSimpleNode()
    {
        delete m_left;
        delete m_right;
    }

    void giveupChildren()
    {
        m_left = m_right = nullptr;
    }

    Node* left()
    {
        return m_left;
    }

    virtual ASTNodeType type() { return ASTNodeType::AssignmentExpressionSimple; }
    bool hasSlowAssigmentOperation()
    {
        bool isSlowMode = true;

        if (m_right->isLiteral()) {
            isSlowMode = false;
        }

        if (m_left->isIdentifier()) {
            isSlowMode = false;
        } else {
            bool isLeftSimple = m_left->isIdentifier() || m_left->isLiteral();
            if (m_left->isMemberExpression()) {
                isLeftSimple = (m_left->asMemberExpression()->object()->isIdentifier() || m_left->asMemberExpression()->object()->isLiteral()) && (m_left->asMemberExpression()->property()->isIdentifier() || m_left->asMemberExpression()->property()->isLiteral());
            }

            bool isRightSimple = m_right->isIdentifier() || m_right->isLiteral();
            if (m_right->isMemberExpression()) {
                isRightSimple = (m_right->asMemberExpression()->object()->isIdentifier() || m_right->asMemberExpression()->object()->isLiteral()) && (m_right->asMemberExpression()->property()->isIdentifier() || m_right->asMemberExpression()->property()->isLiteral());
            }

            if (isLeftSimple && isRightSimple) {
                isSlowMode = false;
            }
        }

        return isSlowMode;
    }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool isSlowMode = hasSlowAssigmentOperation();

        bool isBase = context->m_registerStack->size() == 0;
        size_t rightRegister = dstRegister;
        if (isSlowMode) {
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        } else {
            m_left->generateResolveAddressByteCode(codeBlock, context);
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        }
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool isSlowMode = hasSlowAssigmentOperation();

        if (isSlowMode) {
            size_t rightRegister = m_right->getRegister(codeBlock, context);
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        } else {
            auto rt = m_right->type();
            if (m_left->isIdentifier() && (rt != ASTNodeType::ArrayExpression && rt != ASTNodeType::ObjectExpression) && !context->m_isGlobalScope && !context->m_isEvalCode) {
                auto r = m_left->asIdentifier()->isAllocatedOnStack(context);
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
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        return m_left->getRegister(codeBlock, context);
    }

protected:
    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
}

#endif
