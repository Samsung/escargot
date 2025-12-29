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
#include "runtime/ShadowRealmObject.h"
#include "runtime/WrappedFunctionObject.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#if defined(ENABLE_SHADOWREALM)

ShadowRealmObject::ShadowRealmObject(ExecutionState& state)
    : DerivedObject(state)
    , m_realmContext(nullptr)
{
}

ShadowRealmObject::ShadowRealmObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
    , m_realmContext(nullptr)
{
}

// https://tc39.es/proposal-shadowrealm/#sec-wrappedfunctioncreate
static Value wrappedFunctionCreate(ExecutionState& state, Context* callerRealm, Object* target)
{
    // Let internalSlotsList be the internal slots listed in Table 2, plus [[Prototype]] and [[Extensible]].
    // Let wrapped be MakeBasicObject(internalSlotsList).
    // Set wrapped.[[Prototype]] to callerRealm.[[Intrinsics]].[[%Function.prototype%]].
    // Set wrapped.[[Call]] as described in 2.1.
    // Set wrapped.[[WrappedTargetFunction]] to Target.
    // Set wrapped.[[Realm]] to callerRealm.
    // Let result be Completion(CopyNameAndLength(wrapped, Target)).
    // If result is an abrupt completion, throw a TypeError exception.
    WrappedFunctionObject* wrapped = nullptr;
    try {
        // https://tc39.es/proposal-shadowrealm/#sec-copynameandlength
        double L = 0;
        Value targetName;
        bool targetHasLength = target->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().length));
        if (targetHasLength) {
            Value targetLen = target->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, target);
            if (targetLen.isNumber()) {
                if (targetLen.toNumber(state) == std::numeric_limits<double>::infinity()) {
                    L = std::numeric_limits<double>::infinity();
                } else if (targetLen.toNumber(state) == -std::numeric_limits<double>::infinity()) {
                    L = 0;
                } else {
                    double targetLenAsInt = targetLen.toInteger(state);
                    L = std::max(0.0, targetLenAsInt);
                }
            }
        }
        targetName = target->get(state, ObjectPropertyName(state.context()->staticStrings().name)).value(state, target);
        if (!targetName.isString()) {
            targetName = String::emptyString();
        }

        wrapped = new WrappedFunctionObject(state, target, callerRealm, Value(Value::DoubleToIntConvertibleTestNeeds, L), targetName);
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Copy name and length from target function failed");
    }
    // Return wrapped.
    return wrapped;
}

Value ShadowRealmObject::wrappedValue(ExecutionState& state, Context* callerRealm, const Value& value)
{
    // If value is an Object, then
    if (value.isObject()) {
        // If IsCallable(value) is false, throw a TypeError exception.
        if (!value.asObject()->isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "The result of ShadowRealm.evaluate must be a callable function");
        }
        // Return ? WrappedFunctionCreate(callerRealm, value).
        return wrappedFunctionCreate(state, callerRealm, value.asObject());
    }
    // Return value.
    return value;
}

Value ShadowRealmObject::eval(ExecutionState& state, String* sourceText, Context* callerRealm)
{
    ScriptParser parser(m_realmContext);
    Script* script = parser.initializeScript(nullptr, 0, sourceText, m_realmContext->staticStrings().lazyEvalCode().string(), nullptr, false, true, false, false, false, false, false, false, true).scriptThrowsExceptionIfParseError(state);
    Value scriptResult;
    ExecutionState stateForNewGlobal(m_realmContext);
    try {
        scriptResult = script->execute(state, true, false);
    } catch (const Value& e) {
        StringBuilder builder;
        builder.appendString("ShadowRealm.evaluate failed : ");
        builder.appendString(e.toStringWithoutException(state));
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, builder.finalize());
    }
    return wrappedValue(state, callerRealm, scriptResult);
}

#endif

} // namespace Escargot
