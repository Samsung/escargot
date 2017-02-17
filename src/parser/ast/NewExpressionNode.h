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
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool canUseDisalignedRegisterBefore = context->m_canUseDisalignedRegister;
        context->m_canUseDisalignedRegister = false;

        m_callee->generateExpressionByteCode(codeBlock, context);
        size_t base = context->getLastRegisterIndex();

        for (size_t i = 0; i < m_arguments.size(); i++) {
            m_arguments[i]->generateExpressionByteCode(codeBlock, context);
        }

        codeBlock->pushCode(NewOperation(ByteCodeLOC(m_loc.index), base, m_arguments.size()), context, this);

        for (size_t i = 0; i < m_arguments.size(); i++) {
            context->giveUpRegister();
        }

        codeBlock->m_shouldClearStack = true;
        context->m_canUseDisalignedRegister = canUseDisalignedRegisterBefore;
    }

protected:
    Node* m_callee;
    ArgumentVector m_arguments;
};
}

#endif
