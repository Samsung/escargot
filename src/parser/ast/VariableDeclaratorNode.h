/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
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
    VariableDeclaratorNode(Node* id, Node* init = nullptr)
        : Node()
        , m_id(id)
        , m_init(init)
    {
    }

    virtual ~VariableDeclaratorNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::VariableDeclarator; }
    Node* id() { return m_id; }
    Node* init() { return m_init; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_id->isPattern()) {
            if (context->m_forInOfVarBinding) {
                return;
            }
            PatternNode* pattern = m_id->asPattern(m_init, codeBlock->m_codeBlock->context()->astBuffer());
            context->getRegister();
            pattern->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            context->giveUpRegister();
            return;
        }

        ASSERT(m_id->isIdentifier());
        AtomicString name = m_id->asIdentifier()->name();
        if (m_init) {
            context->getRegister();
            if (!name.string()->equals("arguments")) {
                // check canUseIndexedVariableStorage for give right value to generateStoreByteCode(isInit..) with eval
                AssignmentExpressionSimpleNode assign(m_id, m_init);
                assign.m_loc = m_loc;
                assign.generateResultNotRequiredExpressionByteCode(codeBlock, context);
            } else {
                auto r = m_init->getRegister(codeBlock, context);
                m_init->generateExpressionByteCode(codeBlock, context, r);
                m_id->generateStoreByteCode(codeBlock, context, r, false);
                context->giveUpRegister();
            }
            context->giveUpRegister();
        }
    }

private:
    Node* m_id; // id: Pattern;
    Node* m_init; // init: Expression | null;
};
}

#endif
