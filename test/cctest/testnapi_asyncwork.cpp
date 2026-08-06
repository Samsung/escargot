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

// Exercises src/napi/NapiAsyncWork.cpp - napi_create_async_work/
// napi_queue_async_work/napi_cancel_async_work/napi_delete_async_work and the
// napi_threadsafe_function family. Unlike every other testnapi_*.cpp, these
// TESTs involve real worker std::threads racing NapiEnv's own main-thread
// callback queue - each test spawns work/calls a threadsafe function from
// another thread, then repeatedly calls napiEnv->drainPendingJobs() (via the
// DrainUntil helper below), with short bounded sleeps in between, until the
// expected callback(s) have actually run - never an unbounded wait, so a
// regression here fails the test instead of hanging it.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// shared test helpers
// ---------------------------------------------------------------------------

// Repeatedly drains napiEnv's pending jobs (which, per NapiEnv::drainPendingJobs,
// also drains its main-thread callback queue - see NapiEnv.h) until `done()`
// reports true or `maxIterations` short sleeps have elapsed. Every test below
// bounds its own wait through this, so a stuck/never-firing callback fails the
// assertion that follows instead of hanging the test binary.
template <typename Predicate>
static bool DrainUntil(NapiEnv* napiEnv, Predicate done, int maxIterations = 400, int sleepMs = 5)
{
    for (int i = 0; i < maxIterations; i++) {
        napiEnv->drainPendingJobs();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
    return done();
}

// ---------------------------------------------------------------------------
// napi_create_async_work / napi_queue_async_work
// ---------------------------------------------------------------------------

static std::atomic<bool> g_basicExecuteRan{ false };
static std::atomic<bool> g_basicCompleteRan{ false };
static std::thread::id g_basicExecuteThreadId;
static bool g_basicExecuteRanBeforeComplete = false;
static napi_status g_basicCompleteStatus = napi_generic_failure;
static bool g_basicCompleteCouldCreateValue = false;

static void BasicAsyncExecute(napi_env env, void* data)
{
    // native-only, per N-API contract - no env/napi_value touched here, just
    // native side effects a test can observe afterward
    g_basicExecuteThreadId = std::this_thread::get_id();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    g_basicExecuteRan.store(true);
}

static void BasicAsyncComplete(napi_env env, napi_status status, void* data)
{
    g_basicExecuteRanBeforeComplete = g_basicExecuteRan.load();
    g_basicCompleteStatus = status;

    // `complete` (unlike `execute`) is allowed to touch napi_value/the GC
    // heap - confirm this actually works from here
    napi_value obj = nullptr;
    napi_status createStatus = napi_create_object(env, &obj);
    g_basicCompleteCouldCreateValue = (createStatus == napi_ok && obj != nullptr);

    g_basicCompleteRan.store(true);
}

TEST(NapiAsyncWork, ExecuteRunsOnWorkerCompleteRunsOnMainThread)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_basicExecuteRan = false;
    g_basicCompleteRan = false;
    g_basicExecuteRanBeforeComplete = false;
    g_basicCompleteStatus = napi_generic_failure;
    g_basicCompleteCouldCreateValue = false;
    std::thread::id mainThreadId = std::this_thread::get_id();

    napi_async_work work = nullptr;
    ASSERT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, &work), napi_ok);
    ASSERT_NE(work, nullptr);

    ASSERT_EQ(napi_queue_async_work(env, work), napi_ok);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_basicCompleteRan.load(); }));

    EXPECT_TRUE(g_basicExecuteRan.load());
    EXPECT_NE(g_basicExecuteThreadId, mainThreadId);
    EXPECT_TRUE(g_basicExecuteRanBeforeComplete);
    EXPECT_EQ(g_basicCompleteStatus, napi_ok);
    EXPECT_TRUE(g_basicCompleteCouldCreateValue);

    EXPECT_EQ(napi_delete_async_work(env, work), napi_ok);
}

