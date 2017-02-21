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

#ifndef ForStatementNode_h
#define ForStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class ForStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ForStatementNode(Node *init, Node *test, Node *update, Node *body)
        : StatementNode()
    {
        m_init = (ExpressionNode *)init;
        m_test = (ExpressionNode *)test;
        m_update = (ExpressionNode *)update;
        m_body = (StatementNode *)body;
    }

    virtual ~ForStatementNode()
    {
        delete m_init;
        delete m_test;
        delete m_update;
        delete m_body;
    }

    virtual ASTNodeType type() { return ASTNodeType::ForStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test

        if (m_init) {
            m_init->generateStatementByteCode(codeBlock, &newContext);
        }

        size_t forStart = codeBlock->currentCodeSize();

        if (m_test) {
            m_test->generateExpressionByteCode(codeBlock, &newContext);
        } else {
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), newContext.getRegister(), Value(true)), &newContext, this);
        }

        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), newContext.getLastRegisterIndex()), &newContext, this);
        size_t testPos = codeBlock->lastCodePosition<JumpIfFalse>();

        newContext.giveUpRegister();
        newContext.giveUpRegister();

        m_body->generateStatementByteCode(codeBlock, &newContext);

        size_t updatePosition = codeBlock->currentCodeSize();
        if (m_update) {
            if (!context->m_isEvalCode && !context->m_isGlobalScope && m_update->isUpdateExpression()) {
                m_update->generateResultNotRequiredExpressionByteCode(codeBlock, &newContext);
            } else {
                m_update->generateExpressionByteCode(codeBlock, &newContext);
                newContext.giveUpRegister();
            }
        }
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), forStart), &newContext, this);

        size_t forEnd = codeBlock->currentCodeSize();
        codeBlock->peekCode<JumpIfFalse>(testPos)->m_jumpPosition = forEnd;

        newContext.consumeBreakPositions(codeBlock, forEnd);
        newContext.consumeContinuePositions(codeBlock, updatePosition);
        newContext.m_positionToContinue = updatePosition;
        newContext.propagateInformationTo(*context);
    }

protected:
    ExpressionNode *m_init;
    ExpressionNode *m_test;
    ExpressionNode *m_update;
    StatementNode *m_body;
};
}

#endif
