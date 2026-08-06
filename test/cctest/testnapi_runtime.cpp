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

// Exercises NapiRuntime.cpp's slice of node_api.h - the runtime-integration
// functions that don't need an event loop / thread pool (env/async cleanup
// hooks, node version/module-file-name queries, napi_make_callback, callback
// scopes, async contexts, module registration, buffer-from-arraybuffer, and
// the uv event loop accessor). Self-contained (no dlopen'd addon involved),
// same NapiEnv/Evaluator::execute setup as test/cctest/testnapi.cpp and
// test/cctest/testnapi_arraybuffer.cpp.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// napi_add_env_cleanup_hook / napi_remove_env_cleanup_hook
// ---------------------------------------------------------------------------

static int g_envCleanupCallCount = 0;
static void* g_envCleanupSeenArg = nullptr;

static void RecordEnvCleanup(void* arg)
{
    g_envCleanupCallCount++;
    g_envCleanupSeenArg = arg;
}

TEST(NapiRuntime, EnvCleanupHookRunsOnEnvDestruction)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_envCleanupCallCount = 0;
    g_envCleanupSeenArg = nullptr;

    int marker = 7;
    ASSERT_EQ(napi_add_env_cleanup_hook(napiEnv->env(), RecordEnvCleanup, &marker), napi_ok);

    // must not fire before teardown
    EXPECT_EQ(g_envCleanupCallCount, 0);

    // Must actually be destroyed (not leaked) - the whole point of this test
    // is exercising ~NapiEnv()'s cleanup-hook teardown step. Safe because
    // nothing in this test suite calls NapiEnv::globalFinalize() afterward.
    delete napiEnv;

    EXPECT_EQ(g_envCleanupCallCount, 1);
    EXPECT_EQ(g_envCleanupSeenArg, &marker);
}

static int g_envCleanupSecondCallCount = 0;

static void RecordEnvCleanupSecond(void* arg)
{
    g_envCleanupSecondCallCount++;
}

TEST(NapiRuntime, EnvCleanupHookAddRemovePairing)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_envCleanupCallCount = 0;
    g_envCleanupSecondCallCount = 0;

    int marker = 1;
    ASSERT_EQ(napi_add_env_cleanup_hook(napiEnv->env(), RecordEnvCleanup, &marker), napi_ok);
    ASSERT_EQ(napi_add_env_cleanup_hook(napiEnv->env(), RecordEnvCleanupSecond, nullptr), napi_ok);

    // remove the first hook; only the second should run at teardown
    ASSERT_EQ(napi_remove_env_cleanup_hook(napiEnv->env(), RecordEnvCleanup, &marker), napi_ok);

    delete napiEnv;

    EXPECT_EQ(g_envCleanupCallCount, 0);
    EXPECT_EQ(g_envCleanupSecondCallCount, 1);
}

// ---------------------------------------------------------------------------
// napi_add_async_cleanup_hook / napi_remove_async_cleanup_hook
// ---------------------------------------------------------------------------

static int g_asyncCleanupCallCount = 0;
static napi_async_cleanup_hook_handle g_asyncCleanupSeenHandle = nullptr;
static void* g_asyncCleanupSeenArg = nullptr;

static void RecordAsyncCleanup(napi_async_cleanup_hook_handle handle, void* arg)
{
    g_asyncCleanupCallCount++;
    g_asyncCleanupSeenHandle = handle;
    g_asyncCleanupSeenArg = arg;
}

TEST(NapiRuntime, AsyncCleanupHookRunsOnEnvDestruction)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_asyncCleanupCallCount = 0;
    g_asyncCleanupSeenHandle = nullptr;
    g_asyncCleanupSeenArg = nullptr;

    int marker = 42;
    napi_async_cleanup_hook_handle handle = nullptr;
    ASSERT_EQ(napi_add_async_cleanup_hook(napiEnv->env(), RecordAsyncCleanup, &marker, &handle), napi_ok);
    ASSERT_NE(handle, nullptr);

    EXPECT_EQ(g_asyncCleanupCallCount, 0);

    delete napiEnv;

    EXPECT_EQ(g_asyncCleanupCallCount, 1);
    EXPECT_EQ(g_asyncCleanupSeenHandle, handle);
    EXPECT_EQ(g_asyncCleanupSeenArg, &marker);
}

TEST(NapiRuntime, AsyncCleanupHookRemovedBeforeTeardownDoesNotRun)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    g_asyncCleanupCallCount = 0;

    napi_async_cleanup_hook_handle handle = nullptr;
    ASSERT_EQ(napi_add_async_cleanup_hook(napiEnv->env(), RecordAsyncCleanup, nullptr, &handle), napi_ok);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(napi_remove_async_cleanup_hook(handle), napi_ok);

    delete napiEnv;

    EXPECT_EQ(g_asyncCleanupCallCount, 0);
}

