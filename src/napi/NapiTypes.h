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

#ifndef __EscargotNapiTypes__
#define __EscargotNapiTypes__

#include "api/EscargotPublic.h"
#include "NapiEnv.h"

#include <node_api.h>

#include <string>

// exports each napi_* definition individually with default visibility,
// so the binary can be compiled with -fvisibility=hidden (the project default,
// see target.cmake) while still letting dlopen()'d addons resolve these
// specific symbols against this process (see ESCARGOT_EXPORT for the
// equivalent convention used by the rest of the public API).
#if !defined(ESCARGOT_NAPI_EXPORT)
#if defined(_MSC_VER)
#define ESCARGOT_NAPI_EXPORT __declspec(dllexport)
#else
#define ESCARGOT_NAPI_EXPORT __attribute__((visibility("default")))
#endif
#endif

namespace Escargot {
namespace Napi {

// Values retain the engine's non-moving representation. Explicit handle
// scopes root every value returned through ToNapi using the shared C API.
inline napi_value ToNapi(ValueRef* value)
{
    if (value) CAPI::trackLocal(value);
    return reinterpret_cast<napi_value>(value);
}

inline ValueRef* FromNapi(napi_value value)
{
    return reinterpret_cast<ValueRef*>(value);
}

// records `status` as the env's most recent non-ok status, then returns it
// unchanged - so every napi_*/node_api_* function can simply route its
// `return napi_whatever;` through `return SetLastError(env, napi_whatever);`
// without otherwise changing its control flow. napi_get_last_error_info
// (NapiFunctions.cpp) looks the stashed code up in Node's own
// error_messages[] table to answer with a real message instead of always
// reporting an empty one (env->lastErrorCode, NapiEnv.h). A null `env` is
// tolerated (some callers, e.g. napi_create_function's own env==nullptr
// check, have no env to record against) and simply skips the recording.
inline napi_status SetLastError(napi_env env, napi_status status)
{
    if (env != nullptr) {
        env->lastErrorCode = status;
    }
    return status;
}

inline napi_status SetPendingExceptionFromEvaluatorResult(napi_env env, Evaluator::EvaluatorResult& result)
{
    if (result.isSuccessful()) {
        return napi_ok;
    }

    env->pendingException = result.error.value();
    return SetLastError(env, napi_pending_exception);
}

// Consume a C execution result after transferring its value or exception to
// the N-API adapter. C API errors have no JavaScript exception payload.
inline napi_status SetCResult(napi_env env, escargot_status status, escargot_result result, napi_value* output)
{
    std::unique_ptr<escargot_result__, decltype(&escargot_result_release)> owner(result, escargot_result_release);
    if (status == ESCARGOT_PENDING_EXCEPTION) {
        env->pendingException = CAPI::fromC(escargot_result_exception(result));
        return SetLastError(env, napi_pending_exception);
    }
    if (status != ESCARGOT_OK) {
        return SetLastError(env, status == ESCARGOT_INVALID_ARGUMENT ? napi_invalid_arg : napi_generic_failure);
    }
    if (output) *output = ToNapi(CAPI::fromC(escargot_result_value(result)));
    return napi_ok;
}

#define CHECK_ENV(env)               \
    do {                             \
        if ((env) == nullptr)        \
            return napi_invalid_arg; \
    } while (0)

#define CHECK_ARG(env, arg)                               \
    do {                                                  \
        if ((arg) == nullptr)                             \
            return SetLastError((env), napi_invalid_arg); \
    } while (0)

// Minimum byte/char16_t/latin1-byte length above which napi_create_string_utf8/
// napi_create_string_latin1/napi_create_string_utf16 (NapiFunctions.cpp/
// NapiValue.cpp) transparently route string creation through Escargot's
// compressible-string feature instead of a plain string - see those
// functions' own comments. Chosen well above every existing small test
// string (so none of them shift onto the compressible path) but small enough
// that a real addon's document/JSON-sized strings still benefit.
static const size_t kCompressibleStringThreshold = 1024;

// data stashed on a native FunctionObjectRef via setExtraData(), so the
// callback trampoline can find the user's napi_callback + data pointer
struct CallbackData {
    napi_env env;
    napi_callback callback;
    void* data;
};

} // namespace Napi
} // namespace Escargot

// the opaque type node_api.h forward-declares for napi_get_cb_info et al.
struct napi_callback_info__ {
    size_t argc;
    Escargot::ValueRef** argv;
    Escargot::ValueRef* thisValue;
    void* data;
    Escargot::OptionalRef<Escargot::ValueRef> newTarget; // present only for a `new`-invoked constructor call
};

