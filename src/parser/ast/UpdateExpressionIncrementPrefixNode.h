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

#ifndef UpdateExpressionIncrementPrefixNode_h
#define UpdateExpressionIncrementPrefixNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UpdateExpressionIncrementPrefixNode : public ExpressionNode {
public:
    friend class ScriptParser;

    UpdateExpressionIncrementPrefixNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = (ExpressionNode*)argument;
    }
    virtual ~UpdateExpressionIncrementPrefixNode()
    {
        delete m_argument;
    }

    virtual ASTNodeType type() { return ASTNodeType::UpdateExpressionIncrementPrefix; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t resultRegisterExpect = dstRegister;

        m_argument->generateResolveAddressByteCode(codeBlock, context);
        m_argument->generateReferenceResolvedAddressByteCode(codeBlock, context);
        size_t srcIndex = context->getLastRegisterIndex();
        context->giveUpRegister();
        size_t dstIndex = context->getRegister();
        codeBlock->pushCode(ToNumber(ByteCodeLOC(m_loc.index), srcIndex, dstIndex), context, this);
        size_t resultRegisterIndex = context->getLastRegisterIndex();
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), resultRegisterIndex, resultRegisterIndex), context, this);
        context->giveUpRegister();
        m_argument->generateStoreByteCode(codeBlock, context, resultRegisterIndex, false);
        if (resultRegisterIndex != dstRegister) {
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), resultRegisterIndex, dstRegister), context, this);
        }
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
        size_t dst = context->getRegister();
        codeBlock->pushCode(Increment(ByteCodeLOC(m_loc.index), src, dst), context, this);
        m_argument->generateStoreByteCode(codeBlock, context, dst);
        context->giveUpRegister();
    }


protected:
    ExpressionNode* m_argument;
};
}

#endif
