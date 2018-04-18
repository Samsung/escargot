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

#ifndef SequenceExpressionNode_h
#define SequenceExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

// An sequence expression, i.e., a statement consisting of vector of expressions.
class SequenceExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    SequenceExpressionNode(ExpressionNodeVector&& expressions)
        : ExpressionNode()
    {
        m_expressions = expressions;
    }

    virtual ~SequenceExpressionNode()
    {
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        ByteCodeRegisterIndex r = 0;
        for (size_t i = 0; i < m_expressions.size(); i++) {
            r = m_expressions[i]->getRegister(codeBlock, context);
            m_expressions[i]->generateExpressionByteCode(codeBlock, context, r);
            context->giveUpRegister();
        }
        if (r != dstRegister) {
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r, dstRegister), context, this);
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        for (size_t i = 0; i < m_expressions.size(); i++) {
            m_expressions[i]->iterateChildrenIdentifier(fn);
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::SequenceExpression; }
    const ExpressionNodeVector& expressions() { return m_expressions; }
protected:
    ExpressionNodeVector m_expressions; // expression: Expression;
};
}

#endif
