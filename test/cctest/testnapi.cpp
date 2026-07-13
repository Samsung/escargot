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

#endif // ENABLE_NAPI
