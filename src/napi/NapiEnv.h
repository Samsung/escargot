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

#ifndef __EscargotNapiEnv__
#define __EscargotNapiEnv__

#include "Escargot.h"
#include "api/EscargotPublic.h"

#include <node_api.h>
#include <uv.h>

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// forward-declared here for the same reason as napi_ref__ above (full
// definitions live in NapiTypes.h) - napi_env__::topCallbackScope below only
// ever holds a pointer to the former, and NapiEnv only ever holds pointers to
// the latter in m_asyncCleanupHooks (NapiRuntime.cpp).
struct napi_callback_scope__;
struct napi_async_cleanup_hook_handle__;
struct napi_threadsafe_function__;
struct napi_async_work__;

namespace Escargot {
namespace Napi {

class NapiPlatform;
class NapiEnv;

// OptionalRef retains only a raw GC pointer. Native N-API handles and EnvData
// are outside Escargot's root set, so values retained there across an API
// boundary must be held persistently.
template <typename T>
class PersistentOptionalRef {
public:
    PersistentOptionalRef() = default;
    PersistentOptionalRef(const PersistentOptionalRef&) = delete;
    PersistentOptionalRef& operator=(const PersistentOptionalRef&) = delete;

    PersistentOptionalRef& operator=(T* value)
    {
        m_value.reset(value);
        return *this;
    }

    PersistentOptionalRef& operator=(std::nullptr_t)
    {
        m_value.reset(nullptr);
        return *this;
    }

    bool hasValue()
    {
        return m_value.get() != nullptr;
    }

    T* value()
    {
        return m_value.get();
    }

private:
    PersistentRefHolder<T> m_value;
};

// the real napi_env__ definition (only forward-declared by node_api.h).
// Owned directly by NapiEnv (see NapiEnv::m_env below) instead of being
// stack-allocated per call/per test: a napi_callback (e.g. a napi_wrap
// finalizer) can run later than the call that created it, so the env it
// captures must stay valid for as long as NapiEnv itself does, not just for
// one Evaluator::execute invocation.
struct EnvData {
    NapiEnv* napiEnv = nullptr;
    PersistentOptionalRef<ValueRef> pendingException;
    std::string lastErrorMessage;
    napi_extended_error_info lastErrorInfo;
    // the napi_status of the most recent napi_*/node_api_* call that
    // returned something other than napi_ok, tracked so
    // napi_get_last_error_info (NapiFunctions.cpp) can look up its message
    // from Node's own error_messages[] table (see SetLastError in
    // NapiTypes.h) - mirrors js_native_api_v8.cc's env->last_error.error_code.
    napi_status lastErrorCode = napi_ok;

    // napi_set_instance_data/napi_get_instance_data; the finalizer is
    // invoked once from ~NapiEnv() (NapiEnv.cpp), the only environment-
    // teardown hook this PoC has
    void* instanceData = nullptr;
    napi_finalize instanceDataFinalizer = nullptr;
    void* instanceDataFinalizeHint = nullptr;

    // handle-scope bookkeeping (napi_open_handle_scope/napi_close_handle_scope/
    // napi_open_escapable_handle_scope/napi_close_escapable_handle_scope,
    // NapiFunctions.cpp). napi_value doesn't need real handle buffering here -
    // it's already the GC pointer itself (see ToNapi/FromNapi in
    // NapiTypes.h) - so this exists purely to enforce proper open/close
    // nesting order (LIFO, for napi_handle_scope_mismatch), the same way
    // V8's real handle scope stack does. Full type defined in NapiTypes.h.
    napi_handle_scope__* topHandleScope = nullptr;

    // same LIFO-nesting-order bookkeeping as topHandleScope above, but for
    // napi_open_callback_scope/napi_close_callback_scope (NapiRuntime.cpp) -
    // napi_close_callback_scope returns napi_callback_scope_mismatch if
    // `scope` isn't currently the innermost open one, same contract as
    // napi_close_handle_scope. Full type defined in NapiTypes.h.
    napi_callback_scope__* topCallbackScope = nullptr;
    int32_t callbackScopeDepth = 0;

    ContextRef* context(); // defined below, out-of-line (needs NapiEnv complete)
};

} // namespace Napi
} // namespace Escargot

