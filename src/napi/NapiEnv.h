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

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// forward-declared here (full definition, alongside napi_env__/
// napi_handle_scope__/etc, lives in NapiTypes.h, which includes this header)
// purely so NapiEnv below can hold pointers to it - see
// NapiEnv::trackWeakRefTarget/clearWeakRefTargets.
struct napi_ref__;

// forward-declared here for the same reason as napi_ref__ above (full
// definitions live in NapiTypes.h) - napi_env__::topCallbackScope below only
// ever holds a pointer to the former, and NapiEnv only ever holds pointers to
// the latter in m_asyncCleanupHooks (NapiRuntime.cpp).
struct napi_callback_scope__;
struct napi_async_cleanup_hook_handle__;

namespace Escargot {
namespace Napi {

class NapiPlatform;
class NapiEnv;

// the real napi_env__ definition (only forward-declared by node_api.h).
// Owned directly by NapiEnv (see NapiEnv::m_env below) instead of being
// stack-allocated per call/per test: a napi_callback (e.g. a napi_wrap
// finalizer) can run later than the call that created it, so the env it
// captures must stay valid for as long as NapiEnv itself does, not just for
// one Evaluator::execute invocation.
struct EnvData {
    NapiEnv* napiEnv = nullptr;
    ExecutionStateRef* executionState = nullptr; // only valid for the duration of the current call
    OptionalRef<ValueRef> pendingException;
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
    // this NapiEnv is. Callers must still set env()->executionState before
    // making napi_* calls, since that part is only valid for the duration of
    // the current Evaluator::execute call.
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
    // (e.g. chaining) or call back into JS via env()->executionState, same as
    // real Node-API - both are safe at this point, unlike from within
    // node_api_post_finalizer's own registration call or a GC sweep.
    void drainPostFinalizers();

    // backs napi_ref: PersistentValueRefMap::add()/remove() are the GC-root
    // primitives napi_create_reference/napi_reference_ref/unref/delete_reference
    // build on for strong (refcount > 0) references
    PersistentValueRefMap* persistentValueRefMap()
    {
        return m_persistentValueRefMap.get();
    }

    // per-object side storage for napi_wrap's WrapFinalizeData* (NapiFunctions.cpp),
    // so napi_remove_wrap can find and unregister the GC finalizer napi_wrap
    // registered without needing extraData() for it - that slot must stay
    // exactly the caller's native_object, per napi_unwrap's contract. Cleared
    // by whichever happens first: napi_remove_wrap, or the wrap finalizer
    // itself once the wrapped object is actually collected. `obj` is a
    // non-owning key: safe because Escargot's GC never moves objects, and
    // every insertion is paired with an eventual removal along one of those
    // two paths, so a collected object's address is never left stale here.
    void setWrapFinalizerData(ObjectRef* obj, void* data)
    {
        m_wrapFinalizerData[obj] = data;
    }

    void* takeWrapFinalizerData(ObjectRef* obj)
    {
        auto iter = m_wrapFinalizerData.find(obj);
        if (iter == m_wrapFinalizerData.end()) {
            return nullptr;
        }
        void* data = iter->second;
        m_wrapFinalizerData.erase(iter);
        return data;
    }

    // non-mutating lookup, used by RunEnvCleanupWrapFinalizers (NapiFunctions.cpp)
    // to double-check a snapshotted (obj, data) pair is still the object's
    // *current* wrap-finalizer entry before invoking it (napi_remove_wrap, or
    // a finalizer that itself already ran reentrantly from within another
    // one's finalize_cb, may have changed it since the snapshot was taken).
    void* peekWrapFinalizerData(ObjectRef* obj) const
    {
        auto iter = m_wrapFinalizerData.find(obj);
        return iter == m_wrapFinalizerData.end() ? nullptr : iter->second;
    }

