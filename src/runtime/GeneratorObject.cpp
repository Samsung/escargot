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
#include "Environment.h"
#include "EnvironmentRecord.h"

namespace Escargot {

GeneratorObject::GeneratorObject(ExecutionState& state)
    : Object(state)
    , m_executionState(nullptr)
    , m_byteCodeBlock(nullptr)
    , m_byteCodePosition(SIZE_MAX)
    , m_extraDataByteCodePosition(0)
    , m_generatorState(GeneratorState::Undefined)
    , m_resumeValueIdx(REGISTER_LIMIT)
{
    Object::setPrototype(state, state.context()->globalObject()->generatorPrototype());
}

GeneratorObject::GeneratorObject(ExecutionState& state, ExecutionState* executionState, ByteCodeBlock* blk)
    : Object(state)
    , m_executionState(executionState)
    , m_byteCodeBlock(blk)
    , m_byteCodePosition(SIZE_MAX)
    , m_extraDataByteCodePosition(0)
    , m_generatorState(GeneratorState::SuspendedStart)
    , m_resumeValueIdx(REGISTER_LIMIT)
{
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
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GeneratorObject, m_byteCodeBlock));
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

    if (gen->generatorState() == GeneratorState::Executing) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Generator is already running");
    }

    return gen;
}

class GeneratorExecutionStateParentBinder {
public:
    GeneratorExecutionStateParentBinder(ExecutionState& state, ExecutionState* generatorOriginalState)
        : m_generatorOriginalState(generatorOriginalState)
    {
        generatorOriginalState->setParent(&state);
    }

    ~GeneratorExecutionStateParentBinder()
    {
        m_generatorOriginalState->setParent(nullptr);
    }

    ExecutionState* m_generatorOriginalState;
};

Value generatorExecute(ExecutionState& state, GeneratorObject* gen, Value resumeValue)
{
    ExecutionState* generatorOriginalState = gen->m_executionState;
    while (generatorOriginalState->parent()) {
        generatorOriginalState = generatorOriginalState->parent();
    }

    GeneratorExecutionStateParentBinder parentBinder(state, generatorOriginalState);

    gen->m_generatorState = GeneratorState::Executing;
    if (gen->m_resumeValueIdx != REGISTER_LIMIT) {
        generatorOriginalState->registerFile()[gen->m_resumeValueIdx] = resumeValue;
    }

    Value result;
    try {
        ExecutionState* es;
        size_t startPos = gen->m_byteCodePosition;
        if (startPos == SIZE_MAX) {
            // need to fresh start
            startPos = 0;
            es = gen->m_executionState;
        } else {
            // resume
            startPos = gen->m_extraDataByteCodePosition;
            LexicalEnvironment* env = new LexicalEnvironment(new FunctionEnvironmentRecordOnHeap<false, false>(generatorOriginalState->resolveCallee()), nullptr
#ifndef NDEBUG
                                                             ,
                                                             true
#endif
                                                             );
            es = new ExecutionState(&state, env, false);
        }
        result = ByteCodeInterpreter::interpret(es, gen->m_byteCodeBlock, startPos, generatorOriginalState->registerFile());
    } catch (GeneratorObject::GeneratorExitValue* exitValue) {
        result = exitValue->m_value;
        delete exitValue;
    }

    generatorOriginalState->setParent(nullptr);

    return result;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresume
Value generatorResume(ExecutionState& state, const Value& generator, const Value& value)
{
    GeneratorObject* gen = generatorValidate(state, generator);

    if (gen->m_generatorState >= GeneratorState::CompletedReturn) {
        return createIterResultObject(state, Value(), true);
    }

    ASSERT(gen->m_generatorState == GeneratorState::SuspendedStart || gen->m_generatorState == SuspendedYield);

    Value result = generatorExecute(state, gen, value);

    if (gen->m_generatorState >= GeneratorState::CompletedReturn) {
        return createIterResultObject(state, result, true);
    }

    return createIterResultObject(state, result, false);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresume
Value generatorResumeAbrupt(ExecutionState& state, const Value& generator, const Value& value, GeneratorAbruptType type)
{
    GeneratorObject* gen = generatorValidate(state, generator);

    if (gen->m_generatorState == GeneratorState::SuspendedStart) {
        gen->m_generatorState = GeneratorState::CompletedReturn;
    }

    if (gen->generatorState() >= GeneratorState::CompletedReturn) {
        if (type == GeneratorAbruptType::Return) {
            return createIterResultObject(state, value, true);
        }
        state.throwException(value);
    }

    ASSERT(gen->generatorState() == GeneratorState::SuspendedYield);

    gen->m_generatorState = GeneratorState::Executing;

    Value result = generatorExecute(state, gen, value);

    gen->m_generatorState = (type == GeneratorAbruptType::Return ? GeneratorState::CompletedReturn : GeneratorState::CompletedThrow);

    if (type == GeneratorAbruptType::Throw) {
        state.throwException(value);
    }
    return createIterResultObject(state, value, true);
}
}
