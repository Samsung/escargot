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

// Drives the real, unmodified Node-API TCs vendored at
// test/napi-tc/test/js-native-api/*.
// There is no require()/module-loader integration yet, so this dlopen()s each
// compiled addon directly and calls its exported napi_register_module_v1,
// reproducing the same assertions each addon's own test.js makes.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <dlfcn.h>
#include <cstring>

typedef napi_value (*NapiRegisterModuleFn)(napi_env, napi_value);

TEST(Napi, TwoFunctionArguments)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_2_FUNCTION_ARGUMENTS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));

            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();
            ValueRef* addFn = exportsResult->get(state, StringRef::createFromASCII("add"));

            ValueRef* args[2] = { ValueRef::create(3), ValueRef::create(5) };
            return addFn->call(state, ValueRef::createUndefined(), 2, args);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_EQ(result.result->asNumber(), 8);

    dlclose(handle);
}

// captures what a JS-side callback was invoked with, so the C++ test body can
// inspect it afterward; plain file-static globals since NativeFunctionInfo
// only accepts capture-less function pointers (no per-instance user data
// short of the extraData() mechanism napi_create_function itself uses)
static ValueRef* g_callbackArg = nullptr;
static ValueRef* g_callbackThis = nullptr;

static ValueRef* RecordCallbackArgAndThis(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    g_callbackArg = argc > 0 ? argv[0] : ValueRef::createUndefined();
    g_callbackThis = thisValue;
    return ValueRef::createUndefined();
}

TEST(Napi, Callbacks)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_3_CALLBACKS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    g_callbackArg = nullptr;
    g_callbackThis = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            AtomicStringRef* cbName = AtomicStringRef::create(state->context(), "cb", 2);
            FunctionObjectRef* cb = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(cbName, RecordCallbackArgAndThis, 1, true, false));

            // RunCallback(cb) should call cb.call(global, 'hello world')
            ValueRef* runCallback = exportsResult->get(state, StringRef::createFromASCII("RunCallback"));
            ValueRef* runCallbackArgs[1] = { cb };
            runCallback->call(state, ValueRef::createUndefined(), 1, runCallbackArgs);

            return ValueRef::createUndefined();
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_NE(g_callbackArg, nullptr);
    ASSERT_TRUE(g_callbackArg->isString());
    EXPECT_TRUE(g_callbackArg->asString()->equalsWithASCIIString("hello world", strlen("hello world")));

    dlclose(handle);
}

TEST(Napi, CallbackRecv)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_3_CALLBACKS_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    g_callbackArg = nullptr;
    g_callbackThis = nullptr;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            AtomicStringRef* cbName = AtomicStringRef::create(state->context(), "cb", 2);
            FunctionObjectRef* cb = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(cbName, RecordCallbackArgAndThis, 0, true, false));

            ObjectRef* desiredRecv = ObjectRef::create(state);

            // RunCallbackWithRecv(cb, desiredRecv) should call cb.call(desiredRecv)
            ValueRef* runCallbackWithRecv = exportsResult->get(state, StringRef::createFromASCII("RunCallbackWithRecv"));
            ValueRef* args[2] = { cb, desiredRecv };
            runCallbackWithRecv->call(state, ValueRef::createUndefined(), 2, args);

            return desiredRecv;
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_NE(g_callbackThis, nullptr);
    // Escargot's GC (Boehm) never moves objects, so comparing the raw pointers
    // captured by the callback against the desiredRecv returned from the lambda
    // is a valid identity check
    EXPECT_EQ(g_callbackThis, result.result);

    dlclose(handle);
}

TEST(Napi, ObjectFactory)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_4_OBJECT_FACTORY_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ValueRef* factory = FromNapi(returnedExports);

            ValueRef* helloArgs[1] = { StringRef::createFromASCII("hello") };
            ValueRef* obj1 = factory->call(state, ValueRef::createUndefined(), 1, helloArgs);
            ValueRef* worldArgs[1] = { StringRef::createFromASCII("world") };
            ValueRef* obj2 = factory->call(state, ValueRef::createUndefined(), 1, worldArgs);

            ValueRef* msg1 = obj1->asObject()->get(state, StringRef::createFromASCII("msg"));
            ValueRef* msg2 = obj2->asObject()->get(state, StringRef::createFromASCII("msg"));

            bool ok = msg1->isString() && msg2->isString()
                && msg1->asString()->equalsWithASCIIString("hello", strlen("hello"))
                && msg2->asString()->equalsWithASCIIString("world", strlen("world"));
            return ValueRef::create(ok);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());

    dlclose(handle);
}