TEST(NapiAsyncWork, QueueTwiceRejectsSecondCall)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_basicCompleteRan = false;

    napi_async_work work = nullptr;
    ASSERT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, &work), napi_ok);

    ASSERT_EQ(napi_queue_async_work(env, work), napi_ok);
    // a second napi_queue_async_work on the same (already-queued) work item
    // must be rejected, deterministically regardless of scheduling - `queued`
    // is set synchronously, under `work->mutex`, before the first call even
    // returns
    EXPECT_EQ(napi_queue_async_work(env, work), napi_generic_failure);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_basicCompleteRan.load(); }));
    EXPECT_EQ(napi_delete_async_work(env, work), napi_ok);
}

TEST(NapiAsyncWork, DeleteWhileRunningIsRejectedThenSucceedsAfterCompletion)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_basicCompleteRan = false;

    napi_async_work work = nullptr;
    ASSERT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, &work), napi_ok);
    ASSERT_EQ(napi_queue_async_work(env, work), napi_ok);

    // Deterministic regardless of scheduling: `complete` can only run from
    // inside drainPendingJobs, which this test has not called yet - so `work`
    // cannot possibly be in the Completed state at this point.
    EXPECT_EQ(napi_delete_async_work(env, work), napi_generic_failure);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_basicCompleteRan.load(); }));

    // now safe
    EXPECT_EQ(napi_delete_async_work(env, work), napi_ok);
}

// ---------------------------------------------------------------------------
// napi_cancel_async_work
// ---------------------------------------------------------------------------

static std::atomic<bool> g_cancelExecuteRan{ false };
static std::atomic<bool> g_cancelCompleteRan{ false };
static napi_status g_cancelCompleteStatus = napi_ok;

static void CancelAsyncExecute(napi_env env, void* data)
{
    g_cancelExecuteRan.store(true);
}

static void CancelAsyncComplete(napi_env env, napi_status status, void* data)
{
    g_cancelCompleteStatus = status;
    g_cancelCompleteRan.store(true);
}

TEST(NapiAsyncWork, CancelBeforeQueueSkipsExecuteAndReportsCancelled)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_cancelExecuteRan = false;
    g_cancelCompleteRan = false;
    g_cancelCompleteStatus = napi_ok;

    napi_async_work work = nullptr;
    ASSERT_EQ(napi_create_async_work(env, nullptr, nullptr, CancelAsyncExecute, CancelAsyncComplete, nullptr, &work), napi_ok);

    // best-effort cancel (see NapiAsyncWork.cpp's own comment): succeeds here
    // because `work` is still Idle - napi_queue_async_work hasn't run yet
    ASSERT_EQ(napi_cancel_async_work(env, work), napi_ok);

    ASSERT_EQ(napi_queue_async_work(env, work), napi_ok);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_cancelCompleteRan.load(); }));

    EXPECT_FALSE(g_cancelExecuteRan.load());
    EXPECT_EQ(g_cancelCompleteStatus, napi_cancelled);

    // cancelling an already-completed work item must fail (nothing left to cancel)
    EXPECT_EQ(napi_cancel_async_work(env, work), napi_generic_failure);

    EXPECT_EQ(napi_delete_async_work(env, work), napi_ok);
}

TEST(NapiAsyncWork, CancelAfterCompletionFails)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_basicCompleteRan = false;

    napi_async_work work = nullptr;
    ASSERT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, &work), napi_ok);
    ASSERT_EQ(napi_queue_async_work(env, work), napi_ok);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_basicCompleteRan.load(); }));

    EXPECT_EQ(napi_cancel_async_work(env, work), napi_generic_failure);
    EXPECT_EQ(napi_delete_async_work(env, work), napi_ok);
}

