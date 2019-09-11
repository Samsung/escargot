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

#ifndef SpreadElementNode_h
#define SpreadElementNode_h

#include "Node.h"

namespace Escargot {

class SpreadElementNode : public ExpressionNode {
public:
    explicit SpreadElementNode(Node* arg)
        : ExpressionNode()
        , m_argument(arg)
    {
    }

    virtual ~SpreadElementNode()
    {
    }

    virtual ASTNodeType type() override { return SpreadElement; }
    Node* argument()
    {
        return m_argument.get();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        ByteCodeRegisterIndex argumentIndex = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, argumentIndex);
        codeBlock->pushCode(CreateSpreadArrayObject(ByteCodeLOC(m_loc.index), dstRegister, argumentIndex), context, this);
        context->giveUpRegister();
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_argument->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_argument;
};
}

#endif
