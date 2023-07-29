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

#include "Escargot.h"
#include "ExecutionPauser.h"
#include "runtime/GeneratorObject.h"
#include "runtime/AsyncGeneratorObject.h"
#include "runtime/ScriptAsyncFunctionObject.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/IteratorObject.h"
#include "runtime/PromiseObject.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

COMPILE_ASSERT((int)ExecutionPauser::PauseReason::Yield == (int)ExecutionPause::Yield, "");
COMPILE_ASSERT((int)ExecutionPauser::PauseReason::Await == (int)ExecutionPause::Await, "");
COMPILE_ASSERT((int)ExecutionPauser::PauseReason::GeneratorsInitialize == (int)ExecutionPause::GeneratorsInitialize, "");

void* ExecutionPauser::operator new(size_t size)
{
    ASSERT(size == sizeof(ExecutionPauser));

    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(ExecutionPauser)] = { 0 };
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_executionState));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_sourceObject));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_registerFile));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_byteCodeBlock));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_pausedCode));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_pauseValue));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_resumeValue));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_promiseCapability.m_promise));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_promiseCapability.m_resolveFunction));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_promiseCapability.m_rejectFunction));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(ExecutionPauser));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ExecutionPauser::ExecutionPauser(ExecutionState& state, Object* sourceObject, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk)
    : m_executionState(executionState)
    , m_sourceObject(sourceObject)
    , m_registerFile(registerFile)
    , m_byteCodeBlock(blk)
    , m_byteCodePosition(SIZE_MAX)
    , m_resumeByteCodePosition(SIZE_MAX)
    , m_pauseValue(nullptr)
    , m_resumeValueIndex(REGISTER_LIMIT)
    , m_resumeStateIndex(REGISTER_LIMIT)
#ifdef ESCARGOT_DEBUGGER
    , m_savedStackTrace(nullptr)
#endif /* ESCARGOT_DEBUGGER */
{
}

class ExecutionPauserExecutionStateParentBinder {
public:
    ExecutionPauserExecutionStateParentBinder(ExecutionState& state, ExecutionState* originalState)
        : m_originalState(originalState)
    {
        m_oldParent = m_originalState->parent();


        ExecutionState* pstate = m_originalState;
        while (pstate) {
            if (pstate == &state) {
                // AsyncGeneratorObject::asyncGeneratorResolve can make loop
                return;
            }
            pstate = pstate->parent();
        }

        m_originalState->setParent(&state);
    }

    ~ExecutionPauserExecutionStateParentBinder()
    {
        m_originalState->setParent(m_oldParent);
    }

    ExecutionState* m_originalState;
    ExecutionState* m_oldParent;
};

