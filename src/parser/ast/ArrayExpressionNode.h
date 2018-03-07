/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
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
