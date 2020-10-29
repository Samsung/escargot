/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef YieldExpressionNode_h
#define YieldExpressionNode_h

#include "ExpressionNode.h"
#include "TryStatementNode.h"
#include "ReturnStatementNode.h"
#include "runtime/ExecutionPauser.h"

namespace Escargot {

class YieldExpressionNode : public ExpressionNode {
public:
    YieldExpressionNode(Node* argument, bool isDelegate)
        : ExpressionNode()
        , m_argument(argument)
        , m_isDelegate(isDelegate)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::YieldExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        // https://www.ecma-international.org/ecma-262/10.0/#sec-generator-function-definitions-runtime-semantics-evaluation

        codeBlock->updateMaxPauseStatementExtraDataLength(context);
        size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));
        bool isAsyncGenerator = codeBlock->m_codeBlock->isAsync() && codeBlock->m_codeBlock->isGenerator();

        if (m_isDelegate) {
            size_t argIdx = m_argument->getRegister(codeBlock, context);
            m_argument->generateExpressionByteCode(codeBlock, context, argIdx);

            size_t iteratorRecordIdx = context->getRegister();
            size_t iteratorObjectIdx = context->getRegister();
            size_t valueIdx = context->getRegister();

            // we use state register to record result of each yield operation(normal[0], throw[1], return[2])
            size_t stateIdx = context->getRegister();

            bool isSyncIterator = !codeBlock->m_codeBlock->isAsync();
            // Let iteratorRecord be ? GetIterator(value, generatorKind).

            IteratorOperation::GetIteratorData data;
            data.m_isSyncIterator = isSyncIterator;
            data.m_srcObjectRegisterIndex = argIdx;
            data.m_dstIteratorRecordIndex = iteratorRecordIdx;
            data.m_dstIteratorObjectIndex = iteratorObjectIdx;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), data), context, this);

            // Let received be NormalCompletion(undefined).
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), valueIdx, Value()), context, this);
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateIdx, Value(ExecutionPauser::ResumeState::Normal)), context, this);

            // Repeat,
            size_t loopStart = codeBlock->currentCodeSize();

            // If received.[[Type]] is normal, then
            size_t stateCompareRegister = context->getRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCompareRegister, Value(ExecutionPauser::ResumeState::Normal)), context, this);
            size_t normalStateCompareJump = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCompareRegister, stateIdx, false, true), context, this);

            // Let innerResult be ? Call(iteratorRecord.[[NextMethod]], iteratorRecord.[[Iterator]], « received.[[Value]] »).
            IteratorOperation::IteratorNextData iteratorNextData;
            iteratorNextData.m_iteratorRecordRegisterIndex = iteratorRecordIdx;
            iteratorNextData.m_valueRegisterIndex = valueIdx;
            iteratorNextData.m_returnRegisterIndex = valueIdx;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorNextData), context, this);

            // If generatorKind is async, then set innerResult to ? Await(innerResult).
            if (isAsyncGenerator) {
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
            }

            size_t iterationDoneStart = codeBlock->currentCodeSize();
            // If Type(innerResult) is not Object, throw a TypeError exception.
            IteratorOperation::IteratorTestResultIsObjectData iteratorTestResultIsObjectData;
            iteratorTestResultIsObjectData.m_valueRegisterIndex = valueIdx;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestResultIsObjectData), context, this);

            // Let done be ? IteratorComplete(innerResult).
            size_t doneIndex = context->getRegister();
            IteratorOperation::IteratorTestDoneData iteratorTestDoneData;
            iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex = valueIdx;
            iteratorTestDoneData.m_dstRegisterIndex = doneIndex;
            iteratorTestDoneData.m_isIteratorRecord = false;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestDoneData), context, this);
            // If done is true, then
            // Return ? IteratorValue(innerResult).
            codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), doneIndex), context, this);
            size_t testDoneJumpPos = codeBlock->lastCodePosition<JumpIfFalse>();
            IteratorOperation::IteratorValueData iteratorValueData;
            iteratorValueData.m_srcRegisterIndex = valueIdx;
            iteratorValueData.m_dstRegisterIndex = dstRegister;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorValueData), context, this);

            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
            size_t expressionEndJumpPos = codeBlock->lastCodePosition<Jump>();
            context->giveUpRegister(); // drop doneIndex

            codeBlock->peekCode<Jump>(testDoneJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

            // If generatorKind is async, then set received to AsyncGeneratorYield(? IteratorValue(innerResult)).
            if (isAsyncGenerator) {
                iteratorValueData.m_srcRegisterIndex = valueIdx;
                iteratorValueData.m_dstRegisterIndex = valueIdx;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorValueData), context, this);
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
                pushYield(codeBlock, context, valueIdx, valueIdx, stateIdx, tailDataLength, isAsyncGenerator, false);
            } else {
                // Else, set received to GeneratorYield(innerResult).
                pushYield(codeBlock, context, valueIdx, valueIdx, stateIdx, tailDataLength, isAsyncGenerator, false);
            }

            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), loopStart), context, this); // end loop

            // Else if received.[[Type]] is throw, then
            codeBlock->peekCode<JumpIfEqual>(normalStateCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCompareRegister, Value(ExecutionPauser::ResumeState::Throw)), context, this);
            size_t throwStateCompareJump = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCompareRegister, stateIdx, false, true), context, this);
            // Let throw be ? GetMethod(iterator, "throw").
            size_t throwRegister = context->getRegister();
            codeBlock->pushCode(GetMethod(ByteCodeLOC(m_loc.index), iteratorObjectIdx, throwRegister, codeBlock->m_codeBlock->context()->staticStrings().stringThrow), context, this);
            // If throw is not undefined, then
            size_t throwUndefinedTestRegister = context->getRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwUndefinedTestRegister, Value()), context, this);
            size_t throwUndefinedCompareJump = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), throwUndefinedTestRegister, throwRegister, true, false), context, this);
            context->giveUpRegister(); // for drop throwUndefinedTestRegister
            // Let innerResult be ? Call(throw, iterator, « received.[[Value]] »).
            codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), iteratorObjectIdx, throwRegister, valueIdx, valueIdx, 1), context, this);
            // If generatorKind is async, then set innerResult to ? Await(innerResult).
            if (isAsyncGenerator) {
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
            }

            // NOTE below lines are same with <<received.[[Type]] is normal>> path
            // We can reuse it.
            // If Type(innerResult) is not Object, throw a TypeError exception.
            // Let done be ? IteratorComplete(innerResult).
            // If done is true, then
            // Return ? IteratorValue(innerResult).
            // If generatorKind is async, then set received to AsyncGeneratorYield(? IteratorValue(innerResult)).
            // Else, set received to GeneratorYield(innerResult).
            // -> jump to above
            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), iterationDoneStart), context, this);

            // Else
            codeBlock->peekCode<JumpIfEqual>(throwUndefinedCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();

            // Let closeCompletion be Completion { [[Type]]: normal, [[Value]]: empty, [[Target]]: empty }.
            // If generatorKind is async, perform ? AsyncIteratorClose(iteratorRecord, closeCompletion).
            if (isAsyncGenerator) {
                // AsyncIteratorClose ( iteratorRecord, completion )
                // Let iterator be iteratorRecord.[[Iterator]].
                // Let return be ? GetMethod(iterator, "return").
                size_t returnOrInnerResultRegister = context->getRegister();
                codeBlock->pushCode(GetMethod(ByteCodeLOC(m_loc.index), iteratorObjectIdx, returnOrInnerResultRegister, codeBlock->m_codeBlock->context()->staticStrings().stringReturn), context, this);
                // NOTE yield expression doesn't use completion value of AsyncIteratorClose
                // If return is undefined, return Completion(completion).
                size_t returnUndefinedTestRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, Value()), context, this);
                size_t returnUndefinedCompareJump = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, returnOrInnerResultRegister, true, true), context, this);
                context->giveUpRegister(); // drop returnUndefinedTestRegister

                // Let innerResult be Call(return, iterator, « »).
                // If innerResult.[[Type]] is normal, set   to Await(innerResult.[[Value]]).
                // If completion.[[Type]] is throw, return Completion(completion).
                // If innerResult.[[Type]] is throw, return Completion(innerResult).
                // If Type(innerResult.[[Value]]) is not Object, throw a TypeError exception.
                // Return Completion(completion).
                /*
                we can write rest of AsyncIterator code as like this
                throwTest = false;
                try {
                  innerResult = call(...)
                  throwTest = true
                } catch() {
                }

                if (throwTest) {
                  innerResult = await innerResult;
                }

                if (!throwTest || awaitReturnsThrow) {
                  throw innerResult;
                }

                %IteratorOperation(TestResultIsObject)%
                */

                // throwTest = false
                size_t throwTestRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(false)), context, this);

                // try {
                TryStatementNode::TryStatementByteCodeContext callingReturnContext;
                TryStatementNode::generateTryStatementStartByteCode(codeBlock, context, this, callingReturnContext);
                // innerResult = call(..)
                codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), iteratorObjectIdx, returnOrInnerResultRegister, REGISTER_LIMIT, returnOrInnerResultRegister, 0), context, this);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(true)), context, this);
                // } catch () {}
                TryStatementNode::generateTryStatementBodyEndByteCode(codeBlock, context, this, callingReturnContext);
                TryStatementNode::generateTryFinalizerStatementStartByteCode(codeBlock, context, this, callingReturnContext, false);
                TryStatementNode::generateTryFinalizerStatementEndByteCode(codeBlock, context, this, callingReturnContext, false);

                size_t awaitStateRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), awaitStateRegister, Value(ExecutionPauser::ResumeState::Normal)), context, this);

                // if (throwTest) {
                size_t tempTestPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), throwTestRegister), context, this);
                // innerResult = await innerResult;
                pushAwait(codeBlock, context, returnOrInnerResultRegister, returnOrInnerResultRegister, awaitStateRegister, tailDataLength);
                // }
                codeBlock->peekCode<JumpIfFalse>(tempTestPos)->m_jumpPosition = codeBlock->currentCodeSize();

                // if (!throwTest || awaitReturnsThrow) {
                size_t throwTestIsTrueJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), throwTestRegister), context, this);

                // we don't needThrowTestRegister from here
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), throwTestRegister, Value(ExecutionPauser::ResumeState::Throw)), context, this);

                size_t awaitNotReturnsThrowPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), throwTestRegister, awaitStateRegister, false, false), context, this);

                // throw innerResult;
                codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), returnOrInnerResultRegister), context, this);
                // }
                codeBlock->peekCode<JumpIfFalse>(throwTestIsTrueJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
                codeBlock->peekCode<JumpIfEqual>(awaitNotReturnsThrowPos)->m_jumpPosition = codeBlock->currentCodeSize();

                // %IteratorOperation(TestResultIsObject)%
                IteratorOperation::IteratorTestResultIsObjectData iteratorTestResultIsObjectData;
                iteratorTestResultIsObjectData.m_valueRegisterIndex = returnOrInnerResultRegister;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestResultIsObjectData), context, this);

                context->giveUpRegister(); // drop awaitStateRegister
                context->giveUpRegister(); // drop throwTestRegister
                context->giveUpRegister(); // drop returnOrInnerResultRegister

                codeBlock->peekCode<Jump>(returnUndefinedCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();
            } else {
                // Else, perform ? IteratorClose(iteratorRecord, closeCompletion).
                IteratorOperation::IteratorCloseData iteratorCloseData;
                iteratorCloseData.m_iterRegisterIndex = iteratorRecordIdx;
                iteratorCloseData.m_execeptionRegisterIndexIfExists = REGISTER_LIMIT;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorCloseData), context, this);
            }

            // NOTE: The next step throws a TypeError to indicate that there was a yield* protocol violation: iterator does not have a throw method.
            // Throw a TypeError exception.
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, "yield* violation. there is no throw method on iterator Object"), context, this);

            context->giveUpRegister(); // for drop throwRegister

            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), loopStart), context, this); // end loop

            // Else
            codeBlock->peekCode<JumpIfEqual>(throwStateCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();
            // Let return be ? GetMethod(iterator, "return").
            size_t returnRegister = context->getRegister();
            codeBlock->pushCode(GetMethod(ByteCodeLOC(m_loc.index), iteratorObjectIdx, returnRegister, codeBlock->m_codeBlock->context()->staticStrings().stringReturn), context, this);

            // If return is undefined, then
            size_t returnUndefinedTestRegister = context->getRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, Value()), context, this);
            size_t returnUndefinedCompareJump = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), returnUndefinedTestRegister, returnRegister, true, true), context, this);
            context->giveUpRegister(); // drop returnUndefinedTestRegister

            // If generatorKind is async, then set received.[[Value]] to ? Await(received.[[Value]]).
            if (isAsyncGenerator) {
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
            }
            // Return Completion(received).
            ReturnStatementNode::generateReturnCode(codeBlock, context, this, ByteCodeLOC(m_loc.index), valueIdx);
            codeBlock->peekCode<JumpIfTrue>(returnUndefinedCompareJump)->m_jumpPosition = codeBlock->currentCodeSize();

            // Let innerReturnResult be ? Call(return, iterator, « received.[[Value]] »).
            codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), iteratorObjectIdx, returnRegister, valueIdx, valueIdx, 1), context, this);
            // If generatorKind is async, then set innerReturnResult to ? Await(innerReturnResult).
            if (isAsyncGenerator) {
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
            }

            // If Type(innerReturnResult) is not Object, throw a TypeError exception.
            iteratorTestResultIsObjectData.m_valueRegisterIndex = valueIdx;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestResultIsObjectData), context, this);

            // Let done be ? IteratorComplete(innerReturnResult).
            doneIndex = context->getRegister();
            iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex = valueIdx;
            iteratorTestDoneData.m_dstRegisterIndex = doneIndex;
            iteratorTestDoneData.m_isIteratorRecord = false;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorTestDoneData), context, this);

            // If done is true, then
            testDoneJumpPos = codeBlock->currentCodeSize();
            codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), doneIndex), context, this);

            // Let value be ? IteratorValue(innerReturnResult).
            iteratorValueData.m_srcRegisterIndex = valueIdx;
            iteratorValueData.m_dstRegisterIndex = valueIdx;
            codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorValueData), context, this);
            // Return Completion { [[Type]]: return, [[Value]]: value, [[Target]]: empty }.
            ReturnStatementNode::generateReturnCode(codeBlock, context, this, ByteCodeLOC(m_loc.index), valueIdx);
            codeBlock->peekCode<JumpIfFalse>(testDoneJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

            // If generatorKind is async, then set received to AsyncGeneratorYield(? IteratorValue(innerReturnResult)).
            if (isAsyncGenerator) {
                iteratorValueData.m_srcRegisterIndex = valueIdx;
                iteratorValueData.m_dstRegisterIndex = valueIdx;
                codeBlock->pushCode(IteratorOperation(ByteCodeLOC(m_loc.index), iteratorValueData), context, this);
                pushAwait(codeBlock, context, valueIdx, valueIdx, REGISTER_LIMIT, tailDataLength);
                pushYield(codeBlock, context, valueIdx, valueIdx, stateIdx, tailDataLength, isAsyncGenerator, false);
            } else {
                // Else, set received to GeneratorYield(innerReturnResult).
                pushYield(codeBlock, context, valueIdx, valueIdx, stateIdx, tailDataLength, isAsyncGenerator, false);
            }

            context->giveUpRegister(); // for drop doneIndex
            context->giveUpRegister(); // for drop returnRegister
            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), loopStart), context, this); // end loop

            context->giveUpRegister(); // for drop stateCompareRegister
            context->giveUpRegister(); // for drop stateIdx
            context->giveUpRegister(); // for drop valueIdx
            context->giveUpRegister(); // for drop iteratorObjectIdx
            context->giveUpRegister(); // for drop iteratorRecordIdx

            codeBlock->peekCode<Jump>(expressionEndJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

            context->giveUpRegister(); // for drop argIdx
        } else {
            size_t argIdx;
            if (m_argument) {
                argIdx = m_argument->getRegister(codeBlock, context);
                m_argument->generateExpressionByteCode(codeBlock, context, argIdx);
            } else {
                argIdx = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), argIdx, Value()), context, this);
            }

            if (isAsyncGenerator) {
                pushAwait(codeBlock, context, argIdx, dstRegister, REGISTER_LIMIT, tailDataLength);
                pushYield(codeBlock, context, dstRegister, dstRegister, REGISTER_LIMIT, tailDataLength, isAsyncGenerator, false);
            } else {
                pushYield(codeBlock, context, argIdx, dstRegister, REGISTER_LIMIT, tailDataLength, isAsyncGenerator, true);
            }

            context->giveUpRegister(); // for drop argIdx
        }
    }

    void pushYield(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex valueIndex, ByteCodeRegisterIndex dstRegister, ByteCodeRegisterIndex dstStateRegister, size_t tailDataLength, bool isAsyncGenerator, bool needsToWrapYieldValueWithIterResultObject)
    {
        bool dstStateRegisterCreated = false;
        if (isAsyncGenerator && dstStateRegister == REGISTER_LIMIT) {
            dstStateRegisterCreated = true;
            dstStateRegister = context->getRegister();
        }

        ExecutionPause::ExecutionPauseYieldData data;
        data.m_yieldIndex = valueIndex;
        data.m_dstIndex = dstRegister;
        data.m_dstStateIndex = dstStateRegister;
        data.m_tailDataLength = tailDataLength;
        data.m_needsToWrapYieldValueWithIterResultObject = needsToWrapYieldValueWithIterResultObject;
        codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);

        if (isAsyncGenerator) {
            // https://www.ecma-international.org/ecma-262/10.0/#sec-asyncgeneratoryield
            if (dstStateRegisterCreated) {
                // If resumptionValue.[[Type]] is not return, return Completion(resumptionValue).
                ByteCodeRegisterIndex stateCheckRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCheckRegister, Value(ExecutionPauser::ResumeState::Normal)), context, this);
                size_t stateIsNormalJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCheckRegister, dstStateRegister, false, false), context, this);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCheckRegister, Value(ExecutionPauser::ResumeState::Throw)), context, this);
                size_t stateIsNotThrowJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCheckRegister, dstStateRegister, false, true), context, this);
                codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), dstRegister), context, this);
                codeBlock->peekCode<JumpIfEqual>(stateIsNotThrowJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

                // Let awaited be Await(resumptionValue.[[Value]]).
                ExecutionPause::ExecutionPauseAwaitData data;
                data.m_awaitIndex = dstRegister;
                data.m_dstIndex = dstRegister;
                data.m_dstStateIndex = dstStateRegister;
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);

                // If awaited.[[Type]] is throw, return Completion(awaited).
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCheckRegister, Value(ExecutionPauser::ResumeState::Throw)), context, this);
                stateIsNotThrowJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCheckRegister, dstStateRegister, false, true), context, this);
                codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), dstRegister), context, this);
                codeBlock->peekCode<JumpIfEqual>(stateIsNotThrowJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
                // Assert: awaited.[[Type]] is normal.
                // Return Completion { [[Type]]: return, [[Value]]: awaited.[[Value]], [[Target]]: empty }.
                // NOTE: When one of the above steps returns, it returns to the evaluation of the YieldExpression production that originally called this abstract operation.
                ReturnStatementNode::generateReturnCode(codeBlock, context, this, ByteCodeLOC(m_loc.index), dstRegister);

                codeBlock->peekCode<JumpIfEqual>(stateIsNormalJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

                context->giveUpRegister(); // for drop stateCheckRegister
            } else {
                // If resumptionValue.[[Type]] is not return, return Completion(resumptionValue).
                ByteCodeRegisterIndex stateCheckRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCheckRegister, Value(ExecutionPauser::ResumeState::Return)), context, this);
                size_t stateIsNotReturnJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCheckRegister, dstStateRegister, false, true), context, this);

                // Let awaited be Await(resumptionValue.[[Value]]).
                ExecutionPause::ExecutionPauseAwaitData data;
                data.m_awaitIndex = dstRegister;
                data.m_dstIndex = dstRegister;
                data.m_dstStateIndex = dstStateRegister;
                data.m_tailDataLength = tailDataLength;
                codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);

                // If awaited.[[Type]] is throw, return Completion(awaited).
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stateCheckRegister, Value(ExecutionPauser::ResumeState::Throw)), context, this);
                size_t stateIsNotThrowJumpPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(JumpIfEqual(ByteCodeLOC(m_loc.index), stateCheckRegister, dstStateRegister, false, true), context, this);
                size_t jumpToEndNotThrow = codeBlock->currentCodeSize();
                codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);

                codeBlock->peekCode<JumpIfEqual>(stateIsNotThrowJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
                // Assert: awaited.[[Type]] is normal.
                // Return Completion { [[Type]]: return, [[Value]]: awaited.[[Value]], [[Target]]: empty }.
                // NOTE: When one of the above steps returns, it returns to the evaluation of the YieldExpression production that originally called this abstract operation.
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstStateRegister, Value(ExecutionPauser::ResumeState::Return)), context, this);

                codeBlock->peekCode<Jump>(jumpToEndNotThrow)->m_jumpPosition = codeBlock->currentCodeSize();
                codeBlock->peekCode<JumpIfEqual>(stateIsNotReturnJumpPos)->m_jumpPosition = codeBlock->currentCodeSize();

                context->giveUpRegister(); // for drop stateCheckRegister
            }
        }

        if (dstStateRegisterCreated) {
            context->giveUpRegister();
        }
    }

    void pushAwait(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex valueIndex, ByteCodeRegisterIndex dstRegister, ByteCodeRegisterIndex dstStateRegister, size_t tailDataLength)
    {
        ExecutionPause::ExecutionPauseAwaitData data;
        data.m_awaitIndex = valueIndex;
        data.m_dstIndex = dstRegister;
        data.m_dstStateIndex = dstStateRegister;
        data.m_tailDataLength = tailDataLength;
        codeBlock->pushCode(ExecutionPause(ByteCodeLOC(m_loc.index), data), context, this);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_argument) {
            m_argument->iterateChildren(fn);
        }
    }

private:
    Node* m_argument;
    bool m_isDelegate : 1;
};
}

#endif
