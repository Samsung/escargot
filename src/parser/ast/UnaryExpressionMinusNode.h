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

#ifndef UnaryExpressionMinusNode_h
#define UnaryExpressionMinusNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionMinusNode : public ExpressionNode {
public:
    explicit UnaryExpressionMinusNode(Node* argument)
        : ExpressionNode()
        , m_argument(argument)
    {
    }
    virtual ~UnaryExpressionMinusNode()
    {
    }


    virtual ASTNodeType type() override { return ASTNodeType::UnaryExpressionMinus; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        size_t srcIndex = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, srcIndex);
        context->giveUpRegister();
        codeBlock->pushCode(UnaryMinus(ByteCodeLOC(m_loc.index), srcIndex, dstRegister), context, this);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_argument->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_argument->iterateChildren(fn);
    }

private:
    Node* m_argument;
};
}

#endif
