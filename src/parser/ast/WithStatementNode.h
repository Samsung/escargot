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

#ifndef WithStatementNode_h
#define WithStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class WithStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    WithStatementNode(Node* object, StatementNode* body)
        : StatementNode()
    {
        m_object = object;
        m_body = body;
    }

    virtual ASTNodeType type() { return ASTNodeType::WithStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t start = codeBlock->currentCodeSize();
        context->m_tryStatementScopeCount++;
        m_object->generateExpressionByteCode(codeBlock, context);
        size_t withPos = codeBlock->currentCodeSize();
        codeBlock->pushCode(WithOperation(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex()), context, this);
        context->giveUpRegister();

        m_body->generateStatementByteCode(codeBlock, context);
        context->registerJumpPositionsToComplexCase(start);

        codeBlock->pushCode(TryCatchWithBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
        codeBlock->peekCode<WithOperation>(withPos)->m_withEndPostion = codeBlock->currentCodeSize();

        context->m_tryStatementScopeCount--;
    }

protected:
    Node* m_object;
    StatementNode* m_body; // body: [ Statement ];
};
}

#endif
