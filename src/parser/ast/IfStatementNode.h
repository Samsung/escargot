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

#ifndef IfStatementNode_h
#define IfStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class IfStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    IfStatementNode(Node* test, Node* consequente, Node* alternate)
        : StatementNode()
    {
        m_test = (ExpressionNode*)test;
        m_consequente = (StatementNode*)consequente;
        m_alternate = (StatementNode*)alternate;
    }

    virtual ~IfStatementNode()
    {
        delete m_test;
        delete m_consequente;
        delete m_alternate;
    }

    virtual ASTNodeType type() { return ASTNodeType::IfStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->getRegister(); // ExeuctionResult of m_consequente|m_alternate should not be overwritten by m_test
        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this);
        context->giveUpRegister();
        context->giveUpRegister();
        size_t jPos = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateStatementByteCode(codeBlock, context);
        size_t jPos2 = 0;
        if (m_alternate) {
            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index)), context, this);
            jPos2 = codeBlock->lastCodePosition<Jump>();
        }
        JumpIfFalse* j = codeBlock->peekCode<JumpIfFalse>(jPos);
        j->m_jumpPosition = codeBlock->currentCodeSize();

        if (!m_alternate) {
            if (context->m_isEvalCode || context->m_isGlobalScope) {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value()), context, this);
                context->giveUpRegister();
            }
        } else {
            m_alternate->generateStatementByteCode(codeBlock, context);
            Jump* j2 = codeBlock->peekCode<Jump>(jPos2);
            j2->m_jumpPosition = codeBlock->currentCodeSize();
        }
    }

protected:
    ExpressionNode* m_test;
    StatementNode* m_consequente;
    StatementNode* m_alternate;
};
}

#endif
