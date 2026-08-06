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

// napi_create_async_work/napi_queue_async_work/napi_cancel_async_work/
// napi_delete_async_work and the napi_threadsafe_function family - the two
// slices of node_api.h that need real OS threads, deliberately left out of
// NapiRuntime.cpp (see that file's own header comment).
//
// Threading/execution model (backed by the real system libuv event loop
// owned by this env - NapiEnv::uvLoop(), NapiEnv.h/.cpp):
//  - napi_queue_async_work hands `execute`/`complete` straight to
//    uv_queue_work(): `execute` runs on one of libuv's own thread-pool
//    threads (a small, persistent, reused pool - not one fresh std::thread
//    per work item), and `complete` (the "after work" callback) runs back on
//    the loop thread, i.e. whichever thread calls
//    NapiEnv::drainPendingJobs() (which itself calls uv_run(loop,
//    UV_RUN_NOWAIT) - see NapiEnv.cpp). That is why this file's tests can
//    keep assuming `complete` only ever runs from inside drainPendingJobs.
//  - Every libuv thread-pool thread that reaches this file's ExecuteWork
//    calls Escargot::Globals::initializeThread() at entry and
//    Globals::finalizeThread() at exit, exactly bracketing the `execute`
//    callback (same convention Shell.cpp's builtin262EvalScript/
//    builtin262AgentStart use for a real VMInstance/Context - this worker
//    skips that part entirely, see below). Escargot's GC (Boehm,
//    conservative/stop-the-world) must know about a thread's existence to
//    suspend it during a collection and scan its stack; without this
//    registration a GC pass triggered from any other thread while a
//    thread-pool thread is mid-`execute` would not see (or safely stop) that
//    thread at all. Unlike the one-shot-std::thread predecessor of this file,
//    a libuv thread-pool thread is reused across many work items over its
//    lifetime - initializeThread()/finalizeThread() bracket each individual
//    `execute` call (not the thread's whole lifetime), which is safe because
//    both are symmetric per-call (ThreadLocal::inited, EscargotPublic.cpp, is
//    reset to false by finalizeThread() before any later initializeThread()
//    call on that same, reused OS thread) and Boehm's own thread registration
//    (triggered transitively through GC_init(), inside
//    Globals::initializeThread()'s call chain) is idempotent for a thread
//    that's already known to it.
//  - `execute` is native-only by N-API contract - no JS/napi_value/GC-heap
//    access - so a thread-pool thread never touches this env's
//    VMInstanceRef/ContextRef and never creates a JS value while running it;
//    it may still call Memory::gcMalloc et al for its own native allocations,
//    which is exactly why the Globals::initializeThread() registration above
//    is required even though no JS ever runs there.
//  - Both `complete` (async_work) and a threadsafe function's call_js_cb only
//    ever run on the loop thread (reached via uv_run(..., UV_RUN_NOWAIT) from
//    NapiEnv::drainPendingJobs() - the after-work callback of a queued
//    uv_work_t for async_work, and a napi_threadsafe_function's own
//    uv_async_t callback for threadsafe functions), wrapped in its own
//    Evaluator::execute (not env->executionState, which is only valid nested
//    inside an existing napi call) - same top-level-entry pattern used by
//    test/cctest/testnapi_runtime.cpp's own MakeCallback/
//    BufferFromArrayBuffer tests and by ~NapiEnv()'s own teardown Evaluator
//    call: neither callback is nested inside any other napi_* call's own
//    ExecutionStateRef, so each must create its own SandBox to safely call
//    back into JS (call_js_cb, or the default napi_call_function below, may
//    throw a raw C++ exception on an uncaught JS exception).
//  - Ordering between "push a call onto a threadsafe function's queue" and
//    "the last release setting `closing`" is made safe purely by both
//    happening under the same napi_threadsafe_function__::mutex: whichever
//    happens first under that lock is what the OTHER one observes - a push
//    that wins the race is already in `queue` by the time a subsequent
//    release's teardown check (TsfnAsyncCallback, which always drains `queue`
//    down to empty before deciding whether to tear down) can ever run, and a
//    release that wins instead flips `closing` before that push's own lock
//    acquisition, so the push correctly observes `closing` and is rejected -
//    see TsfnAsyncCallback's own comment for why this holds regardless of how
//    many uv_async_send() calls end up coalesced by libuv.
//
// Approximations (see individual functions for detail):
//  - napi_cancel_async_work is best-effort: succeeds (skipping `execute`
//    entirely) if `execute` hasn't actually started running yet - whether
//    that's because the work item was never queued at all, or because
//    uv_cancel() on an already-queued-but-not-yet-started uv_work_t
//    succeeds - and fails (with `execute` left to run to completion) once
//    it's already running.
//  - napi_ref_threadsafe_function/napi_unref_threadsafe_function are close to
//    no-ops: nothing here actually keeps a whole process alive/lets it exit
//    the way Node's real event loop ref-counting does, so they just record a
//    bookkeeping flag and always succeed.
//  - max_queue_size is honored (napi_queue_full for a non-blocking call
//    against a full bounded queue; a blocking call waits on a condition
//    variable instead) but, like real Node-API, this offers no guarantee once
//    a caller misuses the acquire/release contract concurrently with
//    in-flight calls - see the ordering note above for exactly what *is*
//    guaranteed.

