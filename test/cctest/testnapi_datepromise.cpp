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

// Exercises napi_create_date/napi_get_date_value/napi_is_date,
// napi_create_promise/napi_resolve_deferred/napi_reject_deferred/napi_is_promise
// and napi_run_script (NapiDatePromise.cpp), self-contained (no dlopen'd
// addon needed - unlike test/cctest/testnapi.cpp's TCs).

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstring>

TEST(Napi, Date)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            const double milliseconds = 1700000000123.0;

            napi_value date = nullptr;
            napi_status status = napi_create_date(env, milliseconds, &date);
            if (status != napi_ok) {
                return ValueRef::create(1);
            }

            bool isDate = false;
            napi_is_date(env, date, &isDate);
            if (!isDate) {
                return ValueRef::create(2);
            }

            napi_value notADate = ToNapi(ValueRef::create(1));
            bool notADateIsDate = true;
            napi_is_date(env, notADate, &notADateIsDate);
            if (notADateIsDate) {
                return ValueRef::create(3);
            }

            double roundTripped = 0;
            status = napi_get_date_value(env, date, &roundTripped);
            if (status != napi_ok) {
                return ValueRef::create(4);
            }
            if (roundTripped != milliseconds) {
                return ValueRef::create(5);
            }

            double unusedResult = 0;
            status = napi_get_date_value(env, notADate, &unusedResult);
            if (status != napi_date_expected) {
                return ValueRef::create(6);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

// captures what a promise's .then callback (attached via a direct property
// get + call, since this test has no require()/module loader to install a
// real thenable chain through) observed once napi_resolve_deferred settles
// the deferred and env->napiEnv->drainPendingJobs() runs the reaction job;
// plain file-static globals for the same reason testnapi.cpp's
// g_callbackArg/g_callbackThis are (NativeFunctionInfo only accepts
// capture-less function pointers).
static int g_promiseThenCallCount = 0;
static ValueRef* g_promiseThenArg = nullptr;

static ValueRef* RecordPromiseResolution(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    g_promiseThenCallCount++;
    g_promiseThenArg = argc > 0 ? argv[0] : ValueRef::createUndefined();
    return ValueRef::createUndefined();
}

TEST(Napi, Promise)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_promiseThenCallCount = 0;
    g_promiseThenArg = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value notAPromise = ToNapi(ValueRef::create(42));
            bool notAPromiseIsPromise = true;
            napi_is_promise(env, notAPromise, &notAPromiseIsPromise);
            if (notAPromiseIsPromise) {
                return ValueRef::create(1);
            }

            napi_deferred deferred = nullptr;
            napi_value promise = nullptr;
            napi_status status = napi_create_promise(env, &deferred, &promise);
            if (status != napi_ok) {
                return ValueRef::create(2);
            }

            bool isPromise = false;
            napi_is_promise(env, promise, &isPromise);
            if (!isPromise) {
                return ValueRef::create(3);
            }

            // attach a .then handler via a direct property get + call
            // (napi_call_function would work equally well here; this avoids
            // needing napi_get_named_property, which this slice doesn't add)
            AtomicStringRef* thenCallbackName = AtomicStringRef::create(state->context(), "recordThen", strlen("recordThen"));
            FunctionObjectRef* thenCallback = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(thenCallbackName, RecordPromiseResolution, 1, true, false));

            ObjectRef* promiseObj = FromNapi(promise)->asObject();
            ValueRef* thenFn = promiseObj->get(state, StringRef::createFromASCII("then"));
            ValueRef* thenArgs[1] = { thenCallback };
            thenFn->call(state, promiseObj, 1, thenArgs);

            napi_value resolution = ToNapi(ValueRef::create(42));
            status = napi_resolve_deferred(env, deferred, resolution);
            if (status != napi_ok) {
                return ValueRef::create(4);
            }

            // fulfill()/reject() only enqueue reaction jobs rather than
            // running them synchronously, so the .then callback above has
            // not run yet at this point.
            if (g_promiseThenCallCount != 0) {
                return ValueRef::create(5);
            }

            env->napiEnv->drainPendingJobs();

            if (g_promiseThenCallCount != 1) {
                return ValueRef::create(6);
            }
            if (g_promiseThenArg == nullptr || !g_promiseThenArg->isNumber() || g_promiseThenArg->asNumber() != 42) {
                return ValueRef::create(7);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

TEST(Napi, PromiseReject)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_promiseThenCallCount = 0;
    g_promiseThenArg = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_deferred deferred = nullptr;
            napi_value promise = nullptr;
            napi_status status = napi_create_promise(env, &deferred, &promise);
            if (status != napi_ok) {
                return ValueRef::create(1);
            }

            AtomicStringRef* catchCallbackName = AtomicStringRef::create(state->context(), "recordCatch", strlen("recordCatch"));
            FunctionObjectRef* catchCallback = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(catchCallbackName, RecordPromiseResolution, 1, true, false));

            ObjectRef* promiseObj = FromNapi(promise)->asObject();
            ValueRef* catchFn = promiseObj->get(state, StringRef::createFromASCII("catch"));
            ValueRef* catchArgs[1] = { catchCallback };
            catchFn->call(state, promiseObj, 1, catchArgs);

            napi_value rejection = ToNapi(StringRef::createFromASCII("nope"));
            status = napi_reject_deferred(env, deferred, rejection);
            if (status != napi_ok) {
                return ValueRef::create(2);
            }

            env->napiEnv->drainPendingJobs();

            if (g_promiseThenCallCount != 1) {
                return ValueRef::create(3);
            }
            if (g_promiseThenArg == nullptr || !g_promiseThenArg->isString()) {
                return ValueRef::create(4);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

TEST(Napi, RunScript)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value script = ToNapi(StringRef::createFromASCII("1 + 2"));
            napi_value scriptResult = nullptr;
            napi_status status = napi_run_script(env, script, &scriptResult);
            if (status != napi_ok) {
                return ValueRef::create(1);
            }
            if (!FromNapi(scriptResult)->isNumber() || FromNapi(scriptResult)->asNumber() != 3) {
                return ValueRef::create(2);
            }

            // a non-string napi_value must be rejected up front rather than
            // crashing/misbehaving.
            napi_value notAString = ToNapi(ValueRef::create(5));
            napi_value unusedResult = nullptr;
            status = napi_run_script(env, notAString, &unusedResult);
            if (status != napi_string_expected) {
                return ValueRef::create(3);
            }

            // a throwing script must report napi_pending_exception instead of
            // letting a raw C++ exception unwind past napi_run_script -
            // mirrors testnapi.cpp's CallFunctionReportsExceptionAsPendingStatus.
            napi_value throwingScript = ToNapi(StringRef::createFromASCII("throw new RangeError('boom')"));
            napi_value throwResult = nullptr;
            status = napi_run_script(env, throwingScript, &throwResult);
            if (status != napi_pending_exception) {
                return ValueRef::create(4);
            }

            bool isPending = false;
            napi_is_exception_pending(env, &isPending);
            if (!isPending) {
                return ValueRef::create(5);
            }

            napi_value exception = nullptr;
            napi_get_and_clear_last_exception(env, &exception);
            if (!FromNapi(exception)->isObject()) {
                return ValueRef::create(6);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

#endif // ENABLE_NAPI
