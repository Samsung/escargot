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

#ifndef CallExpressionNode_h
#define CallExpressionNode_h

#include "ExpressionNode.h"
#include "MemberExpressionNode.h"
#include "PatternNode.h"

namespace Escargot {

class CallExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    CallExpressionNode(Node* callee, ArgumentVector&& arguments)
        : ExpressionNode()
    {
        m_callee = callee;
        m_arguments = arguments;
    }

    Node* callee() { return m_callee; }
    virtual ASTNodeType type() { return ASTNodeType::CallExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_callee->isIdentifier() && m_callee->asIdentifier()->name().string()->equals("eval")) {
            bool prevInCallingExpressionScope = context->m_inCallingExpressionScope;
            for (size_t i = 0; i < m_arguments.size(); i++) {
                m_arguments[i]->generateExpressionByteCode(codeBlock, context);
            }
            for (size_t i = 0; i < m_arguments.size(); i++) {
                context->giveUpRegister();
            }
            size_t baseRegister = context->getRegister();
            codeBlock->pushCode(CallEvalFunction(ByteCodeLOC(m_loc.index), baseRegister, m_arguments.size()), context, this);
            return;
        }

        bool prevInCallingExpressionScope = context->m_inCallingExpressionScope;
        if (m_callee->isMemberExpression()) {
            context->m_inCallingExpressionScope = true;
            context->m_isHeadOfMemberExpression = true;
        }
        if (!m_callee->isMemberExpression()) {
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value()), context, this);
        }
        m_callee->generateExpressionByteCode(codeBlock, context);
        size_t baseRegister = context->getLastRegisterIndex() - 1;
        context->m_inCallingExpressionScope = false;

        for (size_t i = 0; i < m_arguments.size(); i++) {
            m_arguments[i]->generateExpressionByteCode(codeBlock, context);
        }
        for (size_t i = 0; i < m_arguments.size(); i++) {
            context->giveUpRegister();
        }

        context->m_inCallingExpressionScope = prevInCallingExpressionScope;
        codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), baseRegister, m_arguments.size()), context, this);

        // drop callee index;
        context->giveUpRegister();
    }

protected:
    Node* m_callee; // callee: Expression;
    ArgumentVector m_arguments; // arguments: [ Expression ];
};
}

#endif
