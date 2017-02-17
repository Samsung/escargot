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

#ifndef UnaryExpressionTypeOfNode_h
#define UnaryExpressionTypeOfNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionTypeOfNode : public ExpressionNode {
public:
    friend class ScriptParser;
    UnaryExpressionTypeOfNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = argument;
    }
    virtual ~UnaryExpressionTypeOfNode()
    {
        delete m_argument;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_argument->isIdentifier()) {
            AtomicString name = m_argument->asIdentifier()->name();
            bool nameCase = false;
            if (!context->m_codeBlock->canUseIndexedVariableStorage()) {
                nameCase = true;
            } else {
                CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(name);
                if (!info.m_isResultSaved) {
                    nameCase = true;
                }
            }
            if (nameCase) {
                if (name.string()->equals("arguments") && !context->isGlobalScope()) {
                    m_argument->generateExpressionByteCode(codeBlock, context);
                    size_t srcIndex = context->getRegister();
                    context->giveUpRegister();
                    size_t dstIndex = context->getRegister();
                    codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), srcIndex, dstIndex, AtomicString()), context, this);
                } else {
                    codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), SIZE_MAX, context->getRegister(), name), context, this);
                }
                return;
            }
        }

        m_argument->generateExpressionByteCode(codeBlock, context);
        size_t srcIndex = context->getLastRegisterIndex();
        context->giveUpRegister();
        size_t dstIndex = context->getRegister();
        codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), srcIndex, dstIndex, AtomicString()), context, this);
    }

    virtual ASTNodeType type() { return ASTNodeType::UnaryExpressionTypeOf; }
protected:
    Node* m_argument;
};
}

#endif
