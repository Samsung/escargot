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

#ifndef ForStatementNode_h
#define ForStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class ForStatementNode : public StatementNode {
public:
    ForStatementNode(Node* init, Node* test, Node* update, Node* body, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex,
                     LexicalBlockIndex iterationLexicalBlockIndex)
        : StatementNode()
        , m_init(init)
        , m_test(test)
        , m_update(update)
        , m_body(body)
        , m_hasLexicalDeclarationOnInit(hasLexicalDeclarationOnInit)
        , m_headLexicalBlockIndex(headLexicalBlockIndex)
        , m_iterationLexicalBlockIndex(
              iterationLexicalBlockIndex)
    {
    }

    virtual ASTNodeType type() override
    {
        return ASTNodeType::ForStatement;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        bool shouldCareScriptExecutionResult = context->shouldCareScriptExecutionResult();

        // TODO remove iterationLexicalBlock if there is no capture from child blocks or self
        size_t headLexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext headBlockContext;
        if (m_headLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_headLexicalBlockIndex;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_headLexicalBlockIndex);
            headBlockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by m_test

        if (m_init && m_init->type() != ASTNodeType::RegExpLiteral) {
            m_init->generateStatementByteCode(codeBlock, &newContext);
        }

        if (shouldCareScriptExecutionResult) {
            // 13.7.4.8 Runtime Semantics: ForBodyEvaluation
            // 1. Let V = undefined.
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), 0, Value()), &newContext, this->m_loc.index);
        }

        // per iteration block
        size_t iterationLexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext iterationBlockContext;
        uint8_t tempNode[sizeof(IdentifierNode)];

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->identifiers().size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->identifiers().size(); i++) {
                IdentifierNode* id = new (tempNode) IdentifierNode(bi->identifiers()[i].m_name);
                id->m_loc = m_loc;
                id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
            }

            newContext.m_lexicalBlockIndex = m_iterationLexicalBlockIndex;
            iterationBlockContext = codeBlock->pushLexicalBlock(&newContext, bi, this);

            for (size_t i = 0; i < bi->identifiers().size(); i++) {
                newContext.addLexicallyDeclaredNames(bi->identifiers()[i].m_name);
            }

            size_t reverse = bi->identifiers().size() - 1;
            for (size_t i = 0; i < bi->identifiers().size(); i++, reverse--) {
                IdentifierNode* id = new (tempNode) IdentifierNode(bi->identifiers()[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }

        size_t forStart = codeBlock->currentCodeSize();

        size_t testIndex = 0;
        size_t testPos = 0;
        if (m_test) {
            // test dosen't affect script result
            if (shouldCareScriptExecutionResult) {
                newContext.getRegister();
            }
            if (m_test->isRelationOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfNotFulfilled>();
            } else if (m_test->isEqualityOperation()) {
                m_test->generateExpressionByteCode(codeBlock, &newContext, REGISTER_LIMIT);
                testPos = codeBlock->lastCodePosition<JumpIfEqual>();
            } else {
                testIndex = m_test->getRegister(codeBlock, &newContext);
                m_test->generateExpressionByteCode(codeBlock, &newContext, testIndex);
                codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testIndex), &newContext, this->m_loc.index);
                testPos = codeBlock->lastCodePosition<JumpIfFalse>();
                newContext.giveUpRegister();
            }
            if (shouldCareScriptExecutionResult) {
                newContext.giveUpRegister();
            }
        }

        newContext.giveUpRegister();

#ifdef ESCARGOT_DEBUGGER
        insertEmptyStatementBreakpoint(context, m_body);
#endif /* ESCARGOT_DEBUGGER */
        m_body->generateStatementByteCode(codeBlock, &newContext);

        // replace env if needed
        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            if (bi->shouldAllocateEnvironment()) {
                newContext.getRegister();

                std::vector<size_t> nameRegisters;
                for (size_t i = 0; i < bi->identifiers().size(); i++) {
                    nameRegisters.push_back(newContext.getRegister());
                }

                for (size_t i = 0; i < bi->identifiers().size(); i++) {
                    IdentifierNode* id = new (tempNode) IdentifierNode(bi->identifiers()[i].m_name);
                    id->m_loc = m_loc;
                    id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
                }

                codeBlock->pushCode(ReplaceBlockLexicalEnvironmentOperation(ByteCodeLOC(m_loc.index), bi), &newContext, this->m_loc.index);

                codeBlock->initFunctionDeclarationWithinBlock(&newContext, bi, this);

                size_t reverse = bi->identifiers().size() - 1;
                for (size_t i = 0; i < bi->identifiers().size(); i++, reverse--) {
                    IdentifierNode* id = new (tempNode) IdentifierNode(bi->identifiers()[reverse].m_name);
                    id->m_loc = m_loc;
                    newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                    id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                    ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                    newContext.giveUpRegister();
                }

                newContext.giveUpRegister();
            }
        }

        size_t updatePosition = codeBlock->currentCodeSize();
        if (m_update) {
            if (!shouldCareScriptExecutionResult) {
                m_update->generateResultNotRequiredExpressionByteCode(codeBlock, &newContext);
            } else {
                // update dosen't affect script result
                newContext.getRegister();
                m_update->generateExpressionByteCode(codeBlock, &newContext, newContext.getRegister());
                newContext.giveUpRegister();
                newContext.giveUpRegister();
            }
        }

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), forStart), &newContext, this->m_loc.index);

        size_t forEnd = codeBlock->currentCodeSize();
        if (m_test) {
            codeBlock->peekCode<Jump>(testPos)->m_jumpPosition = forEnd;
        }

        newContext.consumeBreakPositions(codeBlock, forEnd, newContext.tryCatchWithBlockStatementCount());
        newContext.consumeContinuePositions(codeBlock, updatePosition, newContext.tryCatchWithBlockStatementCount());
        newContext.m_positionToContinue = updatePosition;

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            codeBlock->finalizeLexicalBlock(&newContext, iterationBlockContext);
            newContext.m_lexicalBlockIndex = iterationLexicalBlockIndexBefore;
        }

        newContext.propagateInformationTo(*context);

        if (m_headLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(context, headBlockContext);
            context->m_lexicalBlockIndex = headLexicalBlockIndexBefore;
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_init) {
            m_init->iterateChildren(fn);
        }

        if (m_test) {
            m_test->iterateChildren(fn);
        }

        if (m_update) {
            m_update->iterateChildren(fn);
        }

        if (m_body) {
            m_body->iterateChildren(fn);
        }
    }

private:
    Node* m_init;
    Node* m_test;
    Node* m_update;
    Node* m_body;
    bool m_hasLexicalDeclarationOnInit;
    LexicalBlockIndex m_headLexicalBlockIndex;
    LexicalBlockIndex m_iterationLexicalBlockIndex;
};
} // namespace Escargot

#endif
