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

#ifndef VariableDeclaratorNode_h
#define VariableDeclaratorNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "Node.h"
#include "PatternNode.h"

namespace Escargot {

class VariableDeclaratorNode : public Node {
public:
    friend class ScriptParser;
    VariableDeclaratorNode(Node* id, Node* init = nullptr)
        : Node()
    {
        m_id = id;
        m_init = init;
    }

    virtual ~VariableDeclaratorNode()
    {
        delete m_id;
        delete m_init;
    }

    virtual ASTNodeType type() { return ASTNodeType::VariableDeclarator; }
    Node* id() { return m_id; }
    Node* init() { return m_init; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ASSERT(m_id->isIdentifier());
        ASSERT(context->m_codeBlock->hasName(m_id->asIdentifier()->name()));
        AtomicString name = m_id->asIdentifier()->name();
        if (m_init) {
            context->getRegister();
            if (!name.string()->equals("arguments")) {
                AssignmentExpressionSimpleNode assign(m_id, m_init);
                assign.m_loc = m_loc;
                assign.generateResultNotRequiredExpressionByteCode(codeBlock, context);
                // for avoding double-free
                assign.giveupChildren();
            } else {
                auto r = m_init->getRegister(codeBlock, context);
                m_init->generateExpressionByteCode(codeBlock, context, r);
                m_id->generateStoreByteCode(codeBlock, context, r, false);
                context->giveUpRegister();
            }
            context->giveUpRegister();
        }
    }

protected:
    Node* m_id; // id: Pattern;
    Node* m_init; // init: Expression | null;
};
}

#endif