TEST(Napi, FunctionFactory)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_5_FUNCTION_FACTORY_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ValueRef* factory = FromNapi(returnedExports);

            ValueRef* fn = factory->call(state, ValueRef::createUndefined(), 0, nullptr);
            return fn->call(state, ValueRef::createUndefined(), 0, nullptr);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_TRUE(result.result->isString());
    EXPECT_TRUE(result.result->asString()->equalsWithASCIIString("hello world", strlen("hello world")));

    dlclose(handle);
}

TEST(Napi, ObjectWrap)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_MYOBJECT_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            env->executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            ValueRef* cons = exportsResult->get(state, StringRef::createFromASCII("MyObject"));

            ValueRef* ctorArgs[1] = { ValueRef::create(9) };
            ValueRef* obj = cons->construct(state, 1, ctorArgs);
            ObjectRef* objRef = obj->asObject();

            bool ok = true;
            ok = ok && objRef->get(state, StringRef::createFromASCII("value"))->asNumber() == 9;

            objRef->set(state, StringRef::createFromASCII("value"), ValueRef::create(10));
            ok = ok && objRef->get(state, StringRef::createFromASCII("value"))->asNumber() == 10;
            ok = ok && objRef->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == 10;

            // valueReadonly has no setter (napi_define_class's `value`
            // descriptor's setter field is null), so this assignment must fail
            bool setSucceeded = objRef->set(state, StringRef::createFromASCII("valueReadonly"), ValueRef::create(14));
            ok = ok && !setSucceeded;
            ok = ok && objRef->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == 10;

            ValueRef* plusOne = objRef->get(state, StringRef::createFromASCII("plusOne"));
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 11;
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 12;
            ok = ok && plusOne->call(state, objRef, 0, nullptr)->asNumber() == 13;

            ValueRef* multiply = objRef->get(state, StringRef::createFromASCII("multiply"));

            ValueRef* noArgResult = multiply->call(state, objRef, 0, nullptr);
            ok = ok && noArgResult->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == 13;

            ValueRef* tenArgs[1] = { ValueRef::create(10) };
            ValueRef* tenResult = multiply->call(state, objRef, 1, tenArgs);
            ok = ok && tenResult->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == 130;

            ValueRef* negArgs[1] = { ValueRef::create(-1) };
            ValueRef* newObj = multiply->call(state, objRef, 1, negArgs);
            ok = ok && newObj->asObject()->get(state, StringRef::createFromASCII("value"))->asNumber() == -13;
            ok = ok && newObj->asObject()->get(state, StringRef::createFromASCII("valueReadonly"))->asNumber() == -13;
            ok = ok && (newObj != obj);

            return ValueRef::create(ok);
        },
        napiEnv->env(), registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());

    // Flush out the wrapped MyObject instances created above before this test
    // ends. This is not about env safety anymore (env now lives as long as
    // NapiEnv itself, see NapiEnv::env()) - it is about not leaving
    // unreachable-but-uncollected napi_wrap'd garbage sitting in the shared
    // Boehm heap. Left alone, it can get opportunistically finalized at an
    // arbitrary later point (e.g. mid-construction of a *different* test's
    // brand-new VMInstance, which is an unsafe time to run addon code) -
    // this actually crashed FactoryWrap when this cleanup was missing. Same
    // "clear stack + churn + gc x5" pattern as
    // test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    dlclose(handle);
}

