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

#ifndef NewExpressionNode_h
#define NewExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class NewExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    NewExpressionNode(Node* callee, ArgumentVector&& arguments)
        : ExpressionNode()
    {
        m_callee = callee;
        m_arguments = arguments;
    }

    virtual ~NewExpressionNode()
    {
        delete m_callee;
        for (size_t i = 0; i < m_arguments.size(); i++) {
            delete m_arguments[i];
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::NewExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t callee = m_callee->getRegister(codeBlock, context);
        m_callee->generateExpressionByteCode(codeBlock, context, callee);

        size_t argumentsStartIndex = context->getRegister();
        context->giveUpRegister();

        if (m_arguments.size() == 1) {
            argumentsStartIndex = m_arguments[0]->getRegister(codeBlock, context);
            m_arguments[0]->generateExpressionByteCode(codeBlock, context, argumentsStartIndex);
        } else {
            for (size_t i = 0; i < m_arguments.size(); i++) {
                size_t registerExpect = context->getRegister();
                m_arguments[i]->generateExpressionByteCode(codeBlock, context, registerExpect);
            }
        }

        for (size_t i = 0; i < m_arguments.size(); i++) {
            context->giveUpRegister();
        }

        // give up callee index
        context->giveUpRegister();

        codeBlock->pushCode(NewOperation(ByteCodeLOC(m_loc.index), callee, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);

        codeBlock->m_shouldClearStack = true;
    }

protected:
    Node* m_callee;
    ArgumentVector m_arguments;
};
}

#endif