// ---------------------------------------------------------------------------
// NULL-arg guards (async_work)
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, AsyncWorkNullArgGuards)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_async_work work = nullptr;
    EXPECT_EQ(napi_create_async_work(nullptr, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, &work), napi_invalid_arg);
    EXPECT_EQ(napi_create_async_work(env, nullptr, nullptr, nullptr, BasicAsyncComplete, nullptr, &work), napi_invalid_arg);
    EXPECT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, nullptr, nullptr, &work), napi_invalid_arg);
    EXPECT_EQ(napi_create_async_work(env, nullptr, nullptr, BasicAsyncExecute, BasicAsyncComplete, nullptr, nullptr), napi_invalid_arg);

    EXPECT_EQ(napi_queue_async_work(nullptr, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_cancel_async_work(nullptr, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_delete_async_work(nullptr, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_delete_async_work(env, nullptr), napi_invalid_arg);
}

// ---------------------------------------------------------------------------
// napi_threadsafe_function shared helpers
// ---------------------------------------------------------------------------

// Creates `(function(){ globalThis.<name> = []; return function(x){
// globalThis.<name>.push(x); }; })()` in napiEnv's context and returns the
// inner pusher function, so a threadsafe function's call_js_cb has a real JS
// function (not just a native one) to actually call.
static napi_value CreateArrayPusherFunction(NapiEnv* napiEnv, const char* globalArrayName)
{
    napi_value result = nullptr;
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, const char* globalArrayName, napi_value* outFunc) -> ValueRef* {
            env->executionState = state;
            std::string src = std::string("(function(){ globalThis.") + globalArrayName + " = []; return function(x){ globalThis." + globalArrayName + ".push(x); }; })()";
            ScriptRef* script = state->context()->scriptParser()->initializeScript(StringRef::createFromUTF8(src.data(), src.size()), StringRef::createFromASCII("testnapi_asyncwork"), false).fetchScriptThrowsExceptionIfParseError(state);
            ValueRef* fn = script->execute(state);
            *outFunc = ToNapi(fn);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), globalArrayName, &result);
    return result;
}

static void ReadArrayLengthAndSum(NapiEnv* napiEnv, const char* globalArrayName, uint32_t* outLen, double* outSum)
{
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, const char* globalArrayName, uint32_t* outLen, double* outSum) -> ValueRef* {
            env->executionState = state;
            napi_value global = nullptr;
            napi_get_global(env, &global);
            napi_value arr = nullptr;
            napi_get_named_property(env, global, globalArrayName, &arr);
            uint32_t len = 0;
            napi_get_array_length(env, arr, &len);
            double sum = 0;
            for (uint32_t i = 0; i < len; i++) {
                napi_value elem = nullptr;
                napi_get_element(env, arr, i, &elem);
                double v = 0;
                napi_get_value_double(env, elem, &v);
                sum += v;
            }
            *outLen = len;
            *outSum = sum;
            return ValueRef::createUndefined();
        },
        napiEnv->env(), globalArrayName, outLen, outSum);
}

// Standalone array-length read, wrapped in its own Evaluator::execute (same
// reason as ReadArrayLengthAndSum above: env->executionState is only valid
// nested inside an active call, and is NOT still valid just because some
// earlier, already-returned Evaluator::execute call happened to set it) -
// for use directly inside a DrainUntil predicate.
static uint32_t GetArrayLength(NapiEnv* napiEnv, const char* globalArrayName)
{
    uint32_t len = 0;
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, const char* globalArrayName, uint32_t* outLen) -> ValueRef* {
            env->executionState = state;
            napi_value global = nullptr;
            napi_get_global(env, &global);
            napi_value arr = nullptr;
            napi_get_named_property(env, global, globalArrayName, &arr);
            napi_get_array_length(env, arr, outLen);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), globalArrayName, &len);
    return len;
}

// call_js_cb: converts `data` (a small int smuggled through the void* payload
// via reinterpret_cast, same convention Node's own tests use) into a real
// napi_value and calls `js_callback` with it as its one argument.
static void PushIntCallJs(napi_env env, napi_value js_callback, void* context, void* data)
{
    if (js_callback == nullptr) {
        return;
    }
    napi_value arg = nullptr;
    napi_create_int32(env, static_cast<int32_t>(reinterpret_cast<intptr_t>(data)), &arg);
    napi_value argv[1] = { arg };
    napi_call_function(env, ToNapi(ValueRef::createUndefined()), js_callback, 1, argv, nullptr);
}

static void* IntData(int value)
{
    return reinterpret_cast<void*>(static_cast<intptr_t>(value));
}

static std::atomic<bool> g_tsfnFinalizeRan{ false };
static void* g_tsfnFinalizeSeenData = nullptr;

