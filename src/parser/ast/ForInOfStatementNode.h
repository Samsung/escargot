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

namespace Escargot {

class ForInOfStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ForInOfStatementNode(Node *left, Node *right, Node *body, bool forIn)
        : StatementNode()
        , m_left(left)
        , m_right(right)
        , m_body((StatementNode *)body)
        , m_forIn(forIn)
    {
    }

    virtual ~ForInOfStatementNode()
    {
    }

    virtual ASTNodeType type()
    {
        if (m_forIn) {
            return ASTNodeType::ForInStatement;
        } else {
            return ASTNodeType::ForOfStatement;
        }
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        bool canSkipCopyToRegisterBefore = context->m_canSkipCopyToRegister;
        context->m_canSkipCopyToRegister = false;

        ByteCodeGenerateContext newContext(*context);

        newContext.getRegister(); // ExecutionResult of m_right should not overwrite any reserved value
        if (m_left->type() == ASTNodeType::VariableDeclaration)
            m_left->generateResultNotRequiredExpressionByteCode(codeBlock, &newContext);

        size_t baseCountBefore = newContext.m_registerStack->size();
        size_t rightIdx = m_right->getRegister(codeBlock, &newContext);
        m_right->generateExpressionByteCode(codeBlock, &newContext, rightIdx);
        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), newContext.getRegister(), Value()), &newContext, this);
        size_t literalIdx = newContext.getLastRegisterIndex();
        newContext.giveUpRegister();
        size_t equalResultIndex = newContext.getRegister();
        newContext.giveUpRegister();
        codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
        codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
        size_t exit1Pos = codeBlock->lastCodePosition<JumpIfTrue>();

        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), literalIdx, Value(Value::Null)), &newContext, this);
        codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), literalIdx, rightIdx, equalResultIndex), &newContext, this);
        codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), equalResultIndex), &newContext, this);
        size_t exit2Pos = codeBlock->lastCodePosition<JumpIfTrue>();

        size_t exit3Pos, continuePosition;
        if (m_forIn) {
            // for-in statement
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
            m_left->generateStoreByteCode(codeBlock, &newContext, newContext.getLastRegisterIndex(), true);
            newContext.giveUpRegister();

            context->m_canSkipCopyToRegister = canSkipCopyToRegisterBefore;

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            newContext.giveUpRegister();
            m_body->generateStatementByteCode(codeBlock, &newContext);
            size_t forInIndex = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize++;

            codeBlock->peekCode<EnumerateObject>(ePosition)->m_dataRegisterIndex = forInIndex;
            codeBlock->peekCode<CheckIfKeyIsLast>(checkPos)->m_registerIndex = forInIndex;
            codeBlock->peekCode<EnumerateObjectKey>(enumerateObjectKeyPos)->m_dataRegisterIndex = forInIndex;
        } else {
            // for-of statement
            size_t iPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index)), &newContext, this);
            codeBlock->peekCode<GetIterator>(iPosition)->m_objectRegisterIndex = rightIdx;
            // drop rightIdx
            newContext.giveUpRegister();

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            continuePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index)), &newContext, this);

            size_t iterStepPos = exit3Pos = codeBlock->lastCodePosition<IteratorStep>();
            codeBlock->peekCode<IteratorStep>(iterStepPos)->m_registerIndex = newContext.getRegister();

            m_left->generateStoreByteCode(codeBlock, &newContext, newContext.getLastRegisterIndex(), true);
            newContext.giveUpRegister();

            context->m_canSkipCopyToRegister = canSkipCopyToRegisterBefore;

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            newContext.giveUpRegister();
            m_body->generateStatementByteCode(codeBlock, &newContext);
            size_t iteratorIndex = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize++;

            codeBlock->peekCode<GetIterator>(iPosition)->m_registerIndex = iteratorIndex;
            codeBlock->peekCode<IteratorStep>(iterStepPos)->m_iterRegisterIndex = iteratorIndex;
        }

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), continuePosition), &newContext, this);
        size_t exitPos = codeBlock->currentCodeSize();
        ASSERT(codeBlock->peekCode<CheckIfKeyIsLast>(continuePosition)->m_orgOpcode == CheckIfKeyIsLastOpcode || codeBlock->peekCode<IteratorStep>(continuePosition)->m_orgOpcode == IteratorStepOpcode);

        newContext.consumeBreakPositions(codeBlock, exitPos, context->m_tryStatementScopeCount);
        newContext.consumeContinuePositions(codeBlock, continuePosition, context->m_tryStatementScopeCount);
        newContext.m_positionToContinue = continuePosition;

        codeBlock->peekCode<JumpIfTrue>(exit1Pos)->m_jumpPosition = exitPos;
        codeBlock->peekCode<JumpIfTrue>(exit2Pos)->m_jumpPosition = exitPos;
        if (m_forIn) {
            codeBlock->peekCode<CheckIfKeyIsLast>(exit3Pos)->m_forInEndPosition = exitPos;
        } else {
            codeBlock->peekCode<IteratorStep>(exit3Pos)->m_forOfEndPosition = exitPos;
        }

        newContext.propagateInformationTo(*context);
    }

private:
    RefPtr<Node> m_left;
    RefPtr<Node> m_right;
    RefPtr<StatementNode> m_body;
    bool m_forIn;
};
}

#endif
