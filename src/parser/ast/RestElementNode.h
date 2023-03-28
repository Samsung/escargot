/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef RestElementNode_h
#define RestElementNode_h

#include "IdentifierNode.h"
#include "Node.h"

namespace Escargot {

class RestElementNode : public ExpressionNode {
public:
    RestElementNode(Node* argument)
        : ExpressionNode()
        , m_argument(argument)
    {
    }

    RestElementNode(Node* argument, NodeLOC& loc)
        : ExpressionNode()
        , m_argument(argument)
    {
        m_loc = loc;
    }

    virtual ASTNodeType type() override { return ASTNodeType::RestElement; }
    Node* argument()
    {
        return m_argument;
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        // srcRegister indicates iterator or EnumerateObject
        size_t restElementRegister = m_argument->getRegister(codeBlock, context);
        m_argument->generateResolveAddressByteCode(codeBlock, context);
        codeBlock->pushCode(BindingRestElement(ByteCodeLOC(m_loc.index), srcRegister, restElementRegister), context, this->m_loc.index);
        m_argument->generateStoreByteCode(codeBlock, context, restElementRegister, false);
        context->giveUpRegister();
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t restElementRegister = m_argument->getRegister(codeBlock, context);
        codeBlock->pushCode(CreateRestElement(ByteCodeLOC(m_loc.index), restElementRegister), context, this->m_loc.index);
        m_argument->generateResolveAddressByteCode(codeBlock, context);
        m_argument->generateStoreByteCode(codeBlock, context, restElementRegister, false);
        context->giveUpRegister();
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_argument->iterateChildren(fn);
    }

protected:
    Node* m_argument;
};
} // namespace Escargot

#endif
