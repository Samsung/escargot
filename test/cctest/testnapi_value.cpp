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

// Exercises the value creation/read-out/coercion slice implemented in
// NapiValue.cpp: singletons, int64/string creation, get_value_* readers, and
// napi_coerce_to_*/napi_strict_equals. Unlike testnapi.cpp this doesn't
// dlopen() any vendored addon - every napi_* call is made directly from a C++
// lambda run through Evaluator::execute, the same way NapiFunctions.cpp's own
// non-addon tests (e.g. Napi.CallFunctionReportsExceptionAsPendingStatus,
// Napi.HandleScopeOpenCloseAndMismatch) do.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>
#include <limits>

TEST(Napi, ValueSingletonsAndVersion)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            napi_value nullValue = nullptr;
            EXPECT_EQ(napi_get_null(env, &nullValue), napi_ok);
            EXPECT_TRUE(FromNapi(nullValue)->isNull());

            uint32_t version = 0;
            EXPECT_EQ(napi_get_version(env, &version), napi_ok);
            EXPECT_EQ(version, static_cast<uint32_t>(NAPI_VERSION));

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ValueNumberRoundTrips)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            // napi_create_int64 / napi_get_value_int64 round trip, plus the
            // clamp-to-int64-range and non-finite-maps-to-0 special cases
            // napi_get_value_int64 documents.
            napi_value fortyTwo = nullptr;
            EXPECT_EQ(napi_create_int64(env, 42, &fortyTwo), napi_ok);
            int64_t asInt64 = 0;
            EXPECT_EQ(napi_get_value_int64(env, fortyTwo, &asInt64), napi_ok);
            EXPECT_EQ(asInt64, 42);

            napi_value negative = nullptr;
            EXPECT_EQ(napi_create_int64(env, -123456789012345, &negative), napi_ok);
            int64_t negativeOut = 0;
            EXPECT_EQ(napi_get_value_int64(env, negative, &negativeOut), napi_ok);
            EXPECT_EQ(negativeOut, -123456789012345);

            napi_value truncated = ToNapi(ValueRef::create(3.7));
            int64_t truncatedOut = 0;
            EXPECT_EQ(napi_get_value_int64(env, truncated, &truncatedOut), napi_ok);
            EXPECT_EQ(truncatedOut, 3); // ToInteger truncates toward zero

            napi_value huge = ToNapi(ValueRef::create(1e300));
            int64_t hugeOut = 0;
            EXPECT_EQ(napi_get_value_int64(env, huge, &hugeOut), napi_ok);
            EXPECT_EQ(hugeOut, INT64_MAX);

            napi_value hugeNegative = ToNapi(ValueRef::create(-1e300));
            int64_t hugeNegativeOut = 0;
            EXPECT_EQ(napi_get_value_int64(env, hugeNegative, &hugeNegativeOut), napi_ok);
            EXPECT_EQ(hugeNegativeOut, INT64_MIN);

            napi_value nanValue = ToNapi(ValueRef::create(std::numeric_limits<double>::quiet_NaN()));
            int64_t nanOut = 123;
            EXPECT_EQ(napi_get_value_int64(env, nanValue, &nanOut), napi_ok);
            EXPECT_EQ(nanOut, 0);

            // napi_get_value_int32 wraps like ECMAScript's ToInt32.
            napi_value wraps = ToNapi(ValueRef::create(4294967296.0 + 5.0)); // 2^32 + 5
            int32_t wrapsOut = 0;
            EXPECT_EQ(napi_get_value_int32(env, wraps, &wrapsOut), napi_ok);
            EXPECT_EQ(wrapsOut, 5);

            napi_value minusOne = ToNapi(ValueRef::create(-1));
            int32_t minusOneOut = 0;
            EXPECT_EQ(napi_get_value_int32(env, minusOne, &minusOneOut), napi_ok);
            EXPECT_EQ(minusOneOut, -1);

            // Type-mismatch cases.
            napi_value notANumber = ToNapi(StringRef::createFromASCII("nope"));
            int32_t unused32 = 0;
            EXPECT_EQ(napi_get_value_int32(env, notANumber, &unused32), napi_number_expected);
            int64_t unused64 = 0;
            EXPECT_EQ(napi_get_value_int64(env, notANumber, &unused64), napi_number_expected);

            // napi_get_value_bool.
            napi_value trueValue = ToNapi(ValueRef::create(true));
            napi_value falseValue = ToNapi(ValueRef::create(false));
            bool boolOut = false;
            EXPECT_EQ(napi_get_value_bool(env, trueValue, &boolOut), napi_ok);
            EXPECT_TRUE(boolOut);
            EXPECT_EQ(napi_get_value_bool(env, falseValue, &boolOut), napi_ok);
            EXPECT_FALSE(boolOut);
            EXPECT_EQ(napi_get_value_bool(env, fortyTwo, &boolOut), napi_boolean_expected);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ValueStrings)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            // napi_create_string_latin1 / napi_get_value_string_latin1 round
            // trip, including a byte (0xE9, "e with acute") outside ASCII.
            const unsigned char latin1Bytes[] = { 'H', 'i', 0xE9 };
            napi_value latin1Str = nullptr;
            EXPECT_EQ(napi_create_string_latin1(env, reinterpret_cast<const char*>(latin1Bytes), 3, &latin1Str), napi_ok);

            size_t requiredLen = 0;
            EXPECT_EQ(napi_get_value_string_latin1(env, latin1Str, nullptr, 0, &requiredLen), napi_ok);
            EXPECT_EQ(requiredLen, 3u);

            char latin1Buf[16];
            size_t latin1Copied = 0;
            EXPECT_EQ(napi_get_value_string_latin1(env, latin1Str, latin1Buf, sizeof(latin1Buf), &latin1Copied), napi_ok);
            EXPECT_EQ(latin1Copied, 3u);
            EXPECT_EQ(static_cast<unsigned char>(latin1Buf[0]), 'H');
            EXPECT_EQ(static_cast<unsigned char>(latin1Buf[1]), 'i');
            EXPECT_EQ(static_cast<unsigned char>(latin1Buf[2]), 0xE9);
            EXPECT_EQ(latin1Buf[3], '\0');

            // Truncating copy: only room for 2 chars + NUL.
            char latin1Small[3];
            size_t latin1SmallCopied = 0;
            EXPECT_EQ(napi_get_value_string_latin1(env, latin1Str, latin1Small, sizeof(latin1Small), &latin1SmallCopied), napi_ok);
            EXPECT_EQ(latin1SmallCopied, 2u);
            EXPECT_EQ(latin1Small[2], '\0');

            // NAPI_AUTO_LENGTH variant (NUL-terminated C string).
            napi_value latin1Auto = nullptr;
            EXPECT_EQ(napi_create_string_latin1(env, "auto", NAPI_AUTO_LENGTH, &latin1Auto), napi_ok);
            EXPECT_EQ(FromNapi(latin1Auto)->asString()->length(), 4u);

            // napi_create_string_utf16 / napi_get_value_string_utf16 round
            // trip, including a BMP code unit outside ASCII (U+00E9).
            const char16_t utf16Chars[] = { u'H', u'i', 0x00E9 };
            napi_value utf16Str = nullptr;
            EXPECT_EQ(napi_create_string_utf16(env, utf16Chars, 3, &utf16Str), napi_ok);

            size_t utf16RequiredLen = 0;
            EXPECT_EQ(napi_get_value_string_utf16(env, utf16Str, nullptr, 0, &utf16RequiredLen), napi_ok);
            EXPECT_EQ(utf16RequiredLen, 3u);

            char16_t utf16Buf[16];
            size_t utf16Copied = 0;
            EXPECT_EQ(napi_get_value_string_utf16(env, utf16Str, utf16Buf, 16, &utf16Copied), napi_ok);
            EXPECT_EQ(utf16Copied, 3u);
            EXPECT_EQ(utf16Buf[0], u'H');
            EXPECT_EQ(utf16Buf[1], u'i');
            EXPECT_EQ(utf16Buf[2], 0x00E9);
            EXPECT_EQ(utf16Buf[3], u'\0');

            // bufsize == 0 means "write nothing, report 0 copied".
            size_t utf16ZeroCopied = 123;
            EXPECT_EQ(napi_get_value_string_utf16(env, utf16Str, utf16Buf, 0, &utf16ZeroCopied), napi_ok);
            EXPECT_EQ(utf16ZeroCopied, 0u);

            // napi_get_value_string_utf8 against a genuinely multi-byte UTF-8
            // string ("h\xC3\xA9llo", i.e. "héllo": h,e-acute,l,l,o = 6 bytes).
            napi_value utf8Str = ToNapi(StringRef::createFromUTF8("h\xC3\xA9llo", 6));
            size_t utf8RequiredLen = 0;
            EXPECT_EQ(napi_get_value_string_utf8(env, utf8Str, nullptr, 0, &utf8RequiredLen), napi_ok);
            EXPECT_EQ(utf8RequiredLen, 6u);

            char utf8Buf[16];
            size_t utf8Copied = 0;
            EXPECT_EQ(napi_get_value_string_utf8(env, utf8Str, utf8Buf, sizeof(utf8Buf), &utf8Copied), napi_ok);
            EXPECT_EQ(utf8Copied, 6u);
            EXPECT_EQ(std::string(utf8Buf, utf8Copied), std::string("h\xC3\xA9llo", 6));
            EXPECT_EQ(utf8Buf[6], '\0');

            // Truncating copy: only room for 4 bytes + NUL.
            char utf8Small[5];
            size_t utf8SmallCopied = 0;
            EXPECT_EQ(napi_get_value_string_utf8(env, utf8Str, utf8Small, sizeof(utf8Small), &utf8SmallCopied), napi_ok);
            EXPECT_EQ(utf8SmallCopied, 4u);
            EXPECT_EQ(utf8Small[4], '\0');

            // Type-mismatch cases.
            napi_value notAString = ToNapi(ValueRef::create(1));
            size_t unused = 0;
            EXPECT_EQ(napi_get_value_string_utf8(env, notAString, nullptr, 0, &unused), napi_string_expected);
            EXPECT_EQ(napi_get_value_string_latin1(env, notAString, nullptr, 0, &unused), napi_string_expected);
            EXPECT_EQ(napi_get_value_string_utf16(env, notAString, nullptr, 0, &unused), napi_string_expected);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ValueCoercions)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            // napi_coerce_to_bool.
            napi_value zero = ToNapi(ValueRef::create(0));
            napi_value boolResult = nullptr;
            EXPECT_EQ(napi_coerce_to_bool(env, zero, &boolResult), napi_ok);
            EXPECT_TRUE(FromNapi(boolResult)->isBoolean());
            EXPECT_FALSE(FromNapi(boolResult)->asBoolean());

            napi_value nonEmptyStr = ToNapi(StringRef::createFromASCII("x"));
            EXPECT_EQ(napi_coerce_to_bool(env, nonEmptyStr, &boolResult), napi_ok);
            EXPECT_TRUE(FromNapi(boolResult)->asBoolean());

            // napi_coerce_to_number: "42" -> 42.
            napi_value strFortyTwo = ToNapi(StringRef::createFromASCII("42"));
            napi_value numberResult = nullptr;
            EXPECT_EQ(napi_coerce_to_number(env, strFortyTwo, &numberResult), napi_ok);
            EXPECT_TRUE(FromNapi(numberResult)->isNumber());
            EXPECT_EQ(FromNapi(numberResult)->asNumber(), 42);

            // napi_coerce_to_string: 42 -> "42".
            napi_value numFortyTwo = ToNapi(ValueRef::create(42));
            napi_value stringResult = nullptr;
            EXPECT_EQ(napi_coerce_to_string(env, numFortyTwo, &stringResult), napi_ok);
            EXPECT_TRUE(FromNapi(stringResult)->isString());
            EXPECT_EQ(FromNapi(stringResult)->asString()->toStdUTF8String(), "42");

            // napi_coerce_to_object: a primitive is wrapped, not thrown.
            napi_value objectResult = nullptr;
            EXPECT_EQ(napi_coerce_to_object(env, numFortyTwo, &objectResult), napi_ok);
            EXPECT_TRUE(FromNapi(objectResult)->isObject());

            // napi_coerce_to_object(null) throws a TypeError per ECMAScript's
            // ToObject - proves the Evaluator::execute wrapper actually
            // catches it instead of letting a raw C++ exception cross this
            // call, the same contract napi_call_function relies on.
            napi_value nullValue = nullptr;
            napi_get_null(env, &nullValue);
            napi_value shouldFail = nullptr;
            napi_status coerceStatus = napi_coerce_to_object(env, nullValue, &shouldFail);
            EXPECT_EQ(coerceStatus, napi_pending_exception);

            bool isPending = false;
            napi_is_exception_pending(env, &isPending);
            EXPECT_TRUE(isPending);

            napi_value exception = nullptr;
            napi_get_and_clear_last_exception(env, &exception);
            EXPECT_TRUE(FromNapi(exception)->isObject());

            bool stillPending = true;
            napi_is_exception_pending(env, &stillPending);
            EXPECT_FALSE(stillPending);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ValueStrictEquals)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            napi_value one = ToNapi(ValueRef::create(1));
            napi_value oneAgain = ToNapi(ValueRef::create(1.0));
            napi_value two = ToNapi(ValueRef::create(2));
            napi_value strOne = ToNapi(StringRef::createFromASCII("1"));

            bool areEqual = false;
            EXPECT_EQ(napi_strict_equals(env, one, oneAgain, &areEqual), napi_ok);
            EXPECT_TRUE(areEqual);

            EXPECT_EQ(napi_strict_equals(env, one, two, &areEqual), napi_ok);
            EXPECT_FALSE(areEqual);

            // Different types never strict-equal, even with the "same" value.
            EXPECT_EQ(napi_strict_equals(env, one, strOne, &areEqual), napi_ok);
            EXPECT_FALSE(areEqual);

            // Strings compare by content, not by identity, unlike objects.
            napi_value strA1 = ToNapi(StringRef::createFromASCII("same"));
            napi_value strA2 = ToNapi(StringRef::createFromASCII("same"));
            EXPECT_EQ(napi_strict_equals(env, strA1, strA2, &areEqual), napi_ok);
            EXPECT_TRUE(areEqual);

            napi_value obj1 = ToNapi(ObjectRef::create(state));
            napi_value obj2 = ToNapi(ObjectRef::create(state));
            EXPECT_EQ(napi_strict_equals(env, obj1, obj1, &areEqual), napi_ok);
            EXPECT_TRUE(areEqual);
            EXPECT_EQ(napi_strict_equals(env, obj1, obj2, &areEqual), napi_ok);
            EXPECT_FALSE(areEqual);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

#endif // ENABLE_NAPI
