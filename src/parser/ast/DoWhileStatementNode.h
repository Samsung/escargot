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

#ifndef DoWhileStatementNode_h
#define DoWhileStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class DoWhileStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    DoWhileStatementNode(Node *test, Node *body)
        : StatementNode()
    {
        m_test = (ExpressionNode *)test;
        m_body = (StatementNode *)body;
    }

    virtual ~DoWhileStatementNode()
    {
        delete m_test;
        delete m_body;
    }

    virtual ASTNodeType type() { return ASTNodeType::DoWhileStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        ByteCodeGenerateContext newContext(*context);

        size_t doStart = codeBlock->currentCodeSize();
        m_body->generateStatementByteCode(codeBlock, &newContext);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test
        size_t testPos = codeBlock->currentCodeSize();
        m_test->generateExpressionByteCode(codeBlock, &newContext);
        codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), newContext.getLastRegisterIndex(), doStart), &newContext, this);

        newContext.giveUpRegister();
        newContext.giveUpRegister();

        size_t doEnd = codeBlock->currentCodeSize();
        newContext.consumeContinuePositions(codeBlock, testPos);
        newContext.consumeBreakPositions(codeBlock, doEnd);
        newContext.m_positionToContinue = testPos;
        newContext.propagateInformationTo(*context);
    }

protected:
    ExpressionNode *m_test;
    StatementNode *m_body;
};
}

#endif
