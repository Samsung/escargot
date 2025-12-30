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
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/ModuleNamespaceObject.h"
#include "runtime/WrappedFunctionObject.h"
#include "runtime/SandBox.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#if defined(ENABLE_SHADOWREALM)

ShadowRealmObject::ShadowRealmObject(ExecutionState& state, Object* proto, Context* realmContext, Script* referrer)
    : DerivedObject(state, proto)
    , m_realmContext(realmContext)
    , m_referrer(referrer)
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

static Value shadowRealmImportValueFulfilledFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let f be the active function object.
    auto f = state.resolveCallee()->asExtendedNativeFunctionObject();

    Script::ModuleData::ModulePromiseObject* promise = f->internalSlotAsPointer<Script::ModuleData::ModulePromiseObject>(0);
    Script::ModuleData::ModulePromiseObject* outerPromise = reinterpret_cast<Script::ModuleData::ModulePromiseObject*>(argv[0].asPointerValue()->asPromiseObject());

    SandBox sb(state.context());
    auto namespaceResult = sb.run([](ExecutionState& state, void* data) -> Value {
        Script* s = (Script*)data;
        if (!s->isExecuted()) {
            s->execute(state);
        }
        return Value(s->getModuleNamespace(state));
    },
                                  outerPromise->m_loadedScript);

    if (!namespaceResult.error.isEmpty()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "there is an error on module execution");
    }

    auto ns = namespaceResult.result.asObject();

    // Let string be f.[[ExportNameString]].
    auto string = f->internalSlot(0).asString();
    // 4. Assert: string is a String.
    // Let hasOwn be ? HasOwnProperty(exports, string).
    auto hasOwn = ns->hasOwnProperty(state, ObjectPropertyName(state, string));
    // If hasOwn is false, throw a TypeError exception.
    if (!hasOwn) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "there is no exported property what importValue expected");
    }
    // Let value be ? Get(exports, string).
    Value value = ns->get(state, ObjectPropertyName(state, string)).value(state, argv[0]);
    // Let realm be f.[[Realm]].
    auto realm = f->codeBlock()->context();
    // Return ? GetWrappedValue(realm, value).
    return ShadowRealmObject::wrappedValue(state, realm, value);
}

static Value shadowRealmImportValueRejectedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let realmRecord be the function's associated Realm Record.
    // Let copiedError be CreateTypeErrorCopy(realmRecord, error).
    // Return ThrowCompletion(copiedError).
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ShadowRealm importValue rejected");
    return Value();
}

Value ShadowRealmObject::importValue(ExecutionState& state, String* specifierString, String* exportNameString, Context* callerRealm)
{
    Context* evalRealm = m_realmContext;
    // Let evalContext be GetShadowRealmContext(evalRealm, true).
    ExecutionState evalState(evalRealm);
    // Let innerCapability be ! NewPromiseCapability(%Promise%).
    auto innerCapability = PromiseObject::newPromiseCapability(evalState, state.context()->globalObject()->promise());
    // Let runningContext be the running execution context.
    // If runningContext is not already suspended, suspend runningContext.
    // Push evalContext onto the execution context stack; evalContext is now the running execution context.
    // Let referrer be the Realm component of evalContext.
    // Perform HostLoadImportedModule(referrer, specifierString, empty, innerCapability).
    Global::platform()->hostImportModuleDynamically(evalRealm, m_referrer, specifierString, Platform::ModuleES, innerCapability.m_promise->asPromiseObject());
    // Suspend evalContext and remove it from the execution context stack.
    // Resume the context that is now on the top of the execution context stack as the running execution context.
    // Let steps be the algorithm steps defined in ExportGetter functions.
    // Let onFulfilled be CreateBuiltinFunction(steps, 1, "", « [[ExportNameString]] », callerRealm).
    // Set onFulfilled.[[ExportNameString]] to exportNameString.
    ExtendedNativeFunctionObject* onFulfilled = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), shadowRealmImportValueFulfilledFunction, 1));
    onFulfilled->setInternalSlot(0, exportNameString);
    // Let errorSteps be the algorithm steps defined in ImportValueError functions.
    // Let onRejected be CreateBuiltinFunction(errorSteps, 1, "", «», callerRealm).
    ExtendedNativeFunctionObject* onRejected = new ExtendedNativeFunctionObjectImpl<0>(state, NativeFunctionInfo(AtomicString(), shadowRealmImportValueRejectedFunction, 1));
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // Return PerformPromiseThen(innerCapability.[[Promise]], onFulfilled, onRejected, promiseCapability).
    return innerCapability.m_promise->asPromiseObject()->then(state, onFulfilled, onRejected, promiseCapability).value();
}

#endif

} // namespace Escargot
