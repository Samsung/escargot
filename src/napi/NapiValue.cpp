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

// Value creation/read-out/coercion slice of js_native_api.h: singletons,
// int64/string creation, get_value_* readers, and the napi_coerce_to_*/
// napi_strict_equals family. See NapiFunctions.cpp for the rest of the
// implemented surface and the conventions this file follows.

#include "NapiTypes.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Escargot {
namespace Napi {

// napi_get_value_string_utf8's truncation contract (js_native_api.h) is
// "Returns as many bytes as possible from the string ... into the buffer" -
// implicitly, without ever cutting a multi-byte UTF-8 sequence in half. A
// naive `std::min(utf8.size(), bufsize - 1)` byte-count truncation (this
// file's earlier implementation) doesn't respect that: a buffer that's one
// byte too small to fit an *additional whole* character can still land
// exactly mid-sequence, producing an invalid trailing byte that decodes back
// (e.g. via a later napi_create_string_utf8 on the truncated buffer, as
// test_string/test_string.c's own TestUtf8Insufficient does) as a stray
// U+FFFD replacement character instead of simply omitting that character -
// found via test_string/test.js's latin1Cases, whose 2-byte-per-character
// UTF-8 encoding (U+00A1..U+00BF) makes a 3-byte budget split a character
// right down the middle. Walks the UTF-8 byte string counting whole
// characters (via each character's leading byte) until the *next* one
// wouldn't fit within `maxBytes`, returning the byte length of the largest
// valid whole-character prefix - never more than maxBytes, but sometimes
// less (unlike plain byte-count truncation).
static size_t Utf8PrefixByteLengthWithinBudget(const std::string& utf8, size_t maxBytes)
{
    size_t i = 0;
    while (i < utf8.size()) {
        unsigned char leadByte = static_cast<unsigned char>(utf8[i]);
        size_t charLen;
        if ((leadByte & 0x80) == 0x00) {
            charLen = 1;
        } else if ((leadByte & 0xE0) == 0xC0) {
            charLen = 2;
        } else if ((leadByte & 0xF0) == 0xE0) {
            charLen = 3;
        } else if ((leadByte & 0xF8) == 0xF0) {
            charLen = 4;
        } else {
            // not a valid UTF-8 leading byte (shouldn't happen for a
            // well-formed toStdUTF8String() result) - treat as a single
            // byte so this still makes forward progress instead of looping
            // forever.
            charLen = 1;
        }
        if (i + charLen > utf8.size() || i + charLen > maxBytes) {
            break;
        }
        i += charLen;
    }
    return i;
}

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_get_null(napi_env env, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::createNull());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_version(node_api_basic_env env, uint32_t* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = NAPI_VERSION;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_int64(napi_env env, int64_t value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_string_latin1(napi_env env, const char* str, size_t length, napi_value* result)
{
    if (str == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // see napi_create_string_utf8's identical guard (NapiFunctions.cpp) -
    // same test_string/test.js TestLargeLatin1 rationale.
    if (length != NAPI_AUTO_LENGTH && length > static_cast<size_t>(INT_MAX)) {
        return SetLastError(env, napi_invalid_arg);
    }

    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    // Transparent compressible-string routing - see napi_create_string_utf8's
    // identical comment (NapiFunctions.cpp). Below kCompressibleStringThreshold,
    // the plain path is kept so small strings (every existing test string)
    // are unaffected.
    if (stringLength >= kCompressibleStringThreshold && StringRef::isCompressibleStringEnabled() && env != nullptr && env->napiEnv->vmInstance() != nullptr) {
        *result = ToNapi(StringRef::createFromLatin1ToCompressibleString(env->napiEnv->vmInstance(), reinterpret_cast<const unsigned char*>(str), stringLength));
    } else {
        *result = ToNapi(StringRef::createFromLatin1(reinterpret_cast<const unsigned char*>(str), stringLength));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_string_utf16(napi_env env, const char16_t* str, size_t length, napi_value* result)
{
    if (str == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // see napi_create_string_utf8's identical guard (NapiFunctions.cpp) -
    // same test_string/test.js TestLargeUtf16 rationale.
    if (length != NAPI_AUTO_LENGTH && length > static_cast<size_t>(INT_MAX)) {
        return SetLastError(env, napi_invalid_arg);
    }

    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? std::char_traits<char16_t>::length(str) : length;
    // Transparent compressible-string routing - see napi_create_string_utf8's
    // identical comment (NapiFunctions.cpp). Below kCompressibleStringThreshold,
    // the plain path is kept so small strings (every existing test string)
    // are unaffected.
    if (stringLength >= kCompressibleStringThreshold && StringRef::isCompressibleStringEnabled() && env != nullptr && env->napiEnv->vmInstance() != nullptr) {
        *result = ToNapi(StringRef::createFromUTF16ToCompressibleString(env->napiEnv->vmInstance(), str, stringLength));
    } else {
        *result = ToNapi(StringRef::createFromUTF16(str, stringLength));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_bool(napi_env env, napi_value value, bool* result)
{
    // env/value/result are all checked - and *before* any type inspection of
    // `value` - matching real Node-API's own CHECK_ARG ordering: a null
    // `value` (a null napi_value pointer, not the JS `null`) must report
    // napi_invalid_arg, not napi_boolean_expected (which napi_get_value_bool
    // previously fell into "by accident", relying on FromNapi(nullptr)-
    // >isBoolean() happening not to crash rather than ever explicitly
    // checking) - found via test_conversions/test.js's testNull.getValueBool
    // (test_conversions/test_null.c's GEN_NULL_CHECK_BINDING).
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isBoolean()) {
        return SetLastError(env, napi_boolean_expected);
    }
    *result = v->asBoolean();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_int32(napi_env env, napi_value value, int32_t* result)
{
    // see napi_get_value_bool's identical env/value/result-first reasoning.
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isNumber()) {
        return SetLastError(env, napi_number_expected);
    }
    *result = v->toInt32(env->executionState);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_int64(napi_env env, napi_value value, int64_t* result)
{
    // see napi_get_value_bool's identical env/value/result-first reasoning.
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isNumber()) {
        return SetLastError(env, napi_number_expected);
    }

    double doubleValue = v->asNumber();
    if (!std::isfinite(doubleValue)) {
        // matches Node's own napi_get_value_int64: NaN/+-Infinity map to 0,
        // rather than to the ToInteger-then-clamp result those would
        // otherwise produce
        *result = 0;
        return napi_ok;
    }

    // ToInteger on an already-numeric value never runs user code / throws,
    // so this can be called directly without Evaluator::execute wrapping
    double truncated = v->toInteger(env->executionState);
    if (truncated >= 9223372036854775808.0) { // 2^63, first double >= INT64_MAX+1
        *result = INT64_MAX;
    } else if (truncated < -9223372036854775808.0) { // -2^63 == INT64_MIN exactly
        *result = INT64_MIN;
    } else {
        *result = static_cast<int64_t>(truncated);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_string_utf8(napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result)
{
    // env/value checked (and *before* the isString() type check) - see
    // napi_get_value_bool's identical reasoning (above). buf==nullptr is
    // legitimately optional (query-length mode) - but only if `result` is
    // then non-null to report that length back through; both null at once
    // is otherwise unreportable, hence invalid_arg (found via the same
    // test_null.c GEN_NULL_CHECK_STRING_BINDING macro's
    // "bufAndOutLengthIsNull" case).
    if (env == nullptr || value == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    ValueRef* v = FromNapi(value);
    if (!v->isString()) {
        return SetLastError(env, napi_string_expected);
    }
    std::string utf8 = v->asString()->toStdUTF8String();

    if (buf == nullptr) {
        if (result == nullptr) {
            return SetLastError(env, napi_invalid_arg);
        }
        *result = utf8.size();
        return napi_ok;
    }

    if (bufsize == 0) {
        if (result != nullptr) {
            *result = 0;
        }
        return napi_ok;
    }

    size_t copied = Utf8PrefixByteLengthWithinBudget(utf8, bufsize - 1);
    memcpy(buf, utf8.data(), copied);
    buf[copied] = '\0';
    if (result != nullptr) {
        *result = copied;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_string_latin1(napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result)
{
    // see napi_get_value_string_utf8's identical reasoning (above).
    if (env == nullptr || value == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    ValueRef* v = FromNapi(value);
    if (!v->isString()) {
        return SetLastError(env, napi_string_expected);
    }
    StringRef* str = v->asString();
    size_t length = str->length();

    if (buf == nullptr) {
        if (result == nullptr) {
            return SetLastError(env, napi_invalid_arg);
        }
        *result = length;
        return napi_ok;
    }

    if (bufsize == 0) {
        if (result != nullptr) {
            *result = 0;
        }
        return napi_ok;
    }

    size_t copied = std::min(length, bufsize - 1);
    for (size_t i = 0; i < copied; i++) {
        buf[i] = static_cast<char>(str->charAt(i) & 0xFF);
    }
    buf[copied] = '\0';
    if (result != nullptr) {
        *result = copied;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_string_utf16(napi_env env, napi_value value, char16_t* buf, size_t bufsize, size_t* result)
{
    // see napi_get_value_string_utf8's identical reasoning (above).
    if (env == nullptr || value == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    ValueRef* v = FromNapi(value);
    if (!v->isString()) {
        return SetLastError(env, napi_string_expected);
    }
    StringRef* str = v->asString();
    size_t length = str->length();

    if (buf == nullptr) {
        if (result == nullptr) {
            return SetLastError(env, napi_invalid_arg);
        }
        *result = length;
        return napi_ok;
    }

    if (bufsize == 0) {
        if (result != nullptr) {
            *result = 0;
        }
        return napi_ok;
    }

    size_t copied = std::min(length, bufsize - 1);
    for (size_t i = 0; i < copied; i++) {
        buf[i] = str->charAt(i);
    }
    buf[copied] = u'\0';
    if (result != nullptr) {
        *result = copied;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_coerce_to_bool(napi_env env, napi_value value, napi_value* result)
{
    // see napi_get_value_bool's identical env/value/result-first reasoning
    // (above) - found via the same test_conversions/test_null.c
    // GEN_NULL_CHECK_BINDING macro (CoerceToBool).
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // clear a stale error code left over from an *earlier*, unrelated failed
    // call on this same env (SetLastError only ever runs on an error return,
    // never a success one, so without this a still-succeeding call right
    // after a failed one would otherwise have napi_get_last_error_info keep
    // reporting that earlier failure's message - found via this same test's
    // "inputTypeCheck" case, which deliberately runs right after a
    // resultIsNull-triggered napi_invalid_arg and expects to see a clean
    // napi_ok afterward).
    SetLastError(env, napi_ok);

    // ToBoolean never runs user code / never throws, unlike its
    // to_number/to_object/to_string siblings below, so this can be called
    // directly without Evaluator::execute wrapping
    *result = ToNapi(ValueRef::create(FromNapi(value)->toBoolean(env->executionState)));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_coerce_to_number(napi_env env, napi_value value, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    ValueRef* v = FromNapi(value);

    // ToNumber can invoke a user-defined valueOf/Symbol.toPrimitive/toString,
    // or throw (e.g. for a Symbol) - must not let that C++ exception cross
    // this function's own stack frame (see napi_call_function above)
    Evaluator::EvaluatorResult coerceResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* v) -> ValueRef* {
            return ValueRef::create(v->toNumber(state));
        },
        v);

    if (!coerceResult.isSuccessful()) {
        env->pendingException = coerceResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(coerceResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_coerce_to_object(napi_env env, napi_value value, napi_value* result)
{
    // see napi_coerce_to_bool's identical reasoning (above), including the
    // stale-last-error clear.
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    SetLastError(env, napi_ok);

    ExecutionStateRef* state = env->executionState;
    ValueRef* v = FromNapi(value);

    // ToObject throws for null/undefined, so this needs the same
    // exception-catching wrapper as the other coercions here even though it
    // never runs arbitrary user code itself
    Evaluator::EvaluatorResult coerceResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* v) -> ValueRef* {
            return v->toObject(state);
        },
        v);

    if (!coerceResult.isSuccessful()) {
        env->pendingException = coerceResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(coerceResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_coerce_to_string(napi_env env, napi_value value, napi_value* result)
{
    // see napi_coerce_to_bool's identical reasoning (above), including the
    // stale-last-error clear.
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    SetLastError(env, napi_ok);

    ExecutionStateRef* state = env->executionState;
    ValueRef* v = FromNapi(value);

    // ToString can invoke a user-defined toString/Symbol.toPrimitive, or
    // throw (e.g. for a Symbol) - same reasoning as napi_coerce_to_number
    Evaluator::EvaluatorResult coerceResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* v) -> ValueRef* {
            return v->toString(state);
        },
        v);

    if (!coerceResult.isSuccessful()) {
        env->pendingException = coerceResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(coerceResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_strict_equals(napi_env env, napi_value lhs, napi_value rhs, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // === never runs user code / never throws, so this can be called
    // directly without Evaluator::execute wrapping
    *result = FromNapi(lhs)->equalsTo(env->executionState, FromNapi(rhs));
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
