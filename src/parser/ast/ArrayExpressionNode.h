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

    virtual ~ArrayExpressionNode()
    {
        for (unsigned i = 0; i < m_elements.size(); i++) {
            delete m_elements[i];
        }
        m_elements.clear();
    }

    virtual ASTNodeType type() { return ASTNodeType::ArrayExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t arrayIndex = codeBlock->currentCodeSize();
        size_t arrLen = 0;
        codeBlock->pushCode(CreateArray(ByteCodeLOC(m_loc.index), dstRegister), context, this);
        size_t objIndex = dstRegister;
        for (unsigned i = 0; i < m_elements.size(); i++) {
            arrLen = i + 1;
            size_t valueIndex;
            if (m_elements[i]) {
                valueIndex = m_elements[i]->getRegister(codeBlock, context);
                m_elements[i]->generateExpressionByteCode(codeBlock, context, valueIndex);
            } else {
                continue;
            }
            codeBlock->pushCode(ArrayDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, i, valueIndex), context, this);
            // drop value register
            context->giveUpRegister();
        }
        codeBlock->peekCode<CreateArray>(arrayIndex)->m_length = arrLen;

        codeBlock->m_shouldClearStack = true;
    }

protected:
    ExpressionNodeVector m_elements;
};
}

#endif
