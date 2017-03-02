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

#ifndef ThisExpressionNode_h
#define ThisExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ThisExpressionNode : public ExpressionNode {
public:
    ThisExpressionNode()
        : ExpressionNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ThisExpression; }
    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (context->m_canUseDisalignedRegister) {
            size_t i = context->m_codeBlock->thisSymbolIndex();
            context->pushRegister(REGULAR_REGISTER_LIMIT + i);
            return REGULAR_REGISTER_LIMIT + i;
        } else {
            return context->getRegister();
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t i = context->m_codeBlock->thisSymbolIndex();
        if (dstRegister != REGULAR_REGISTER_LIMIT + i) {
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + i, dstRegister), context, this);
        }
    }

protected:
};
}

#endif
