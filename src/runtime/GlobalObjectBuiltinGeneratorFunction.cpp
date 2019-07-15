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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "GeneratorObject.h"

namespace Escargot {

static Value builtinGeneratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return generatorResume(state, thisValue, argc > 0 ? argv[0] : Value());
}

static Value builtinGeneratorReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return generatorResumeAbrupt(state, thisValue, argc > 0 ? argv[0] : Value(), GeneratorAbruptType::Return);
}

static Value builtinGeneratorThrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return generatorResumeAbrupt(state, thisValue, argc > 0 ? argv[0] : Value(), GeneratorAbruptType::Throw);
}

void GlobalObject::installGenerator(ExecutionState& state)
{
    m_generator = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Generator, nullptr, 0, NativeFunctionInfo::Strict));
    m_generator->markThisObjectDontNeedStructureTransitionTable(state);
    m_generator->setPrototype(state, m_functionPrototype);

    m_generatorPrototype = m_objectPrototype;
    m_generatorPrototype = new GeneratorObject(state);
    m_generatorPrototype->setPrototype(state, m_iteratorPrototype);
    m_generatorPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_generatorPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_generator, ObjectPropertyDescriptor::ConfigurablePresent));

    m_generatorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                           ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinGeneratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_generatorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringReturn),
                                                           ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringReturn, builtinGeneratorReturn, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_generatorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringThrow),
                                                           ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringThrow, builtinGeneratorThrow, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_generatorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                           ObjectPropertyDescriptor(Value(state.context()->staticStrings().Generator.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
