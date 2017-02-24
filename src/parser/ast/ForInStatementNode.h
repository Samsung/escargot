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

#ifndef ForInStatementNode_h
#define ForInStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class ForInStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ForInStatementNode(Node *left, Node *right, Node *body, bool each)
        : StatementNode()
    {
        m_left = (ExpressionNode *)left;
        m_right = (ExpressionNode *)right;
        m_body = (StatementNode *)body;
        m_each = each;
    }

    virtual ~ForInStatementNode()
    {
        delete m_left;
        delete m_right;
        delete m_body;
    }

    virtual ASTNodeType type() { return ASTNodeType::ForInStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        bool canUseDisalignedRegisterBefore = context->m_canUseDisalignedRegister;
        context->m_canUseDisalignedRegister = false;

        ByteCodeGenerateContext newContext(*context);

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

        size_t ePosition = codeBlock->currentCodeSize();
        codeBlock->pushCode(EnumerateObject(ByteCodeLOC(m_loc.index)), &newContext, this);
        codeBlock->peekCode<EnumerateObject>(ePosition)->m_objectRegisterIndex = rightIdx;
        // drop rightIdx
        newContext.giveUpRegister();

        ASSERT(newContext.m_baseRegisterCount == 0);
        size_t continuePosition = codeBlock->currentCodeSize();
        codeBlock->pushCode(CheckIfKeyIsLast(ByteCodeLOC(m_loc.index)), &newContext, this);

        size_t exit3Pos = codeBlock->lastCodePosition<CheckIfKeyIsLast>();
        codeBlock->pushCode(EnumerateObjectKey(ByteCodeLOC(m_loc.index)), &newContext, this);
        size_t enumerateObjectKeyPos = codeBlock->lastCodePosition<EnumerateObjectKey>();

        codeBlock->peekCode<EnumerateObjectKey>(enumerateObjectKeyPos)->m_registerIndex = newContext.getRegister();
        size_t pushPos = codeBlock->currentCodeSize();
        m_left->generateStoreByteCode(codeBlock, &newContext, newContext.getLastRegisterIndex(), true);
        newContext.giveUpRegister();

        ASSERT(newContext.m_baseRegisterCount == 0);
        m_body->generateStatementByteCode(codeBlock, &newContext);
        size_t forInIndex = codeBlock->m_requiredRegisterFileSizeInValueSize;
        codeBlock->m_requiredRegisterFileSizeInValueSize++;

        codeBlock->peekCode<EnumerateObject>(ePosition)->m_dataRegisterIndex = forInIndex;
        codeBlock->peekCode<CheckIfKeyIsLast>(exit3Pos)->m_registerIndex = forInIndex;
        codeBlock->peekCode<EnumerateObjectKey>(enumerateObjectKeyPos)->m_dataRegisterIndex = forInIndex;

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), continuePosition), &newContext, this);
        size_t forInEnd = codeBlock->currentCodeSize();
        ASSERT(codeBlock->peekCode<CheckIfKeyIsLast>(continuePosition)->m_orgOpcode == CheckIfKeyIsLastOpcode);

        newContext.consumeBreakPositions(codeBlock, forInEnd);
        newContext.consumeContinuePositions(codeBlock, continuePosition);
        newContext.m_positionToContinue = continuePosition;

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index)), &newContext, this);
        size_t jPos = codeBlock->lastCodePosition<Jump>();

        size_t exitPos = codeBlock->currentCodeSize();
        codeBlock->peekCode<JumpIfTrue>(exit1Pos)->m_jumpPosition = exitPos;
        codeBlock->peekCode<JumpIfTrue>(exit2Pos)->m_jumpPosition = exitPos;
        codeBlock->peekCode<CheckIfKeyIsLast>(exit3Pos)->m_forInEndPosition = exitPos;

        codeBlock->peekCode<Jump>(jPos)->m_jumpPosition = codeBlock->currentCodeSize();

        newContext.propagateInformationTo(*context);

        context->m_canUseDisalignedRegister = canUseDisalignedRegisterBefore;
    }

protected:
    ExpressionNode *m_left;
    ExpressionNode *m_right;
    StatementNode *m_body;
    bool m_each;
};
}

#endif
