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

#include "Node.h"
#include "PatternNode.h"
#include "ExpressionNode.h"
#include "IdentifierNode.h"

namespace Escargot {

class VariableDeclaratorNode : public Node {
public:
    friend class ScriptParser;
    VariableDeclaratorNode(Node* id, ExpressionNode* init = nullptr)
        : Node()
    {
        m_id = id;
        m_init = init;
    }

    virtual ASTNodeType type() { return ASTNodeType::VariableDeclarator; }
    Node* id() { return m_id; }
    ExpressionNode* init() { return m_init; }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ASSERT(m_id->isIdentifier());
        AtomicString name = m_id->asIdentifier()->name();
        if (!context->m_codeBlock->hasName(name))
            codeBlock->pushCode(DeclareVarVariable(ByteCodeLOC(m_loc.line, m_loc.column, m_loc.index), name), context, this);
        if (m_init) {
            m_init->generateExpressionByteCode(codeBlock, context);
            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.line, m_loc.column, m_loc.index), context->getLastRegisterIndex(), name), context, this);
            context->giveUpRegister();
        }
    }

protected:
    Node* m_id; // id: Pattern;
    ExpressionNode* m_init; // init: Expression | null;
};


}

#endif