    // a point-in-time copy of every still-registered napi_wrap finalizer
    // entry, for env-cleanup-time forced finalization (RunEnvCleanupWrapFinalizers,
    // NapiFunctions.cpp): iterating m_wrapFinalizerData directly there isn't
    // safe, since invoking one entry's finalizer removes *itself* from that
    // same map (NapiWrapFinalizer calls takeWrapFinalizerData up front).
    std::vector<std::pair<ObjectRef*, void*>> snapshotWrapFinalizerData() const
    {
        std::vector<std::pair<ObjectRef*, void*>> result;
        result.reserve(m_wrapFinalizerData.size());
        for (auto& entry : m_wrapFinalizerData) {
            result.push_back(entry);
        }
        return result;
    }

    // Tracks every still-weak napi_ref pointing at a given GC-heap target
    // (`target` is that target's identity as a raw pointer - an ObjectRef*
    // for napi_wrap/napi_add_finalizer's purposes below, but napi_ref targets
    // in general can be any heap value a weak napi_ref can point at, e.g. a
    // Symbol from napi_create_reference; same non-owning-key rationale as
    // m_wrapFinalizerData above: Escargot's GC never moves objects), tracked
    // independently of Escargot's own GC finalizer list
    // (Memory::gcRegisterFinalizer, EscargotPublic.cpp), whose per-object
    // finalizer callbacks fire in plain registration order - unlike V8,
    // which clears every weak handle to a dying object *before* running any
    // of that object's second-pass finalizer callbacks. Without this, an
    // object that has both a napi_wrap/napi_add_finalizer finalizer *and* a
    // separate weak napi_ref to the same object
    // (napi_create_reference/napi_reference_unref) could have its
    // wrap/add_finalizer callback observe napi_get_reference_value as
    // still-live (not yet nulled) if that finalizer happened to be
    // registered - i.e. napi_wrap/napi_add_finalizer called - before the weak
    // napi_ref was created, which is exactly Escargot's registration order in
    // that case. NapiWrapFinalizer/NapiAddFinalizerFinalizer
    // (NapiFunctions.cpp/NapiExtras.cpp) call clearWeakRefTargets() up front,
    // before invoking the user's own finalize_cb, to force that same
    // already-nulled guarantee regardless of registration order (found via
    // test_reference/test.js's validateDeleteBeforeFinalize/
    // DeleteBeforeFinalizeFinalizer, which asserts exactly this).
    // NapiWeakRefFinalizer (the plain, no-other-finalizer case) still exists
    // and independently nulls the same napi_ref__::value - redundantly but
    // harmlessly, since by the time it runs here it's already null.
    // `ref` is stored as a type-erased void* (rather than napi_ref__*) so
    // this header doesn't need napi_ref__ to be a complete type (it's only
    // forward-declared here) - clearWeakRefTargets casts back once it's
    // defined out-of-line in NapiTypes.h, after napi_ref__'s real definition.
    void trackWeakRefTarget(void* target, napi_ref__* ref)
    {
        m_weakRefTargets[target].push_back(ref);
    }

    void untrackWeakRefTarget(void* target, napi_ref__* ref)
    {
        if (m_weakRefTargets.count(target) == 0) {
            return;
        }
        std::vector<void*>& refs = m_weakRefTargets[target];
        for (size_t i = 0; i < refs.size(); i++) {
            if (refs[i] == ref) {
                refs.erase(refs.begin() + i);
                break;
            }
        }
        if (refs.empty()) {
            m_weakRefTargets.erase(target);
        }
    }

    // defined out-of-line in NapiTypes.h, once napi_ref__ is a complete type
    // (this dereferences ref->value, unlike the two methods above).
    void clearWeakRefTargets(void* target);

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

private:
    NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context);

    PersistentRefHolder<VMInstanceRef> m_vmInstance;
    PersistentRefHolder<ContextRef> m_context;
    PersistentRefHolder<PersistentValueRefMap> m_persistentValueRefMap;
    HashMap<ObjectRef*, void*> m_wrapFinalizerData;
    HashMap<void*, std::vector<void*>> m_weakRefTargets;
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