// the opaque type node_api.h forward-declares; napi_env is `EnvData*` in disguise
struct napi_env__ : public Escargot::Napi::EnvData {
};

namespace Escargot {
namespace Napi {

// Owns the VMInstanceRef + ContextRef pair (and the napi_env__ itself) that a
// napi_env is backed by.
class NapiEnv {
public:
    // process-wide setup/teardown; call once before creating any NapiEnv, and once at shutdown
    static void globalInit();
    static void globalFinalize();

    // per-thread setup/teardown; call once per thread that will create/use a NapiEnv
    static void threadInit();
    static void threadFinalize();

    // creates a fresh, independent VMInstance + Context
    static NapiEnv* create(const char* locale = nullptr, const char* timezone = nullptr);
    // creates a Context on top of a VMInstance shared with other NapiEnv instances
    static NapiEnv* create(VMInstanceRef* sharedVMInstance);

    ~NapiEnv();

    VMInstanceRef* vmInstance()
    {
        return m_vmInstance.get();
    }

    ContextRef* context()
    {
        return m_context.get();
    }

    // the napi_env to pass across the N-API boundary; valid for as long as
    // this NapiEnv is. Each napi_* operation that needs an ExecutionStateRef
    // creates its own scoped Evaluator::execute call from context().
    napi_env env()
    {
        return &m_env;
    }

    // runs every job queued on this env's VMInstance (e.g. resolved Promise
    // reactions), then drains the main-thread callback queue below - this is
    // the single pump the whole N-API host (test harness / any embedder) is
    // expected to keep calling on the main (JS) thread; wiring the
    // main-thread callback queue in here means async-work completion
    // callbacks and threadsafe-function calls both surface through the exact
    // same pump an embedder is already required to drive for Promise
    // reactions, without needing a second, separate API to remember to call.
    //
    // As of the libuv integration (see NapiAsyncWork.cpp/NapiRuntime.cpp),
    // this is also where uvLoop() actually gets pumped
    // (uv_run(..., UV_RUN_NOWAIT)): async_work's uv_queue_work completions and
    // a threadsafe_function's uv_async_t wakeups are both delivered from
    // there, on this same thread. The legacy main-thread callback queue above
    // is drained alongside it (kept as a fallback path/for any other user of
    // enqueueMainThreadCallback), and the whole thing loops, bounded, until a
    // full pass makes no further progress - a uv callback or a main-thread
    // callback may itself resolve a Promise (queuing a VM job), and running a
    // VM job may itself be what unblocks something waiting on the JS side, so
    // a single one-shot drain of each independently isn't enough.
    // Returns true if any pass made progress (a VM job ran, a main-thread
    // callback fired, or a uv callback fed new work) - lets an external pump
    // loop (e.g. the NapiSuite test harness) fold uv-loop progress into its
    // own quiescence check rather than treating uv work as invisible.
    bool drainPendingJobs();

    // the real libuv event loop backing this env - owned for its whole
    // lifetime (uv_loop_init in the constructor, uv_loop_close at teardown,
    // see NapiEnv.cpp). napi_get_uv_event_loop (NapiRuntime.cpp) hands this
    // straight back to callers; NapiAsyncWork.cpp queues uv_work_t/uv_async_t
    // on it directly.
    uv_loop_t* uvLoop()
    {
        return &m_uvLoop;
    }

    // True while the loop still has genuinely pending work: an in-flight
    // uv_queue_work (async_work still running on the thread pool) or a ref'd,
    // active handle. An *unref'd* idle handle (e.g. an unref'd
    // threadsafe_function's uv_async_t) does NOT keep this true - matching
    // libuv's own "should the loop keep the process alive" semantics. An
    // external pump (the NapiSuite harness) uses this to keep pumping until
    // asynchronously-completing work has actually landed, rather than settling
    // the instant a single uv_run(NOWAIT) pass happens to find nothing ready.
    bool uvLoopAlive()
    {
        return uv_loop_alive(&m_uvLoop) != 0;
    }

