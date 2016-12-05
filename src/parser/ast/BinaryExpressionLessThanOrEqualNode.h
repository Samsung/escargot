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

#ifndef BinaryExpressionLessThanOrEqualNode_h
#define BinaryExpressionLessThanOrEqualNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionLessThanOrEqualNode : public ExpressionNode {
public:
    friend class ScriptParser;

    BinaryExpressionLessThanOrEqualNode(Node *left, Node* right)
        : ExpressionNode()
    {
        m_left = (ExpressionNode*)left;
        m_right = (ExpressionNode*)right;
    }

    virtual ASTNodeType type() { return ASTNodeType::BinaryExpressionLessThanOrEqual; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        m_left->generateExpressionByteCode(codeBlock, context);
        m_right->generateExpressionByteCode(codeBlock, context);

        size_t src1 = context->getLastRegisterIndex();
        context->giveUpRegister();
        size_t src0 = context->getLastRegisterIndex();

        codeBlock->pushCode(BinaryLessThanOrEqual(ByteCodeLOC(m_loc.index), src0, src1), context, this);
    }
protected:
    ExpressionNode* m_left;
    ExpressionNode* m_right;
};

}

#endif
