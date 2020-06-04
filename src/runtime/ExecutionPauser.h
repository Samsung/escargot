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

#ifndef __EscargotExecutionPauser__
#define __EscargotExecutionPauser__

#include "runtime/Object.h"
#include "runtime/PromiseObject.h"

namespace Escargot {

class ByteCodeBlock;

class ExecutionPauser : public gc {
public:
    friend class GeneratorObject;
    friend class AsyncGeneratorObject;
    friend class ByteCodeInterpreter;

    ExecutionPauser(ExecutionState& state, Object* sourceObject, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk);

    enum PauseReason {
        Yield,
        Await,
        GeneratorsInitialize
    };

    enum ResumeState {
        Normal,
        Throw,
        Return
    };

    // yield is implemented throw this type of by this class
    // we cannot use normal return logic because we must not modify ExecutionState(some statements(block,with..) needs modifying control flow data for exit function)
    struct PauseValue : public gc {
        PauseReason m_pauseReason;
        Value m_value;

        void* operator new(size_t size)
        {
            // we MUST use uncollectable malloc. bdwgc cannot track thrown value
            return GC_MALLOC_UNCOLLECTABLE(size);
        }
    };

    void release()
    {
        m_executionState = nullptr;
        m_registerFile = nullptr;
        m_byteCodeBlock = nullptr;
        m_resumeValue = SmallValue();
        m_promiseCapability.m_promise = nullptr;
        m_promiseCapability.m_resolveFunction = nullptr;
        m_promiseCapability.m_rejectFunction = nullptr;
    }

    enum StartFrom {
        Generator,
        Async,
        AsyncGenerator
    };

    static Value start(ExecutionState& state, ExecutionPauser* self, Object* source, const Value& resumeValue, bool isAbruptReturn, bool isAbruptThrow, StartFrom from);
    static void pause(ExecutionState& state, Value returnValue, size_t tailDataPosition, size_t tailDataLength, size_t nextProgramCounter, ByteCodeRegisterIndex dstRegisterIndex, ByteCodeRegisterIndex dstStateRegisterIndex, PauseReason reason);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    Object* sourceObject() const
    {
        return m_sourceObject;
    }

private:
    ExecutionState* m_executionState;
    Object* m_sourceObject;
    Value* m_registerFile;
    ByteCodeBlock* m_byteCodeBlock;
    size_t m_byteCodePosition; // this indicates where we should execute next in interpreter
    size_t m_extraDataByteCodePosition; // this indicates where we can gather information about running state(recursive statement)
    size_t m_resumeByteCodePosition; // this indicates where ResumeByteCode located in
    SmallValue m_resumeValue;
    ByteCodeRegisterIndex m_resumeValueIndex;
    ByteCodeRegisterIndex m_resumeStateIndex;
    PromiseReaction::Capability m_promiseCapability; // async function needs this
};
}

#endif