// ---------------------------------------------------------------------------
// napi_get_node_version
// ---------------------------------------------------------------------------

TEST(NapiRuntime, GetNodeVersionReturnsSyntheticValues)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    const napi_node_version* version = nullptr;
    ASSERT_EQ(napi_get_node_version(napiEnv->env(), &version), napi_ok);
    ASSERT_NE(version, nullptr);

    // Synthetic (documented in NapiRuntime.cpp) - this engine is not Node.js.
    EXPECT_EQ(version->major, 20u);
    EXPECT_EQ(version->minor, 0u);
    EXPECT_EQ(version->patch, 0u);
    ASSERT_NE(version->release, nullptr);
    EXPECT_STREQ(version->release, "escargot");

    // the returned pointer must stay valid (backed by a function-local
    // static) - fetch it again and confirm it's the very same pointer.
    const napi_node_version* versionAgain = nullptr;
    ASSERT_EQ(napi_get_node_version(napiEnv->env(), &versionAgain), napi_ok);
    EXPECT_EQ(version, versionAgain);

    EXPECT_EQ(napi_get_node_version(nullptr, &version), napi_invalid_arg);
    EXPECT_EQ(napi_get_node_version(napiEnv->env(), nullptr), napi_invalid_arg);
}

// ---------------------------------------------------------------------------
// node_api_get_module_file_name
// ---------------------------------------------------------------------------

TEST(NapiRuntime, ModuleFileNameDefaultsEmpty)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    const char* fileName = nullptr;
    ASSERT_EQ(node_api_get_module_file_name(napiEnv->env(), &fileName), napi_ok);
    ASSERT_NE(fileName, nullptr);
    EXPECT_STREQ(fileName, "");
}

TEST(NapiRuntime, ModuleFileNameReflectsSetModuleFileName)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    napiEnv->setModuleFileName("/path/to/addon.node");

    const char* fileName = nullptr;
    ASSERT_EQ(node_api_get_module_file_name(napiEnv->env(), &fileName), napi_ok);
    ASSERT_NE(fileName, nullptr);
    EXPECT_STREQ(fileName, "/path/to/addon.node");
}

// ---------------------------------------------------------------------------
// napi_make_callback
// ---------------------------------------------------------------------------

TEST(NapiRuntime, MakeCallbackCallsFunctionAndDrainsMicrotask)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            // `f` returns 42 and, as a side effect, queues a microtask (a
            // resolved Promise's .then reaction) that stashes a value on the
            // global object - proving (once observed right after
            // napi_make_callback returns, with no explicit drain call of the
            // test's own) that napi_make_callback drained it automatically,
            // unlike napi_call_function.
            ScriptRef* parsedScript = state->context()->scriptParser()->initializeScript(StringRef::createFromASCII("(function f() { Promise.resolve().then(() => { globalThis.__napiMakeCallbackMicrotaskRan = true; }); return 42; })"), StringRef::createFromASCII("testnapi_runtime"), false).fetchScriptThrowsExceptionIfParseError(state);
            ValueRef* fn = parsedScript->execute(state);

            napi_value callResult = nullptr;
            napi_status status = napi_make_callback(env, nullptr, ToNapi(ValueRef::createUndefined()), ToNapi(fn), 0, nullptr, &callResult);
            if (status != napi_ok) {
                return ValueRef::create(1);
            }
            if (FromNapi(callResult)->asNumber() != 42) {
                return ValueRef::create(2);
            }

            ValueRef* flag = state->context()->globalObject()->get(state, StringRef::createFromASCII("__napiMakeCallbackMicrotaskRan"));
            if (flag->isUndefined() || !flag->toBoolean(state)) {
                return ValueRef::create(3);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

// ---------------------------------------------------------------------------
// napi_open_callback_scope / napi_close_callback_scope
// ---------------------------------------------------------------------------

TEST(NapiRuntime, CallbackScopeOpenCloseAndMismatch)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value resource = ToNapi(ValueRef::createUndefined());

    napi_callback_scope outer = nullptr;
    napi_callback_scope inner = nullptr;
    ASSERT_EQ(napi_open_callback_scope(env, resource, nullptr, &outer), napi_ok);
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(napi_open_callback_scope(env, resource, nullptr, &inner), napi_ok);
    ASSERT_NE(inner, nullptr);

    // Closing out of LIFO order must fail and must not actually pop anything.
    EXPECT_EQ(napi_close_callback_scope(env, outer), napi_callback_scope_mismatch);

    ASSERT_EQ(napi_close_callback_scope(env, inner), napi_ok);
    ASSERT_EQ(napi_close_callback_scope(env, outer), napi_ok);
}

// ---------------------------------------------------------------------------
// napi_async_init / napi_async_destroy
// ---------------------------------------------------------------------------