    // Thread-safe main-thread callback queue - the single mechanism
    // napi_queue_async_work's completion callback and
    // napi_call_threadsafe_function use to get back onto the main (JS)
    // thread (see src/napi/NapiAsyncWork.cpp). enqueueMainThreadCallback may
    // be called from ANY thread (it only ever touches m_mainThreadCallbacks
    // under m_mainThreadCallbacksMutex); drainMainThreadCallbacks must only
    // ever be called from the main thread (it's what actually invokes the
    // queued std::function<void()>s, which are free to touch JS/napi_value/
    // the GC heap).
    void enqueueMainThreadCallback(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> guard(m_mainThreadCallbacksMutex);
        m_mainThreadCallbacks.push_back(std::move(callback));
    }

    // invokes and clears every main-thread callback queued so far, in FIFO
    // order. Swaps the queue out first (rather than iterating it directly)
    // so a callback that itself enqueues further work (or a concurrent
    // enqueueMainThreadCallback call from another thread, racing this drain)
    // is picked up by a *later* drain instead of invalidating the container
    // being iterated or being invoked out of this same loop.
    void drainMainThreadCallbacks()
    {
        for (;;) {
            std::deque<std::function<void()>> batch;
            {
                std::lock_guard<std::mutex> guard(m_mainThreadCallbacksMutex);
                if (m_mainThreadCallbacks.empty()) {
                    return;
                }
                batch.swap(m_mainThreadCallbacks);
            }
            for (std::function<void()>& callback : batch) {
                callback();
            }
        }
    }

    void trackThreadsafeFunction(napi_threadsafe_function func);
    void untrackThreadsafeFunction(napi_threadsafe_function func);
    void trackAsyncWork(napi_async_work work);
    void untrackAsyncWork(napi_async_work work);
    std::vector<napi_async_work> snapshotAsyncWorks();
    std::vector<napi_threadsafe_function> snapshotThreadsafeFunctions();

    // backs node_api_post_finalizer (NapiExtras.cpp): unlike a plain
    // finalize_cb passed to napi_wrap/napi_add_finalizer (which runs
    // synchronously from inside the GC's finalizer sweep, where calling back
    // into JS is unsafe), a post-finalizer is only ever recorded here and
    // actually invoked later, once execution has returned to a safe point -
    // draining happens in drainPostFinalizers(), which the test harness
    // pump loop (test/cctest/testnapi_suite.cpp) or any other safe-point
    // caller invokes. Entries are queued, not invoked, from
    // node_api_post_finalizer itself, exactly matching Node's own contract.
    struct PostFinalizerEntry {
        napi_finalize finalizeCb;
        void* finalizeData;
        void* finalizeHint;
    };

    void enqueuePostFinalizer(napi_finalize finalizeCb, void* finalizeData, void* finalizeHint)
    {
        m_pendingPostFinalizers.push_back({ finalizeCb, finalizeData, finalizeHint });
    }

    // invokes and clears every post-finalizer queued so far, in FIFO order.
    // A finalizer running here is free to enqueue further post-finalizers
    // (e.g. chaining) or call back into JS through regular napi_* calls, same
    // as real Node-API - both are safe at this point, unlike from within
    // node_api_post_finalizer's own registration call or a GC sweep.
    void drainPostFinalizers();

    // Side storage for napi_wrap. This registry lives in native memory, so a
    // raw ObjectRef* key would neither root the object nor be cleared by GC.
    struct WrapFinalizerEntry {
        PersistentRefHolder<ObjectRef> target;
        void* data;
    };

    bool hasWrapFinalizerData(ObjectRef* obj) const;
    void setWrapFinalizerData(ObjectRef* obj, void* data);
    void* takeWrapFinalizerData(ObjectRef* obj);
    void* takeWrapFinalizerDataByData(void* data);
    void* peekWrapFinalizerData(ObjectRef* obj) const;
    std::vector<std::pair<ObjectRef*, void*>> snapshotWrapFinalizerData() const;

    struct EnvFinalizerEntry {
        PersistentRefHolder<ValueRef> target;
        Memory::GCAllocatedMemoryFinalizer callback;
        void* data;
    };

    void registerEnvFinalizer(ValueRef* target, Memory::GCAllocatedMemoryFinalizer callback, void* data);
    void takeEnvFinalizerData(void* data);
    void runEnvFinalizers();

    struct NativeFinalizerEntry {
        void (*callback)(void*);
        void* data;
    };

    void registerNativeFinalizer(void (*callback)(void*), void* data);
    void takeNativeFinalizerData(void* data);
    void runNativeFinalizers();

