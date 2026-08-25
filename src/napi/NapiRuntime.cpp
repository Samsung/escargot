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

// The slice of node_api.h that doesn't require an event loop / thread pool -
// i.e. everything in node_api.h except napi_create_async_work/
// napi_queue_async_work/napi_cancel_async_work (need a thread pool) and the
// napi_threadsafe_function family (needs a real libuv loop to marshal calls
// across threads). Following the same conventions as NapiFunctions.cpp/
// NapiExtras.cpp (napi_value <-> ValueRef* punning via ToNapi/FromNapi,
// Evaluator::execute-wrapped exception boundaries, SetLastError-routed error
// returns).
//
// What's a full implementation here vs an approximation:
//  - napi_add_env_cleanup_hook/napi_remove_env_cleanup_hook,
//    napi_open_callback_scope/napi_close_callback_scope,
//    napi_async_init/napi_async_destroy, napi_module_register,
//    node_api_create_buffer_from_arraybuffer: full, matching real Node-API
//    semantics as closely as this PoC's engine integration allows.
//  - napi_add_async_cleanup_hook/napi_remove_async_cleanup_hook: a
//    synchronous approximation - the hook is invoked and immediately treated
//    as done, with no real asynchronous waiting (this PoC has no event loop
//    to wait on in the first place).
//  - napi_get_node_version: synthetic - this engine is not Node.js, so
//    {major, minor, patch, release} is a reasonable made-up stand-in, not a
//    real Node version.
//  - node_api_get_module_file_name: defaults to "" (NapiEnv::m_moduleFileName
//    has no setter call anywhere yet) until a real module loader exists to
//    set it via NapiEnv::setModuleFileName.
//  - napi_make_callback: behaves like napi_call_function plus draining
//    pending jobs; async_context is accepted but ignored (no async_hooks
//    integration to route it through).
//  - napi_get_uv_event_loop: returns the real libuv event loop owned by this
//    call's NapiEnv (NapiEnv::uvLoop(), NapiEnv.h/.cpp) - the same loop
//    async_work/threadsafe_function (NapiAsyncWork.cpp) queue work onto and
//    NapiEnv::drainPendingJobs() pumps.

#include "NapiTypes.h"

#include <utility>
#include <vector>

