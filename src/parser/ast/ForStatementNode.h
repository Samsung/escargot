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

#ifndef ForStatementNode_h
#define ForStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class ForStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ForStatementNode(Node *init, Node *test, Node *update, Node *body, bool hasLexicalDeclarationOnInit, LexcialBlockIndex headLexcialBlockIndex, LexcialBlockIndex iterationLexcialBlockIndex)
        : StatementNode()
        , m_init((ExpressionNode *)init)
        , m_test((ExpressionNode *)test)
        , m_update((ExpressionNode *)update)
        , m_body((StatementNode *)body)
        , m_hasLexicalDeclarationOnInit(hasLexicalDeclarationOnInit)
        , m_headLexcialBlockIndex(headLexcialBlockIndex)
        , m_iterationLexcialBlockIndex(iterationLexcialBlockIndex)
    {
    }

    virtual ~ForStatementNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ForStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        size_t headLexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext headBlockContext;
        if (m_headLexcialBlockIndex != LEXCIAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_headLexcialBlockIndex;
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_headLexcialBlockIndex);
            headBlockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test

        if (m_init && m_init->type() != ASTNodeType::RegExpLiteral) {
            m_init->generateStatementByteCode(codeBlock, &newContext);
        }

        size_t forStart = codeBlock->currentCodeSize();

        size_t testIndex = 0;
        size_t testPos = 0;
        if (m_test) {
            if (m_test->isRelationOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfRelation>();
            } else if (m_test->isEqualityOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfEqual>();
            } else {
                testIndex = m_test->getRegister(codeBlock, &newContext);
                m_test->generateExpressionByteCode(codeBlock, &newContext, testIndex);
                codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testIndex), &newContext, this);
                testPos = codeBlock->lastCodePosition<JumpIfFalse>();
                newContext.giveUpRegister();
            }
        }

        newContext.giveUpRegister();

        m_body->generateStatementByteCode(codeBlock, &newContext);

        size_t updatePosition = codeBlock->currentCodeSize();

        // Creating a new block after each iteration.
        if (m_iterationLexcialBlockIndex != LEXCIAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexcialBlockIndex);
            uint8_t tmpIdentifierNode[sizeof(IdentifierNode)];

            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[i].m_name);
                id->m_loc = m_loc;
                id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
            }

            newContext.m_lexicalBlockIndex = m_iterationLexcialBlockIndex;
            codeBlock->replacePerIterationLexicalBlock(&newContext, bi, this);

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                newContext.m_lexicallyDeclaredNames->push_back(bi->m_identifiers[i].m_name);
            }

            size_t reverse = bi->m_identifiers.size() - 1;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++, reverse--) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], false);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }

        if (m_update) {
            if (!context->m_isEvalCode && !context->m_isGlobalScope) {
                m_update->generateResultNotRequiredExpressionByteCode(codeBlock, &newContext);
            } else {
                m_update->generateExpressionByteCode(codeBlock, &newContext, newContext.getRegister());
                newContext.giveUpRegister();
            }
        }

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), forStart), &newContext, this);

        size_t forEnd = codeBlock->currentCodeSize();
        if (m_test) {
            codeBlock->peekCode<JumpByteCode>(testPos)->m_jumpPosition = forEnd;
        }

        newContext.consumeBreakPositions(codeBlock, forEnd, context->m_tryStatementScopeCount);
        newContext.consumeContinuePositions(codeBlock, updatePosition, context->m_tryStatementScopeCount);
        if (context->m_currentLabels->size() > 0 && context->m_currentLabels->back().second == (size_t)this) {
            newContext.consumeLabeledContinuePositions(codeBlock, updatePosition, context->m_currentLabels->back().first, context->m_tryStatementScopeCount);
        }

        newContext.m_positionToContinue = updatePosition;
        newContext.propagateInformationTo(*context);

        if (m_headLexcialBlockIndex != LEXCIAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(context, headBlockContext);
            context->m_lexicalBlockIndex = headLexicalBlockIndexBefore;
        }
    }

private:
    RefPtr<ExpressionNode> m_init;
    RefPtr<ExpressionNode> m_test;
    RefPtr<ExpressionNode> m_update;
    RefPtr<StatementNode> m_body;
    bool m_hasLexicalDeclarationOnInit;
    LexcialBlockIndex m_headLexcialBlockIndex;
    LexcialBlockIndex m_iterationLexcialBlockIndex;
};
}

#endif
