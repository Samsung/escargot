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

#ifndef FunctionNode_h
#define FunctionNode_h

#include "Node.h"
#include "StatementNode.h"
#include "BlockStatementNode.h"

namespace Escargot {

class FunctionNode : public StatementNode {
public:
    FunctionNode(StatementContainer* params, BlockStatementNode* body, NumeralLiteralVector&& numeralLiteralVector)
        : StatementNode()
        , m_params(params)
        , m_body(body)
        , m_numeralLiteralVector(std::move(numeralLiteralVector))
    {
    }

    NumeralLiteralVector& numeralLiteralVector() { return m_numeralLiteralVector; }
    virtual ASTNodeType type() override { return ASTNodeType::Function; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (codeBlock->m_codeBlock->functionBodyBlockIndex() != 0) {
            ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
            context->m_lexicalBlockIndex = 0;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(0);
            blockContext = codeBlock->pushLexicalBlock(context, bi, this);

            m_params->generateStatementByteCode(codeBlock, context);

            addExecutionPauseIfNeeds(codeBlock, context);

            m_body->generateStatementByteCode(codeBlock, context);

            codeBlock->finalizeLexicalBlock(context, blockContext);
        } else {
            size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
            ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
            if (m_body->lexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                context->m_lexicalBlockIndex = m_body->lexicalBlockIndex();
                InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_body->lexicalBlockIndex());
                blockContext = codeBlock->pushLexicalBlock(context, bi, this, false);
            }

            m_params->generateStatementByteCode(codeBlock, context);

            addExecutionPauseIfNeeds(codeBlock, context);

            if (m_body->lexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_body->lexicalBlockIndex());
                codeBlock->initFunctionDeclarationWithinBlock(context, bi, this);
            }

            m_body->container()->generateStatementByteCode(codeBlock, context);

            if (m_body->lexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                codeBlock->finalizeLexicalBlock(context, blockContext);
                context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
            }
        }
    }

    void addExecutionPauseIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (codeBlock->m_codeBlock->isGenerator()) {
            codeBlock->updateMaxPauseStatementExtraDataLength(context);
            size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));

            ExecutionPause::ExecutionPauseGeneratorsInitializeData data;
            data.m_tailDataLength = tailDataLength;

            codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
            codeBlock->pushPauseStatementExtraData(context);
        }
    }

    BlockStatementNode* body()
    {
        ASSERT(!!m_body);
        return m_body;
    }

private:
    StatementContainer* m_params;
    BlockStatementNode* m_body;
    NumeralLiteralVector m_numeralLiteralVector;
};
}

#endif