namespace Escargot {
namespace Napi {

namespace {

// process-wide, like Node's own module registry - napi_module_register
// itself takes no napi_env (see node_api.h's own signature: this predates
// per-addon-instance module registration entirely). This PoC has no module
// loader to actually invoke nm_register_func, so this is purely a recording
// point - see GetLastRegisteredNapiModule's own comment (NapiTypes.h).
napi_module* g_lastRegisteredModule = nullptr;

} // namespace

napi_module* GetLastRegisteredNapiModule()
{
    return g_lastRegisteredModule;
}

extern "C" {

// Deprecated in real Node-API (superseded by NAPI_MODULE/NAPI_MODULE_INIT),
// but still the mechanism those macros' constructor-time registration
// ultimately calls into - see node_api.h's own "Used by deprecated
// registration method napi_module_register" comment on napi_module. Returns
// void (not napi_status), matching node_api.h's declaration exactly: there is
// no napi_env available at module-registration time to report a status
// through.
ESCARGOT_NAPI_EXPORT void napi_module_register(napi_module* mod)
{
    g_lastRegisteredModule = mod;
}

ESCARGOT_NAPI_EXPORT napi_status napi_add_env_cleanup_hook(node_api_basic_env env, napi_cleanup_hook fun, void* arg)
{
    if (env == nullptr || fun == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    env->napiEnv->addEnvCleanupHook(fun, arg);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_remove_env_cleanup_hook(node_api_basic_env env, napi_cleanup_hook fun, void* arg)
{
    if (env == nullptr || fun == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    env->napiEnv->removeEnvCleanupHook(fun, arg);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_add_async_cleanup_hook(node_api_basic_env env, napi_async_cleanup_hook hook, void* arg, napi_async_cleanup_hook_handle* remove_handle)
{
    if (env == nullptr || hook == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // Synchronous approximation (see this file's header comment): the real
    // contract lets `hook` do asynchronous work and only actually finish
    // once it later calls back through `handle` - there is no event loop
    // here for it to do that work on, so teardown (~NapiEnv(), NapiEnv.cpp)
    // just invokes it and moves on immediately.
    napi_async_cleanup_hook_handle__* handle = env->napiEnv->addAsyncCleanupHook(hook, arg);
    if (remove_handle != nullptr) {
        *remove_handle = handle;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_remove_async_cleanup_hook(napi_async_cleanup_hook_handle remove_handle)
{
    // No napi_env parameter - matches node_api.h's own signature exactly:
    // the handle itself remembers which NapiEnv registered it (napiEnv,
    // NapiTypes.h), so removal doesn't need one.
    if (remove_handle == nullptr) {
        return napi_invalid_arg;
    }
    remove_handle->napiEnv->removeAsyncCleanupHook(remove_handle);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_node_version(node_api_basic_env env, const napi_node_version** version)
{
    if (env == nullptr || version == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // Synthetic: this engine is not Node.js, so there is no real Node
    // version to report here - a reasonable, clearly-fake stand-in (`major`/
    // `minor` land on a recent-ish Node release for addons that
    // feature-switch on them; `release` unambiguously identifies the real
    // source). Static so the returned pointer stays valid indefinitely, per
    // this function's own contract.
    static const napi_node_version kSyntheticNodeVersion = { 20, 0, 0, "escargot" };
    *version = &kSyntheticNodeVersion;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_uv_event_loop(node_api_basic_env env, struct uv_loop_s** result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // A real libuv loop, owned by this env's NapiEnv for as long as it lives
    // (uv_loop_init in NapiEnv's constructor; uv_loop_close at teardown - see
    // NapiEnv.h/.cpp) - the same loop napi_queue_async_work/
    // napi_call_threadsafe_function queue work onto and NapiEnv::
    // drainPendingJobs() pumps (NapiAsyncWork.cpp).
    *result = env->napiEnv->uvLoop();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_get_module_file_name(node_api_basic_env env, const char** result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // Defaults to "" (NapiEnv::m_moduleFileName) until a real module loader
    // exists to set it via NapiEnv::setModuleFileName - see this file's
    // header comment.
    *result = env->napiEnv->moduleFileName().c_str();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_async_init(napi_env env, napi_value async_resource, napi_value async_resource_name, napi_async_context* result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // Minimal: no async_hooks integration exists to route this through (see
    // napi_async_context__'s own comment, NapiTypes.h) - just remembers the
    // two resource values for the caller's own napi_make_callback/
    // napi_open_callback_scope use.
    napi_async_context__* context = new napi_async_context__();
    if (async_resource != nullptr) {
        context->resource = FromNapi(async_resource);
    }
    if (async_resource_name != nullptr) {
        context->resourceName = FromNapi(async_resource_name);
    }
    *result = context;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_async_destroy(napi_env env, napi_async_context async_context)
{
    if (env == nullptr || async_context == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    delete async_context;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_open_callback_scope(napi_env env, napi_value resource_object, napi_async_context async_context, napi_callback_scope* result)
{
    CHECK_ENV(env);
    CHECK_ARG(env, result);
    // Minimal LIFO-nesting bookkeeping, same rationale as
    // napi_open_handle_scope (NapiFunctions.cpp): `resource_object`/
    // `async_context` aren't otherwise used since there is no async_hooks
    // integration to feed them into.
    napi_callback_scope__* scope = new napi_callback_scope__();
    scope->parent = env->topCallbackScope;
    env->topCallbackScope = scope;
    env->callbackScopeDepth++;
    *result = scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_close_callback_scope(napi_env env, napi_callback_scope scope)
{
    CHECK_ENV(env);
    CHECK_ARG(env, scope);
    if (scope != env->topCallbackScope) {
        return SetLastError(env, napi_callback_scope_mismatch);
    }
    env->topCallbackScope = scope->parent;
    env->callbackScopeDepth--;

    // Node-API Spec: Drain microtasks and nextTicks only when the outermost callback scope closes.
    if (env->callbackScopeDepth == 0) {
        env->napiEnv->drainPendingJobs();
    }

    delete scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_make_callback(napi_env env, napi_async_context async_context, napi_value recv, napi_value func, size_t argc, const napi_value* argv, napi_value* result)
{
    CHECK_ENV(env);
    CHECK_ARG(env, func);
    // async_context is ignored - accepted purely for API-surface
    // compatibility (see napi_async_context__'s own comment, NapiTypes.h).

    env->callbackScopeDepth++;
    ValueRef* fn = FromNapi(func);
    ValueRef* thisArg = FromNapi(recv);

    std::vector<ValueRef*> args(argc);
    for (size_t i = 0; i < argc; i++) {
        args[i] = FromNapi(argv[i]);
    }

    // Sandboxed exactly like napi_call_function (NapiFunctions.cpp): ->call()
    // throws a raw C++ exception on an uncaught JS exception, which must not
    // be allowed to cross this function's own stack frame.
    Evaluator::EvaluatorResult callResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, ValueRef* fn, ValueRef* thisArg, size_t argc, ValueRef** argv) -> ValueRef* {
            return fn->call(state, thisArg, argc, argv);
        },
        fn, thisArg, argc, args.data());

    env->callbackScopeDepth--;

    if (!callResult.isSuccessful()) {
        env->pendingException = callResult.error.value();
        // Still drain pending jobs when the outermost scope finishes even on failure.
        if (env->callbackScopeDepth == 0) {
            env->napiEnv->drainPendingJobs();
        }
        return SetLastError(env, napi_pending_exception);
    }

    // Node-API Spec: Drain microtasks and nextTicks only when the outermost callback scope finishes.
    if (env->callbackScopeDepth == 0) {
        env->napiEnv->drainPendingJobs();
    }

    if (result != nullptr) {
        *result = ToNapi(callResult.result);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_buffer_from_arraybuffer(napi_env env, napi_value arraybuffer, size_t byte_offset, size_t byte_length, napi_value* result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* bufValue = FromNapi(arraybuffer);
    if (!bufValue->isArrayBuffer()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferRef* buf = bufValue->asArrayBuffer();

    // Same out-of-bounds check (and RangeError, matching real Node-API) as
    // napi_create_typedarray's identical one (NapiArrayBuffer.cpp) - a
    // Buffer view may not extend past the end of its backing ArrayBuffer.
    // No alignment check is needed here (unlike a multi-byte-element typed
    // array): Buffer is a Uint8Array, whose element size is 1.
    if (byte_offset + byte_length > buf->byteLength()) {
        napi_throw_range_error(env, nullptr, "byte_offset + byte_length must be smaller than the size in bytes of the buffer passed in");
        return SetLastError(env, napi_pending_exception);
    }

    // Node's Buffer is a Uint8Array subclass - see napi_create_buffer's own
    // comment (NapiArrayBuffer.cpp) for why a plain Uint8ArrayObjectRef
    // stands in for it here.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, ArrayBufferRef* buffer, size_t byteOffset, size_t byteLength) -> ValueRef* {
            Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
            view->setBuffer(buffer, byteOffset, byteLength, byteLength);
            return view;
        },
        buf, byte_offset, byte_length);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_platform(int argc, char** argv, napi_platform* result)
{
    if (result == nullptr) {
        return napi_invalid_arg;
    }
    Escargot::Napi::NapiEnv::globalInit();

    static napi_platform__ s_platform;
    s_platform.active = 1;
    *result = &s_platform;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_destroy_platform(napi_platform platform)
{
    if (platform == nullptr) {
        return napi_invalid_arg;
    }
#ifndef ESCARGOT_ENABLE_TEST
    Escargot::Napi::NapiEnv::globalFinalize();
#endif
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_environment(napi_platform platform, napi_env* result)
{
    if (platform == nullptr || result == nullptr) {
        return napi_invalid_arg;
    }
    Escargot::Napi::NapiEnv* napiEnv = Escargot::Napi::NapiEnv::create();
    if (napiEnv == nullptr) {
        return napi_generic_failure;
    }
    *result = napiEnv->env();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_destroy_environment(napi_env env)
{
    if (env == nullptr || env->napiEnv == nullptr) {
        return napi_invalid_arg;
    }
    delete env->napiEnv;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status escargot_napi_perform_microtask_checkpoint(napi_env env)
{
    if (env == nullptr || env->napiEnv == nullptr) {
        return napi_invalid_arg;
    }
    auto* instance = env->napiEnv->vmInstance();
    while (instance->hasPendingJob()) {
        instance->executePendingJob();
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status escargot_napi_pump_message_loop(napi_env env, bool* out_has_more_work)
{
    if (env == nullptr || env->napiEnv == nullptr) {
        return napi_invalid_arg;
    }
    bool progressed = env->napiEnv->drainPendingJobs();
    env->napiEnv->drainPostFinalizers();

    if (out_has_more_work != nullptr) {
        *out_has_more_work = env->napiEnv->uvLoopAlive() || env->napiEnv->vmInstance()->hasPendingJob();
    }
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
