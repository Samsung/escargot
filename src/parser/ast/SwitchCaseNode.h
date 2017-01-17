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

#ifndef SwitchCaseNode_h
#define SwitchCaseNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class SwitchCaseNode : public StatementNode {
public:
    friend class ScriptParser;
    friend class SwitchStatementNode;
    SwitchCaseNode(Node* test, StatementNodeVector&& consequent)
        : StatementNode()
    {
        m_test = (ExpressionNode*)test;
        m_consequent = consequent;
    }

    virtual ~SwitchCaseNode()
    {
        delete m_test;
        size_t len = m_consequent.size();
        for (size_t i = 0; i < len; i++) {
            delete m_consequent[i];
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::SwitchCase; }
    bool isDefaultNode()
    {
        return !m_test;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t len = m_consequent.size();
        for (size_t i = 0; i < len; i++) {
            m_consequent[i]->generateStatementByteCode(codeBlock, context);
        }
    }

protected:
    ExpressionNode* m_test;
    StatementNodeVector m_consequent;
};
}

#endif