static void RecordThreadFinalize(napi_env env, void* data, void* hint)
{
    g_tsfnFinalizeSeenData = data;
    g_tsfnFinalizeRan.store(true);
}

// ---------------------------------------------------------------------------
// napi_create_threadsafe_function / napi_call_threadsafe_function -
// multi-producer delivery
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, TsfnDeliversCallsFromMultipleThreads)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnMultiResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;
    g_tsfnFinalizeSeenData = nullptr;
    int finalizeMarker = 0xBEEF;

    constexpr int kThreadCount = 3;
    constexpr int kCallsPerThread = 5;
    constexpr int kTotalCalls = kThreadCount * kCallsPerThread;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, /*max_queue_size*/ 0, /*initial_thread_count*/ kThreadCount, &finalizeMarker, RecordThreadFinalize, /*context*/ &finalizeMarker, PushIntCallJs, &tsfn), napi_ok);
    ASSERT_NE(tsfn, nullptr);

    std::atomic<int> nextValue{ 0 };
    std::vector<std::thread> producers;
    for (int t = 0; t < kThreadCount; t++) {
        producers.emplace_back([tsfn, &nextValue]() {
            for (int c = 0; c < kCallsPerThread; c++) {
                int value = nextValue.fetch_add(1);
                napi_status status = napi_call_threadsafe_function(tsfn, IntData(value), napi_tsfn_blocking);
                EXPECT_EQ(status, napi_ok);
            }
            EXPECT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
        });
    }
    for (std::thread& t : producers) {
        t.join();
    }

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));
    EXPECT_EQ(g_tsfnFinalizeSeenData, &finalizeMarker);

    uint32_t len = 0;
    double sum = 0;
    ReadArrayLengthAndSum(napiEnv, "__tsfnMultiResults", &len, &sum);
    EXPECT_EQ(len, static_cast<uint32_t>(kTotalCalls));
    // values delivered are exactly {0, ..., kTotalCalls-1} in some order (one
    // per fetch_add), so their sum is fully determined regardless of delivery
    // order across the 3 producer threads
    double expectedSum = (kTotalCalls - 1) * kTotalCalls / 2.0;
    EXPECT_DOUBLE_EQ(sum, expectedSum);
}

// ---------------------------------------------------------------------------
// napi_acquire_threadsafe_function / napi_release_threadsafe_function lifecycle
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, TsfnAcquireReleaseLifecycle)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnLifecycleResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, 0, /*initial_thread_count*/ 1, nullptr, RecordThreadFinalize, nullptr, PushIntCallJs, &tsfn), napi_ok);

    // acquire bumps the count to 2; one matching release must NOT tear it down yet
    ASSERT_EQ(napi_acquire_threadsafe_function(tsfn), napi_ok);
    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    EXPECT_FALSE(g_tsfnFinalizeRan.load());

    // still usable - one acquisition remains
    ASSERT_EQ(napi_call_threadsafe_function(tsfn, IntData(7), napi_tsfn_blocking), napi_ok);
    ASSERT_TRUE(DrainUntil(napiEnv, [napiEnv]() {
        return GetArrayLength(napiEnv, "__tsfnLifecycleResults") == 1;
    }));

    // final release tears it down - `closing` flips synchronously, inside
    // this very call, before teardown (freeing `tsfn`) is actually enqueued
    // to run later on the main thread - so an extra release attempted right
    // here (and only right here, before draining lets teardown actually run)
    // can safely observe `napi_closing` without touching already-freed memory.
    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    EXPECT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_closing);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));
    // `tsfn` is freed as of the drain above - must not be touched again
}

// ---------------------------------------------------------------------------
// max_queue_size: non-blocking napi_queue_full + blocking wait-for-room
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, TsfnMaxQueueSizeNonBlockingReturnsQueueFull)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnQueueFullResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, /*max_queue_size*/ 1, /*initial_thread_count*/ 1, nullptr, RecordThreadFinalize, nullptr, PushIntCallJs, &tsfn), napi_ok);

    // fills the single slot - deterministic, no drain has happened yet
    ASSERT_EQ(napi_call_threadsafe_function(tsfn, IntData(1), napi_tsfn_nonblocking), napi_ok);
    // queue is now full; a second non-blocking call must fail immediately
    EXPECT_EQ(napi_call_threadsafe_function(tsfn, IntData(2), napi_tsfn_nonblocking), napi_queue_full);

    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));
}

