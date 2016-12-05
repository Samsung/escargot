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
#include "PatternNode.h"
#include "MemberExpressionNode.h"

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
            // TODO
            // process eval
            RELEASE_ASSERT_NOT_REACHED();
        }

        // TODO
        RELEASE_ASSERT(!m_callee->isMemberExpression());

        m_callee->generateExpressionByteCode(codeBlock, context);
        size_t baseRegister = context->getLastRegisterIndex();

        for (size_t i = 0; i < m_arguments.size(); i ++) {
            m_arguments[i]->generateExpressionByteCode(codeBlock, context);
        }

        for (size_t i = 0; i < m_arguments.size(); i ++) {
            context->giveUpRegister();
        }

        codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), baseRegister, m_arguments.size()), context, this);
    }

protected:
    Node* m_callee; // callee: Expression;
    ArgumentVector m_arguments; // arguments: [ Expression ];
};

}

#endif
