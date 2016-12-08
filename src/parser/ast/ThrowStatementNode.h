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

#ifndef ThrowStatementNode_h
#define ThrowStatementNode_h

#include "StatementNode.h"

namespace Escargot {

// interface ThrowStatement <: Statement {
class ThrowStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ThrowStatementNode(Node* argument)
        : StatementNode()
    {
        m_argument = argument;
    }

    virtual ASTNodeType type() { return ASTNodeType::ThrowStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        m_argument->generateExpressionByteCode(codeBlock, context);
        codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex()), context, this);
        context->giveUpRegister();
    }

protected:
    Node* m_argument;
};
}

#endif
