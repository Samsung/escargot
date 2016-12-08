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

#ifndef ConditionalExpressionNode_h
#define ConditionalExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ConditionalExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ConditionalExpressionNode(Node* test, Node* consequente, Node* alternate)
        : ExpressionNode()
    {
        m_test = (ExpressionNode*)test;
        m_consequente = (ExpressionNode*)consequente;
        m_alternate = (ExpressionNode*)alternate;
    }

    virtual ASTNodeType type() { return ASTNodeType::ConditionalExpression; }
protected:
    ExpressionNode* m_test;
    ExpressionNode* m_consequente;
    ExpressionNode* m_alternate;
};
}

#endif
