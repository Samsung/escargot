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

#ifndef TryStatementNode_h
#define TryStatementNode_h

#include "CatchClauseNode.h"
#include "StatementNode.h"
#include "runtime/ExecutionContext.h"

namespace Escargot {

class TryStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    TryStatementNode(Node *block, Node *handler, CatchClauseNodeVector &&guardedHandlers, Node *finalizer)
        : StatementNode()
    {
        m_block = (BlockStatementNode *)block;
        m_handler = (CatchClauseNode *)handler;
        m_guardedHandlers = guardedHandlers;
        m_finalizer = (BlockStatementNode *)finalizer;
    }

    virtual ASTNodeType type() { return ASTNodeType::TryStatement; }
protected:
    BlockStatementNode *m_block;
    CatchClauseNode *m_handler;
    CatchClauseNodeVector m_guardedHandlers;
    BlockStatementNode *m_finalizer;
};
}

#endif
