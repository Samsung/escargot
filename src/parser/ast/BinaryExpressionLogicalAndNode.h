/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef BinaryExpressionLogicalAndNode_h
#define BinaryExpressionLogicalAndNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionLogicalAndNode : public ExpressionNode {
public:
    BinaryExpressionLogicalAndNode(Node* left, Node* right)
        : ExpressionNode()
    {
        m_left = (ExpressionNode*)left;
        m_right = (ExpressionNode*)right;
    }

    virtual ~BinaryExpressionLogicalAndNode()
    {
        delete m_left;
        delete m_right;
    }

    virtual ASTNodeType type() { return ASTNodeType::BinaryExpressionLogicalAnd; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t resultRegisterExpected = context->getRegister();
        context->giveUpRegister();
        m_left->generateExpressionByteCode(codeBlock, context);

        size_t r = context->getLastRegisterIndex();
        if (resultRegisterExpected != r) {
            context->giveUpRegister();
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r, context->getRegister()), context, this);
        }
        codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex()), context, this);
        size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();
        context->giveUpRegister();

        m_right->generateExpressionByteCode(codeBlock, context);

        r = context->getLastRegisterIndex();
        if (resultRegisterExpected != r) {
            context->giveUpRegister();
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r, context->getRegister()), context, this);
        }

        codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
    }

protected:
    ExpressionNode* m_left;
    ExpressionNode* m_right;
};
}

#endif
