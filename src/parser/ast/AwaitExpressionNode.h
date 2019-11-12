/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef AwaitExpressionNode_h
#define AwaitExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class AwaitExpressionNode : public ExpressionNode {
public:
    AwaitExpressionNode(Node* argument)
        : ExpressionNode()
        , m_argument(argument)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::AwaitExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        m_argument->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->updateMaxPauseStatementExtraDataLength(context);
        size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));

        ExecutionPause::ExecutionPauseAwaitData data;
        data.m_awaitIndex = dstRegister;
        data.m_dstIndex = dstRegister;
        data.m_tailDataLength = tailDataLength;
        codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
        codeBlock->pushPauseStatementExtraData(context);
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
