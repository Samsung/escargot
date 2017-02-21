/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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

    Node* left()
    {
        return m_left;
    }

    virtual ASTNodeType type() { return ASTNodeType::AssignmentExpressionSimple; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
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


        if (isSlowMode) {
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;
            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context);
            m_left->generateStoreByteCode(codeBlock, context, false);
        } else {
            m_left->generateResolveAddressByteCode(codeBlock, context);
            m_right->generateExpressionByteCode(codeBlock, context);
            m_left->generateStoreByteCode(codeBlock, context, false);
        }
    }

protected:
    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
}

#endif
