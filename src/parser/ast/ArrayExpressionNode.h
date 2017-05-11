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
        for (size_t i = 0; i < m_elements.size(); i += ARRAY_DEFINE_OPERATION_MERGE_COUNT) {
            size_t fillCount = 0;
            size_t regCount = 0;
            ByteCodeRegisterIndex regs[ARRAY_DEFINE_OPERATION_MERGE_COUNT];
            for (size_t j = 0; j < ARRAY_DEFINE_OPERATION_MERGE_COUNT && ((i + j) < m_elements.size()); j++) {
                arrLen = j + i + 1;

                ByteCodeRegisterIndex valueIndex = std::numeric_limits<ByteCodeRegisterIndex>::max();
                if (m_elements[i + j]) {
                    valueIndex = m_elements[i + j]->getRegister(codeBlock, context);
                    m_elements[i + j]->generateExpressionByteCode(codeBlock, context, valueIndex);
                    regCount++;
                }
                fillCount++;
                regs[j] = valueIndex;
            }
            codeBlock->pushCode(ArrayDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, i, fillCount), context, this);
            memcpy(codeBlock->peekCode<ArrayDefineOwnPropertyOperation>(codeBlock->lastCodePosition<ArrayDefineOwnPropertyOperation>())->m_loadRegisterIndexs,
                   regs, sizeof(regs));
            for (size_t j = 0; j < regCount; j++) {
                // drop value register
                context->giveUpRegister();
            }
        }
        codeBlock->peekCode<CreateArray>(arrayIndex)->m_length = arrLen;

        codeBlock->m_shouldClearStack = true;
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name)>& fn)
    {
        for (size_t i = 0; i < m_elements.size(); i++) {
            if (m_elements[i])
                m_elements[i]->iterateChildrenIdentifier(fn);
        }
    }

protected:
    ExpressionNodeVector m_elements;
};
}

#endif
