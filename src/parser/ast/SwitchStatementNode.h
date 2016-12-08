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

#ifndef SwitchStatementNode_h
#define SwitchStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "SwitchCaseNode.h"

namespace Escargot {

class SwitchStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    SwitchStatementNode(Node* discriminant, StatementNodeVector&& casesA, Node* deflt, StatementNodeVector&& casesB, bool lexical)
        : StatementNode()
    {
        m_discriminant = (ExpressionNode*)discriminant;
        m_casesA = casesA;
        m_default = (StatementNode*)deflt;
        m_casesB = casesB;
        m_lexical = lexical;
    }

    virtual ASTNodeType type() { return ASTNodeType::SwitchStatement; }
protected:
    ExpressionNode* m_discriminant;
    StatementNodeVector m_casesA;
    StatementNode* m_default;
    StatementNodeVector m_casesB;
    bool m_lexical;
};
}

#endif
