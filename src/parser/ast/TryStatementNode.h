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
    virtual ~TryStatementNode()
    {
        delete m_block;
        delete m_handler;
        delete m_finalizer;
        // TODO delete m_guardedHandlers
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        context->m_tryStatementScopeCount++;
        codeBlock->pushCode(TryOperation(ByteCodeLOC(m_loc.index)), context, this);
        size_t pos = codeBlock->lastCodePosition<TryOperation>();
        m_block->generateStatementByteCode(codeBlock, context);
        codeBlock->pushCode(TryCatchWithBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
        size_t tryCatchBodyPos = codeBlock->lastCodePosition<TryCatchWithBodyEnd>();
        if (m_handler) {
            size_t prev = context->m_catchScopeCount;
            context->m_catchScopeCount++;
            context->m_lastCatchVariableName = m_handler->param()->name();
            codeBlock->peekCode<TryOperation>(pos)->m_catchPosition = codeBlock->currentCodeSize();
            m_handler->body()->generateStatementByteCode(codeBlock, context);
            codeBlock->peekCode<TryOperation>(pos)->m_hasCatch = true;
            codeBlock->peekCode<TryOperation>(pos)->m_catchVariableName = m_handler->param()->name();
            codeBlock->pushCode(TryCatchWithBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
            context->m_catchScopeCount = prev;
        }

        context->registerJumpPositionsToComplexCase(pos);

        codeBlock->peekCode<TryOperation>(pos)->m_tryCatchEndPosition = codeBlock->currentCodeSize();
        if (m_finalizer) {
            m_finalizer->generateStatementByteCode(codeBlock, context);
        }
        codeBlock->pushCode(FinallyEnd(ByteCodeLOC(m_loc.index)), context, this);
        codeBlock->peekCode<FinallyEnd>(codeBlock->lastCodePosition<FinallyEnd>())->m_tryDupCount = context->m_tryStatementScopeCount;
        context->m_tryStatementScopeCount--;

        codeBlock->m_shouldClearStack = true;
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
