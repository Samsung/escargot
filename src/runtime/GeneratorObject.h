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
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ExecutionPauser.h"
#include "interpreter/ByteCode.h"

namespace Escargot {

class GeneratorObject : public Object {
    friend class ByteCodeInterpreter;
    friend class ExecutionPauser;

public:
    enum GeneratorState {
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

    GeneratorObject(ExecutionState& state);
    GeneratorObject(ExecutionState& state, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk, const Value& prototype);

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

    ExecutionPauser* executionPauser()
    {
        return &m_executionPauser;
    }

private:
    friend Value generatorExecute(ExecutionState& state, GeneratorObject* gen, Value resumeValue, bool isAbruptReturn, bool isAbruptThrow);
    friend Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
    friend Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorObject::GeneratorAbruptType type);

    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_executionState));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_sourceObject));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_registerFile));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_byteCodeBlock));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_resumeValue));
    }

    GeneratorState m_generatorState;
    ExecutionPauser m_executionPauser;
};

GeneratorObject* generatorValidate(ExecutionState& state, const Value& generator);
Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorObject::GeneratorAbruptType type);
}

#endif
