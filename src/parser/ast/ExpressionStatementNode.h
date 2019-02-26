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

#ifndef ExpressionStatementNode_h
#define ExpressionStatementNode_h

#include "ExpressionNode.h"

namespace Escargot {

// An expression statement, i.e., a statement consisting of a single expression.
class ExpressionStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    explicit ExpressionStatementNode(Node* expression)
        : StatementNode()
        , m_expression(expression)
    {
    }

    virtual ~ExpressionStatementNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ExpressionStatement; }
    Node* expression() { return m_expression.get(); }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (!context->m_isEvalCode && !context->m_isGlobalScope) {
            m_expression->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            return;
        }
#ifndef NDEBUG
        size_t before = context->m_registerStack->size();
#endif
        m_expression->generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
        ASSERT(context->m_registerStack->size() == before);
    }

private:
    RefPtr<Node> m_expression; // expression: Expression;
};
}

#endif
