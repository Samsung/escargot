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

#ifndef ForInOfStatementNode_h
#define ForInOfStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "TryStatementNode.h"

namespace Escargot {

class ForInOfStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ForInOfStatementNode(Node *left, Node *right, Node *body, bool forIn, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
        : StatementNode()
        , m_left(left)
        , m_right(right)
        , m_body((StatementNode *)body)
        , m_forIn(forIn)
        , m_hasLexicalDeclarationOnInit(hasLexicalDeclarationOnInit)
        , m_headLexicalBlockIndex(headLexicalBlockIndex)
        , m_iterationLexicalBlockIndex(iterationLexicalBlockIndex)
    {
    }

    virtual ~ForInOfStatementNode()
    {
    }

    virtual ASTNodeType type() override
    {
        if (m_forIn) {
            return ASTNodeType::ForInStatement;
        } else {
            return ASTNodeType::ForOfStatement;
        }
    }

    void generateBodyByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, ByteCodeGenerateContext &newContext)
    {
        // per iteration block
        size_t iterationLexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext iterationBlockContext;
        uint8_t tmpIdentifierNode[sizeof(IdentifierNode)];

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[i].m_name);
                id->m_loc = m_loc;
                id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
            }

            newContext.m_lexicalBlockIndex = m_iterationLexicalBlockIndex;
            iterationBlockContext = codeBlock->pushLexicalBlock(&newContext, bi, this);

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                newContext.addLexicallyDeclaredNames(bi->m_identifiers[i].m_name);
            }

            size_t reverse = bi->m_identifiers.size() - 1;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++, reverse--) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }

        m_body->generateStatementByteCode(codeBlock, &newContext);

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[i].m_name);
                id->m_loc = m_loc;
                id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
            }

            codeBlock->finalizeLexicalBlock(&newContext, iterationBlockContext);
            newContext.m_lexicalBlockIndex = iterationLexicalBlockIndexBefore;

            size_t reverse = bi->m_identifiers.size() - 1;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++, reverse--) {
                IdentifierNode *id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context) override
    {
        bool canSkipCopyToRegisterBefore = context->m_canSkipCopyToRegister;
        context->m_canSkipCopyToRegister = false;

        size_t headLexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext headBlockContext;
        if (m_headLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_headLexicalBlockIndex;
            InterpretedCodeBlock::BlockInfo *bi = codeBlock->m_codeBlock->blockInfo(m_headLexicalBlockIndex);
            headBlockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExecutionResult of m_right should not overwrite any reserved value
        if (m_left->type() == ASTNodeType::VariableDeclaration) {
            newContext.m_forInOfVarBinding = true;
            m_left->generateResultNotRequiredExpressionByteCode(codeBlock, &newContext);
            newContext.m_forInOfVarBinding = false;
        }

        size_t baseCountBefore = newContext.m_registerStack->size();

        size_t exit1Pos, exit2Pos, exit3Pos, continuePosition;
        // for-of only
        TryStatementNode::TryStatementByteCodeContext forOfTryStatmentContext;
        ByteCodeRegisterIndex finishCheckRegisterIndex, iteratorDataRegisterIndex;
        size_t forOfEndCheckRegisterHeadStartPosition;
        size_t forOfEndCheckRegisterHeadEndPosition;
        size_t forOfEndCheckRegisterBodyEndPosition;

        if (m_forIn) {
            // for-in statement
            auto oldRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;

            size_t rightIdx = m_right->getRegister(codeBlock, &newContext);
            m_right->generateExpressionByteCode(codeBlock, &newContext, rightIdx);
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), newContext.getRegister(), Value()), &newContext, this);
            size_t literalIdx = newContext.getLastRegisterIndex();
            newContext.giveUpRegister();
            size_t equalResultIndex = newContext.getRegister();
            newContext.giveUpRegister();
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
            exit1Pos = codeBlock->lastCodePosition<JumpIfTrue>();

            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), literalIdx, Value(Value::Null)), &newContext, this);
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
            exit2Pos = codeBlock->lastCodePosition<JumpIfTrue>();

            size_t ePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(EnumerateObject(ByteCodeLOC(m_loc.index)), &newContext, this);
            codeBlock->peekCode<EnumerateObject>(ePosition)->m_objectRegisterIndex = rightIdx;
            // drop rightIdx
            newContext.giveUpRegister();

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            continuePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(CheckIfKeyIsLast(ByteCodeLOC(m_loc.index)), &newContext, this);

            size_t checkPos = exit3Pos = codeBlock->lastCodePosition<CheckIfKeyIsLast>();
            codeBlock->pushCode(EnumerateObjectKey(ByteCodeLOC(m_loc.index)), &newContext, this);
            size_t enumerateObjectKeyPos = codeBlock->lastCodePosition<EnumerateObjectKey>();

            codeBlock->peekCode<EnumerateObjectKey>(enumerateObjectKeyPos)->m_registerIndex = newContext.getRegister();

            newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
            m_left->generateStoreByteCode(codeBlock, &newContext, newContext.getLastRegisterIndex(), true);
            ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
            newContext.giveUpRegister();

            context->m_canSkipCopyToRegister = canSkipCopyToRegisterBefore;

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            newContext.giveUpRegister();

            auto headRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;

            oldRequiredRegisterFileSizeInValueSize = std::max(oldRequiredRegisterFileSizeInValueSize, codeBlock->m_requiredRegisterFileSizeInValueSize);
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;
            generateBodyByteCode(codeBlock, context, newContext);

            auto bodyRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            auto dataRegisterIndex = std::max(headRequiredRegisterFileSizeInValueSize, bodyRequiredRegisterFileSizeInValueSize);

            codeBlock->m_requiredRegisterFileSizeInValueSize = std::max({ oldRequiredRegisterFileSizeInValueSize, codeBlock->m_requiredRegisterFileSizeInValueSize, (ByteCodeRegisterIndex)(dataRegisterIndex + 1) });

            codeBlock->peekCode<EnumerateObject>(ePosition)->m_dataRegisterIndex = dataRegisterIndex;
            codeBlock->peekCode<CheckIfKeyIsLast>(checkPos)->m_registerIndex = dataRegisterIndex;
            codeBlock->peekCode<EnumerateObjectKey>(enumerateObjectKeyPos)->m_dataRegisterIndex = dataRegisterIndex;
        } else {
            // for-of statement
            TryStatementNode::generateTryStatementStartByteCode(codeBlock, &newContext, this, forOfTryStatmentContext);

            auto oldRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;

            forOfEndCheckRegisterHeadStartPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), SIZE_MAX, Value(true)), &newContext, this);

            size_t rightIdx = m_right->getRegister(codeBlock, &newContext);
            m_right->generateExpressionByteCode(codeBlock, &newContext, rightIdx);

            size_t iPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index)), &newContext, this);
            codeBlock->peekCode<GetIterator>(iPosition)->m_objectRegisterIndex = rightIdx;

            size_t literalIdx = newContext.getRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), literalIdx, Value()), &newContext, this);
            newContext.giveUpRegister();
            size_t equalResultIndex = newContext.getRegister();
            newContext.giveUpRegister();
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
            exit1Pos = codeBlock->lastCodePosition<JumpIfTrue>();

            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), literalIdx, Value(Value::Null)), &newContext, this);
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
            exit2Pos = codeBlock->lastCodePosition<JumpIfTrue>();

            // drop rightIdx
            newContext.giveUpRegister();

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            continuePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index)), &newContext, this);

            size_t iterStepPos = exit3Pos = codeBlock->lastCodePosition<IteratorStep>();
            codeBlock->peekCode<IteratorStep>(iterStepPos)->m_registerIndex = newContext.getRegister();

            forOfEndCheckRegisterHeadEndPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), SIZE_MAX, Value(false)), &newContext, this);

            newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
            m_left->generateStoreByteCode(codeBlock, &newContext, newContext.getLastRegisterIndex(), true);
            ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
            newContext.giveUpRegister();

            context->m_canSkipCopyToRegister = canSkipCopyToRegisterBefore;

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            newContext.giveUpRegister();

            auto headRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;

            oldRequiredRegisterFileSizeInValueSize = std::max(oldRequiredRegisterFileSizeInValueSize, codeBlock->m_requiredRegisterFileSizeInValueSize);
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;

            generateBodyByteCode(codeBlock, context, newContext);

            forOfEndCheckRegisterBodyEndPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), SIZE_MAX, Value(true)), &newContext, this);

            auto bodyRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            iteratorDataRegisterIndex = std::max(headRequiredRegisterFileSizeInValueSize, bodyRequiredRegisterFileSizeInValueSize);
            finishCheckRegisterIndex = iteratorDataRegisterIndex + 1;

            codeBlock->m_requiredRegisterFileSizeInValueSize = std::max({ oldRequiredRegisterFileSizeInValueSize, codeBlock->m_requiredRegisterFileSizeInValueSize, (ByteCodeRegisterIndex)(iteratorDataRegisterIndex + 2) });

            codeBlock->peekCode<GetIterator>(iPosition)->m_registerIndex = iteratorDataRegisterIndex;
            codeBlock->peekCode<IteratorStep>(iterStepPos)->m_iterRegisterIndex = iteratorDataRegisterIndex;
        }

        size_t blockExitPos = codeBlock->currentCodeSize();

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), continuePosition), &newContext, this);
        size_t exitPos = codeBlock->currentCodeSize();
        ASSERT(codeBlock->peekCode<CheckIfKeyIsLast>(continuePosition)->m_orgOpcode == CheckIfKeyIsLastOpcode || codeBlock->peekCode<IteratorStep>(continuePosition)->m_orgOpcode == IteratorStepOpcode);

        // we need to add 1 on third parameter because we add try operation manually
        newContext.consumeBreakPositions(codeBlock, exitPos, context->tryCatchWithBlockStatementCount() + (m_forIn ? 0 : 1));
        newContext.consumeContinuePositions(codeBlock, continuePosition, context->tryCatchWithBlockStatementCount() + (m_forIn ? 0 : 1));
        newContext.m_positionToContinue = continuePosition;

        if (!m_forIn) {
            TryStatementNode::generateTryStatementBodyEndByteCode(codeBlock, &newContext, this, forOfTryStatmentContext);
            TryStatementNode::generateTryFinalizerStatementStartByteCode(codeBlock, &newContext, this, forOfTryStatmentContext, true);

            size_t exceptionThrownCheckStartJumpPos = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), finishCheckRegisterIndex, SIZE_MAX), &newContext, this);
            codeBlock->pushCode(IteratorClose(ByteCodeLOC(m_loc.index), iteratorDataRegisterIndex), &newContext, this);

            codeBlock->peekCode<JumpIfTrue>(exceptionThrownCheckStartJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
            TryStatementNode::generateTryFinalizerStatementEndByteCode(codeBlock, &newContext, this, forOfTryStatmentContext, true);

            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterHeadStartPosition)->m_registerIndex = finishCheckRegisterIndex;
            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterHeadEndPosition)->m_registerIndex = finishCheckRegisterIndex;
            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterBodyEndPosition)->m_registerIndex = finishCheckRegisterIndex;
        }

        codeBlock->peekCode<JumpIfTrue>(exit1Pos)->m_jumpPosition = exitPos;
        codeBlock->peekCode<JumpIfTrue>(exit2Pos)->m_jumpPosition = exitPos;
        if (m_forIn) {
            codeBlock->peekCode<CheckIfKeyIsLast>(exit3Pos)->m_forInEndPosition = exitPos;
        } else {
            codeBlock->peekCode<IteratorStep>(exit3Pos)->m_forOfEndPosition = exitPos;
        }

        newContext.propagateInformationTo(*context);

        if (m_headLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(context, headBlockContext);
            context->m_lexicalBlockIndex = headLexicalBlockIndexBefore;
        }
    }

    virtual void iterateChildren(const std::function<void(Node *node)> &fn) override
    {
        fn(this);

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
        m_body->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_left;
    RefPtr<Node> m_right;
    RefPtr<StatementNode> m_body;
    bool m_forIn;
    bool m_hasLexicalDeclarationOnInit;
    LexicalBlockIndex m_headLexicalBlockIndex;
    LexicalBlockIndex m_iterationLexicalBlockIndex;
};
}

#endif
