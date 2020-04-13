/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "runtime/ScriptAsyncGeneratorFunctionObject.h"
#include "runtime/AsyncGeneratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

static Value builtinAsyncGeneratorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    size_t argumentVectorCount = argc > 1 ? argc - 1 : 0;
    Value sourceValue = argc >= 1 ? argv[argc - 1] : Value(String::emptyString);
    auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().anonymous, argumentVectorCount, argv, sourceValue, false, true, true, false);

    // Let proto be ? GetPrototypeFromConstructor(newTarget, fallbackProto).
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->asyncGenerator();
    });

    return new ScriptAsyncGeneratorFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment);
}

static Value builtinAsyncGeneratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return AsyncGeneratorObject::asyncGeneratorEnqueue(state, thisValue, AsyncGeneratorObject::AsyncGeneratorEnqueueType::Next, argv[0]);
}

static Value builtinAsyncGeneratorReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return AsyncGeneratorObject::asyncGeneratorEnqueue(state, thisValue, AsyncGeneratorObject::AsyncGeneratorEnqueueType::Return, argv[0]);
}

static Value builtinAsyncGeneratorThrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return AsyncGeneratorObject::asyncGeneratorEnqueue(state, thisValue, AsyncGeneratorObject::AsyncGeneratorEnqueueType::Throw, argv[0]);
}

void GlobalObject::installAsyncGeneratorFunction(ExecutionState& state)
{
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorfunction
    m_asyncGeneratorFunction = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().AsyncGeneratorFunction, builtinAsyncGeneratorFunction, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_asyncGeneratorFunction->setGlobalIntrinsicObject(state);
    m_asyncGeneratorFunction->setPrototype(state, m_function);

    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-properties-of-asyncgeneratorfunction-prototype
    m_asyncGenerator = new Object(state, m_functionPrototype);
    m_asyncGenerator->setGlobalIntrinsicObject(state, true);

    m_asyncGeneratorFunction->setFunctionPrototype(state, m_asyncGenerator);

    m_asyncGenerator->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                        ObjectPropertyDescriptor(m_asyncGeneratorFunction, ObjectPropertyDescriptor::ConfigurablePresent));

    m_asyncGenerator->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                        ObjectPropertyDescriptor(state.context()->staticStrings().AsyncGeneratorFunction.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-properties-of-asyncgenerator-prototype
    m_asyncGeneratorPrototype = new Object(state, m_asyncIteratorPrototype);
    m_asyncGeneratorPrototype->setGlobalIntrinsicObject(state, true);

    m_asyncGenerator->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().prototype), ObjectPropertyDescriptor(m_asyncGeneratorPrototype, ObjectPropertyDescriptor::ConfigurablePresent));
    m_asyncGeneratorPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_asyncGenerator, ObjectPropertyDescriptor::ConfigurablePresent));

    m_asyncGeneratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinAsyncGeneratorNext, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_asyncGeneratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringReturn),
                                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringReturn, builtinAsyncGeneratorReturn, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_asyncGeneratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringThrow),
                                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringThrow, builtinAsyncGeneratorThrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_asyncGeneratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                                ObjectPropertyDescriptor(Value(state.context()->staticStrings().AsyncGenerator.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

} // namespace Escargot
