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

#ifndef SequenceExpressionNode_h
#define SequenceExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

// An sequence expression, i.e., a statement consisting of vector of expressions.
class SequenceExpressionNode : public ExpressionNode {
public:
    explicit SequenceExpressionNode(const NodeList& expressions)
        : ExpressionNode()
        , m_expressions(expressions)
    {
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        ASSERT(m_expressions.size());
        ByteCodeRegisterIndex r = 0;
        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.back(); expression = expression->next()) {
            r = expression->astNode()->getRegister(codeBlock, context);
            expression->astNode()->generateExpressionByteCode(codeBlock, context, r);
            context->giveUpRegister();
        }

        // directly store the result of the last expression on to dstRegister
        m_expressions.back()->astNode()->generateExpressionByteCode(codeBlock, context, dstRegister);
    }

#if defined(ENABLE_TCO)
    virtual void generateTCOExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister, bool& isTailCallForm) override
    {
        ASSERT(m_expressions.size());
        ByteCodeRegisterIndex r = 0;
        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.back(); expression = expression->next()) {
            r = expression->astNode()->getRegister(codeBlock, context);
            expression->astNode()->generateExpressionByteCode(codeBlock, context, r);
            context->giveUpRegister();
        }

        // directly store the result of the last expression on to dstRegister
        m_expressions.back()->astNode()->generateTCOExpressionByteCode(codeBlock, context, dstRegister, isTailCallForm);
    }
#endif

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
            expression->astNode()->iterateChildrenIdentifier(fn);
        }
    }

    virtual ASTNodeType type() override { return ASTNodeType::SequenceExpression; }
    NodeList& expressions() { return m_expressions; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
            expression->astNode()->iterateChildren(fn);
        }
    }

private:
    NodeList m_expressions; // expression: Expression;
};
} // namespace Escargot

#endif
