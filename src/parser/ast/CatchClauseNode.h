/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#ifndef CatchClauseNode_h
#define CatchClauseNode_h

#include "BlockStatementNode.h"
#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "Node.h"

namespace Escargot {

class FunctionDeclarationNode;

// interface CatchClause <: Node {
class CatchClauseNode : public Node {
public:
    CatchClauseNode(Node* param, Node* guard, Node* body, LexicalBlockIndex paramLexicalBlockIndex)
        : Node()
        , m_param(param)
        , m_guard(guard)
        , m_body(body)
        , m_paramLexicalBlockIndex(paramLexicalBlockIndex)
    {
    }

    Node* param()
    {
        return m_param;
    }

    BlockStatementNode* body()
    {
        return (BlockStatementNode*)m_body;
    }

    LexicalBlockIndex paramLexicalBlockIndex()
    {
        return m_paramLexicalBlockIndex;
    }

    virtual ASTNodeType type() override { return ASTNodeType::CatchClause; }

private:
    Node* m_param;
    Node* m_guard;
    Node* m_body;
    LexicalBlockIndex m_paramLexicalBlockIndex;
};
} // namespace Escargot

#endif
