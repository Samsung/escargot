#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

// Implements Date, Promise (deferred-based) and napi_run_script, on top of
// the same conventions NapiFunctions.cpp establishes (napi_value <-> ValueRef*
// punning, Evaluator::execute-wrapped exception boundaries, etc).

#include "NapiTypes.h"

// the opaque type node_api.h forward-declares for napi_create_promise et al.
// `promise` is a raw GC pointer, not itself rooted by this struct - unlike
// napi_value (which is fine sitting bare in a native stack local, since
// Boehm GC conservatively scans the stack), this struct is heap-allocated
// with plain `new`, so it is *not* itself a GC root and would not keep
// `promise` alive on its own. It is rooted explicitly via
// env->napiEnv->persistentValueRefMap()->add()/remove() (the same
// PersistentValueRefMap primitive napi_create_reference/napi_reference_ref
// use for strong napi_ref - see NapiFunctions.cpp), for exactly as long as
// the deferred is outstanding: one add() in napi_create_promise, matched by
// one remove() in whichever of napi_resolve_deferred/napi_reject_deferred
// settles it (both also delete the deferred itself, matching real Node-API's
// contract that a deferred may only be settled once).
struct napi_deferred__ {
    Escargot::PromiseObjectRef* promise;
};

namespace Escargot {
namespace Napi {

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_create_date(napi_env env, double time, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;
    DateObjectRef* date = DateObjectRef::create(state);
    // DateObjectRef only exposes an int64_t setter; `time` (already
    // milliseconds since epoch, per the napi_create_date contract) is
    // truncated to fit. Pure - constructing/initializing a DateObject cannot
    // run user JS, so unlike napi_call_function there is nothing to wrap in
    // Evaluator::execute here.
    date->setTimeValue(static_cast<int64_t>(time));
    *result = ToNapi(date);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_date_value(napi_env env, napi_value value, double* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isDateObject()) {
        return SetLastError(env, napi_date_expected);
    }
    *result = v->asDateObject()->primitiveValue();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_date(napi_env env, napi_value value, bool* result)
{
    *result = FromNapi(value)->isDateObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_promise(napi_env env, napi_deferred* deferred, napi_value* promise)
{
    if (deferred == nullptr || promise == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;
    PromiseObjectRef* promiseObj = PromiseObjectRef::create(state);

    napi_deferred__* def = new napi_deferred__();
    def->promise = promiseObj;
    // root the promise for as long as `def` is outstanding - see the
    // napi_deferred__ comment above.
    env->napiEnv->persistentValueRefMap()->add(promiseObj);

    *deferred = def;
    *promise = ToNapi(promiseObj);
    return napi_ok;
}

// shared by napi_resolve_deferred/napi_reject_deferred: settles `deferred`'s
// promise, unroots it, and deletes the deferred, regardless of outcome (a
// deferred may only be settled once, successfully or not - same as real
// Node-API).
static napi_status SettleDeferred(napi_env env, napi_deferred deferred, napi_value resolution, bool isFulfill)
{
    ExecutionStateRef* state = env->executionState;
    PromiseObjectRef* promiseObj = deferred->promise;
    ValueRef* value = FromNapi(resolution);

    // PromiseObjectRef::fulfill/reject only enqueue reaction jobs (see
    // PromiseObject::fulfill/reject) rather than running any reaction
    // synchronously, so neither can currently throw a JS exception - but we
    // still route the call through Evaluator::execute, exactly like
    // napi_call_function (NapiFunctions.cpp), so this boundary stays safe
    // even if that internal behavior ever changes (e.g. a registered
    // PromiseHook), instead of ever letting a raw C++ exception unwind past
    // this function.
    Evaluator::EvaluatorResult settleResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, PromiseObjectRef* promiseObj, ValueRef* value, bool isFulfill) -> ValueRef* {
            if (isFulfill) {
                promiseObj->fulfill(state, value);
            } else {
                promiseObj->reject(state, value);
            }
            return ValueRef::createUndefined();
        },
        promiseObj, value, isFulfill);

    env->napiEnv->persistentValueRefMap()->remove(promiseObj);
    delete deferred;

    if (!settleResult.isSuccessful()) {
        env->pendingException = settleResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_resolve_deferred(napi_env env, napi_deferred deferred, napi_value resolution)
{
    return SettleDeferred(env, deferred, resolution, true);
}

ESCARGOT_NAPI_EXPORT napi_status napi_reject_deferred(napi_env env, napi_deferred deferred, napi_value rejection)
{
    return SettleDeferred(env, deferred, rejection, false);
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_promise(napi_env env, napi_value value, bool* result)
{
    *result = FromNapi(value)->isPromiseObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_run_script(napi_env env, napi_value script, napi_value* result)
{
    ValueRef* scriptValue = FromNapi(script);
    if (!scriptValue->isString()) {
        return SetLastError(env, napi_string_expected);
    }

    ExecutionStateRef* state = env->executionState;
    ContextRef* context = env->context();
    StringRef* source = scriptValue->toStringWithoutException(context);

    Evaluator::EvaluatorResult evalResult = (state != nullptr) ? Evaluator::execute(
                                                                     state, [](ExecutionStateRef* state, ContextRef* context, StringRef* source) -> ValueRef* {
                                                                         ScriptRef* parsedScript = context->scriptParser()->initializeScript(source, StringRef::createFromASCII("napi_run_script"), false).fetchScriptThrowsExceptionIfParseError(state);
                                                                         return parsedScript->execute(state);
                                                                     },
                                                                     context, source)
                                                               : Evaluator::execute(context, [](ExecutionStateRef* state, napi_env env, StringRef* source) -> ValueRef* {
                env->executionState = state;
                ContextRef* context = env->context();
                ScriptRef* parsedScript = context->scriptParser()->initializeScript(source, StringRef::createFromASCII("napi_run_script"), false).fetchScriptThrowsExceptionIfParseError(state);
                ValueRef* res = parsedScript->execute(state);
                env->executionState = nullptr;
                return res; }, env, source);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (result != nullptr) {
        *result = ToNapi(evalResult.result);
    }
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
