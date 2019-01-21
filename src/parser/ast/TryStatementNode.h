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
        m_guardedHandlers = std::move(guardedHandlers);
        m_finalizer = (BlockStatementNode *)finalizer;
    }
    virtual ~TryStatementNode()
    {
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
            auto &innerFDs = m_handler->innerFDs();
            for (size_t i = 0; i < innerFDs.size(); i++) {
                size_t r = context->getRegister();

                CodeBlock *blk = nullptr;
                size_t cnt = 0;
                for (size_t j = 0; j < context->m_codeBlock->asInterpretedCodeBlock()->childBlocks().size(); j++) {
                    CodeBlock *c = context->m_codeBlock->asInterpretedCodeBlock()->childBlocks()[j];
                    if (c->isFunctionDeclarationWithSpecialBinding()) {
                        if (cnt == i) {
                            blk = c;
                            break;
                        }
                        cnt++;
                    }
                }
                codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), r, blk), context, this);

                RefPtr<IdentifierNode> node = adoptRef(new IdentifierNode(blk->functionName()));
                node->generateStoreByteCode(codeBlock, context, r, false);
                context->giveUpRegister();
            }

            m_handler->body()->generateStatementByteCode(codeBlock, context);
            codeBlock->peekCode<TryOperation>(pos)->m_hasCatch = true;
            codeBlock->peekCode<TryOperation>(pos)->m_catchVariableName = m_handler->param()->name();
            codeBlock->pushCode(TryCatchWithBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
            context->m_catchScopeCount = prev;
        }

        context->registerJumpPositionsToComplexCase(pos);

        codeBlock->peekCode<TryOperation>(pos)->m_tryCatchEndPosition = codeBlock->currentCodeSize();
        if (m_finalizer) {
            context->getRegister();
            m_finalizer->generateStatementByteCode(codeBlock, context);
            context->giveUpRegister();
        }
        codeBlock->pushCode(FinallyEnd(ByteCodeLOC(m_loc.index)), context, this);
        codeBlock->peekCode<FinallyEnd>(codeBlock->lastCodePosition<FinallyEnd>())->m_tryDupCount = context->m_tryStatementScopeCount;
        context->m_tryStatementScopeCount--;

        codeBlock->m_shouldClearStack = true;
    }

    virtual ASTNodeType type() { return ASTNodeType::TryStatement; }
protected:
    RefPtr<BlockStatementNode> m_block;
    RefPtr<CatchClauseNode> m_handler;
    CatchClauseNodeVector m_guardedHandlers;
    RefPtr<BlockStatementNode> m_finalizer;
};
}

#endif
