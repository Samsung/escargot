/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "ExecutionPauser.h"
#include "runtime/GeneratorObject.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/IteratorOperations.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

void* ExecutionPauser::operator new(size_t size)
{
    ASSERT(size == sizeof(ExecutionPauser));

    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(ExecutionPauser)] = { 0 };
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_executionState));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_sourceObject));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_registerFile));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_byteCodeBlock));
        GC_set_bit(desc, GC_WORD_OFFSET(ExecutionPauser, m_resumeValue));
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
    , m_extraDataByteCodePosition(0)
    , m_resumeByteCodePosition(SIZE_MAX)
    , m_resumeValueIndex(REGISTER_LIMIT)
{
}

class ExecutionPauserExecutionStateParentBinder {
public:
    ExecutionPauserExecutionStateParentBinder(ExecutionState& state, ExecutionState* originalState)
        : m_originalState(originalState)
    {
        originalState->setParent(&state);
    }

    ~ExecutionPauserExecutionStateParentBinder()
    {
        m_originalState->setParent(nullptr);
    }

    ExecutionState* m_originalState;
};


Value ExecutionPauser::start(ExecutionState& state, ExecutionPauser* self, Object* source, const Value& resumeValue, bool isAbruptReturn, bool isAbruptThrow, StartFrom from)
{
    ExecutionState* originalState = self->m_executionState;
    while (originalState->parent()) {
        originalState = originalState->parent();
    }

    ExecutionPauserExecutionStateParentBinder parentBinder(state, originalState);

    if (from == StartFrom::Generator) {
        source->asGeneratorObject()->m_generatorState = GeneratorState::Executing;
    }
    if (self->m_resumeValueIndex != REGISTER_LIMIT) {
        self->m_registerFile[self->m_resumeValueIndex] = resumeValue;
    }

    self->m_resumeValue = resumeValue;
    if (self->m_resumeByteCodePosition != SIZE_MAX) {
        ExecutionResume* gr = (ExecutionResume*)(self->m_byteCodeBlock->m_code.data() + self->m_resumeByteCodePosition);
        gr->m_needsReturn = isAbruptReturn;
        gr->m_needsThrow = isAbruptThrow;
    }

    Value result;
    try {
        ExecutionState* es;
        size_t startPos = self->m_byteCodePosition;
        if (startPos == SIZE_MAX) {
            // need to fresh start
            startPos = 0;
            es = self->m_executionState;
        } else {
            // resume
            startPos = self->m_extraDataByteCodePosition;
            LexicalEnvironment* env = new LexicalEnvironment(new FunctionEnvironmentRecordOnHeap<false, false>(originalState->resolveCallee()), nullptr
#ifndef NDEBUG
                                                             ,
                                                             true
#endif
                                                             );
            es = new ExecutionState(&state, env, false);
        }
        result = ByteCodeInterpreter::interpret(es, self->m_byteCodeBlock, startPos, self->m_registerFile);
        // normal return means generator end
        if (from == StartFrom::Generator) {
            source->asGeneratorObject()->m_generatorState = GeneratorState::CompletedReturn;
            result = createIterResultObject(state, result, true);
        }
        self->release();
    } catch (PauseValue* exitValue) {
        result = exitValue->m_value;
        auto isDelegateOperation = exitValue->m_isDelegateOperation;
        delete exitValue;

        if (isDelegateOperation) {
            return result;
        }

        if (from == Generator) {
            if (source->asGeneratorObject()->m_generatorState >= GeneratorState::CompletedReturn) {
                return createIterResultObject(state, result, true);
            }
            return createIterResultObject(state, result, false);
        }
    } catch (const Value& thrownValue) {
        if (from == Generator) {
            source->asGeneratorObject()->m_generatorState = GeneratorState::CompletedThrow;
        }
        self->release();
        throw thrownValue;
    }

    return result;
}

