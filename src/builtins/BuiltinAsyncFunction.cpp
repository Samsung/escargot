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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/GeneratorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ScriptAsyncFunctionObject.h"

namespace Escargot {

static Value builtinAsyncFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    size_t argumentVectorCount = argc > 1 ? argc - 1 : 0;
    Value sourceValue = argc >= 1 ? argv[argc - 1] : Value(String::emptyString);
    auto functionSource = FunctionObject::createDynamicFunctionScript(state, state.context()->staticStrings().anonymous, argumentVectorCount, argv, sourceValue, false, false, true, false);
    RETURN_VALUE_IF_PENDING_EXCEPTION

    // Let proto be ? GetPrototypeFromConstructor(newTarget, fallbackProto).
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->asyncFunctionPrototype();
    });
    RETURN_VALUE_IF_PENDING_EXCEPTION

    return new ScriptAsyncFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment);
}

void GlobalObject::initializeAsyncFunction(ExecutionState& state)
{
    // do nothing
}

void GlobalObject::installAsyncFunction(ExecutionState& state)
{
    m_asyncFunction = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().AsyncFunction, builtinAsyncFunction, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_asyncFunction->setGlobalIntrinsicObject(state);
    m_asyncFunction->setPrototype(state, m_function);

    m_asyncFunctionPrototype = new PrototypeObject(state, m_functionPrototype);
    m_asyncFunctionPrototype->setGlobalIntrinsicObject(state, true);
    m_asyncFunction->setFunctionPrototype(state, m_asyncFunctionPrototype);

    m_asyncFunctionPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                                      ObjectPropertyDescriptor(m_asyncFunction, ObjectPropertyDescriptor::ConfigurablePresent));

    m_asyncFunctionPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                      ObjectPropertyDescriptor(state.context()->staticStrings().AsyncFunction.string(), ObjectPropertyDescriptor::ConfigurablePresent));
}
} // namespace Escargot
