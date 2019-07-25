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
#include "GeneratorObject.h"
#include "IteratorOperations.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "Context.h"

namespace Escargot {
GeneratorObject::GeneratorObject(ExecutionState& state)
    : Object(state)
    , m_executionState(nullptr)
    , m_blk(nullptr)
    , m_byteCodePosition(0)
    , m_state(GeneratorState::Undefined)
    , m_resumeValueIdx(UINT16_MAX)
{
    Object::setPrototype(state, state.context()->globalObject()->generatorPrototype());
}

GeneratorObject::GeneratorObject(ExecutionState& state, ExecutionState* executionState, ByteCodeBlock* blk)
    : Object(state)
    , m_executionState(executionState)
    , m_blk(blk)
    , m_byteCodePosition(0)
    , m_state(GeneratorState::SuspendedStart)
    , m_resumeValueIdx(UINT16_MAX)
{
    ASSERT(blk != nullptr);
    Object::setPrototype(state, state.context()->globalObject()->generatorPrototype());
}

void* GeneratorObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GeneratorObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_executionState));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_blk));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_resumeValue));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GeneratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorvalidate
GeneratorObject* generatorValidate(ExecutionState& state, const Value& generator)
{
    if (!generator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotObject);
    }

    if (!generator.asObject()->isGeneratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Generator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }

    GeneratorObject* gen = generator.asObject()->asGeneratorObject();
    ASSERT(gen != nullptr);

    if (gen->state() == GeneratorState::Executing) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Generator is already running");
    }

    return gen;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresume
Value generatorResume(ExecutionState& state, const Value& generator, const Value& value)
{
    GeneratorObject* gen = generatorValidate(state, generator);
    ASSERT(gen != nullptr);

    if (gen->state() >= GeneratorState::CompletedReturn) {
        return createIterResultObject(state, Value(), true);
    }

    ASSERT(gen->state() == GeneratorState::SuspendedStart || gen->state() == SuspendedYield);

    gen->setState(GeneratorState::Executing);
    if (gen->resumeValueIdx() != UINT16_MAX) {
        gen->executionState()->registerFile()[gen->resumeValueIdx()] = value;
    }

    Value result = ByteCodeInterpreter::interpret(*gen->executionState(), gen->block(), gen->byteCodePosition(), gen->executionState()->registerFile());

    if (gen->state() >= GeneratorState::CompletedReturn) {
        return createIterResultObject(state, result, true);
    }

    return createIterResultObject(state, result, false);
}
// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresume
Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorAbruptType type)
{
    GeneratorObject* gen = generatorValidate(state, generator);
    ASSERT(gen != nullptr);

    if (gen->state() == GeneratorState::SuspendedStart) {
        gen->setState(GeneratorState::CompletedReturn);
    }

    if (gen->state() >= GeneratorState::CompletedReturn) {
        if (type == GeneratorAbruptType::Return) {
            return createIterResultObject(state, value, true);
        }
        state.throwException(value);
    }

    ASSERT(gen->state() == GeneratorState::SuspendedYield);

    gen->setState(GeneratorState::Executing);

    const Value result = ByteCodeInterpreter::interpret(*gen->executionState(), gen->block(), gen->byteCodePosition(), gen->executionState()->registerFile());

    gen->setState(type == GeneratorAbruptType::Return ? GeneratorState::CompletedReturn : GeneratorState::CompletedThrow);

    if (type == GeneratorAbruptType::Throw) {
        state.throwException(value);
    }
    return createIterResultObject(state, value, true);
}
}
