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
public:
    GeneratorObject(ExecutionState& state);
    GeneratorObject(ExecutionState& state, ExecutionState* executionState, ByteCodeBlock* blk);

    virtual const char* internalClassProperty() override
    {
        return "Generator";
    }

    virtual bool isGeneratorObject() const override
    {
        return true;
    }

    ExecutionState* executionState()
    {
        return m_executionState;
    }

    ByteCodeBlock* block()
    {
        return m_blk;
    }

    size_t byteCodePosition()
    {
        return m_byteCodePosition;
    }

    GeneratorState state()
    {
        return m_state;
    }

    uint16_t resumeValueIdx()
    {
        return m_resumeValueIdx;
    }

    void setState(GeneratorState state)
    {
        m_state = state;
    }

    void setByteCodePosition(size_t pos)
    {
        m_byteCodePosition = pos;
    }

    void setResumeValueIdx(uint16_t idx)
    {
        m_resumeValueIdx = idx;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    ExecutionState* m_executionState;
    ByteCodeBlock* m_blk;
    size_t m_byteCodePosition;
    GeneratorState m_state;
    SmallValue m_resumeValue;
    uint16_t m_resumeValueIdx;
};

GeneratorObject* generatorValidate(ExecutionState& state, const Value& generator);
Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorAbruptType type);
}

#endif
