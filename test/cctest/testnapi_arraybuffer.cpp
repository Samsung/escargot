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

// Exercises NapiArrayBuffer.cpp's slice of js_native_api.h/node_api.h
// (ArrayBuffer/TypedArray/DataView/External/type-tag/Buffer), self-contained
// (no dlopen'd addon involved) - same NapiEnv/Evaluator::execute setup as
// test/cctest/testnapi.cpp.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstdint>

TEST(Napi, ArrayBuffer)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            void* data = nullptr;
            napi_value buf = nullptr;
            napi_status status = napi_create_arraybuffer(env, 16, &data, &buf);
            if (status != napi_ok || data == nullptr) {
                return ValueRef::create(false);
            }

            // write through the returned data pointer, then read the same bytes
            // back via napi_get_arraybuffer_info
            static_cast<uint8_t*>(data)[0] = 0x42;
            static_cast<uint8_t*>(data)[15] = 0x7f;

            bool isAB = false;
            napi_is_arraybuffer(env, buf, &isAB);
            if (!isAB) {
                return ValueRef::create(false);
            }

            bool nonBufIsAB = true;
            napi_is_arraybuffer(env, ToNapi(ValueRef::create(5)), &nonBufIsAB);
            if (nonBufIsAB) {
                return ValueRef::create(false);
            }

            void* readData = nullptr;
            size_t byteLength = 0;
            napi_get_arraybuffer_info(env, buf, &readData, &byteLength);
            if (byteLength != 16 || readData != data) {
                return ValueRef::create(false);
            }
            if (static_cast<uint8_t*>(readData)[0] != 0x42 || static_cast<uint8_t*>(readData)[15] != 0x7f) {
                return ValueRef::create(false);
            }

            bool detachedBefore = true;
            napi_is_detached_arraybuffer(env, buf, &detachedBefore);
            if (detachedBefore) {
                return ValueRef::create(false);
            }

            napi_detach_arraybuffer(env, buf);
            bool detachedAfter = false;
            napi_is_detached_arraybuffer(env, buf, &detachedAfter);
            if (!detachedAfter) {
                return ValueRef::create(false);
            }

            return ValueRef::create(true);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, TypedArray)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            void* data = nullptr;
            napi_value buf = nullptr;
            napi_create_arraybuffer(env, 16, &data, &buf);

            // 8-element Uint8Array starting 4 bytes into the backing arraybuffer
            napi_value ta = nullptr;
            napi_status status = napi_create_typedarray(env, napi_uint8_array, 8, buf, 4, &ta);
            if (status != napi_ok) {
                return ValueRef::create(false);
            }

            bool isTA = false;
            napi_is_typedarray(env, ta, &isTA);
            if (!isTA) {
                return ValueRef::create(false);
            }

            napi_typedarray_type type;
            size_t length = 0;
            void* taData = nullptr;
            napi_value taBuf = nullptr;
            size_t byteOffset = 0;
            napi_status infoStatus = napi_get_typedarray_info(env, ta, &type, &length, &taData, &taBuf, &byteOffset);
            if (infoStatus != napi_ok) {
                return ValueRef::create(false);
            }

            bool ok = (type == napi_uint8_array) && (length == 8) && (byteOffset == 4) && (taData == static_cast<uint8_t*>(data) + 4) && (FromNapi(taBuf) == FromNapi(buf));
            return ValueRef::create(ok);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, DataView)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            void* data = nullptr;
            napi_value buf = nullptr;
            napi_create_arraybuffer(env, 16, &data, &buf);

            napi_value dv = nullptr;
            napi_status status = napi_create_dataview(env, 8, buf, 2, &dv);
            if (status != napi_ok) {
                return ValueRef::create(false);
            }

            bool isDV = false;
            napi_is_dataview(env, dv, &isDV);
            if (!isDV) {
                return ValueRef::create(false);
            }

            // a DataView must not also report as a TypedArray
            bool isTA = true;
            napi_is_typedarray(env, dv, &isTA);
            if (isTA) {
                return ValueRef::create(false);
            }

            size_t byteLength = 0;
            void* dvData = nullptr;
            napi_value dvBuf = nullptr;
            size_t byteOffset = 0;
            napi_status infoStatus = napi_get_dataview_info(env, dv, &byteLength, &dvData, &dvBuf, &byteOffset);
            if (infoStatus != napi_ok) {
                return ValueRef::create(false);
            }

            bool ok = (byteLength == 8) && (byteOffset == 2) && (dvData == static_cast<uint8_t*>(data) + 2) && (FromNapi(dvBuf) == FromNapi(buf));
            return ValueRef::create(ok);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, External)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    int payload = 1234;

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, int* payload) -> ValueRef* {
            env->executionState = state;

            napi_value ext = nullptr;
            napi_status status = napi_create_external(env, payload, nullptr, nullptr, &ext);
            if (status != napi_ok) {
                return ValueRef::create(false);
            }

            void* roundTripped = nullptr;
            napi_get_value_external(env, ext, &roundTripped);
            bool pointerMatches = (roundTripped == static_cast<void*>(payload));

            // documented limitation: napi_typeof cannot distinguish this from
            // a plain object, so it reports napi_object rather than
            // napi_external (see NapiArrayBuffer.cpp's napi_create_external)
            napi_valuetype type;
            napi_typeof(env, ext, &type);
            bool typeofIsObject = (type == napi_object);

            return ValueRef::create(pointerMatches && typeofIsObject);
        },
        napiEnv->env(), &payload);

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, TypeTag)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            ObjectRef* obj = ObjectRef::create(state);
            napi_value objValue = ToNapi(obj);

            napi_type_tag tag = { 0x1111111111111111ULL, 0x2222222222222222ULL };
            napi_status tagStatus = napi_type_tag_object(env, objValue, &tag);
            if (tagStatus != napi_ok) {
                return ValueRef::create(false);
            }

            bool matches = false;
            napi_check_object_type_tag(env, objValue, &tag, &matches);
            if (!matches) {
                return ValueRef::create(false);
            }

            napi_type_tag otherTag = { 0x3333333333333333ULL, 0x4444444444444444ULL };
            bool mismatches = true;
            napi_check_object_type_tag(env, objValue, &otherTag, &mismatches);
            if (mismatches) {
                return ValueRef::create(false);
            }

            // an untagged object must not match any tag
            ObjectRef* untaggedObj = ObjectRef::create(state);
            bool untaggedMatches = true;
            napi_check_object_type_tag(env, ToNapi(untaggedObj), &tag, &untaggedMatches);
            if (untaggedMatches) {
                return ValueRef::create(false);
            }

            // re-tagging an already-tagged object must be rejected
            napi_status retagStatus = napi_type_tag_object(env, objValue, &tag);
            if (retagStatus == napi_ok) {
                return ValueRef::create(false);
            }

            return ValueRef::create(true);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

// Buffer is implemented as a plain Uint8Array over a backing store (see
// NapiArrayBuffer.cpp's napi_create_buffer) - this exercises that
// approximation end-to-end.
TEST(Napi, Buffer)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            void* data = nullptr;
            napi_value buf = nullptr;
            napi_status status = napi_create_buffer(env, 4, &data, &buf);
            if (status != napi_ok || data == nullptr) {
                return ValueRef::create(false);
            }
            static_cast<uint8_t*>(data)[0] = 9;

            bool isBuf = false;
            napi_is_buffer(env, buf, &isBuf);
            if (!isBuf) {
                return ValueRef::create(false);
            }

            void* readData = nullptr;
            size_t length = 0;
            napi_get_buffer_info(env, buf, &readData, &length);

            bool ok = (length == 4) && (static_cast<uint8_t*>(readData)[0] == 9);
            return ValueRef::create(ok);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

#endif // ENABLE_NAPI