#include "NapiTypes.h"

#include <uv.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

// the opaque type node_api.h forward-declares for napi_create_async_work et
// al (node_api_types.h: `typedef struct napi_async_work__* napi_async_work`).
// Plain heap allocation (not GC-managed): nothing here is a JS/GC-heap value,
// and its lifetime is explicitly managed by napi_create_async_work/
// napi_delete_async_work, same contract as napi_ref__ (NapiTypes.h).
struct napi_async_work__ {
    napi_env env;
    napi_async_execute_callback execute;
    napi_async_complete_callback complete;
    void* data;

    // Idle -> Running -> Completed is the normal path (set from
    // ExecuteWork/AfterWork below, libuv's thread-pool-thread/loop-thread
    // callbacks for this work item's uv_work_t). Idle -> Cancelled happens
    // instead if napi_cancel_async_work runs before ExecuteWork has actually
    // taken `mutex` for the first time (whether or not this work item has
    // even been queued to libuv yet).
    enum class State {
        Idle,
        Running,
        Cancelled,
        Completed
    };

    std::mutex mutex; // guards state/queued below (also serializes against ExecuteWork/AfterWork's own transitions)
    State state = State::Idle;
    bool queued = false; // true once napi_queue_async_work has actually called uv_queue_work, guards against double-queueing
    uv_work_t req; // req.data == this once queued; the uv_work_t this napi_async_work is queued as
};

// the opaque type node_api.h forward-declares for
// napi_create_threadsafe_function et al (node_api_types.h:
// `typedef struct napi_threadsafe_function__* napi_threadsafe_function`).
// Same plain-heap-allocation rationale as napi_async_work__ above.
struct napi_threadsafe_function__ {
    napi_env env;
    napi_ref funcRef; // strong napi_ref to `func` (napi_create_reference w/ initial_refcount 1), so it survives for as long as this tsfn does; null if `func` was itself NULL (call_js_cb-only usage)
    void* context;
    napi_threadsafe_function_call_js callJsCb;
    void* threadFinalizeData;
    napi_finalize threadFinalizeCb;

    uv_async_t async; // async.data == this; the uv_async_t producers (napi_call_threadsafe_function, from ANY thread) wake to get delivery running on the loop thread - see TsfnAsyncCallback

    std::mutex mutex; // guards every field below (queue/maxQueueSize/threadCount/refd/closing/tornDown), including the push-vs-closing ordering (see this file's header comment)
    std::condition_variable cv; // signalled whenever `queue` shrinks or `closing` becomes true, for a blocking napi_call_threadsafe_function waiting on queue space
    std::deque<void*> queue;
    size_t maxQueueSize; // 0 means unbounded
    size_t threadCount; // acquire/release count - seeded from initial_thread_count
    bool refd = true; // napi_ref_threadsafe_function/napi_unref_threadsafe_function bookkeeping only (see this file's header comment) - not read anywhere else
    bool closing = false; // set once the last release (or any napi_tsfn_abort release) happens; no further calls are accepted once true
    bool tornDown = false; // guards TsfnAsyncCallback's one-shot teardown (funcRef/finalizer/uv_close) against running more than once
};