    // True for the duration of a synchronous, GC-triggered napi_wrap/
    // napi_add_finalizer/napi_create_external finalize_cb (NapiWrapFinalizer/
    // NapiAddFinalizerFinalizer/NapiExternalObjectFinalizer) - i.e. NOT during
    // a node_api_post_finalizer drain (NapiEnv::drainPostFinalizers), which is
    // explicitly safe to call back into JS from. Real Node-API finalizers of
    // this synchronous kind receive only a `node_api_basic_env` for exactly
    // this reason: casting it back to a real napi_env and calling something
    // that needs to allocate/run JS (e.g. napi_create_object) is a contract
    // violation matching V8's own "may affect GC state" fatal error - see
    // this flag's one reader, napi_create_object (NapiFunctions.cpp).
    void setInGCUnsafeFinalizer(bool value)
    {
        m_inGCUnsafeFinalizer = value;
    }

    bool isInGCUnsafeFinalizer() const
    {
        return m_inGCUnsafeFinalizer;
    }

    // napi_add_env_cleanup_hook/napi_remove_env_cleanup_hook (NapiRuntime.cpp):
    // every still-registered hook runs exactly once at environment teardown,
    // most-recently-added-first (~NapiEnv(), NapiEnv.cpp) - mirroring Node's
    // own env_cleanup_hooks_ stack. removeEnvCleanupHook removes the
    // most-recently-added entry whose (fun, arg) pair matches exactly, same
    // as Node's RemoveCleanupHook.
    void addEnvCleanupHook(napi_cleanup_hook fun, void* arg);
    void removeEnvCleanupHook(napi_cleanup_hook fun, void* arg);

    // napi_add_async_cleanup_hook/napi_remove_async_cleanup_hook
    // (NapiRuntime.cpp): same LIFO-at-teardown contract as the env cleanup
    // hooks above, but keyed by an opaque napi_async_cleanup_hook_handle__*
    // (rather than a (fun, arg) pair) so a single hook can be registered more
    // than once and still be individually removable. Synchronous
    // approximation: at teardown each hook is simply invoked and then
    // considered done immediately, with no real waiting for asynchronous
    // completion (see NapiRuntime.cpp's own comment on napi_add_async_cleanup_hook).
    napi_async_cleanup_hook_handle__* addAsyncCleanupHook(napi_async_cleanup_hook hook, void* arg);
    void removeAsyncCleanupHook(napi_async_cleanup_hook_handle__* handle);

    // node_api_get_module_file_name (NapiRuntime.cpp): defaults to "" until a
    // real module loader exists to set it via setModuleFileName.
    void setModuleFileName(const std::string& name)
    {
        m_moduleFileName = name;
    }

    const std::string& moduleFileName() const
    {
        return m_moduleFileName;
    }

    std::vector<NativeFinalizerEntry> m_nativeFinalizers;

private:
    NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context);

    std::mutex m_asyncWorksMutex;
    std::vector<napi_async_work> m_asyncWorks;
    PersistentRefHolder<VMInstanceRef> m_vmInstance;
    PersistentRefHolder<ContextRef> m_context;
    std::mutex m_threadsafeFunctionsMutex;
    std::vector<napi_threadsafe_function> m_threadsafeFunctions;
    std::vector<WrapFinalizerEntry*> m_wrapFinalizerData;
    std::vector<EnvFinalizerEntry*> m_envFinalizers;
    std::vector<PostFinalizerEntry> m_pendingPostFinalizers;
    std::vector<std::pair<napi_cleanup_hook, void*>> m_envCleanupHooks;
    std::vector<napi_async_cleanup_hook_handle__*> m_asyncCleanupHooks;
    std::string m_moduleFileName;
    bool m_inGCUnsafeFinalizer = false;
    napi_env__ m_env;

    // see enqueueMainThreadCallback/drainMainThreadCallbacks above
    std::mutex m_mainThreadCallbacksMutex;
    std::deque<std::function<void()>> m_mainThreadCallbacks;

    // see uvLoop() above; initialized/torn down in the constructor/destructor (NapiEnv.cpp)
    uv_loop_t m_uvLoop;
};

} // namespace Napi
} // namespace Escargot

inline Escargot::ContextRef* Escargot::Napi::EnvData::context()
{
    return napiEnv->context();
}

#endif
#endif // ENABLE_NAPI
