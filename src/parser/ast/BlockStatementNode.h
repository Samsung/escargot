/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef BlockStatementNode_h
#define BlockStatementNode_h

#include "StatementNode.h"

namespace Escargot {

// A block statement, i.e., a sequence of statements surrounded by braces.
class BlockStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    BlockStatementNode(StatementNodeVector&& body)
        : StatementNode()
    {
        m_body = body;
    }

    virtual ~BlockStatementNode()
    {
        size_t len = m_body.size();
        for (size_t i = 0; i < len; i++) {
            delete m_body[i];
        }
        m_body.clear();
    }
    virtual ASTNodeType type() { return ASTNodeType::BlockStatement; }
    size_t size() { return m_body.size(); }
    StatementNodeVector& body() { return m_body; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t len = m_body.size();
        for (size_t i = 0; i < len; i++) {
            m_body[i]->generateStatementByteCode(codeBlock, context);
        }
    }

protected:
    StatementNodeVector m_body; // body: [ Statement ];
};
}

#endif