TEST(NapiRuntime, AsyncInitDestroyRoundTrip)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();
    napi_env env = napiEnv->env();

    napi_value resource = ToNapi(ValueRef::createUndefined());
    napi_value resourceName = ToNapi(StringRef::createFromASCII("testnapi_runtime resource"));

    napi_async_context context = nullptr;
    ASSERT_EQ(napi_async_init(env, resource, resourceName, &context), napi_ok);
    ASSERT_NE(context, nullptr);

    EXPECT_EQ(napi_async_destroy(env, context), napi_ok);

    // NULL-arg guards
    napi_async_context unused = nullptr;
    EXPECT_EQ(napi_async_init(env, resource, resourceName, nullptr), napi_invalid_arg);
    EXPECT_EQ(napi_async_destroy(env, nullptr), napi_invalid_arg);
    (void)unused;
}

// ---------------------------------------------------------------------------
// node_api_create_buffer_from_arraybuffer
// ---------------------------------------------------------------------------

TEST(NapiRuntime, BufferFromArrayBufferViewsCorrectBytes)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            void* abData = nullptr;
            napi_value ab = nullptr;
            if (napi_create_arraybuffer(env, 16, &abData, &ab) != napi_ok) {
                return ValueRef::create(1);
            }
            for (uint8_t i = 0; i < 16; i++) {
                static_cast<uint8_t*>(abData)[i] = i;
            }

            // a view over bytes [4, 4+8)
            napi_value buffer = nullptr;
            napi_status status = node_api_create_buffer_from_arraybuffer(env, ab, 4, 8, &buffer);
            if (status != napi_ok) {
                return ValueRef::create(2);
            }

            void* bufData = nullptr;
            size_t bufLength = 0;
            if (napi_get_buffer_info(env, buffer, &bufData, &bufLength) != napi_ok) {
                return ValueRef::create(3);
            }
            if (bufLength != 8) {
                return ValueRef::create(4);
            }
            for (uint8_t i = 0; i < 8; i++) {
                if (static_cast<uint8_t*>(bufData)[i] != static_cast<uint8_t>(4 + i)) {
                    return ValueRef::create(5);
                }
            }

            bool isBuffer = false;
            napi_is_buffer(env, buffer, &isBuffer);
            if (!isBuffer) {
                return ValueRef::create(6);
            }

            // writing through the Buffer view must be visible through the
            // original ArrayBuffer's own data pointer (they share storage).
            static_cast<uint8_t*>(bufData)[0] = 0xAB;
            if (static_cast<uint8_t*>(abData)[4] != 0xAB) {
                return ValueRef::create(7);
            }

            // out-of-bounds must raise a RangeError, not silently wrap/clip
            napi_value oob = nullptr;
            napi_status oobStatus = node_api_create_buffer_from_arraybuffer(env, ab, 10, 8, &oob);
            if (oobStatus != napi_pending_exception) {
                return ValueRef::create(8);
            }
            bool isPending = false;
            napi_is_exception_pending(env, &isPending);
            if (!isPending) {
                return ValueRef::create(9);
            }
            napi_value exception = nullptr;
            napi_get_and_clear_last_exception(env, &exception);
            if (!FromNapi(exception)->isObject()) {
                return ValueRef::create(10);
            }

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->isUndefined());
}

// ---------------------------------------------------------------------------
// napi_get_uv_event_loop
// ---------------------------------------------------------------------------

TEST(NapiRuntime, GetUvEventLoopReturnsRealLoop)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    struct uv_loop_s* loop = nullptr;
    EXPECT_EQ(napi_get_uv_event_loop(napiEnv->env(), &loop), napi_ok);
    // the exact same loop NapiEnv::uvLoop()/drainPendingJobs() use - not a
    // fabricated/unusable pointer
    EXPECT_EQ(loop, napiEnv->uvLoop());
    EXPECT_NE(loop, nullptr);

    // a real, usable libuv loop: run it (nothing queued, so this returns
    // immediately) rather than merely checking it's non-null
    EXPECT_EQ(uv_run(loop, UV_RUN_NOWAIT), 0);

    EXPECT_EQ(napi_get_uv_event_loop(nullptr, &loop), napi_invalid_arg);
    EXPECT_EQ(napi_get_uv_event_loop(napiEnv->env(), nullptr), napi_invalid_arg);
}

// ---------------------------------------------------------------------------
// napi_module_register
// ---------------------------------------------------------------------------

TEST(NapiRuntime, ModuleRegisterRecordsModule)
{
    static napi_module testModule = {
        1, // nm_version
        0, // nm_flags
        "testnapi_runtime.cpp", // nm_filename
        nullptr, // nm_register_func
        "testnapi_runtime_module", // nm_modname
        nullptr, // nm_priv
        { nullptr, nullptr, nullptr, nullptr } // reserved
    };

    napi_module_register(&testModule);

    EXPECT_EQ(GetLastRegisteredNapiModule(), &testModule);
}

#endif // ENABLE_NAPI
