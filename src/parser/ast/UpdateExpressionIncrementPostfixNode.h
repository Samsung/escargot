/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef UpdateExpressionIncrementPostfixNode_h
#define UpdateExpressionIncrementPostfixNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UpdateExpressionIncrementPostfixNode : public ExpressionNode {
public:
    friend class ScriptParser;

    UpdateExpressionIncrementPostfixNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = (ExpressionNode*)argument;
    }
    virtual ~UpdateExpressionIncrementPostfixNode()
    {
        delete m_argument;
    }

    virtual ASTNodeType type() { return ASTNodeType::UpdateExpressionIncrementPostfix; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        m_argument->generateResolveAddressByteCode(codeBlock, context);
        m_argument->generateReferenceResolvedAddressByteCode(codeBlock, context);
        size_t srcIndex = context->getLastRegisterIndex();
        codeBlock->pushCode(ToNumber(ByteCodeLOC(m_loc.index), srcIndex, dstRegister), context, this);
        size_t tmpR = m_argument->getRegister(codeBlock, context);
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), dstRegister, tmpR), context, this);
        context->giveUpRegister();
        context->giveUpRegister();
        m_argument->generateStoreByteCode(codeBlock, context, tmpR, false);
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_argument->isIdentifier()) {
            auto r = m_argument->asIdentifier()->isAllocatedOnStack(context);
            if (r.first) {
                codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), r.second, r.second), context, this);
                return;
            }
        }
        size_t src = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, src);
        context->giveUpRegister();
        size_t dst = m_argument->getRegister(codeBlock, context);
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), src, dst), context, this);
        m_argument->generateStoreByteCode(codeBlock, context, dst);
        context->giveUpRegister();
    }

protected:
    ExpressionNode* m_argument;
};
}

#endif
