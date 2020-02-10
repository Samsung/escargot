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
    TryStatementNode(Node *block, Node *handler, Node *finalizer)
        : StatementNode()
        , m_block((BlockStatementNode *)block)
        , m_handler((CatchClauseNode *)handler)
        , m_finalizer((BlockStatementNode *)finalizer)
    {
    }

    struct TryStatementByteCodeContext {
        size_t tryStartPosition;
        size_t tryCatchBodyPos;

        TryStatementByteCodeContext()
        {
            tryCatchBodyPos = tryStartPosition = SIZE_MAX;
        }
    };

    static void generateTryStatementStartByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, Node *self, TryStatementByteCodeContext &ctx)
    {
        codeBlock->pushCode(TryOperation(ByteCodeLOC(self->loc().index)), context, self);
        ctx.tryStartPosition = codeBlock->lastCodePosition<TryOperation>();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Try, ctx.tryStartPosition));
    }

    static void generateTryStatementBodyEndByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, Node *self, TryStatementByteCodeContext &ctx)
    {
        codeBlock->pushCode(TryCatchFinallyWithBlockBodyEnd(ByteCodeLOC(self->loc().index)), context, self);
        ctx.tryCatchBodyPos = codeBlock->lastCodePosition<TryCatchFinallyWithBlockBodyEnd>();
    }

    static void generateTryHandlerStatementStartByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, Node *self, TryStatementByteCodeContext &ctx, CatchClauseNode *handler)
    {
        context->m_recursiveStatementStack.pop_back();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Catch, ctx.tryStartPosition));
        codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_hasCatch = true;
        codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_catchPosition = codeBlock->currentCodeSize();

        // catch paramter block
        size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
        if (handler->paramLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = handler->paramLexicalBlockIndex();
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(handler->paramLexicalBlockIndex());
            blockContext = codeBlock->pushLexicalBlock(context, bi, self);
        }

        // use dummy register for avoiding ruin script result
        context->getRegister();

        auto catchedValueRegister = context->getRegister();
        codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_catchedValueRegisterIndex = catchedValueRegister;

        // cached variable is treated as let variable initialization in default
        if (handler->param() != nullptr) {
            context->m_isLexicallyDeclaredBindingInitialization = true;
            handler->param()->generateResolveAddressByteCode(codeBlock, context);
            handler->param()->generateStoreByteCode(codeBlock, context, catchedValueRegister, false);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
        }
        // drop the catchedValueRegister
        // because catchedValueRegister is a catchedValue holder and after initialzation,
        // catchedValueRegister is no longer necessary.
        context->giveUpRegister();
        context->giveUpRegister(); // drop dummy register

        handler->body()->generateStatementByteCode(codeBlock, context);

        if (handler->paramLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(context, blockContext);
            context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
        }

        codeBlock->pushCode(TryCatchFinallyWithBlockBodyEnd(ByteCodeLOC(self->loc().index)), context, self);
        context->m_recursiveStatementStack.pop_back();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Try, ctx.tryStartPosition));
    }

    static void generateTryFinalizerStatementStartByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, Node *self, TryStatementByteCodeContext &ctx, bool hasFinalizer)
    {
        codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_tryCatchEndPosition = codeBlock->currentCodeSize();
        context->m_recursiveStatementStack.pop_back();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Finally, ctx.tryStartPosition));

        if (hasFinalizer) {
            codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_hasFinalizer = true;
            context->getRegister();
        }
    }

    static void generateTryFinalizerStatementEndByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, Node *self, TryStatementByteCodeContext &ctx, bool hasFinalizer)
    {
        if (hasFinalizer) {
            context->giveUpRegister();
        }
        context->registerJumpPositionsToComplexCase(ctx.tryStartPosition);
        if (codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_hasFinalizer) {
            // we can use `End` here because we don't care about return value of interpret function
            codeBlock->pushCode(End(ByteCodeLOC(self->loc().index), 0), context, self);
        }
        codeBlock->peekCode<TryOperation>(ctx.tryStartPosition)->m_finallyEndPosition = codeBlock->currentCodeSize();
        codeBlock->m_shouldClearStack = true;
        context->m_recursiveStatementStack.pop_back();
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context) override
    {
        TryStatementByteCodeContext ctx;
        generateTryStatementStartByteCode(codeBlock, context, this, ctx);

        m_block->generateStatementByteCode(codeBlock, context);

        generateTryStatementBodyEndByteCode(codeBlock, context, this, ctx);

        if (m_handler) {
            generateTryHandlerStatementStartByteCode(codeBlock, context, this, ctx, m_handler);
        }

        generateTryFinalizerStatementStartByteCode(codeBlock, context, this, ctx, m_finalizer != nullptr);

        if (m_finalizer) {
            m_finalizer->generateStatementByteCode(codeBlock, context);
        }

        generateTryFinalizerStatementEndByteCode(codeBlock, context, this, ctx, m_finalizer != nullptr);
    }

    virtual ASTNodeType type() override { return ASTNodeType::TryStatement; }
    virtual void iterateChildren(const std::function<void(Node *node)> &fn) override
    {
        fn(this);

        m_block->iterateChildren(fn);

        if (m_handler) {
            m_handler->param()->iterateChildren(fn);
            m_handler->body()->iterateChildren(fn);
        }

        if (m_finalizer) {
            m_finalizer->iterateChildren(fn);
        }
    }

private:
    BlockStatementNode *m_block;
    CatchClauseNode *m_handler;
    BlockStatementNode *m_finalizer;
};
}

#endif
