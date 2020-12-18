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

#ifndef UpdateExpressionIncrementPrefixNode_h
#define UpdateExpressionIncrementPrefixNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UpdateExpressionIncrementPrefixNode : public ExpressionNode {
public:
    explicit UpdateExpressionIncrementPrefixNode(Node* argument)
        : ExpressionNode()
        , m_argument(argument)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::UpdateExpressionIncrementPrefix; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        m_argument->generateResolveAddressByteCode(codeBlock, context);
        m_argument->generateReferenceResolvedAddressByteCode(codeBlock, context);
        size_t srcIndex = context->getLastRegisterIndex();
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), srcIndex, dstRegister), context, this);
        context->giveUpRegister();
        m_argument->generateStoreByteCode(codeBlock, context, dstRegister, false);
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (m_argument->isIdentifier()) {
            auto r = m_argument->asIdentifier()->isAllocatedOnStack(context);
            if (std::get<0>(r)) {
                codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), std::get<1>(r), std::get<1>(r)), context, this);
                return;
            }
        }
        size_t src = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, src);
        context->giveUpRegister();
        size_t dst = m_argument->getRegister(codeBlock, context);
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), src, dst), context, this);
        m_argument->generateStoreByteCode(codeBlock, context, dst, true);
        context->giveUpRegister();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_argument->iterateChildrenIdentifierAssigmentCase(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_argument->iterateChildren(fn);
    }

private:
    Node* m_argument;
};
} // namespace Escargot

#endif
