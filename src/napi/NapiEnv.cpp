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

#include "NapiEnv.h"
#include "NapiPlatform.h"
#include "NapiTypes.h" // RunEnvCleanupWrapFinalizers

#include <utility>

namespace Escargot {
namespace Napi {

static NapiPlatform* g_platform = nullptr;

void NapiEnv::globalInit()
{
    if (Globals::isInitialized()) {
        return;
    }
    g_platform = new NapiPlatform();
    Globals::initialize(g_platform);
}

void NapiEnv::globalFinalize()
{
    if (!Globals::isInitialized()) {
        return;
    }
    Globals::finalize();
    g_platform = nullptr;
}

void NapiEnv::threadInit()
{
    Globals::initializeThread();
}

void NapiEnv::threadFinalize()
{
    Globals::finalizeThread();
}

NapiEnv* NapiEnv::create(const char* locale, const char* timezone)
{
    PersistentRefHolder<VMInstanceRef> vmInstance = VMInstanceRef::create(locale, timezone);
    PersistentRefHolder<ContextRef> context = ContextRef::create(vmInstance.get());
    return new NapiEnv(std::move(vmInstance), std::move(context));
}

NapiEnv* NapiEnv::create(VMInstanceRef* sharedVMInstance)
{
    PersistentRefHolder<VMInstanceRef> vmInstance(sharedVMInstance);
    PersistentRefHolder<ContextRef> context = ContextRef::create(sharedVMInstance);
    return new NapiEnv(std::move(vmInstance), std::move(context));
}

NapiEnv::NapiEnv(PersistentRefHolder<VMInstanceRef>&& vmInstance, PersistentRefHolder<ContextRef>&& context)
    : m_vmInstance(std::move(vmInstance))
    , m_context(std::move(context))
{
    m_env.napiEnv = this;
    // napi_get_uv_event_loop (NapiRuntime.cpp) and every async_work/
    // threadsafe_function (NapiAsyncWork.cpp) created against this env queue
    // work directly onto this loop - see uvLoop() and drainPendingJobs below.
    int rc = uv_loop_init(&m_uvLoop);
    RELEASE_ASSERT(rc == 0);
}

NapiEnv::~NapiEnv()
{
    // napi_add_async_cleanup_hook/napi_add_env_cleanup_hook (NapiRuntime.cpp):
    // every still-registered cleanup hook runs exactly once here, at
    // environment teardown, most-recently-added-first - mirroring Node's own
    // cleanup-hook stack. Run before RunEnvCleanupWrapFinalizers/the instance
    // data finalizer below, since a cleanup hook's job (freeing native
    // resources this env's addons allocated) is meant to happen at the very
    // start of teardown, same as Node's RunCleanup does before an
    // Environment's other teardown steps.
    //
    // Async cleanup hooks first: each is simply invoked and treated as done
    // immediately (no real async waiting for completion - see
    // napi_add_async_cleanup_hook's own comment, NapiRuntime.cpp), then
    // popped/freed. A hook is popped *before* being invoked so that a hook
    // which itself calls napi_remove_async_cleanup_hook on another
    // still-pending handle only ever operates on the (unpopped) remainder of
    // this same list.
    while (!m_asyncCleanupHooks.empty()) {
        napi_async_cleanup_hook_handle__* handle = m_asyncCleanupHooks.back();
        m_asyncCleanupHooks.pop_back();
        napi_async_cleanup_hook hook = handle->hook;
        void* arg = handle->arg;
        hook(handle, arg);
        delete handle;
    }
    while (!m_envCleanupHooks.empty()) {
        std::pair<napi_cleanup_hook, void*> entry = m_envCleanupHooks.back();
        m_envCleanupHooks.pop_back();
        entry.first(entry.second);
    }

    // A queued async work request owns both its native handle and this env.
    // Cancel what has not started and wait for every after-work callback
    // before any finalizer can tear down the values those callbacks use.
    DrainEnvAsyncWorks(this);

    // Close unreleased TSFNs while callbacks can still use the complete env.
    AbortEnvThreadsafeFunctions(this);

    // Real Node-API environment-teardown semantics: every still-registered
    // napi_wrap finalizer runs now, regardless of whether its wrapped object
    // is even still reachable (e.g. kept alive by module.exports) - see
    // RunEnvCleanupWrapFinalizers's own comment (NapiFunctions.cpp).
    RunEnvCleanupWrapFinalizers(this);
    runEnvFinalizers();
    runNativeFinalizers();

    // napi_set_instance_data's finalizer runs exactly once, here at
    // environment teardown - this is the only other teardown hook this PoC has.
    if (m_env.instanceDataFinalizer != nullptr) {
        napi_finalize finalizer = m_env.instanceDataFinalizer;
        m_env.instanceDataFinalizer = nullptr;
        finalizer(&m_env, m_env.instanceData, m_env.instanceDataFinalizeHint);
    }

    // libuv teardown: deliver close callbacks queued by TSFN teardown, then
    // force-close any remaining addon-owned handles (plus
    // libuv's own internal handles) and run the loop until those close
    // callbacks have actually fired, so uv_loop_close below never sees a
    // handle still open (which it would otherwise refuse to close).
    for (int i = 0; i < 64; i++) {
        drainPendingJobs();
        if (!uv_loop_alive(&m_uvLoop)) {
            break;
        }
    }
    uv_walk(
        &m_uvLoop, [](uv_handle_t* handle, void*) {
            if (!uv_is_closing(handle)) {
                uv_close(handle, nullptr);
            }
        },
        nullptr);
    // A few more passes to run the close callbacks queued by uv_walk above
    // (and, transitively, any napi_threadsafe_function teardown/finalizer
    // they still needed to run - see NapiAsyncWork.cpp's TsfnAsyncCallback) -
    // bounded (not "while still alive"): an uncompleted uv_work_t REQUEST
    // (as opposed to a handle - uv_walk above only visits handles) from a
    // still-in-flight async_work would otherwise keep uv_run reporting
    // "still alive" indefinitely, spinning this loop until that unrelated
    // background thread-pool item happens to finish - a handle's own close
    // callback, by contrast, reliably fires within the first pass or two.
    for (int i = 0; i < 16 && uv_run(&m_uvLoop, UV_RUN_NOWAIT) != 0; i++) {
    }
    int closeRc = uv_loop_close(&m_uvLoop);
    // EBUSY means some handle is still open - shouldn't happen after the walk
    // above, but isn't fatal (the loop object itself just leaks its internal
    // bookkeeping) so this is a soft assertion, not a RELEASE_ASSERT.
    ASSERT(closeRc == 0);
    (void)closeRc;

    // Best-effort: flush any napi_wrap'd/finalizer-bearing garbage created
    // through this env before its Context/VMInstance actually go away below
    // (member destructors run after this body, in reverse declaration
    // order). Left alone, such garbage can otherwise sit uncollected and get
    // opportunistically finalized far later - even mid-construction of a
    // totally unrelated NapiEnv's VMInstance - which is not a safe time to
    // run arbitrary addon finalizer code (this actually crashed a test once,
    // see napi-notes.md). Same "clear stack + churn + gc x5" pattern used in
    // test/cctest/testnapi.cpp and test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic.
    //
    // Must run before NapiEnv::globalFinalize() (Globals::finalize()) tears
    // down the GC itself; callers are expected to destroy every NapiEnv first.
    ContextRef* ctx = m_context.get();
    Evaluator::execute(ctx, [](ExecutionStateRef* state) -> ValueRef* {
        return ValueRef::create(100);
    });
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
}


bool NapiEnv::hasWrapFinalizerData(ObjectRef* obj) const
{
    return peekWrapFinalizerData(obj) != nullptr;
}

void NapiEnv::setWrapFinalizerData(ObjectRef* obj, void* data)
{
    WrapFinalizerEntry* entry = new WrapFinalizerEntry();
    entry->target.reset(obj);
    entry->target.setWeak();
    entry->data = data;
    m_wrapFinalizerData.push_back(entry);
}

void* NapiEnv::takeWrapFinalizerData(ObjectRef* obj)
{
    for (auto iter = m_wrapFinalizerData.begin(); iter != m_wrapFinalizerData.end(); ++iter) {
        WrapFinalizerEntry* entry = *iter;
        if (entry->target.get() == obj) {
            void* data = entry->data;
            m_wrapFinalizerData.erase(iter);
            delete entry;
            return data;
        }
    }
    return nullptr;
}

void* NapiEnv::takeWrapFinalizerDataByData(void* data)
{
    for (auto iter = m_wrapFinalizerData.begin(); iter != m_wrapFinalizerData.end(); ++iter) {
        WrapFinalizerEntry* entry = *iter;
        if (entry->data == data) {
            m_wrapFinalizerData.erase(iter);
            delete entry;
            return data;
        }
    }
    return nullptr;
}

void* NapiEnv::peekWrapFinalizerData(ObjectRef* obj) const
{
    for (WrapFinalizerEntry* entry : m_wrapFinalizerData) {
        if (entry->target.get() == obj) {
            return entry->data;
        }
    }
    return nullptr;
}

std::vector<std::pair<ObjectRef*, void*>> NapiEnv::snapshotWrapFinalizerData() const
{
    std::vector<std::pair<ObjectRef*, void*>> result;
    result.reserve(m_wrapFinalizerData.size());
    for (WrapFinalizerEntry* entry : m_wrapFinalizerData) {
        ObjectRef* target = entry->target.get();
        if (target != nullptr) {
            result.push_back({ target, entry->data });
        }
    }
    return result;
}
void NapiEnv::registerEnvFinalizer(ValueRef* target, Memory::GCAllocatedMemoryFinalizer callback, void* data)
{
    EnvFinalizerEntry* entry = new EnvFinalizerEntry();
    entry->target.reset(target);
    entry->target.setWeak();
    entry->callback = callback;
    entry->data = data;
    m_envFinalizers.push_back(entry);
    Memory::gcRegisterFinalizer(target, callback, data);
}

void NapiEnv::takeEnvFinalizerData(void* data)
{
    for (auto iter = m_envFinalizers.begin(); iter != m_envFinalizers.end(); ++iter) {
        EnvFinalizerEntry* entry = *iter;
        if (entry->data == data) {
            m_envFinalizers.erase(iter);
            delete entry;
            return;
        }
    }
}

void NapiEnv::runEnvFinalizers()
{
    for (;;) {
        // Flush callbacks whose weak target was already cleared before
        // deciding which still-live targets need explicit env finalization.
        Memory::gc();
        GC_invoke_finalizers();
        if (m_envFinalizers.empty()) {
            return;
        }

        bool invoked = false;
        std::vector<EnvFinalizerEntry*> snapshot = m_envFinalizers;
        for (EnvFinalizerEntry* candidate : snapshot) {
            auto live = std::find(m_envFinalizers.begin(), m_envFinalizers.end(), candidate);
            if (live == m_envFinalizers.end()) {
                continue;
            }

            EnvFinalizerEntry* entry = *live;
            ValueRef* target = entry->target.get();
            if (target == nullptr) {
                continue;
            }
            Memory::GCAllocatedMemoryFinalizer callback = entry->callback;
            void* data = entry->data;
            Memory::gcUnregisterFinalizer(target, callback, data);
            callback(target, data);
            invoked = true;
        }

        if (!invoked) {
            GC_invoke_finalizers();
            RELEASE_ASSERT(m_envFinalizers.empty());
            return;
        }
    }
}

void NapiEnv::registerNativeFinalizer(void (*callback)(void*), void* data)
{
    m_nativeFinalizers.push_back({ callback, data });
}

void NapiEnv::takeNativeFinalizerData(void* data)
{
    for (auto iter = m_nativeFinalizers.begin(); iter != m_nativeFinalizers.end(); ++iter) {
        if (iter->data == data) {
            m_nativeFinalizers.erase(iter);
            return;
        }
    }
}

void NapiEnv::runNativeFinalizers()
{
    // Pop before invoking: one callback may release another backing store,
    // whose deleter removes its own still-pending entry reentrantly.
    while (!m_nativeFinalizers.empty()) {
        NativeFinalizerEntry entry = m_nativeFinalizers.back();
        m_nativeFinalizers.pop_back();
        entry.callback(entry.data);
    }
}

void NapiEnv::trackThreadsafeFunction(napi_threadsafe_function func)
{
    std::lock_guard<std::mutex> guard(m_threadsafeFunctionsMutex);
    m_threadsafeFunctions.push_back(func);
}

void NapiEnv::untrackThreadsafeFunction(napi_threadsafe_function func)
{
    std::lock_guard<std::mutex> guard(m_threadsafeFunctionsMutex);
    for (auto iter = m_threadsafeFunctions.begin(); iter != m_threadsafeFunctions.end(); ++iter) {
        if (*iter == func) {
            m_threadsafeFunctions.erase(iter);
            return;
        }
    }
}

std::vector<napi_threadsafe_function> NapiEnv::snapshotThreadsafeFunctions()
{
    std::lock_guard<std::mutex> guard(m_threadsafeFunctionsMutex);
    return m_threadsafeFunctions;
}

void NapiEnv::trackAsyncWork(napi_async_work work)
{
    std::lock_guard<std::mutex> guard(m_asyncWorksMutex);
    m_asyncWorks.push_back(work);
}

void NapiEnv::untrackAsyncWork(napi_async_work work)
{
    std::lock_guard<std::mutex> guard(m_asyncWorksMutex);
    for (auto iter = m_asyncWorks.begin(); iter != m_asyncWorks.end(); ++iter) {
        if (*iter == work) {
            m_asyncWorks.erase(iter);
            return;
        }
    }
}

std::vector<napi_async_work> NapiEnv::snapshotAsyncWorks()
{
    std::lock_guard<std::mutex> guard(m_asyncWorksMutex);
    return m_asyncWorks;
}
void NapiEnv::addEnvCleanupHook(napi_cleanup_hook fun, void* arg)
{
    m_envCleanupHooks.push_back({ fun, arg });
}

void NapiEnv::removeEnvCleanupHook(napi_cleanup_hook fun, void* arg)
{
    // removes the most-recently-added matching entry (rbegin/rend), matching
    // the LIFO framing used everywhere else in this file - functionally
    // equivalent to removing any single matching entry, since (fun, arg)
    // pairs are otherwise indistinguishable from one another.
    for (auto it = m_envCleanupHooks.rbegin(); it != m_envCleanupHooks.rend(); ++it) {
        if (it->first == fun && it->second == arg) {
            m_envCleanupHooks.erase(std::next(it).base());
            break;
        }
    }
}

napi_async_cleanup_hook_handle__* NapiEnv::addAsyncCleanupHook(napi_async_cleanup_hook hook, void* arg)
{
    napi_async_cleanup_hook_handle__* handle = new napi_async_cleanup_hook_handle__();
    handle->napiEnv = this;
    handle->hook = hook;
    handle->arg = arg;
    m_asyncCleanupHooks.push_back(handle);
    return handle;
}

void NapiEnv::removeAsyncCleanupHook(napi_async_cleanup_hook_handle__* handle)
{
    for (size_t i = 0; i < m_asyncCleanupHooks.size(); i++) {
        if (m_asyncCleanupHooks[i] == handle) {
            m_asyncCleanupHooks.erase(m_asyncCleanupHooks.begin() + i);
            delete handle;
            return;
        }
    }
}

bool NapiEnv::drainPendingJobs()
{
    VMInstanceRef* instance = m_vmInstance.get();
    bool anyProgress = false;
    // Alternates between three sources of work, each of which may feed one of
    // the others:
    //  - this env's VMInstance's own pending-job queue (e.g. resolved Promise
    //    reactions);
    //  - the libuv loop (uv_run(..., UV_RUN_NOWAIT)) - an async_work's
    //    uv_queue_work completion or a threadsafe_function's uv_async_t
    //    wakeup, both delivered from here (see NapiAsyncWork.cpp), may
    //    themselves run JS (`complete`/call_js_cb) that queues further
    //    Promise-reaction jobs;
    //  - the legacy main-thread callback queue (enqueueMainThreadCallback/
    //    drainMainThreadCallbacks above) - kept as a fallback path, in case
    //    anything still posts through it directly.
    // A resolved Promise reaction may itself be what a napi_ref'd JS callback
    // was waiting to observe, so this keeps alternating until a full pass
    // makes no further progress, rather than draining each source exactly
    // once. Bounded (not "until literally nothing is left"): a
    // napi_threadsafe_function's uv_async_t handle (or any other open libuv
    // handle) keeps the loop "alive" for as long as it stays open, which is
    // not the same thing as there being actual work to do right now - an
    // unbounded loop here would spin forever on a long-lived, otherwise-idle
    // handle.
    for (int i = 0; i < 32; i++) {
        bool progressed = false;

        while (instance->hasPendingJob()) {
            instance->executePendingJob();
            progressed = true;
        }

        size_t mainThreadCallbacksBefore;
        {
            std::lock_guard<std::mutex> guard(m_mainThreadCallbacksMutex);
            mainThreadCallbacksBefore = m_mainThreadCallbacks.size();
        }
        drainMainThreadCallbacks();
        progressed = progressed || (mainThreadCallbacksBefore > 0);

        // Runs any already-completed uv_work_t (async_work)/uv_async_t
        // (threadsafe_function) callbacks without blocking; its own return
        // value (whether the loop still has active handles/requests) isn't a
        // reliable "did anything actually happen" signal on its own - e.g. an
        // un-released threadsafe_function's uv_async_t handle alone keeps it
        // non-zero - so this loop tracks progress via the VM job queue below
        // instead, which is what a uv callback would actually feed into.
        //
        // Raw libuv callbacks may call napi_* functions directly. Operations
        // that need an ExecutionStateRef create a scoped Evaluator::execute
        // call from this env's ContextRef, so no stack state is retained here.
        uv_run(&m_uvLoop, UV_RUN_NOWAIT);

        while (instance->hasPendingJob()) {
            instance->executePendingJob();
            progressed = true;
        }

        if (!progressed) {
            break;
        }
        anyProgress = true;
    }
    return anyProgress;
}

void NapiEnv::drainPostFinalizers()
{
    // swap out first (rather than iterating m_pendingPostFinalizers directly)
    // so a finalizer that itself calls node_api_post_finalizer/enqueues more
    // work during this drain gets picked up by a *later* drain call instead
    // of being invoked recursively out of this same loop or invalidating the
    // vector being iterated.
    while (!m_pendingPostFinalizers.empty()) {
        std::vector<PostFinalizerEntry> batch;
        batch.swap(m_pendingPostFinalizers);
        for (const PostFinalizerEntry& entry : batch) {
            entry.finalizeCb(&m_env, entry.finalizeData, entry.finalizeHint);
        }
    }
}

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