Value ExecutionPauser::start(ExecutionState& state, ExecutionPauser* self, Object* source, const Value& resumeValue, bool isAbruptReturn, bool isAbruptThrow, StartFrom from)
{
    ExecutionState* originalState = self->m_executionState;
    while (!originalState->pauseSource()) {
        originalState = originalState->parent();
    }

    ExecutionPauserExecutionStateParentBinder parentBinder(state, originalState);

    if (self->m_resumeValueIndex != REGISTER_LIMIT) {
        self->m_registerFile[self->m_resumeValueIndex] = resumeValue;
    }

    if (self->m_resumeStateIndex != REGISTER_LIMIT) {
        if (isAbruptThrow) {
            self->m_registerFile[self->m_resumeStateIndex] = Value(ResumeState::Throw);
        } else if (isAbruptReturn) {
            self->m_registerFile[self->m_resumeStateIndex] = Value(ResumeState::Return);
        } else {
            self->m_registerFile[self->m_resumeStateIndex] = Value(ResumeState::Normal);
        }
    }

    self->m_resumeValue = resumeValue;
    if (self->m_resumeByteCodePosition != SIZE_MAX) {
        ExecutionResume* gr = (ExecutionResume*)(self->m_pausedCode.data() + self->m_resumeByteCodePosition);
        gr->m_needsReturn = isAbruptReturn;
        gr->m_needsThrow = isAbruptThrow;
    }

    // AsyncFunction
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-async-functions-abstract-operations-async-function-start
    // AsyncGeneratorFunction
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorstart

    const size_t programStart = reinterpret_cast<size_t>(self->m_byteCodeBlock->m_code.data());
    Value result;
    {
        ExecutionState* es;
        size_t startPos = self->m_byteCodePosition;
        if (startPos == SIZE_MAX) {
            // need to fresh start
            startPos = programStart;
            es = self->m_executionState;
        } else {
            // resume
            startPos = reinterpret_cast<size_t>(self->m_pausedCode.data());

            LexicalEnvironment* env;
            if (originalState->resolveCallee() && originalState->resolveCallee()->isScriptFunctionObject()) {
                env = new LexicalEnvironment(new FunctionEnvironmentRecordOnHeap<false, false>(originalState->resolveCallee()->asScriptFunctionObject()), nullptr
#ifndef NDEBUG
                                             ,
                                             true
#endif
                );
            } else {
                // top-level-await
                env = new LexicalEnvironment(new FunctionEnvironmentRecordOnHeap<false, false>(self->m_sourceObject->asScriptAsyncFunctionObject()), nullptr
#ifndef NDEBUG
                                             ,
                                             true
#endif
                );
            }
            es = new ExecutionState(&state, env, false);
        }

#ifdef ESCARGOT_DEBUGGER
        Debugger* debugger = state.context()->debugger();
        ExecutionState* activeSavedStackTraceExecutionState = ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE;
        Debugger::SavedStackTraceDataVector* activeSavedStackTrace = nullptr;

        if (debugger != nullptr && self->m_savedStackTrace != nullptr) {
            activeSavedStackTraceExecutionState = debugger->activeSavedStackTraceExecutionState();
            activeSavedStackTrace = debugger->activeSavedStackTrace();
            debugger->setActiveSavedStackTrace(&state, self->m_savedStackTrace);
        }
#endif /* ESCARGOT_DEBUGGER */

        result = Interpreter::interpret(es, self->m_byteCodeBlock, startPos, self->m_registerFile);
        // check exception
        if (UNLIKELY(result.isException())) {
            ASSERT(es->hasPendingException());
            es->unsetPendingException();
            state.setPendingException();
            goto IfAbrupt;
        }

#ifdef ESCARGOT_DEBUGGER
        if (activeSavedStackTraceExecutionState != ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE) {
            debugger->setActiveSavedStackTrace(activeSavedStackTraceExecutionState, activeSavedStackTrace);
        }

        debugger = state.context()->debugger();
        if (from != Generator && self->m_savedStackTrace == nullptr && debugger != nullptr) {
            self->m_savedStackTrace = Debugger::saveStackTrace(state);
        }
#endif /* ESCARGOT_DEBUGGER */

        if (self->m_pauseValue) {
            PauseValue* exitValue = self->m_pauseValue;
            self->m_pauseValue = nullptr;
            result = exitValue->m_value;
            auto pauseReason = exitValue->m_pauseReason;

            if (pauseReason == ExecutionPauser::PauseReason::GeneratorsInitialize) {
                return result;
            }

            if (from == StartFrom::Generator) {
                if (source->asGeneratorObject()->m_generatorState >= GeneratorObject::GeneratorState::CompletedReturn) {
                    return IteratorObject::createIterResultObject(state, result, true);
                }
                return result;
            } else if (from == StartFrom::Async) {
                // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-async-functions-abstract-operations-async-function-start
                // Return Completion { [[Type]]: return, [[Value]]: promiseCapability.[[Promise]], [[Target]]: empty }.
                result = self->m_promiseCapability.m_promise;
            }
        } else {
            // normal execution end
            if (from == StartFrom::Generator) {
                source->asGeneratorObject()->m_generatorState = GeneratorObject::GeneratorState::CompletedReturn;
                result = IteratorObject::createIterResultObject(state, result, true);
            } else if (from == StartFrom::AsyncGenerator) {
                source->asAsyncGeneratorObject()->m_asyncGeneratorState = AsyncGeneratorObject::Completed;
                result = AsyncGeneratorObject::asyncGeneratorResolve(state, source->asAsyncGeneratorObject(), result, true);
                if (UNLIKELY(state.hasPendingException()))
                    goto IfAbrupt;
            } else {
                ASSERT(from == StartFrom::Async);
                Value argv = result;
                Object::call(state, self->m_promiseCapability.m_resolveFunction, Value(), 1, &argv);
                if (UNLIKELY(state.hasPendingException()))
                    goto IfAbrupt;
                result = self->m_promiseCapability.m_promise;
            }
            self->release();
        }
    }

    if (self->m_executionState) {
        self->m_executionState->m_programCounter = nullptr;
    }

    return result;

IfAbrupt : {
    ASSERT(state.hasPendingException());
    auto promiseCapability = self->m_promiseCapability;
    self->release();
    if (from == StartFrom::Generator) {
        ASSERT(from == StartFrom::Generator);
        source->asGeneratorObject()->m_generatorState = GeneratorObject::GeneratorState::CompletedThrow;
        return Value(Value::Exception);
    } else if (from == StartFrom::AsyncGenerator) {
        if (source->asAsyncGeneratorObject()->m_asyncGeneratorState == AsyncGeneratorObject::SuspendedStart) {
            return Value(Value::Exception);
        } else {
            Value thrownValue = state.detachPendingException();
            source->asAsyncGeneratorObject()->m_asyncGeneratorState = AsyncGeneratorObject::Completed;
            return AsyncGeneratorObject::asyncGeneratorReject(state, source->asAsyncGeneratorObject(), thrownValue);
        }
    } else {
        // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-async-functions-abstract-operations-async-function-start
        ASSERT(from == StartFrom::Async);
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return Completion { [[Type]]: return, [[Value]]: promiseCapability.[[Promise]], [[Target]]: empty }.
        return promiseCapability.m_promise;
    }
}
}

