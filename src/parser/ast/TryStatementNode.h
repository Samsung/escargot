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

namespace Escargot {

class TryStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    TryStatementNode(Node *block, Node *handler, CatchClauseNodeVector &&guardedHandlers, Node *finalizer)
        : StatementNode()
        , m_block((BlockStatementNode *)block)
        , m_handler((CatchClauseNode *)handler)
        , m_guardedHandlers(std::move(guardedHandlers))
        , m_finalizer((BlockStatementNode *)finalizer)
    {
    }
    virtual ~TryStatementNode()
    {
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        codeBlock->pushCode(TryOperation(ByteCodeLOC(m_loc.index)), context, this);
        size_t tryStartPosition = codeBlock->lastCodePosition<TryOperation>();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Try, tryStartPosition));
        m_block->generateStatementByteCode(codeBlock, context);
        codeBlock->pushCode(TryCatchWithBlockBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
        size_t tryCatchBodyPos = codeBlock->lastCodePosition<TryCatchWithBlockBodyEnd>();

        if (m_handler) {
            context->m_recursiveStatementStack.pop_back();
            context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Catch, tryStartPosition));
            codeBlock->peekCode<TryOperation>(tryStartPosition)->m_hasCatch = true;
            codeBlock->peekCode<TryOperation>(tryStartPosition)->m_catchPosition = codeBlock->currentCodeSize();

            // catch paramter block
            size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
            ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
            if (m_handler->paramLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                context->m_lexicalBlockIndex = m_handler->paramLexicalBlockIndex();
                InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_handler->paramLexicalBlockIndex());
                blockContext = codeBlock->pushLexicalBlock(context, bi, this);
            }

            // use dummy register for avoiding ruin script result
            context->getRegister();

            auto catchedValueRegister = context->getRegister();
            codeBlock->peekCode<TryOperation>(tryStartPosition)->m_catchedValueRegisterIndex = catchedValueRegister;
            RefPtr<RegisterReferenceNode> registerRef = adoptRef(new (alloca(sizeof(RegisterReferenceNode))) RegisterReferenceNode(catchedValueRegister));
            RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new (alloca(sizeof(AssignmentExpressionSimpleNode))) AssignmentExpressionSimpleNode(m_handler->param(), registerRef.get()));
            assign->m_loc = m_handler->m_loc;
            context->m_isLexicallyDeclaredBindingInitialization = true;
            assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            assign->giveupChildren();
            assign.release().leakRef();
            registerRef.release().leakRef();

            context->giveUpRegister();

            m_handler->body()->generateStatementByteCode(codeBlock, context);

            if (m_handler->paramLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                codeBlock->finalizeLexicalBlock(context, blockContext);
                context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
            }

            codeBlock->pushCode(TryCatchWithBlockBodyEnd(ByteCodeLOC(m_loc.index)), context, this);
            context->m_recursiveStatementStack.pop_back();
            context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Try, tryStartPosition));
        }

        context->registerJumpPositionsToComplexCase(tryStartPosition);
        codeBlock->peekCode<TryOperation>(tryStartPosition)->m_tryCatchEndPosition = codeBlock->currentCodeSize();
        context->m_recursiveStatementStack.pop_back();

        if (m_finalizer) {
            context->getRegister();
            m_finalizer->generateStatementByteCode(codeBlock, context);
            context->giveUpRegister();
        }
        codeBlock->pushCode(FinallyEnd(ByteCodeLOC(m_loc.index)), context, this);

        codeBlock->m_shouldClearStack = true;
    }

    virtual ASTNodeType type() { return ASTNodeType::TryStatement; }
private:
    RefPtr<BlockStatementNode> m_block;
    RefPtr<CatchClauseNode> m_handler;
    CatchClauseNodeVector m_guardedHandlers;
    RefPtr<BlockStatementNode> m_finalizer;
};
}

#endif
