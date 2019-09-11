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
    : GeneratorObject(state, nullptr, nullptr, nullptr)
{
}

GeneratorObject::GeneratorObject(ExecutionState& state, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk)
    : Object(state)
    , m_executionState(executionState)
    , m_registerFile(registerFile)
    , m_byteCodeBlock(blk)
    , m_byteCodePosition(SIZE_MAX)
    , m_extraDataByteCodePosition(0)
    , m_generatorResumeByteCodePosition(SIZE_MAX)
    , m_generatorState(GeneratorState::SuspendedStart)
    , m_resumeValueIdx(REGISTER_LIMIT)
{
    Object* prototype = new Object(state);
    prototype->setPrototype(state, state.context()->globalObject()->generatorPrototype());
    setPrototype(state, prototype);
}

void* GeneratorObject::operator new(size_t size)
{
    ASSERT(size == sizeof(GeneratorObject));

    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GeneratorObject)] = { 0 };
        fillGCDescriptor(obj_bitmap);
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

Value generatorExecute(ExecutionState& state, GeneratorObject* gen, Value resumeValue, bool isAbruptReturn, bool isAbruptThrow)
{
    ExecutionState* generatorOriginalState = gen->m_executionState;
    while (generatorOriginalState->parent()) {
        generatorOriginalState = generatorOriginalState->parent();
    }

    GeneratorExecutionStateParentBinder parentBinder(state, generatorOriginalState);

    gen->m_generatorState = GeneratorState::Executing;
    if (gen->m_resumeValueIdx != REGISTER_LIMIT) {
        gen->m_registerFile[gen->m_resumeValueIdx] = resumeValue;
    }

    gen->m_resumeValue = resumeValue;
    if (gen->m_generatorResumeByteCodePosition != SIZE_MAX) {
        GeneratorResume* gr = (GeneratorResume*)(gen->m_byteCodeBlock->m_code.data() + gen->m_generatorResumeByteCodePosition);
        gr->m_needsReturn = isAbruptReturn;
        gr->m_needsThrow = isAbruptThrow;
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
        result = ByteCodeInterpreter::interpret(es, gen->m_byteCodeBlock, startPos, gen->m_registerFile);
        // normal return means generator end
        gen->m_generatorState = GeneratorState::CompletedReturn;
        gen->releaseExecutionVariables();
    } catch (GeneratorObject::GeneratorExitValue* exitValue) {
        result = exitValue->m_value;
        delete exitValue;
    } catch (const Value& thrownValue) {
        gen->m_generatorState = GeneratorState::CompletedThrow;
        gen->releaseExecutionVariables();
        throw thrownValue;
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

    Value result = generatorExecute(state, gen, value, false, false);

    if (gen->m_generatorState >= GeneratorState::CompletedReturn) {
        return createIterResultObject(state, result, true);
    }

    return createIterResultObject(state, result, false);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-generatorresumeabrupt
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

    Value result = generatorExecute(state, gen, value, type == GeneratorAbruptType::Return, type == GeneratorAbruptType::Throw);

    return createIterResultObject(state, result, gen->generatorState() == GeneratorState::CompletedReturn || gen->generatorState() == GeneratorState::CompletedThrow);
}
}
