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

    virtual ~CallExpressionNode()
    {
        delete m_callee;
        for (size_t i = 0; i < m_arguments.size(); i++) {
            delete m_arguments[i];
        }
        m_arguments.clear();
    }
    Node* callee() { return m_callee; }
    virtual ASTNodeType type() { return ASTNodeType::CallExpression; }
    void generateArguments(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool clearInCallingExpressionScope = true)
    {
        context->m_inCallingExpressionScope = !clearInCallingExpressionScope;

        for (size_t i = 0; i < m_arguments.size(); i++) {
            size_t registerExpect = context->getRegister();
            context->giveUpRegister();

            m_arguments[i]->generateExpressionByteCode(codeBlock, context);
            size_t r = context->getLastRegisterIndex();
            if (r != registerExpect) {
                context->giveUpRegister();
                size_t newR = context->getRegister();
                ASSERT(newR == registerExpect);
                codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r, newR), context, this);
            }
        }

        for (size_t i = 0; i < m_arguments.size(); i++) {
            context->giveUpRegister();
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_callee->isIdentifier() && m_callee->asIdentifier()->name().string()->equals("eval")) {
            generateArguments(codeBlock, context, false);
            size_t baseRegister = context->getRegister();
            codeBlock->pushCode(CallEvalFunction(ByteCodeLOC(m_loc.index), baseRegister, m_arguments.size()), context, this);
            return;
        }

        bool prevInCallingExpressionScope = context->m_inCallingExpressionScope;
        bool useWithObjectAsReceiver = context->m_isWithScope && m_callee->isIdentifier();
        if (UNLIKELY(useWithObjectAsReceiver)) {
            // CallFunction should check whether receiver is global obj or with obj.
            ASSERT(m_callee->isIdentifier());
            AtomicString calleeName = m_callee->asIdentifier()->name();
            generateArguments(codeBlock, context);
            context->m_inCallingExpressionScope = prevInCallingExpressionScope;
            codeBlock->pushCode(CallFunctionInWithScope(ByteCodeLOC(m_loc.index), context->getRegister(), calleeName, m_arguments.size()), context, this);
            return;
        }

        size_t baseRegister = context->getRegister();
        context->giveUpRegister();

        if (m_callee->isMemberExpression()) {
            context->m_inCallingExpressionScope = true;
            context->m_isHeadOfMemberExpression = true;
        } else {
            // NOTE: receiver is always global Object => undefined
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value()), context, this);
        }

        size_t calleeExpect = baseRegister + 1;

        m_callee->generateExpressionByteCode(codeBlock, context);

        size_t r = context->getLastRegisterIndex();
        if (r != calleeExpect && !m_callee->isMemberExpression()) {
            context->giveUpRegister();
            size_t callee = context->getRegister();
            ASSERT(callee == calleeExpect);
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r, callee), context, this);
        }

        generateArguments(codeBlock, context);
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
