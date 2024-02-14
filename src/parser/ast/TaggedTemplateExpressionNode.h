/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef TaggedTemplateExpressionNode_h
#define TaggedTemplateExpressionNode_h

#include "Node.h"
#include "TemplateLiteralNode.h"

namespace Escargot {

class TaggedTemplateExpressionNode : public ExpressionNode {
public:
    TaggedTemplateExpressionNode(Node* expr, Node* quasi, Node* convertedExpression)
        : ExpressionNode()
        , m_expr(expr)
        , m_quasi(quasi)
        , m_convertedExpression(convertedExpression)
    {
        ASSERT(convertedExpression->type() == CallExpression);
    }

    Node* expr()
    {
        return m_expr;
    }

    Node* quasi()
    {
        return m_quasi;
    }

    virtual ASTNodeType type() override { return ASTNodeType::TaggedTemplateExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        return m_convertedExpression->generateExpressionByteCode(codeBlock, context, dstRegister);
    }

#if defined(ENABLE_TCO)
    virtual void generateTCOExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister, bool& isTailCallForm) override
    {
        return m_convertedExpression->generateTCOExpressionByteCode(codeBlock, context, dstRegister, isTailCallForm);
    }
#endif

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_expr->iterateChildren(fn);
        m_quasi->iterateChildren(fn);
        m_convertedExpression->iterateChildren(fn);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_expr->iterateChildrenIdentifier(fn);
        m_quasi->iterateChildrenIdentifier(fn);
        m_convertedExpression->iterateChildrenIdentifier(fn);
    }

private:
    Node* m_expr;
    Node* m_quasi;
    Node* m_convertedExpression;
};
} // namespace Escargot

#endif
