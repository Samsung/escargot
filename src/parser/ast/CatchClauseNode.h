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

#ifndef CatchClauseNode_h
#define CatchClauseNode_h

#include "BlockStatementNode.h"
#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "Node.h"

namespace Escargot {

// interface CatchClause <: Node {
class CatchClauseNode : public Node {
public:
    friend class ScriptParser;
    CatchClauseNode(Node *param, Node *guard, Node *body)
        : Node()
    {
        m_param = (IdentifierNode *)param;
        m_guard = (ExpressionNode *)guard;
        m_body = (BlockStatementNode *)body;
    }

    virtual ~CatchClauseNode()
    {
        delete m_param;
        delete m_guard;
        delete m_body;
    }

    IdentifierNode *param()
    {
        return m_param;
    }

    BlockStatementNode *body()
    {
        return m_body;
    }

    virtual ASTNodeType type() { return ASTNodeType::CatchClause; }
protected:
    IdentifierNode *m_param;
    ExpressionNode *m_guard;
    BlockStatementNode *m_body;
};

typedef Vector<Node *, gc_malloc_ignore_off_page_allocator<Node *>> CatchClauseNodeVector;
}

#endif