namespace Escargot {
namespace Napi {
namespace {

// Delivers exactly one already-popped call to `func`'s JS function, on the
// loop thread (called only from TsfnAsyncCallback below, which pops `data`
// off `func->queue` itself).
void InvokeThreadsafeFunctionCall(napi_threadsafe_function func, void* data)
{
    napi_env env = func->env;
    ContextRef* ctx = env->context();
    // Evaluator::execute (not env->executionState, which is only valid
    // nested inside an existing napi call) - see this file's header comment.
    Evaluator::execute(
        ctx, [](ExecutionStateRef* state, napi_threadsafe_function func, napi_env env, void* data) -> ValueRef* {
            env->executionState = state;

            napi_value jsFunc = nullptr;
            if (func->funcRef != nullptr) {
                napi_get_reference_value(env, func->funcRef, &jsFunc);
            }

            if (func->callJsCb != nullptr) {
                func->callJsCb(env, jsFunc, func->context, data);
            } else if (jsFunc != nullptr) {
                // no call_js_cb: default behavior is calling `func` with no
                // arguments, per napi_create_threadsafe_function's own
                // contract
                napi_call_function(env, ToNapi(ValueRef::createUndefined()), jsFunc, 0, nullptr, nullptr);
            }
            return ValueRef::createUndefined();
        },
        func, env, data);
}

// Runs exactly once, the first time TsfnAsyncCallback (below) observes
// `func->closing` with an already-empty queue: releases the strong napi_ref
// to `func`'s JS function, runs the user's thread_finalize_cb, then closes
// `func`'s uv_async_t (asynchronously freeing `func` itself from that
// handle's close callback, once libuv actually gets around to running it -
// see this function's own uv_close call). By the time this runs,
// `func->closing` is already true and TsfnAsyncCallback always drains
// `func->queue` down to empty before ever reaching this call - see this
// file's header comment for why that ordering guarantee means no in-flight
// delivery ever observes `func` already torn down.
void TearDownThreadsafeFunction(napi_threadsafe_function func)
{
    napi_env env = func->env;
    if (func->funcRef != nullptr) {
        napi_delete_reference(env, func->funcRef);
        func->funcRef = nullptr;
    }
    if (func->threadFinalizeCb != nullptr) {
        // Node-API contract: the thread finalize callback is invoked as
        // thread_finalize_cb(env, thread_finalize_data, context) - i.e. the
        // tsfn's `context` is passed as the finalize_hint, NOT nullptr. Addons
        // routinely stash their state in `context` and recover it here as
        // finalize_hint (e.g. test_threadsafe_function_abort's tsfn_finalize
        // does `static_cast<Context*>(finalize_hint)`); passing nullptr made
        // that a null-deref crash on abort teardown.
        func->threadFinalizeCb(env, func->threadFinalizeData, func->context);
    }
    uv_close(reinterpret_cast<uv_handle_t*>(&func->async), [](uv_handle_t* handle) {
        delete static_cast<napi_threadsafe_function>(handle->data);
    });
}

// napi_threadsafe_function's uv_async_t callback - runs on the loop thread
// (via NapiEnv::drainPendingJobs()'s uv_run(loop, UV_RUN_NOWAIT), see
// NapiEnv.cpp), woken by uv_async_send() from napi_call_threadsafe_function
// (any thread) or napi_release_threadsafe_function (the releasing thread).
// libuv may coalesce multiple uv_async_send() calls that happen before this
// callback gets to run into a single invocation - safe here because this
// always drains `func->queue` down to empty in a loop (rather than assuming
// exactly one queued item per invocation), so no pushed call is ever skipped
// regardless of how many sends coalesced into the invocation that eventually
// observes it.
void TsfnAsyncCallback(uv_async_t* handle)
{
    napi_threadsafe_function func = static_cast<napi_threadsafe_function>(handle->data);

    for (;;) {
        void* data = nullptr;
        bool hasData = false;
        {
            std::lock_guard<std::mutex> lock(func->mutex);
            if (!func->queue.empty()) {
                data = func->queue.front();
                func->queue.pop_front();
                hasData = true;
            }
        }
        if (!hasData) {
            break;
        }
        // wake any blocking napi_call_threadsafe_function producer waiting
        // for room, now that a slot just freed up
        func->cv.notify_all();
        InvokeThreadsafeFunctionCall(func, data);
    }

    bool shouldTearDown = false;
    {
        std::lock_guard<std::mutex> lock(func->mutex);
        if (func->closing && func->queue.empty() && !func->tornDown) {
            func->tornDown = true;
            shouldTearDown = true;
        }
    }
    if (shouldTearDown) {
        TearDownThreadsafeFunction(func);
    }
}

// uv_queue_work's "work" callback - runs on one of libuv's thread-pool
// threads (see this file's header comment for the Boehm-GC-registration
// rationale of the initializeThread()/finalizeThread() bracket below).
void ExecuteWork(uv_work_t* req)
{
    napi_async_work work = static_cast<napi_async_work>(req->data);

    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(work->mutex);
        if (work->state == napi_async_work__::State::Cancelled) {
            cancelled = true;
        } else {
            work->state = napi_async_work__::State::Running;
        }
    }

