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
        , m_arg(arg)
    {
    }

    virtual ~SpreadElementNode()
    {
    }

    virtual ASTNodeType type()
    {
        return SpreadElement;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t spreadIndex = m_arg->getRegister(codeBlock, context);
        m_arg->generateExpressionByteCode(codeBlock, context, spreadIndex);
        codeBlock->pushCode(CreateSpreadObject(ByteCodeLOC(m_loc.index), dstRegister, spreadIndex), context, this);
        context->giveUpRegister();
    }

private:
    RefPtr<Node> m_arg;
};
}

#endif
