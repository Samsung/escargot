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

// Drives the real, unmodified Node-API TC vendored at
// test/napi-tc/test/js-native-api/2_function_arguments.
// There is no require()/module-loader integration yet, so this dlopen()s the
// compiled addon directly and calls its exported napi_register_module_v1,
// reproducing the same assertion the addon's own test.js makes.

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));

            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();
            ValueRef* addFn = exportsResult->get(state, StringRef::createFromASCII("add"));

            ValueRef* args[2] = { ValueRef::create(3), ValueRef::create(5) };
            return addFn->call(state, ValueRef::createUndefined(), 2, args);
        },
        napiEnv, registerModule);

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));
            ObjectRef* exportsResult = FromNapi(returnedExports)->asObject();

            AtomicStringRef* cbName = AtomicStringRef::create(state->context(), "cb", 2);
            FunctionObjectRef* cb = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(cbName, RecordCallbackArgAndThis, 1, true, false));

            // RunCallback(cb) should call cb.call(global, 'hello world')
            ValueRef* runCallback = exportsResult->get(state, StringRef::createFromASCII("RunCallback"));
            ValueRef* runCallbackArgs[1] = { cb };
            runCallback->call(state, ValueRef::createUndefined(), 1, runCallbackArgs);

            return ValueRef::createUndefined();
        },
        napiEnv, registerModule);

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));
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
        napiEnv, registerModule);

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));
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
        napiEnv, registerModule);

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));
            ValueRef* factory = FromNapi(returnedExports);

            ValueRef* fn = factory->call(state, ValueRef::createUndefined(), 0, nullptr);
            return fn->call(state, ValueRef::createUndefined(), 0, nullptr);
        },
        napiEnv, registerModule);

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
        napiEnv->context(), [](ExecutionStateRef* state, NapiEnv* env, NapiRegisterModuleFn registerModule) -> ValueRef* {
            napi_env__ envData;
            envData.napiEnv = env;
            envData.executionState = state;

            ObjectRef* exports = ObjectRef::create(state);
            napi_value returnedExports = registerModule(&envData, ToNapi(exports));
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
        napiEnv, registerModule);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());

    dlclose(handle);
}

#endif // ENABLE_NAPI
