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

#ifndef DirectiveNode_h
#define DirectiveNode_h

#include "ExpressionNode.h"
#include "Node.h"

namespace Escargot {

class DirectiveNode : public StatementNode {
public:
    DirectiveNode(Node* expr)
        : StatementNode()
        , m_expr(expr)
    {
    }

    virtual ~DirectiveNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::Directive; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        m_expr->generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_expr->iterateChildren(fn);
    }

private:
    Node* m_expr;
};
}

#endif