TEST(NapiAsyncWork, TsfnMaxQueueSizeBlockingWaitsForRoom)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnBlockingResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, /*max_queue_size*/ 1, /*initial_thread_count*/ 1, nullptr, RecordThreadFinalize, nullptr, PushIntCallJs, &tsfn), napi_ok);

    // fills the single slot
    ASSERT_EQ(napi_call_threadsafe_function(tsfn, IntData(1), napi_tsfn_nonblocking), napi_ok);

    std::atomic<bool> blockingCallReturned{ false };
    std::thread blocker([tsfn, &blockingCallReturned]() {
        napi_status status = napi_call_threadsafe_function(tsfn, IntData(2), napi_tsfn_blocking);
        EXPECT_EQ(status, napi_ok);
        blockingCallReturned.store(true);
    });

    // give the blocking call a moment to actually start waiting on the full queue
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_FALSE(blockingCallReturned.load());

    // draining delivers the first queued item, freeing a slot and waking the
    // blocked producer
    ASSERT_TRUE(DrainUntil(napiEnv, [&blockingCallReturned]() { return blockingCallReturned.load(); }));
    blocker.join();
    EXPECT_TRUE(blockingCallReturned.load());

    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));

    uint32_t len = 0;
    double sum = 0;
    ReadArrayLengthAndSum(napiEnv, "__tsfnBlockingResults", &len, &sum);
    EXPECT_EQ(len, 2u);
    EXPECT_DOUBLE_EQ(sum, 3.0); // 1 + 2
}

// ---------------------------------------------------------------------------
// napi_tsfn_abort: already-queued calls are still delivered, THEN the
// finalizer runs. abort does not discard pre-abort items (matches real
// Node-API; see napi_release_threadsafe_function's note and
// node-api/test_threadsafe_function_abort). It only forces teardown and
// rejects *new* calls with napi_closing.
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, TsfnAbortDeliversQueuedCallsThenRunsFinalizer)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnAbortResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, /*max_queue_size*/ 0, /*initial_thread_count*/ 1, nullptr, RecordThreadFinalize, nullptr, PushIntCallJs, &tsfn), napi_ok);

    // queue a few calls but never drain before aborting
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(napi_call_threadsafe_function(tsfn, IntData(i), napi_tsfn_nonblocking), napi_ok);
    }

    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_abort), napi_ok);

    // further calls must be rejected once closing
    EXPECT_EQ(napi_call_threadsafe_function(tsfn, IntData(99), napi_tsfn_nonblocking), napi_closing);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));

    // the 3 pre-abort calls ARE delivered before teardown (0 + 1 + 2 = 3);
    // the post-abort call (99) was rejected with napi_closing, so it is not
    // included.
    uint32_t len = 0;
    double sum = 0;
    ReadArrayLengthAndSum(napiEnv, "__tsfnAbortResults", &len, &sum);
    EXPECT_EQ(len, 3u);
    EXPECT_EQ(sum, 3.0);
}

// ---------------------------------------------------------------------------
// default call_js_cb (NULL): calls `func` with no arguments
// ---------------------------------------------------------------------------

static std::atomic<int> g_defaultCallJsInvocations{ 0 };

static napi_value CountingNativeFunction(napi_env env, napi_callback_info info)
{
    g_defaultCallJsInvocations.fetch_add(1);
    napi_value undef = nullptr;
    napi_get_undefined(env, &undef);
    return undef;
}

