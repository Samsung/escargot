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
#include "GeneratorObject.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "Context.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "IteratorObject.h"

namespace Escargot {

GeneratorObject::GeneratorObject(ExecutionState& state, Object* proto, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk)
    : DerivedObject(state, proto)
    , m_generatorState(GeneratorState::SuspendedStart)
    , m_executionPauser(state, this, executionState, registerFile, blk)
{
}

void* GeneratorObject::operator new(size_t size)
{
    ASSERT(size == sizeof(GeneratorObject));

    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GeneratorObject)] = { 0 };
        fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GeneratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorvalidate
GeneratorObject* GeneratorObject::generatorValidate(ExecutionState& state, const Value& generator)
{
    if (!generator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    if (!generator.asObject()->isGeneratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Generator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    GeneratorObject* gen = generator.asObject()->asGeneratorObject();

    if (gen->generatorState() == GeneratorObject::GeneratorState::Executing) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Generator is already running");
    }

    return gen;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresume
Value GeneratorObject::generatorResume(ExecutionState& state, const Value& generator, const Value& value)
{
    GeneratorObject* gen = generatorValidate(state, generator);

    if (gen->m_generatorState >= GeneratorObject::GeneratorState::CompletedReturn) {
        return IteratorObject::createIterResultObject(state, Value(), true);
    }

    ASSERT(gen->m_generatorState == GeneratorObject::GeneratorState::SuspendedStart || gen->m_generatorState == GeneratorObject::GeneratorState::SuspendedYield);

    gen->m_generatorState = GeneratorObject::GeneratorState::Executing;
    return ExecutionPauser::start(state, gen->executionPauser(), gen, value, false, false, ExecutionPauser::Generator);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresumeabrupt
Value GeneratorObject::generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorObject::GeneratorAbruptType type)
{
    GeneratorObject* gen = generatorValidate(state, generator);

    if (gen->m_generatorState == GeneratorObject::GeneratorState::SuspendedStart) {
        gen->m_generatorState = GeneratorObject::GeneratorState::CompletedReturn;
    }

    if (gen->generatorState() >= GeneratorObject::GeneratorState::CompletedReturn) {
        if (type == GeneratorObject::GeneratorAbruptType::Return) {
            return IteratorObject::createIterResultObject(state, value, true);
        }
        state.throwException(value);
    }

    ASSERT(gen->generatorState() == GeneratorObject::GeneratorState::SuspendedYield);

    gen->m_generatorState = GeneratorObject::GeneratorState::Executing;
    return ExecutionPauser::start(state, gen->executionPauser(), gen, value, type == GeneratorObject::GeneratorAbruptType::Return, type == GeneratorObject::GeneratorAbruptType::Throw, ExecutionPauser::Generator);
}
} // namespace Escargot
