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

#ifndef YieldExpressionNode_h
#define YieldExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class YieldExpressionNode : public ExpressionNode {
public:
    YieldExpressionNode(Node* argument, bool isDelegate)
        : ExpressionNode()
        , m_argument(argument)
        , m_isDelegate(isDelegate)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::YieldExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        codeBlock->updateMaxPauseStatementExtraDataLength(context);
        size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));

        if (m_argument == nullptr) {
            ExecutionPause::ExecutionPauseYieldData data;
            data.m_yieldIndex = REGISTER_LIMIT;
            data.m_dstIndex = dstRegister;
            data.m_tailDataLength = tailDataLength;
            codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
            codeBlock->pushPauseStatementExtraData(context);
        } else {
            size_t argIdx = m_argument->getRegister(codeBlock, context);
            m_argument->generateExpressionByteCode(codeBlock, context, argIdx);

            if (m_isDelegate) {
                size_t iteratorIdx = context->getRegister();
                codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index), argIdx, iteratorIdx), context, this);
                size_t loopStart = codeBlock->currentCodeSize();

                size_t valueIdx = context->getRegister();

                ExecutionPause::ExecutionPauseYieldDelegateData data;
                data.m_iterIntex = iteratorIdx;
                data.m_valueIndex = valueIdx;
                data.m_dstIndex = dstRegister;
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
                size_t iterStepPos = codeBlock->lastCodePosition<ExecutionPause>();

                codeBlock->pushPauseStatementExtraData(context);

                context->giveUpRegister(); // for drop valueIdx

                codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), loopStart), context, this);
                codeBlock->peekCode<ExecutionPause>(iterStepPos)->m_yieldDelegateData.m_endPosition = codeBlock->currentCodeSize();
                context->giveUpRegister(); // for drop iteratorIdx
            } else {
                ExecutionPause::ExecutionPauseYieldData data;
                data.m_yieldIndex = argIdx;
                data.m_dstIndex = dstRegister;
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
                codeBlock->pushPauseStatementExtraData(context);
            }

            context->giveUpRegister(); // for drop argIdx
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_argument) {
            m_argument->iterateChildren(fn);
        }
    }

private:
    Node* m_argument;
    bool m_isDelegate : 1;
};
}

#endif
