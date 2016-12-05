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

#ifndef WhileStatementNode_h
#define WhileStatementNode_h

#include "StatementNode.h"
#include "ExpressionNode.h"

namespace Escargot {

class WhileStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    WhileStatementNode(Node *test, Node *body)
        : StatementNode()
    {
        m_test = (ExpressionNode*) test;
        m_body = (StatementNode*) body;
    }


    virtual ASTNodeType type() { return ASTNodeType::WhileStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ByteCodeGenerateContext newContext(*context);

        size_t whileStart = codeBlock->currentCodeSize();
        m_test->generateExpressionByteCode(codeBlock, &newContext);

        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), newContext.getLastRegisterIndex()), &newContext, this);
        size_t testPos = codeBlock->lastCodePosition<JumpIfFalse>();

        m_body->generateStatementByteCode(codeBlock, &newContext);

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), whileStart), &newContext, this);
        newContext.consumeContinuePositions(codeBlock, whileStart);
        size_t whileEnd = codeBlock->currentCodeSize();
        newContext.consumeBreakPositions(codeBlock, whileEnd);
        codeBlock->peekCode<JumpIfFalse>(testPos)->m_jumpPosition = whileEnd;
        newContext.m_positionToContinue = context->m_positionToContinue;
        newContext.propagateInformationTo(*context);
    }

protected:
    ExpressionNode *m_test;
    StatementNode *m_body;
};

}

#endif
