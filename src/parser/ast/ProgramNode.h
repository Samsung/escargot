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

#ifndef ProgramNode_h
#define ProgramNode_h

#include "Node.h"
#include "StatementNode.h"

namespace Escargot {

class ProgramNode : public Node {
public:
    friend class ScriptParser;
    ProgramNode(StatementNodeVector&& body, ASTScopeContext* scopeContext)
        : Node()
    {
        m_body = body;
        m_scopeContext = scopeContext;
        m_scopeContext->m_associateNode = this;
    }

    virtual ~ProgramNode()
    {
        size_t len = m_body.size();

        for (size_t i = 0; i < len; i++) {
            delete m_body[i];
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::Program; }
    ASTScopeContext* scopeContext() { return m_scopeContext; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        codeBlock->pushCode(ResetExecuteResult(ByteCodeLOC(SIZE_MAX)), context, this);
        size_t len = m_body.size();

        for (size_t i = 0; i < len; i++) {
            m_body[i]->generateStatementByteCode(codeBlock, context);
        }
        codeBlock->pushCode(End(ByteCodeLOC(SIZE_MAX)), context, this);
    }

protected:
    StatementNodeVector m_body; // body: [ Statement ];
    ASTScopeContext* m_scopeContext;
};
}

#endif
