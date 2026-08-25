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

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstring>

// Every assertion below is performed inside the Evaluator::execute closure
// (property reads like ObjectRef::get need a live ExecutionStateRef*, which
// only exists for the duration of that call - see CallFunctionReportsExceptionAsPendingStatus
// in testnapi.cpp for the same pattern). Failures are reported by returning a
// human-readable StringRef describing what went wrong; StringRef::asString()/
// toStdUTF8String() need no ExecutionStateRef, so the outer test body can
// inspect that string once Evaluator::execute has returned.
static ValueRef* MakeOkString()
{
    return StringRef::createFromASCII("OK");
}

static ValueRef* MakeFailString(const std::string& msg)
{
    return StringRef::createFromUTF8(msg.c_str(), msg.length());
}

TEST(Napi, ErrorCreateAndIsError)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            napi_value codeValue;
            napi_create_string_utf8(env, "ERR_CODE", NAPI_AUTO_LENGTH, &codeValue);
            napi_value msgValue;
            napi_create_string_utf8(env, "something failed", NAPI_AUTO_LENGTH, &msgValue);

            struct Kind {
                const char* name;
                napi_status (*create)(napi_env, napi_value, napi_value, napi_value*);
            };
            Kind kinds[] = {
                { "Error", napi_create_error },
                { "TypeError", napi_create_type_error },
                { "RangeError", napi_create_range_error },
                { "SyntaxError", node_api_create_syntax_error },
            };

            for (const Kind& kind : kinds) {
                napi_value errorValue = nullptr;
                napi_status status = kind.create(env, codeValue, msgValue, &errorValue);
                if (status != napi_ok) {
                    return MakeFailString(std::string(kind.name) + ": create failed");
                }

                bool isError = false;
                napi_is_error(env, errorValue, &isError);
                if (!isError) {
                    return MakeFailString(std::string(kind.name) + ": napi_is_error was false");
                }

                napi_valuetype type;
                napi_typeof(env, errorValue, &type);
                if (type != napi_object) {
                    return MakeFailString(std::string(kind.name) + ": typeof was not object");
                }

                ObjectRef* obj = FromNapi(errorValue)->asObject();
                ValueRef* message = obj->get(state, StringRef::createFromASCII("message"));
                if (!message->isString() || !message->asString()->equalsWithASCIIString("something failed", strlen("something failed"))) {
                    return MakeFailString(std::string(kind.name) + ": message mismatch");
                }

                ValueRef* code = obj->get(state, StringRef::createFromASCII("code"));
                if (!code->isString() || !code->asString()->equalsWithASCIIString("ERR_CODE", strlen("ERR_CODE"))) {
                    return MakeFailString(std::string(kind.name) + ": code mismatch");
                }
            }

            // a napi_value that isn't an Error must be reported as such.
            bool isErrorForPlainObject = true;
            napi_value plainObject;
            napi_create_object(env, &plainObject);
            napi_is_error(env, plainObject, &isErrorForPlainObject);
            if (isErrorForPlainObject) {
                return StringRef::createFromASCII("plain object: napi_is_error was true");
            }

            // omitting the optional `code` napi_value must not set ".code".
            napi_value errorNoCode = nullptr;
            napi_create_error(env, nullptr, msgValue, &errorNoCode);
            bool hasCode = FromNapi(errorNoCode)->asObject()->hasOwnProperty(state, StringRef::createFromASCII("code"));
            if (hasCode) {
                return StringRef::createFromASCII("errorNoCode: code should be absent");
            }

            return MakeOkString();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_TRUE(result.result->isString());
    EXPECT_EQ(result.result->asString()->toStdUTF8String(), "OK");
}

TEST(Napi, ErrorThrowSetsPendingException)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            struct Kind {
                const char* name;
                napi_status (*throwFn)(napi_env, const char*, const char*);
            };
            Kind kinds[] = {
                { "TypeError", napi_throw_type_error },
                { "RangeError", napi_throw_range_error },
                { "SyntaxError", node_api_throw_syntax_error },
            };

            for (const Kind& kind : kinds) {
                bool isPendingBefore = true;
                napi_is_exception_pending(env, &isPendingBefore);
                if (isPendingBefore) {
                    return MakeFailString(std::string(kind.name) + ": exception already pending");
                }

                napi_status status = kind.throwFn(env, "ERR_CODE", "boom");
                if (status != napi_ok) {
                    return MakeFailString(std::string(kind.name) + ": throw call failed");
                }

                bool isPendingAfter = false;
                napi_is_exception_pending(env, &isPendingAfter);
                if (!isPendingAfter) {
                    return MakeFailString(std::string(kind.name) + ": exception not pending");
                }

                napi_value exception = nullptr;
                napi_get_and_clear_last_exception(env, &exception);

                bool isError = false;
                napi_is_error(env, exception, &isError);
                if (!isError) {
                    return MakeFailString(std::string(kind.name) + ": thrown value is not an Error");
                }

                ObjectRef* obj = FromNapi(exception)->asObject();
                ValueRef* message = obj->get(state, StringRef::createFromASCII("message"));
                if (!message->isString() || !message->asString()->equalsWithASCIIString("boom", strlen("boom"))) {
                    return MakeFailString(std::string(kind.name) + ": message mismatch");
                }

                ValueRef* code = obj->get(state, StringRef::createFromASCII("code"));
                if (!code->isString() || !code->asString()->equalsWithASCIIString("ERR_CODE", strlen("ERR_CODE"))) {
                    return MakeFailString(std::string(kind.name) + ": code mismatch");
                }

                bool isPendingAfterClear = true;
                napi_is_exception_pending(env, &isPendingAfterClear);
                if (isPendingAfterClear) {
                    return MakeFailString(std::string(kind.name) + ": still pending after clear");
                }
            }

            return MakeOkString();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    ASSERT_TRUE(result.result->isString());
    EXPECT_EQ(result.result->asString()->toStdUTF8String(), "OK");
}

#endif // ENABLE_NAPI