TEST(NapiAsyncWork, TsfnDefaultCallJsCbInvokesFuncWithNoArgs)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    g_defaultCallJsInvocations = 0;
    g_tsfnFinalizeRan = false;

    napi_value fn = nullptr;
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, napi_value* outFn) -> ValueRef* {
            env->executionState = state;
            napi_create_function(env, "counting", NAPI_AUTO_LENGTH, CountingNativeFunction, nullptr, outFn);
            return ValueRef::createUndefined();
        },
        env, &fn);
    ASSERT_NE(fn, nullptr);

    napi_threadsafe_function tsfn = nullptr;
    // call_js_cb == nullptr: default behavior is calling `func` with no args
    ASSERT_EQ(napi_create_threadsafe_function(env, fn, nullptr, nullptr, 0, 1, nullptr, RecordThreadFinalize, nullptr, nullptr, &tsfn), napi_ok);

    ASSERT_EQ(napi_call_threadsafe_function(tsfn, nullptr, napi_tsfn_blocking), napi_ok);
    ASSERT_EQ(napi_call_threadsafe_function(tsfn, nullptr, napi_tsfn_blocking), napi_ok);

    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_defaultCallJsInvocations.load() == 2; }));

    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));
}

// ---------------------------------------------------------------------------
// napi_get_threadsafe_function_context / napi_ref_threadsafe_function /
// napi_unref_threadsafe_function
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, TsfnGetContextAndRefUnref)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnContextResults");
    ASSERT_NE(jsFunc, nullptr);

    g_tsfnFinalizeRan = false;
    int contextMarker = 123;

    napi_threadsafe_function tsfn = nullptr;
    ASSERT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, 0, 1, nullptr, RecordThreadFinalize, &contextMarker, PushIntCallJs, &tsfn), napi_ok);

    void* contextOut = nullptr;
    ASSERT_EQ(napi_get_threadsafe_function_context(tsfn, &contextOut), napi_ok);
    EXPECT_EQ(contextOut, &contextMarker);

    // minimal keepalive bookkeeping (see NapiAsyncWork.cpp's own comment) -
    // must not crash and must always succeed
    EXPECT_EQ(napi_ref_threadsafe_function(env, tsfn), napi_ok);
    EXPECT_EQ(napi_unref_threadsafe_function(env, tsfn), napi_ok);
    EXPECT_EQ(napi_ref_threadsafe_function(env, tsfn), napi_ok);

    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release), napi_ok);
    ASSERT_TRUE(DrainUntil(napiEnv, []() { return g_tsfnFinalizeRan.load(); }));
}

// ---------------------------------------------------------------------------
// NULL-arg guards (threadsafe function)
// ---------------------------------------------------------------------------

TEST(NapiAsyncWork, ThreadsafeFunctionNullArgGuards)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value jsFunc = CreateArrayPusherFunction(napiEnv, "__tsfnNullArgResults");
    ASSERT_NE(jsFunc, nullptr);

    napi_threadsafe_function tsfn = nullptr;
    EXPECT_EQ(napi_create_threadsafe_function(nullptr, jsFunc, nullptr, nullptr, 0, 1, nullptr, nullptr, nullptr, PushIntCallJs, &tsfn), napi_invalid_arg);
    EXPECT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, 0, 1, nullptr, nullptr, nullptr, PushIntCallJs, nullptr), napi_invalid_arg);
    // initial_thread_count == 0 is rejected
    EXPECT_EQ(napi_create_threadsafe_function(env, jsFunc, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, PushIntCallJs, &tsfn), napi_invalid_arg);
    // func == NULL and call_js_cb == NULL together leave nothing to ever call
    EXPECT_EQ(napi_create_threadsafe_function(env, nullptr, nullptr, nullptr, 0, 1, nullptr, nullptr, nullptr, nullptr, &tsfn), napi_invalid_arg);

    EXPECT_EQ(napi_get_threadsafe_function_context(nullptr, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_call_threadsafe_function(nullptr, nullptr, napi_tsfn_nonblocking), napi_invalid_arg);
    EXPECT_EQ(napi_acquire_threadsafe_function(nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_release_threadsafe_function(nullptr, napi_tsfn_release), napi_invalid_arg);
    EXPECT_EQ(napi_ref_threadsafe_function(nullptr, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_unref_threadsafe_function(env, nullptr), napi_invalid_arg);
}

#endif // ENABLE_NAPI
