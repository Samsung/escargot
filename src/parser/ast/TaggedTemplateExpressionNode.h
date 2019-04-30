/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef TaggedTemplateExpressionNode_h
#define TaggedTemplateExpressionNode_h

#include "Node.h"
#include "TemplateLiteralNode.h"

namespace Escargot {

class TaggedTemplateExpressionNode : public ExpressionNode {
public:
    TaggedTemplateExpressionNode(Node* expr, Node* quasi)
        : ExpressionNode()
        , m_expr(expr)
        , m_quasi(quasi)
    {
    }

    Node* expr()
    {
        return m_expr.get();
    }

    Node* quasi()
    {
        return m_quasi.get();
    }

    virtual ASTNodeType type() { return ASTNodeType::TaggedTemplateExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    RefPtr<Node> m_expr;
    RefPtr<Node> m_quasi;
};
}

#endif
