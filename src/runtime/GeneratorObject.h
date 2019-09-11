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

#ifndef __EscargotGeneratorObject__
#define __EscargotGeneratorObject__

#include "runtime/Object.h"
#include "interpreter/ByteCode.h"

namespace Escargot {

enum GeneratorState {
    Undefined,
    SuspendedStart,
    SuspendedYield,
    Executing,
    CompletedReturn,
    CompletedThrow,
};

enum GeneratorAbruptType {
    Return,
    Throw
};

class GeneratorObject : public Object {
    friend class ByteCodeInterpreter;
    friend Value generatorExecute(ExecutionState& state, GeneratorObject* gen, Value resumeValue, bool isAbruptReturn, bool isAbruptThrow);
    friend Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
    friend Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorAbruptType type);

public:
    GeneratorObject(ExecutionState& state);
    GeneratorObject(ExecutionState& state, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk);

    virtual const char* internalClassProperty() override
    {
        return "Generator";
    }

    virtual bool isGeneratorObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    GeneratorState generatorState() const
    {
        return m_generatorState;
    }

private:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionState));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_registerFile));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_byteCodeBlock));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_resumeValue));
    }

    // yield is implemented throw this type of by this class
    // we cannot use normal return logic because we must not modify ExecutionState(some statements(block,with..) needs modifying control flow data for exit function)
    struct GeneratorExitValue : public gc {
        Value m_value;

        void* operator new(size_t size)
        {
            // we MUST use uncollectable malloc. bdwgc cannot track thrown value
            return GC_MALLOC_UNCOLLECTABLE(size);
        }
    };

    void releaseExecutionVariables()
    {
        m_executionState = nullptr;
        m_byteCodeBlock = nullptr;
    }

    ExecutionState* m_executionState;
    Value* m_registerFile;
    ByteCodeBlock* m_byteCodeBlock;
    size_t m_byteCodePosition; // this indicates where we should execute next in interpreter
    size_t m_extraDataByteCodePosition; // this indicates where we can gather information about running state(recursive statement)
    size_t m_generatorResumeByteCodePosition; // this indicates where GeneratorResumeLocatedIn
    GeneratorState m_generatorState;
    SmallValue m_resumeValue;
    uint16_t m_resumeValueIdx;
};

GeneratorObject* generatorValidate(ExecutionState& state, const Value& generator);
Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorAbruptType type);
}

#endif