    if (!cancelled) {
        Globals::initializeThread();
        work->execute(work->env, work->data);
        Globals::finalizeThread();
    }
}

// uv_queue_work's "after work" callback - runs on the loop thread (i.e.
// whichever thread calls NapiEnv::drainPendingJobs(), see NapiEnv.cpp).
// `status` is UV_ECANCELED if uv_cancel() (napi_cancel_async_work below)
// actually managed to cancel this work item before libuv's thread pool ever
// started running it; ExecuteWork itself may also have separately observed
// `state == Cancelled` and skipped calling `execute` (the
// cancelled-before-ever-being-queued case) without libuv itself considering
// the uv_work_t cancelled - both are reported identically as napi_cancelled.
void AfterWork(uv_work_t* req, int status)
{
    napi_async_work work = static_cast<napi_async_work>(req->data);

    napi_status napiStatus;
    {
        std::lock_guard<std::mutex> lock(work->mutex);
        bool cancelled = (status == UV_ECANCELED) || (work->state == napi_async_work__::State::Cancelled);
        napiStatus = cancelled ? napi_cancelled : napi_ok;
    }

    // `complete` (unlike `execute`) has full napi_env access (napi_value/
    // GC-heap-creating calls included) - it needs a valid
    // env->executionState to do that, same reason
    // InvokeThreadsafeFunctionCall above wraps its own call_js_cb invocation
    // in Evaluator::execute rather than calling straight through.
    napi_env env = work->env;
    ContextRef* ctx = env->context();
    Evaluator::execute(
        ctx, [](ExecutionStateRef* state, napi_async_work work, napi_status status) -> ValueRef* {
            work->env->executionState = state;
            work->complete(work->env, status, work->data);
            return ValueRef::createUndefined();
        },
        work, napiStatus);

    std::lock_guard<std::mutex> lock(work->mutex);
    work->state = napi_async_work__::State::Completed;
}

} // namespace

extern "C" {

// ---------------------------------------------------------------------------
// napi_create_async_work / napi_queue_async_work / napi_cancel_async_work /
// napi_delete_async_work
// ---------------------------------------------------------------------------

ESCARGOT_NAPI_EXPORT napi_status napi_create_async_work(napi_env env, napi_value async_resource, napi_value async_resource_name, napi_async_execute_callback execute, napi_async_complete_callback complete, void* data, napi_async_work* result)
{
    if (env == nullptr || execute == nullptr || complete == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // async_resource/async_resource_name are accepted purely for API-surface
    // compatibility, same as napi_async_init (NapiRuntime.cpp) - there is no
    // async_hooks integration here to route them through.

    napi_async_work__* work = new napi_async_work__();
    work->env = env;
    work->execute = execute;
    work->complete = complete;
    work->data = data;
    *result = work;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_queue_async_work(node_api_basic_env env, napi_async_work work)
{
    if (env == nullptr || work == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    {
        std::lock_guard<std::mutex> lock(work->mutex);
        if (work->queued) {
            return SetLastError(env, napi_generic_failure);
        }
        work->queued = true;
    }

    work->req.data = work;
    // Queued unconditionally, even if napi_cancel_async_work already flipped
    // `state` to Cancelled before this call (Idle-but-not-yet-queued is a
    // valid state for that) - ExecuteWork/AfterWork above both check for
    // Cancelled themselves and skip running/report napi_cancelled
    // accordingly, so this still ends up calling `complete` exactly once,
    // same as the normal path.
    int rc = uv_queue_work(env->napiEnv->uvLoop(), &work->req, ExecuteWork, AfterWork);
    if (rc != 0) {
        std::lock_guard<std::mutex> lock(work->mutex);
        work->queued = false;
        return SetLastError(env, napi_generic_failure);
    }

    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_cancel_async_work(node_api_basic_env env, napi_async_work work)
{
    if (env == nullptr || work == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    std::lock_guard<std::mutex> lock(work->mutex);
    if (work->state == napi_async_work__::State::Idle) {
        bool wasQueued = work->queued;
        work->state = napi_async_work__::State::Cancelled;
        if (wasQueued) {
            // Best-effort (see this file's header comment): succeeds only if
            // libuv's thread pool hasn't actually started ExecuteWork for
            // this uv_work_t yet - a no-op failure otherwise, which is fine,
            // since ExecuteWork's own Cancelled check (set just above, under
            // this same mutex, so already visible to it) is what actually
            // guarantees `execute` never runs in that case too.
            uv_cancel(reinterpret_cast<uv_req_t*>(&work->req));
        }
        return napi_ok;
    }
    // Not Idle: `execute` is already running (or has already finished) -
    // there is no way to interrupt it from here.
    return SetLastError(env, napi_generic_failure);
}

ESCARGOT_NAPI_EXPORT napi_status napi_delete_async_work(napi_env env, napi_async_work work)
{
    if (env == nullptr || work == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    {
        std::lock_guard<std::mutex> lock(work->mutex);
        // Safe to delete only once this work item's own `complete` has
        // actually finished running (state Completed, matching real
        // Node-API's contract that napi_delete_async_work runs after
        // `complete` returns - typically called as the last thing `complete`
        // itself does), or if it was never queued at all. Anything else
        // (Running, or Idle-but-queued/Cancelled-but-not-yet-run) means
        // libuv's thread pool may still touch `work->req` later - freeing it
        // now would be a use-after-free once ExecuteWork/AfterWork gets
        // there.
        bool safeToDelete = (work->state == napi_async_work__::State::Completed) || !work->queued;
        if (!safeToDelete) {
            return SetLastError(env, napi_generic_failure);
        }
    }

    delete work;
    return napi_ok;
}

// ---------------------------------------------------------------------------
// napi_create_threadsafe_function / napi_get_threadsafe_function_context /
// napi_call_threadsafe_function / napi_acquire_threadsafe_function /
// napi_release_threadsafe_function / napi_ref_threadsafe_function /
// napi_unref_threadsafe_function
// ---------------------------------------------------------------------------

ESCARGOT_NAPI_EXPORT napi_status napi_create_threadsafe_function(napi_env env, napi_value func, napi_value async_resource, napi_value async_resource_name, size_t max_queue_size, size_t initial_thread_count, void* thread_finalize_data, napi_finalize thread_finalize_cb, void* context, napi_threadsafe_function_call_js call_js_cb, napi_threadsafe_function* result)
{
    if (env == nullptr || result == nullptr || initial_thread_count == 0) {
        return SetLastError(env, napi_invalid_arg);
    }
    if (func == nullptr && call_js_cb == nullptr) {
        // nothing this tsfn could ever actually call
        return SetLastError(env, napi_invalid_arg);
    }
    // async_resource/async_resource_name: same API-surface-only acceptance
    // as napi_create_async_work above.

    napi_ref funcRef = nullptr;
    if (func != nullptr) {
        napi_status refStatus = napi_create_reference(env, func, 1, &funcRef);
        if (refStatus != napi_ok) {
            return SetLastError(env, refStatus);
        }
    }

    napi_threadsafe_function__* tsfn = new napi_threadsafe_function__();
    tsfn->env = env;
    tsfn->funcRef = funcRef;
    tsfn->context = context;
    tsfn->callJsCb = call_js_cb;
    tsfn->threadFinalizeData = thread_finalize_data;
    tsfn->threadFinalizeCb = thread_finalize_cb;
    tsfn->maxQueueSize = max_queue_size;
    tsfn->threadCount = initial_thread_count;

    tsfn->async.data = tsfn;
    int rc = uv_async_init(env->napiEnv->uvLoop(), &tsfn->async, TsfnAsyncCallback);
    if (rc != 0) {
        if (funcRef != nullptr) {
            napi_delete_reference(env, funcRef);
        }
        delete tsfn;
        return SetLastError(env, napi_generic_failure);
    }

    *result = tsfn;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_threadsafe_function_context(napi_threadsafe_function func, void** result)
{
    if (func == nullptr || result == nullptr) {
        return napi_invalid_arg;
    }
    *result = func->context;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_call_threadsafe_function(napi_threadsafe_function func, void* data, napi_threadsafe_function_call_mode is_blocking)
{
    if (func == nullptr) {
        return napi_invalid_arg;
    }

    std::unique_lock<std::mutex> lock(func->mutex);
    if (func->closing) {
        return SetLastError(func->env, napi_closing);
    }
    if (func->maxQueueSize > 0 && func->queue.size() >= func->maxQueueSize) {
        if (is_blocking == napi_tsfn_nonblocking) {
            return SetLastError(func->env, napi_queue_full);
        }
        func->cv.wait(lock, [func]() {
            return func->closing || func->queue.size() < func->maxQueueSize;
        });
        if (func->closing) {
            return SetLastError(func->env, napi_closing);
        }
    }

    func->queue.push_back(data);
    lock.unlock();
    // Wakes the loop thread (see TsfnAsyncCallback) - safe to call from any
    // thread, and safe with respect to a concurrent
    // napi_release_threadsafe_function's own teardown check, which always
    // drains `queue` down to empty before ever tearing down (see this file's
    // header comment).
    uv_async_send(&func->async);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_acquire_threadsafe_function(napi_threadsafe_function func)
{
    if (func == nullptr) {
        return napi_invalid_arg;
    }
    std::lock_guard<std::mutex> lock(func->mutex);
    if (func->closing) {
        return SetLastError(func->env, napi_closing);
    }
    func->threadCount++;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_release_threadsafe_function(napi_threadsafe_function func, napi_threadsafe_function_release_mode mode)
{
    if (func == nullptr) {
        return napi_invalid_arg;
    }

    std::unique_lock<std::mutex> lock(func->mutex);
    if (func->closing) {
        return SetLastError(func->env, napi_closing);
    }
    if (func->threadCount == 0) {
        // unbalanced release - nothing left to release
        return SetLastError(func->env, napi_invalid_arg);
    }

    func->threadCount--;
    // abort forces teardown regardless of remaining thread count; a normal
    // release only tears down once the last thread has released.
    bool shouldTearDown = (func->threadCount == 0 || mode == napi_tsfn_abort);
    if (shouldTearDown) {
        func->closing = true;
        // NOTE: abort does NOT discard already-queued items. Real Node-API
        // dispatches everything enqueued before the abort to the JS callback,
        // then finalizes (verified against Node's own ThreadSafeFunction and
        // required by node-api/test_threadsafe_function_abort, whose finalizer
        // asserts its one pre-abort call actually ran). abort only (a) forces
        // teardown here even if threadCount > 0, and (b) makes subsequent
        // napi_call_threadsafe_function return napi_closing (via func->closing
        // above). TsfnAsyncCallback drains the queue - delivering those items -
        // before it runs the teardown.
    }
    lock.unlock();
    func->cv.notify_all();
    if (shouldTearDown) {
        // Wakes the loop thread so TsfnAsyncCallback drains any remaining
        // queued items (delivering them) and then runs the actual teardown -
        // see this file's header comment for why this is safe regardless of
        // whether a concurrent napi_call_threadsafe_function's own push+send
        // is still in flight.
        uv_async_send(&func->async);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_ref_threadsafe_function(node_api_basic_env env, napi_threadsafe_function func)
{
    if (env == nullptr || func == nullptr) {
        return SetLastError(reinterpret_cast<napi_env>(env), napi_invalid_arg);
    }
    std::lock_guard<std::mutex> lock(func->mutex);
    if (!func->refd) {
        func->refd = true;
        uv_ref(reinterpret_cast<uv_handle_t*>(&func->async));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_unref_threadsafe_function(node_api_basic_env env, napi_threadsafe_function func)
{
    if (env == nullptr || func == nullptr) {
        return SetLastError(reinterpret_cast<napi_env>(env), napi_invalid_arg);
    }
    std::lock_guard<std::mutex> lock(func->mutex);
    if (func->refd) {
        func->refd = false;
        uv_unref(reinterpret_cast<uv_handle_t*>(&func->async));
    }
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
