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

#ifndef __EscargotGeneratorObject__
#define __EscargotGeneratorObject__

#include "runtime/Object.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ExecutionPauser.h"
#include "interpreter/ByteCode.h"

namespace Escargot {

class GeneratorObject : public DerivedObject {
    friend class InterpreterSlowPath;
    friend class ExecutionPauser;

public:
    enum GeneratorState : unsigned {
        SuspendedStart,
        SuspendedYield,
        Executing,
        CompletedReturn,
        CompletedThrow,
    };

    enum GeneratorAbruptType : unsigned {
        Return,
        Throw
    };

    GeneratorObject(ExecutionState& state, Object* proto, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk);

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
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_executionPauser.value();
#else
        return &m_executionPauser;
#endif
    }

    static GeneratorObject* generatorValidate(ExecutionState& state, const Value& generator);
    static Value generatorResume(ExecutionState& state, const Value& generator, const Value& value);
    static Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorObject::GeneratorAbruptType type);

private:
#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_executionState));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_sourceObject));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_registerFile));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_byteCodeBlock));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_pausedCode));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_pauseValue));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_resumeValue));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_promiseCapability.m_promise));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_promiseCapability.m_resolveFunction));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_promiseCapability.m_rejectFunction));
#ifdef ESCARGOT_DEBUGGER
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_promiseCapability.m_savedStackTrace));
        GC_set_bit(desc, GC_WORD_OFFSET(GeneratorObject, m_executionPauser.m_savedStackTrace));
#endif /* ESCARGOT_DEBUGGER */
    }
#endif

    GeneratorState m_generatorState;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    CompressibleHeapPointer<ExecutionPauser> m_executionPauser;
#else
    ExecutionPauser m_executionPauser;
#endif
};
} // namespace Escargot

#endif
