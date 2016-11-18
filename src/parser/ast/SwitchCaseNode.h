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

#include "StatementNode.h"
#include "ExpressionNode.h"

namespace Escargot {

class SwitchCaseNode : public StatementNode {
public:
    friend class ScriptParser;
    friend class SwitchStatementNode;
    SwitchCaseNode(Node* test, StatementNodeVector&& consequent)
        : StatementNode()
    {
        m_test = (ExpressionNode*) test;
        m_consequent = consequent;
    }

    virtual ASTNodeType type() { return ASTNodeType::SwitchCase; }

    bool isDefaultNode()
    {
        return !m_test;
    }

protected:
    ExpressionNode* m_test;
    StatementNodeVector m_consequent;
};

}

#endif
