/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#include "Object.h"
#include "runtime/Context.h"
#include "runtime/FunctionObject.h"
#include "runtime/ShadowRealmObject.h"
#include "WrappedFunctionObject.h"

namespace Escargot {

#if defined(ENABLE_SHADOWREALM)

WrappedFunctionObject::WrappedFunctionObject(ExecutionState& state, Object* wrappedTargetFunction, Context* realm, const Value& length, const Value& name)
    : DerivedObject(state, state.context()->globalObject()->objectPrototype(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
    , m_wrappedTargetFunction(wrappedTargetFunction)
    , m_realm(realm)
{
    m_structure = state.context()->defaultStructureForWrappedFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = length;
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = name;

    Value proto = m_wrappedTargetFunction->getPrototype(state);
    Object::setPrototype(state, proto);
}

// https://tc39.es/proposal-shadowrealm/#sec-ordinary-wrapped-function-call
Value WrappedFunctionObject::ordinaryWrappedFunctionCall(ExecutionState& state, const Value& thisValue, const size_t calledArgc, Value* calledArgv)
{
    Object* target = m_wrappedTargetFunction;
    ASSERT(target->isCallable());

    Context* callerRealm = m_realm;
    Context* targetRealm = target->getFunctionRealm(state);

    Value* wrappedArgv = ALLOCA(calledArgc * sizeof(Value), Value);
    for (size_t i = 0; i < calledArgc; i++) {
        Value wrappedValue = ShadowRealmObject::getWrappedValue(state, targetRealm, calledArgv[i]);
        wrappedArgv[i] = wrappedValue;
    }

    Value wrappedThisValue = ShadowRealmObject::getWrappedValue(state, targetRealm, thisValue);
    Value result;
    try {
        result = Object::call(state, target, wrappedThisValue, calledArgc, wrappedArgv);
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Call target function failed");
    }

    return ShadowRealmObject::getWrappedValue(state, callerRealm, result);
}

// https://tc39.es/proposal-shadowrealm/#sec-wrapped-function-exotic-objects-call-thisargument-argumentslist
Value WrappedFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t calledArgc, Value* calledArgv)
{
    // Let result be Completion(OrdinaryWrappedFunctionCall(F, thisArgument, argumentsList)).
    Value result = ordinaryWrappedFunctionCall(state, thisValue, calledArgc, calledArgv);
    // Return ? result.
    return result;
}

#endif

} // namespace Escargot
