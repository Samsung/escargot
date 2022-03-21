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
        // binding function name if needs
        if (context->m_codeBlock->functionName().string()->length()) {
            if (context->m_codeBlock->isFunctionNameSaveOnHeap() || context->m_codeBlock->isFunctionNameUsedBySelf()) {
                codeBlock->pushCode(BindingCalleeIntoRegister(ByteCodeLOC(m_loc.index)), context, this);
            }
        }

        // init stack-allocated vars
        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            auto varInfo = codeBlock->m_codeBlock->identifierInfos();
            auto fnName = codeBlock->m_codeBlock->functionName();
            for (size_t i = 0; i < varInfo.size(); i++) {
                const auto& var = varInfo[i];
                if (var.m_name == fnName && !var.m_isExplicitlyDeclaredOrParameterName) {
                    // this case, init from outside
                    continue;
                }
                if (var.m_needToAllocateOnStack && !var.m_isParameterName) {
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + var.m_indexForIndexedStorage, Value()), context, this);
                }
            }
        }

        // init literal values
        if (context->m_numeralLiteralData) {
            for (size_t i = 0; i < context->m_numeralLiteralData->size(); i++) {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT + i, context->m_numeralLiteralData->data()[i]), context, this);
            }
        }

        if (codeBlock->m_codeBlock->functionBodyBlockIndex() != 0) {
            size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;

            ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
            context->m_lexicalBlockIndex = 0;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(0);
            blockContext = codeBlock->pushLexicalBlock(context, bi, this);

            generateFunctionNameByteCode(codeBlock, context);

            m_params->generateStatementByteCode(codeBlock, context);

            addExecutionPauseIfNeeds(codeBlock, context);

            m_body->generateStatementByteCode(codeBlock, context);

            codeBlock->finalizeLexicalBlock(context, blockContext);

            context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
        } else {
            size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
            ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
            if (m_body->lexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
                context->m_lexicalBlockIndex = m_body->lexicalBlockIndex();
                InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_body->lexicalBlockIndex());
                blockContext = codeBlock->pushLexicalBlock(context, bi, this, false);
            }

            generateFunctionNameByteCode(codeBlock, context);

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

    void generateFunctionNameByteCode(ByteCodeBlock* block, ByteCodeGenerateContext* context)
    {
        InterpretedCodeBlock* codeBlock = block->codeBlock();
        AtomicString name = codeBlock->functionName();
        if (name.string()->length()) {
            if (UNLIKELY(codeBlock->isFunctionNameExplicitlyDeclared())) {
                if (codeBlock->canUseIndexedVariableStorage()) {
                    if (!codeBlock->isFunctionNameSaveOnHeap()) {
                        auto r = context->getRegister();
                        block->pushCode(LoadLiteral(ByteCodeLOC(0), r, Value()), context, nullptr);
                        block->pushCode(Move(ByteCodeLOC(0), r, REGULAR_REGISTER_LIMIT + 1), context, nullptr);
                        context->giveUpRegister();
                    }
                }
            } else if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap() && !name.string()->equals("arguments"))) {
                context->m_isVarDeclaredBindingInitialization = true;
                IdentifierNode* id = new (alloca(sizeof(IdentifierNode))) IdentifierNode(codeBlock->functionName());
                id->generateStoreByteCode(block, context, REGULAR_REGISTER_LIMIT + 1, true);
            }
        }
    }

    void addExecutionPauseIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (codeBlock->m_codeBlock->isGenerator()) {
            size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));

            ExecutionPause::ExecutionPauseGeneratorsInitializeData data;
            data.m_tailDataLength = tailDataLength;

            codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
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
} // namespace Escargot

#endif