// The opaque type node_api.h forward-declares for napi_create_reference.
// The holder is a normal persistent root while refcount > 0 and a BDWGC
// disappearing-link-backed weak root while refcount == 0. This keeps the
// handle's storage and lifetime in one place instead of relying on a raw
// pointer plus an independently ordered GC finalizer.
struct napi_ref__ {
    Escargot::CAPI::Persistent<Escargot::ValueRef> value;
    uint32_t refcount = 0;
};

namespace Escargot {
namespace Napi {

// Defined (non-static) in NapiFunctions.cpp, alongside napi_wrap/
// NapiWrapFinalizer, whose registry (NapiEnv::m_wrapFinalizerData) this walks
// and forces every still-live entry's finalize_cb to run - called from
// NapiEnv::~NapiEnv() (NapiEnv.cpp) to implement real Node-API environment-
// teardown finalization semantics (test_general/testEnvCleanup.js).
void RunEnvCleanupWrapFinalizers(NapiEnv* napiEnv);
void AbortEnvThreadsafeFunctions(NapiEnv* napiEnv);
void DrainEnvAsyncWorks(NapiEnv* napiEnv);

// Defined (non-static) in NapiRuntime.cpp, alongside napi_module_register -
// this PoC has no module loader to actually invoke a registered module's
// nm_register_func, so this exists purely as a tiny internal accessor a
// caller (or a TC) can use to confirm napi_module_register recorded the
// descriptor it was given. Process-wide (like Node's own module registry),
// not per-NapiEnv: napi_module_register itself takes no napi_env.
napi_module* GetLastRegisteredNapiModule();

} // namespace Napi
} // namespace Escargot

struct napi_platform__ {
    // Escargot runs on a singleton platform, but we can track its initialization
    // state and any custom configurations here if needed.
    int active = 0;
};
typedef struct napi_platform__* napi_platform;

// N-API keeps its per-environment ordering checks; C API scopes own the roots.
struct napi_handle_scope__ {
    Escargot::Optional<napi_handle_scope__*> parent;
    escargot_handle_scope roots;
    explicit napi_handle_scope__(escargot_handle_scope roots) : roots(roots) { }
};

struct napi_escapable_handle_scope__ : public napi_handle_scope__ {
    bool escapeCalled = false;
    explicit napi_escapable_handle_scope__(escargot_handle_scope roots) : napi_handle_scope__(roots) { }
};

// the opaque type node_api.h forward-declares for napi_open_callback_scope/
// napi_close_callback_scope (NapiRuntime.cpp). Same intrusive-stack shape and
// rationale as napi_handle_scope__ above, via napi_env__::topCallbackScope
// (NapiEnv.h): enforces LIFO close order, returning
// napi_callback_scope_mismatch if `scope` isn't currently the innermost open
// one.
struct napi_callback_scope__ {
    napi_callback_scope__* parent = nullptr;
};

// the opaque type node_api.h forward-declares for napi_async_init/
// napi_async_destroy/napi_make_callback/napi_open_callback_scope
// (NapiRuntime.cpp). This PoC has no async_hooks integration to route
// through, so an async context is just inert bookkeeping: it persistently
// roots the two resource values napi_async_init was given, in case a future
// async_hooks implementation wants them, and exists solely so
// napi_async_init/napi_async_destroy have something to allocate/free that
// satisfies the API's opaque-handle contract.
struct napi_async_context__ {
    Escargot::Napi::PersistentOptionalRef<Escargot::ValueRef> resource;
    Escargot::Napi::PersistentOptionalRef<Escargot::ValueRef> resourceName;
};

// the opaque type node_api.h forward-declares for napi_add_async_cleanup_hook/
// napi_remove_async_cleanup_hook (NapiRuntime.cpp). Tracked on the
// registering NapiEnv (NapiEnv::m_asyncCleanupHooks) so every still-live one
// can be invoked at environment teardown (~NapiEnv(), NapiEnv.cpp); `napiEnv`
// lets napi_remove_async_cleanup_hook find (and remove itself from) that
// owner's list without an explicit napi_env parameter of its own - matching
// node_api.h's actual signature, which has none.
struct napi_async_cleanup_hook_handle__ {
    Escargot::Napi::NapiEnv* napiEnv;
    napi_async_cleanup_hook hook;
    void* arg;
};

#endif
#endif // ENABLE_NAPI