void ExecutionPauser::pause(ExecutionState& state, Value returnValue, size_t tailDataPosition, size_t tailDataLength, size_t nextProgramCounter, ByteCodeRegisterIndex dstRegisterIndex, PauseReason reason)
{
    ExecutionState* originalState = &state;
    ExecutionPauser* self;
    while (true) {
        self = originalState->pauseSource();
        if (self) {
            originalState->rareData()->m_parent = nullptr;
            break;
        }
        originalState = originalState->parent();
    }
    Object* pauser = self->m_sourceObject;
    self->m_executionState = &state;
    self->m_byteCodePosition = nextProgramCounter;
    self->m_resumeValueIndex = dstRegisterIndex;

    if (reason == PauseReason::Yield || reason == PauseReason::YieldDelegate) {
        pauser->asGeneratorObject()->m_generatorState = GeneratorState::SuspendedYield;
    } else {
        ASSERT(reason == PauseReason::Await);
    }

    // read & fill recursive statement self
    char* start = (char*)(tailDataPosition);
    char* end = (char*)(start + tailDataLength);

    size_t startupDataPosition = self->m_byteCodeBlock->m_code.size();
    self->m_extraDataByteCodePosition = startupDataPosition;
    std::vector<size_t> codeStartPositions;
    size_t codePos = startupDataPosition;
    while (start != end) {
        size_t e = *((size_t*)start);
        start += sizeof(ByteCodeGenerateContext::RecursiveStatementKind);
        size_t startPos = *((size_t*)start);
        codeStartPositions.push_back(startPos);
        if (e == ByteCodeGenerateContext::Block) {
            self->m_byteCodeBlock->m_code.resize(self->m_byteCodeBlock->m_code.size() + sizeof(BlockOperation));
            BlockOperation* code = new (self->m_byteCodeBlock->m_code.data() + codePos) BlockOperation(ByteCodeLOC(SIZE_MAX), nullptr);
            code->assignOpcodeInAddress();

            codePos += sizeof(BlockOperation);
        } else if (e == ByteCodeGenerateContext::With) {
            self->m_byteCodeBlock->m_code.resize(self->m_byteCodeBlock->m_code.size() + sizeof(WithOperation));
            WithOperation* code = new (self->m_byteCodeBlock->m_code.data() + codePos) WithOperation(ByteCodeLOC(SIZE_MAX), REGISTER_LIMIT);
            code->assignOpcodeInAddress();

            codePos += sizeof(WithOperation);
        } else if (e == ByteCodeGenerateContext::Try) {
            self->m_byteCodeBlock->m_code.resize(self->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (self->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isTryResumeProcess = true;

            codePos += sizeof(TryOperation);
        } else if (e == ByteCodeGenerateContext::Catch) {
            self->m_byteCodeBlock->m_code.resize(self->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (self->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isCatchResumeProcess = true;

            codePos += sizeof(TryOperation);
        } else {
            ASSERT(e == ByteCodeGenerateContext::Finally);
            self->m_byteCodeBlock->m_code.resize(self->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (self->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isFinallyResumeProcess = true;

            codePos += sizeof(TryOperation);
        }
        start += sizeof(size_t); // start pos
    }

    size_t resumeCodePos = self->m_byteCodeBlock->m_code.size();
    self->m_resumeByteCodePosition = resumeCodePos;
    self->m_byteCodeBlock->m_code.resizeWithUninitializedValues(resumeCodePos + sizeof(ExecutionResume));
    auto resumeCode = new (self->m_byteCodeBlock->m_code.data() + resumeCodePos) ExecutionResume(ByteCodeLOC(SIZE_MAX), self);
    resumeCode->assignOpcodeInAddress();

    size_t stackSizePos = self->m_byteCodeBlock->m_code.size();
    self->m_byteCodeBlock->m_code.resizeWithUninitializedValues(stackSizePos + sizeof(size_t));
    new (self->m_byteCodeBlock->m_code.data() + stackSizePos) size_t(codeStartPositions.size());

    // add ByteCodePositions
    for (size_t i = 0; i < codeStartPositions.size(); i++) {
        size_t pos = self->m_byteCodeBlock->m_code.size();
        self->m_byteCodeBlock->m_code.resizeWithUninitializedValues(pos + sizeof(size_t));
        new (self->m_byteCodeBlock->m_code.data() + pos) size_t(codeStartPositions[i]);
    }

    PauseValue* exitValue = new PauseValue();
    exitValue->m_value = returnValue;
    exitValue->m_isDelegateOperation = reason == PauseReason::YieldDelegate;
    throw exitValue;
}
}