void ExecutionPauser::pause(ExecutionState& state, Value returnValue, size_t tailDataPosition, size_t tailDataLength, size_t nextProgramCounter, ByteCodeRegisterIndex dstRegisterIndex, ByteCodeRegisterIndex dstStateRegisterIndex, PauseReason reason)
{
    ExecutionState* originalState = &state;
    ExecutionPauser* self;
    while (true) {
        originalState->m_inExecutionStopState = true;
        self = originalState->pauseSource();
        if (self) {
            break;
        }
        originalState = originalState->parent();
    }
    Object* pauser = self->m_sourceObject;
    self->m_executionState = &state;
    self->m_byteCodePosition = nextProgramCounter;
    self->m_resumeValueIndex = dstRegisterIndex;
    self->m_resumeStateIndex = dstStateRegisterIndex;

    bool isGenerator = pauser->isGeneratorObject();
    bool isAsyncGenerator = pauser->isAsyncGeneratorObject();

    if (reason == PauseReason::Yield) {
        if (isGenerator) {
            pauser->asGeneratorObject()->m_generatorState = GeneratorObject::GeneratorState::SuspendedYield;
        } else if (isAsyncGenerator) {
            pauser->asAsyncGeneratorObject()->m_asyncGeneratorState = AsyncGeneratorObject::AsyncGeneratorState::SuspendedYield;
            returnValue = AsyncGeneratorObject::asyncGeneratorResolve(state, pauser->asAsyncGeneratorObject(), returnValue, false);
            RETURN_IF_PENDING_EXCEPTION
        }
    } else {
        ASSERT(reason == PauseReason::Await || reason == PauseReason::GeneratorsInitialize);
    }

    // we need to reset parent here beacuse asyncGeneratorResolve access parent
    originalState->rareData()->m_parent = nullptr;
    originalState->m_programCounter = nullptr;

    self->m_pausedCode.clear();

    // some case(async generator), the function execution ended before pause
    if (self->m_byteCodeBlock) {
        // read & fill recursive statement self
        char* start = (char*)(tailDataPosition);
        char* end = (char*)(start + tailDataLength);

        size_t startupDataPosition = self->m_pausedCode.size();
        std::vector<size_t> codeStartPositions;
        size_t codePos = startupDataPosition;
        while (start != end) {
            size_t e = *((size_t*)start);
            start += sizeof(ByteCodeGenerateContext::RecursiveStatementKind);
            size_t startPos = *((size_t*)start);
            codeStartPositions.push_back(startPos);
            if (e == ByteCodeGenerateContext::Block) {
                self->m_pausedCode.resizeWithUninitializedValues(self->m_pausedCode.size() + sizeof(BlockOperation));
                BlockOperation* code = new (self->m_pausedCode.data() + codePos) BlockOperation(ByteCodeLOC(SIZE_MAX), nullptr);
                code->assignOpcodeInAddress();

                codePos += sizeof(BlockOperation);
            } else if (e == ByteCodeGenerateContext::OpenEnv) {
                self->m_pausedCode.resizeWithUninitializedValues(self->m_pausedCode.size() + sizeof(OpenLexicalEnvironment));
                OpenLexicalEnvironment* code = new (self->m_pausedCode.data() + codePos) OpenLexicalEnvironment(ByteCodeLOC(SIZE_MAX), OpenLexicalEnvironment::ResumeExecution, REGISTER_LIMIT);
                code->assignOpcodeInAddress();

                codePos += sizeof(OpenLexicalEnvironment);
            } else if (e == ByteCodeGenerateContext::Try) {
                self->m_pausedCode.resizeWithUninitializedValues(self->m_pausedCode.size() + sizeof(TryOperation));
                TryOperation* code = new (self->m_pausedCode.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
                code->assignOpcodeInAddress();
                code->m_isTryResumeProcess = true;

                codePos += sizeof(TryOperation);
            } else if (e == ByteCodeGenerateContext::Catch) {
                self->m_pausedCode.resizeWithUninitializedValues(self->m_pausedCode.size() + sizeof(TryOperation));
                TryOperation* code = new (self->m_pausedCode.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
                code->assignOpcodeInAddress();
                code->m_isCatchResumeProcess = true;

                codePos += sizeof(TryOperation);
            } else {
                ASSERT(e == ByteCodeGenerateContext::Finally);
                self->m_pausedCode.resizeWithUninitializedValues(self->m_pausedCode.size() + sizeof(TryOperation));
                TryOperation* code = new (self->m_pausedCode.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
                code->assignOpcodeInAddress();
                code->m_isFinallyResumeProcess = true;

                codePos += sizeof(TryOperation);
            }
            start += sizeof(size_t); // start pos
        }

        size_t resumeCodePos = self->m_pausedCode.size();
        self->m_resumeByteCodePosition = resumeCodePos;
        self->m_pausedCode.resizeWithUninitializedValues(resumeCodePos + sizeof(ExecutionResume));
        auto resumeCode = new (self->m_pausedCode.data() + resumeCodePos) ExecutionResume(ByteCodeLOC(SIZE_MAX), self);
        resumeCode->assignOpcodeInAddress();

        size_t stackSizePos = self->m_pausedCode.size();
        self->m_pausedCode.resizeWithUninitializedValues(stackSizePos + sizeof(size_t));
        new (self->m_pausedCode.data() + stackSizePos) size_t(codeStartPositions.size());

        // add ByteCodePositions
        for (size_t i = 0; i < codeStartPositions.size(); i++) {
            size_t pos = self->m_pausedCode.size();
            self->m_pausedCode.resizeWithUninitializedValues(pos + sizeof(size_t));
            new (self->m_pausedCode.data() + pos) size_t(codeStartPositions[i]);
        }
    }

    PauseValue* exitValue = new PauseValue();
    exitValue->m_value = returnValue;
    exitValue->m_pauseReason = (ExecutionPauser::PauseReason)reason;

    self->m_pauseValue = exitValue;
}
} // namespace Escargot
