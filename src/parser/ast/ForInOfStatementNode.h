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

#ifndef ForInOfStatementNode_h
#define ForInOfStatementNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "TryStatementNode.h"

namespace Escargot {

class ForInOfStatementNode : public StatementNode {
public:
    ForInOfStatementNode(Node* left, Node* right, Node* body, bool forIn, bool hasLexicalDeclarationOnInit, bool isForAwaitOf, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex headRightLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
        : StatementNode()
        , m_left(left)
        , m_right(right)
        , m_body(body)
        , m_forIn(forIn)
        , m_hasLexicalDeclarationOnInit(hasLexicalDeclarationOnInit)
        , m_isForAwaitOf(isForAwaitOf)
        , m_headLexicalBlockIndex(headLexicalBlockIndex)
        , m_headRightLexicalBlockIndex(headRightLexicalBlockIndex)
        , m_iterationLexicalBlockIndex(iterationLexicalBlockIndex)
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

    void generateBodyByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeGenerateContext& newContext)
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        // per iteration block
        size_t iterationLexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext iterationBlockContext;
        uint8_t tmpIdentifierNode[sizeof(IdentifierNode)];

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                IdentifierNode* id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[i].m_name);
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
                IdentifierNode* id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }

        m_body->generateStatementByteCode(codeBlock, &newContext);

        if (m_iterationLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_iterationLexicalBlockIndex);
            std::vector<size_t> nameRegisters;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                nameRegisters.push_back(newContext.getRegister());
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                IdentifierNode* id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[i].m_name);
                id->m_loc = m_loc;
                id->generateExpressionByteCode(codeBlock, &newContext, nameRegisters[i]);
            }

            codeBlock->finalizeLexicalBlock(&newContext, iterationBlockContext);
            newContext.m_lexicalBlockIndex = iterationLexicalBlockIndexBefore;

            size_t reverse = bi->m_identifiers.size() - 1;
            for (size_t i = 0; i < bi->m_identifiers.size(); i++, reverse--) {
                IdentifierNode* id = new (tmpIdentifierNode) IdentifierNode(bi->m_identifiers[reverse].m_name);
                id->m_loc = m_loc;
                newContext.m_isLexicallyDeclaredBindingInitialization = m_hasLexicalDeclarationOnInit;
                id->generateStoreByteCode(codeBlock, &newContext, nameRegisters[reverse], true);
                ASSERT(!newContext.m_isLexicallyDeclaredBindingInitialization);
                newContext.giveUpRegister();
            }
        }
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        // https://www.ecma-international.org/ecma-262/10.0/#sec-runtime-semantics-forin-div-ofbodyevaluation-lhs-stmt-iterator-lhskind-labelset
        if (context->shouldCareScriptExecutionResult()) {
            // 13.7.5.13 Runtime Semantics: ForIn/OfBodyEvaluation
            // 2. Let V = undefined.
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), 0, Value()), context, this);
        }

        bool canSkipCopyToRegisterBefore = context->m_canSkipCopyToRegister;
        context->m_canSkipCopyToRegister = false;

        size_t headLexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext headBlockContext;
        if (m_headLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_headLexicalBlockIndex;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_headLexicalBlockIndex);
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
        TryStatementNode::TryStatementByteCodeContext forOfTryStatementContext;
        ByteCodeRegisterIndex finishCheckRegisterIndex = REGISTER_LIMIT, iteratorRecordRegisterIndex = REGISTER_LIMIT, iteratorObjectRegisterIndex = REGISTER_LIMIT;
        size_t forOfEndCheckRegisterHeadStartPosition = SIZE_MAX;
        size_t forOfEndCheckRegisterHeadEndPosition = SIZE_MAX;
        size_t forOfEndCheckRegisterBodyEndPosition = SIZE_MAX;

        if (m_forIn) {
            // for-in statement
            auto oldRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;

            size_t headRightLexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
            ByteCodeBlock::ByteCodeLexicalBlockContext headRightBlockContext;
            if (m_headRightLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                newContext.m_lexicalBlockIndex = m_headRightLexicalBlockIndex;
                InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_headRightLexicalBlockIndex);
                headRightBlockContext = codeBlock->pushLexicalBlock(&newContext, bi, this);
            }
            size_t rightIdx = m_right->getRegister(codeBlock, &newContext);
            m_right->generateExpressionByteCode(codeBlock, &newContext, rightIdx);
            if (m_headRightLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                codeBlock->finalizeLexicalBlock(&newContext, headRightBlockContext);
                newContext.m_lexicalBlockIndex = headRightLexicalBlockIndexBefore;
            }

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
            codeBlock->pushCode(CreateEnumerateObject(ByteCodeLOC(m_loc.index)), &newContext, this);
            codeBlock->peekCode<CreateEnumerateObject>(ePosition)->m_objectRegisterIndex = rightIdx;
            // drop rightIdx
            newContext.giveUpRegister();

            ASSERT(newContext.m_registerStack->size() == baseCountBefore);
            continuePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(CheckLastEnumerateKey(ByteCodeLOC(m_loc.index)), &newContext, this);

            size_t checkPos = exit3Pos = codeBlock->lastCodePosition<CheckLastEnumerateKey>();
            codeBlock->pushCode(GetEnumerateKey(ByteCodeLOC(m_loc.index)), &newContext, this);
            size_t enumerateObjectKeyPos = codeBlock->lastCodePosition<GetEnumerateKey>();

            codeBlock->peekCode<GetEnumerateKey>(enumerateObjectKeyPos)->m_registerIndex = newContext.getRegister();

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

            codeBlock->peekCode<CreateEnumerateObject>(ePosition)->m_dataRegisterIndex = dataRegisterIndex;
            codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_registerIndex = dataRegisterIndex;
            codeBlock->peekCode<GetEnumerateKey>(enumerateObjectKeyPos)->m_dataRegisterIndex = dataRegisterIndex;
        } else {
            // for-of statement
            TryStatementNode::generateTryStatementStartByteCode(codeBlock, &newContext, this, forOfTryStatementContext);

            auto oldRequiredRegisterFileSizeInValueSize = codeBlock->m_requiredRegisterFileSizeInValueSize;
            codeBlock->m_requiredRegisterFileSizeInValueSize = 0;

            forOfEndCheckRegisterHeadStartPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), SIZE_MAX, Value(true)), &newContext, this);

            size_t headRightLexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
            ByteCodeBlock::ByteCodeLexicalBlockContext headRightBlockContext;
            if (m_headRightLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                newContext.m_lexicalBlockIndex = m_headRightLexicalBlockIndex;
                InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_headRightLexicalBlockIndex);
                headRightBlockContext = codeBlock->pushLexicalBlock(&newContext, bi, this);
            }
            size_t rightIdx = m_right->getRegister(codeBlock, &newContext);
            m_right->generateExpressionByteCode(codeBlock, &newContext, rightIdx);
            if (m_headRightLexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
                codeBlock->finalizeLexicalBlock(&newContext, headRightBlockContext);
                newContext.m_lexicalBlockIndex = headRightLexicalBlockIndexBefore;
            }

            size_t getIteratorOperationPosition = codeBlock->currentCodeSize();

            IteratorOperation::GetIteratorData data;
            data.m_isSyncIterator = !m_isForAwaitOf;
            data.m_srcObjectRegisterIndex = rightIdx;
            data.m_dstIteratorRecordIndex = REGISTER_LIMIT;
            data.m_dstIteratorObjectIndex = REGISTER_LIMIT;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), data), &newContext, this);

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

            // Let nextResult be ? Call(iteratorRecord.[[NextMethod]], iteratorRecord.[[Iterator]], « »).
            IteratorOperation::IteratorNextData iteratorNextData;
            iteratorNextData.m_iteratorRecordRegisterIndex = REGISTER_LIMIT;
            iteratorNextData.m_valueRegisterIndex = REGISTER_LIMIT;
            iteratorNextData.m_returnRegisterIndex = newContext.getRegister();

            size_t iteratorNextOperationPos = codeBlock->currentCodeSize();
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorNextData), &newContext, this);

            // If iteratorKind is async, then set nextResult to ? Await(nextResult).
            if (m_isForAwaitOf) {
                codeBlock->updateMaxPauseStatementExtraDataLength(&newContext);
                size_t tailDataLength = newContext.m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));
                ExecutionPause::ExecutionPauseAwaitData data;
                data.m_awaitIndex = iteratorNextData.m_returnRegisterIndex;
                data.m_dstIndex = iteratorNextData.m_returnRegisterIndex;
                data.m_dstStateIndex = REGISTER_LIMIT;
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), &newContext, this);
            }
            // If Type(nextResult) is not Object, throw a TypeError exception.
            IteratorOperation::IteratorTestResultIsObjectData iteratorTestResultIsObjectData;
            iteratorTestResultIsObjectData.m_valueRegisterIndex = iteratorNextData.m_returnRegisterIndex;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestResultIsObjectData), &newContext, this);

            // Let done be ? IteratorComplete(nextResult).
            size_t doneRegister = newContext.getRegister();
            IteratorOperation::IteratorTestDoneData iteratorTestDoneData;
            iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex = iteratorNextData.m_returnRegisterIndex;
            iteratorTestDoneData.m_dstRegisterIndex = doneRegister;
            iteratorTestDoneData.m_isIteratorRecord = false;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestDoneData), &newContext, this);

            // If done is true, return NormalCompletion(V).
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), doneRegister), &newContext, this);
            exit3Pos = codeBlock->lastCodePosition<JumpIfTrue>();
            newContext.giveUpRegister(); // drop doneRegister

            // Let nextValue be ? IteratorValue(nextResult).
            IteratorOperation::IteratorValueData iteratorValueData;
            iteratorValueData.m_srcRegisterIndex = iteratorNextData.m_returnRegisterIndex;
            iteratorValueData.m_dstRegisterIndex = iteratorNextData.m_returnRegisterIndex;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorValueData), &newContext, this);

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
            iteratorRecordRegisterIndex = std::max(headRequiredRegisterFileSizeInValueSize, bodyRequiredRegisterFileSizeInValueSize);
            iteratorObjectRegisterIndex = iteratorRecordRegisterIndex + 1;
            finishCheckRegisterIndex = iteratorRecordRegisterIndex + 2;

            codeBlock->m_requiredRegisterFileSizeInValueSize = std::max({ oldRequiredRegisterFileSizeInValueSize, codeBlock->m_requiredRegisterFileSizeInValueSize, (ByteCodeRegisterIndex)(iteratorRecordRegisterIndex + 3) });

            codeBlock->peekCode<IteratorOperation>(getIteratorOperationPosition)->m_getIteratorData.m_dstIteratorRecordIndex = iteratorRecordRegisterIndex;
            codeBlock->peekCode<IteratorOperation>(getIteratorOperationPosition)->m_getIteratorData.m_dstIteratorObjectIndex = iteratorObjectRegisterIndex;
            codeBlock->peekCode<IteratorOperation>(iteratorNextOperationPos)->m_iteratorNextData.m_iteratorRecordRegisterIndex = iteratorRecordRegisterIndex;
        }

        size_t blockExitPos = codeBlock->currentCodeSize();

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), continuePosition), &newContext, this);
        size_t exitPos = codeBlock->currentCodeSize();
        ASSERT(codeBlock->peekCode<CheckLastEnumerateKey>(continuePosition)->m_orgOpcode == CheckLastEnumerateKeyOpcode || codeBlock->peekCode<IteratorOperation>(continuePosition)->m_orgOpcode == IteratorOperationOpcode);

        // we need to add 1 on third parameter because we add try operation manually
        newContext.consumeBreakPositions(codeBlock, exitPos, context->tryCatchWithBlockStatementCount() + (m_forIn ? 0 : 1));
        newContext.consumeContinuePositions(codeBlock, continuePosition, context->tryCatchWithBlockStatementCount() + (m_forIn ? 0 : 1));
        newContext.m_positionToContinue = continuePosition;

        if (!m_forIn) {
            TryStatementNode::generateTryStatementBodyEndByteCode(codeBlock, &newContext, this, forOfTryStatementContext);
            TryStatementNode::generateTryFinalizerStatementStartByteCode(codeBlock, &newContext, this, forOfTryStatementContext, true);

            size_t exceptionThrownCheckStartJumpPos = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), finishCheckRegisterIndex, SIZE_MAX), &newContext, this);

            if (m_isForAwaitOf) {
                // AsyncIteratorClose ( iteratorRecord, completion )
                // The abstract operation AsyncIteratorClose with arguments iteratorRecord and completion is used to notify an async iterator that it should perform any actions it would normally perform when it has reached its completed state:

                // Assert: Type(iteratorRecord.[[Iterator]]) is Object.
                // Assert: completion is a Completion Record.
                // Let iterator be iteratorRecord.[[Iterator]].
                // Let return be ? GetMethod(iterator, "return").
                size_t returnOrInnerResultRegister = newContext.getRegister();
                codeBlock->pushCode(GetMethod(ByteCodeLOC(m_loc.index), iteratorObjectRegisterIndex, returnOrInnerResultRegister, codeBlock->m_codeBlock->context()->staticStrings().stringReturn), &newContext, this);

                // If return is undefined, return Completion(completion).
                size_t returnUndefinedTestRegister = newContext.getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, Value()), &newContext, this);
                size_t returnUndefinedCompareJump = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, returnOrInnerResultRegister, false, false), &newContext, this);
                newContext.giveUpRegister(); // drop returnUndefinedTestRegister

                // Let innerResult be Call(return, iterator, « »).
                // If innerResult.[[Type]] is normal, set innerResult to Await(innerResult.[[Value]]).
                // If completion.[[Type]] is throw, return Completion(completion).
                // If innerResult.[[Type]] is throw, return Completion(innerResult).
                // If Type(innerResult.[[Value]]) is not Object, throw a TypeError exception.
                // Return Completion(completion).

                /* we can write rest of AsyncIteratorClose code as like this
                throwTest = false;

                try {
                  innerResult = call(return, iterator, « »)
                  throwTest = true
                } catch() {
                }

                if (throwTest) {
                  innerResult = await innerResult;
                }

                %IteratorOperation(checkOngoingException)%

                if (!throwTest || awaitReturnsThrow) {
                  throw innerResult;
                }

                %IteratorOperation(TestResultIsObject)%
                */

                // throwTest = false
                size_t throwTestRegister = newContext.getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(false)), &newContext, this);

                // try {
                TryStatementNode::TryStatementByteCodeContext callingReturnContext;
                TryStatementNode::generateTryStatementStartByteCode(codeBlock, &newContext, this, callingReturnContext);
                // innerResult = call(..)
                codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), iteratorObjectRegisterIndex, returnOrInnerResultRegister, REGISTER_LIMIT, returnOrInnerResultRegister, 0), &newContext, this);
                // throwTest = true;
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(true)), &newContext, this);
                // } catch () {}
                TryStatementNode::generateTryStatementBodyEndByteCode(codeBlock, &newContext, this, callingReturnContext);
                TryStatementNode::generateTryFinalizerStatementStartByteCode(codeBlock, &newContext, this, callingReturnContext, false);
                TryStatementNode::generateTryFinalizerStatementEndByteCode(codeBlock, &newContext, this, callingReturnContext, false);

                size_t awaitStateRegister = newContext.getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), awaitStateRegister, Value(ExecutionPauser::ResumeState::Normal)), &newContext, this);

                // if (throwTest) {
                size_t tempTestPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), throwTestRegister), &newContext, this);
                // innerResult = await innerResult;
                ExecutionPause::ExecutionPauseAwaitData data;
                data.m_awaitIndex = returnOrInnerResultRegister;
                data.m_dstIndex = returnOrInnerResultRegister;
                data.m_dstStateIndex = awaitStateRegister;
                size_t tailDataLength = newContext.m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), &newContext, this);
                // }
                codeBlock->peekCode<JumpIfFalse>(tempTestPos)->m_jumpPosition = codeBlock->currentCodeSize();

                // %IteratorOperation(checkOngoingException)%
                IteratorOperation::IteratorCheckOngoingExceptionOnAsyncIteratorCloseData iteratorCheckOngoingExceptionData;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorCheckOngoingExceptionData), &newContext, this);

                // if (!throwTest || awaitReturnsThrow) {
                size_t throwTestIsTrueJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), throwTestRegister), &newContext, this);

                // we don't needThrowTestRegister from here
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(ExecutionPauser::ResumeState::Throw)), &newContext, this);

                size_t awaitNotReturnsThrowPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), throwTestRegister, awaitStateRegister, false, false), &newContext, this);

                // throw innerResult;
                codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), returnOrInnerResultRegister), &newContext, this);
                // }
                codeBlock->peekCode<JumpIfFalse>(throwTestIsTrueJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
                codeBlock->peekCode<JumpIfEqual>(awaitNotReturnsThrowPos)->m_jumpPosition = codeBlock->currentCodeSize();

                // %IteratorOperation(TestResultIsObject)%
                IteratorOperation::IteratorTestResultIsObjectData iteratorTestResultIsObjectData;
                iteratorTestResultIsObjectData.m_valueRegisterIndex = returnOrInnerResultRegister;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestResultIsObjectData), &newContext, this);

                newContext.giveUpRegister(); // drop awaitStateRegister
                newContext.giveUpRegister(); // drop throwTestRegister
                newContext.giveUpRegister(); // drop returnOrInnerResultRegister

                codeBlock->peekCode<Jump>(returnUndefinedCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();
            } else {
                IteratorOperation::IteratorCloseData iteratorCloseData;
                iteratorCloseData.m_iterRegisterIndex = iteratorRecordRegisterIndex;
                iteratorCloseData.m_execeptionRegisterIndexIfExists = REGISTER_LIMIT;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorCloseData), &newContext, this);
            }

            codeBlock->peekCode<JumpIfTrue>(exceptionThrownCheckStartJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
            TryStatementNode::generateTryFinalizerStatementEndByteCode(codeBlock, &newContext, this, forOfTryStatementContext, true);

            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterHeadStartPosition)->m_registerIndex = finishCheckRegisterIndex;
            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterHeadEndPosition)->m_registerIndex = finishCheckRegisterIndex;
            codeBlock->peekCode<LoadLiteral>(forOfEndCheckRegisterBodyEndPosition)->m_registerIndex = finishCheckRegisterIndex;
        }

        codeBlock->peekCode<JumpIfTrue>(exit1Pos)->m_jumpPosition = exitPos;
        codeBlock->peekCode<JumpIfTrue>(exit2Pos)->m_jumpPosition = exitPos;
        if (m_forIn) {
            codeBlock->peekCode<CheckLastEnumerateKey>(exit3Pos)->m_exitPosition = exitPos;
        } else {
            codeBlock->peekCode<JumpIfTrue>(exit3Pos)->m_jumpPosition = exitPos;
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

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
        m_body->iterateChildren(fn);
    }

private:
    Node* m_left;
    Node* m_right;
    Node* m_body;
    bool m_forIn;
    bool m_hasLexicalDeclarationOnInit;
    bool m_isForAwaitOf;
    LexicalBlockIndex m_headLexicalBlockIndex;
    LexicalBlockIndex m_headRightLexicalBlockIndex;
    LexicalBlockIndex m_iterationLexicalBlockIndex;
};
}

#endif
