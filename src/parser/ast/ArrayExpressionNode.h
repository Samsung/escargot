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

#ifndef ArrayExpressionNode_h
#define ArrayExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ArrayExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ArrayExpressionNode(ExpressionNodeVector&& elements)
        : ExpressionNode()
    {
        m_elements = elements;
    }

    virtual ASTNodeType type() { return ASTNodeType::ArrayExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        codeBlock->pushCode(CreateArray(ByteCodeLOC(m_loc.index), context->getRegister()), context, this);
        size_t objIndex = context->getLastRegisterIndex();
        for (unsigned i = 0; i < m_elements.size(); i++) {
            if (m_elements[i]) {
                m_elements[i]->generateExpressionByteCode(codeBlock, context);
            } else {
                continue;
            }
            size_t valueIndex = context->getLastRegisterIndex();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value(i)), context, this);
            size_t property = context->getLastRegisterIndex();
            codeBlock->pushCode(SetObject(ByteCodeLOC(m_loc.index), objIndex, property, valueIndex), context, this);
            // drop property, value register
            context->giveUpRegister();
            context->giveUpRegister();
        }
        ASSERT(objIndex == context->getLastRegisterIndex());
    }

protected:
    ExpressionNodeVector m_elements;
};
}

#endif