TEST(Napi, FactoryWrap)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    void* handle = dlopen(NAPI_7_FACTORY_WRAP_SO_PATH, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << dlerror();

    NapiRegisterModuleFn registerModule = reinterpret_cast<NapiRegisterModuleFn>(dlsym(handle, "napi_register_module_v1"));
    ASSERT_NE(registerModule, nullptr) << dlerror();

    ObjectRef* exports = nullptr;

    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, NapiRegisterModuleFn registerModule, ObjectRef** exportsOut) -> ValueRef* {
            env->executionState = state;
            ObjectRef* exportsObj = ObjectRef::create(state);
            napi_value returnedExports = registerModule(env, ToNapi(exportsObj));
            *exportsOut = FromNapi(returnedExports)->asObject();

            ValueRef* finalizeCount = (*exportsOut)->get(state, StringRef::createFromASCII("finalizeCount"));
            EXPECT_EQ(finalizeCount->asNumber(), 0);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), registerModule, &exports);
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    // napi_wrap creates a weak napi_ref (refcount 0, see NapiFunctions.cpp),
    // so nothing artificially roots the wrapped MyObject instance created
    // below - it becomes collectible the moment the JS side drops it.
    for (int round = 0; round < 2; round++) {
        int base = (round == 0) ? 10 : 20;

        // Create one wrapped MyObject and exercise it inside its own
        // Evaluator::execute call, so no local variable in FactoryWrap's own
        // stack frame keeps referencing it once this call returns.
        Evaluator::EvaluatorResult r2 = Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, napi_env env, ObjectRef* exports, int base) -> ValueRef* {
                env->executionState = state;
                ValueRef* createObject = exports->get(state, StringRef::createFromASCII("createObject"));
                ValueRef* args[1] = { ValueRef::create(base) };
                ValueRef* obj = createObject->call(state, ValueRef::createUndefined(), 1, args);
                ObjectRef* objRef = obj->asObject();
                ValueRef* plusOne = objRef->get(state, StringRef::createFromASCII("plusOne"));
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 1);
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 2);
                EXPECT_EQ(plusOne->call(state, objRef, 0, nullptr)->asNumber(), base + 3);
                return ValueRef::createUndefined();
            },
            napiEnv->env(), exports, base);
        ASSERT_TRUE(r2.isSuccessful()) << r2.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

        // "clear stack": an unrelated Evaluator::execute call reuses the
        // native stack frame the previous call's locals (obj/objRef/plusOne)
        // sat in, so Boehm's conservative stack scan doesn't keep seeing
        // those stale bit patterns as live roots. Same pattern as
        // test/cctest/testapi.cpp's WeakPtr.*/Finalizer.Basic tests.
        Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
                return ValueRef::create(100);
            },
            StringRef::createFromUTF8("qwer"));

        for (size_t i = 0; i < 100; i++) {
            PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
        }
        Memory::gc();
        Memory::gc();
        Memory::gc();
        Memory::gc();
        Memory::gc();

        Evaluator::EvaluatorResult r3 = Evaluator::execute(
            napiEnv->context(), [](ExecutionStateRef* state, napi_env env, ObjectRef* exports, int expectedCount) -> ValueRef* {
                env->executionState = state;
                ValueRef* finalizeCount = exports->get(state, StringRef::createFromASCII("finalizeCount"));
                EXPECT_EQ(finalizeCount->asNumber(), expectedCount);
                return ValueRef::createUndefined();
            },
            napiEnv->env(), exports, round + 1);
        ASSERT_TRUE(r3.isSuccessful()) << r3.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    }

    dlclose(handle);
}

TEST(Napi, WeakReferenceStaleAfterGC)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    napi_ref ref = nullptr;

    // Create the target object and the weak (refcount 0) napi_ref to it
    // inside their own Evaluator::execute call, so no local variable in this
    // test's own stack frame keeps referencing the object once this call
    // returns (same "own call frame" discipline as Napi.FactoryWrap above).
    Evaluator::EvaluatorResult r1 = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, napi_ref* refOut) -> ValueRef* {
            env->executionState = state;
            ObjectRef* obj = ObjectRef::create(state);
            napi_create_reference(env, ToNapi(obj), 0, refOut);
            return ValueRef::createUndefined();
        },
        napiEnv->env(), &ref);
    ASSERT_TRUE(r1.isSuccessful()) << r1.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    napi_value value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));
    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_NE(value, nullptr);

    // Overwrite the stale pointer bit pattern this test's own stack slot is
    // still holding before collecting, so Boehm's conservative stack scan
    // doesn't keep the object alive through it (the nested-call trick below
    // only clears the *inner* lambda's frame, not this function's own).
    value = reinterpret_cast<napi_value>(static_cast<uintptr_t>(1));

    // "clear stack" + churn + gc x5, same pattern as Napi.FactoryWrap: nothing
    // else roots the object created above, so this must collect it.
    Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, StringRef* s) -> ValueRef* {
            return ValueRef::create(100);
        },
        StringRef::createFromUTF8("qwer"));
    for (size_t i = 0; i < 100; i++) {
        PersistentRefHolder<StringRef> dummy = StringRef::createFromUTF8("asdf");
    }
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();
    Memory::gc();

    napi_get_reference_value(napiEnv->env(), ref, &value);
    EXPECT_EQ(value, nullptr);

    napi_delete_reference(napiEnv->env(), ref);
}

#endif // ENABLE_NAPI
