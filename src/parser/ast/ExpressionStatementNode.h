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

#ifndef ExpressionStatementNode_h
#define ExpressionStatementNode_h

#include "ExpressionNode.h"

namespace Escargot {

// An expression statement, i.e., a statement consisting of a single expression.
class ExpressionStatementNode : public StatementNode {
public:
    explicit ExpressionStatementNode(Node* expression)
        : StatementNode()
        , m_expression(expression)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ExpressionStatement; }
    Node* expression() { return m_expression; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        if (m_expression->type() != Escargot::InitializeParameterExpression) {
            insertBreakpoint(context);
        }
#endif /* ESCARGOT_DEBUGGER */

        if (!context->shouldCareScriptExecutionResult()) {
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

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_expression->iterateChildren(fn);
    }

private:
    Node* m_expression; // expression: Expression;
};
}

#endif
