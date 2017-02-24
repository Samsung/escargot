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

#ifndef DirectiveNode_h
#define DirectiveNode_h

#include "ExpressionNode.h"
#include "Node.h"

namespace Escargot {

class DirectiveNode : public Node {
public:
    DirectiveNode(ExpressionNode* expr, StringView value)
        : Node()
    {
        m_expr = expr;
        m_value = value;
    }

    virtual ~DirectiveNode()
    {
        delete m_expr;
    }

    virtual ASTNodeType type() { return ASTNodeType::Directive; }
    StringView value() { return m_value; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        m_expr->generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

protected:
    ExpressionNode* m_expr;
    StringView m_value;
};
}

#endif
