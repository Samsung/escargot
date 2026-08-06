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

// Implements the napi_create_symbol and BigInt (js_native_api.h) slice of
// N-API: napi_create_bigint_int64/uint64/words and
// napi_get_value_bigint_int64/uint64/words.

#include "NapiTypes.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace Escargot {
namespace Napi {

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_create_symbol(napi_env env, napi_value description, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    OptionalRef<StringRef> desc;
    if (description != nullptr) {
        ValueRef* descValue = FromNapi(description);
        if (!descValue->isString()) {
            return SetLastError(env, napi_string_expected);
        }
        desc = descValue->asString();
    }

    *result = ToNapi(SymbolRef::create(desc));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_bigint_int64(napi_env env, int64_t value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(BigIntRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_bigint_uint64(napi_env env, uint64_t value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(BigIntRef::create(value));
    return napi_ok;
}

// EscargotPublic.h has no words-based BigInt factory/decomposer, so both this
// function and napi_get_value_bigint_words below round-trip through a hex
// string instead: BigIntRef::create(StringRef*, 16) already parses an
// arbitrary-precision hex literal (see BigIntData::init in BigInt.cpp) into
// exactly the magnitude a little-endian base-2^64 words[] array encodes, for
// any word_count - the sign (js_native_api.h's separate sign_bit) is then
// applied on top via BigIntRef::negativeValue, see the comment below.
ESCARGOT_NAPI_EXPORT napi_status napi_create_bigint_words(napi_env env, int sign_bit, size_t word_count, const uint64_t* words, napi_value* result)
{
    if (word_count == 0) {
        *result = ToNapi(BigIntRef::create(static_cast<int64_t>(0)));
        return napi_ok;
    }

    // A word_count so large it would overflow this function's own hex-buffer
    // size_t bookkeeping below (e.g. SIZE_MAX, as in test_bigint/test.js's
    // CreateTooBigBigInt) can't even be attempted - reject outright as
    // napi_invalid_arg (matching real Node-API, which also rejects a
    // word_count that implausible before ever reaching V8's BigInt
    // allocator).
    static const size_t kOverflowGuardWords = (SIZE_MAX - 16) / 16;
    if (word_count > kOverflowGuardWords) {
        return SetLastError(env, napi_invalid_arg);
    }

    // A merely very-large-but-computable word_count (e.g. INT_MAX, as in
    // that same test's MakeBigIntWordsThrow) is instead the kind that reaches
    // real BigInt construction in Node-API and fails there with a
    // RangeError - Escargot's BigInt has no exposed max-length query to check
    // against directly here, so kMaxBigIntWords stands in for it (chosen
    // well below any word_count this implementation could plausibly build a
    // hex string for) and this reports the same RangeError/message real
    // Node-API does for hitting its own limit, as a pending exception rather
    // than a bare napi_invalid_arg status.
    static const size_t kMaxBigIntWords = 1 << 20;
    if (word_count > kMaxBigIntWords) {
        env->pendingException = ErrorObjectRef::create(env->executionState, ErrorObjectRef::RangeError, StringRef::createFromASCII("Maximum BigInt size exceeded"));
        return SetLastError(env, napi_pending_exception);
    }

    // Build a "0x"-prefixed, *unsigned* magnitude string, then apply the sign
    // afterwards via BigIntRef::negativeValue instead of folding it in here.
    // Two independent guards in BigIntData::init (BigInt.cpp), both meant for
    // plain (non-N-API) BigInt literal parsing, would otherwise misfire on a
    // bare (no "0x") hex digit string like the one this function would
    // naturally produce:
    //  - any 'e'/'E' digit is rejected unless the string starts with "0x"
    //    (guards against decimal-exponent ambiguity, but 'e' is also a valid
    //    hex digit);
    //  - a leading '-' combined with any 'b'/'o'/'x' character anywhere in
    //    the string is rejected outright, and 'b' is itself a valid hex
    //    digit.
    // Always prefixing with "0x" satisfies the first guard and (since the
    // sign is applied separately, never producing a leading '-' here at all)
    // sidesteps the second entirely.
    std::string hex = "0x";
    hex.reserve(hex.size() + word_count * 16);

    char buf[24];
    for (size_t i = word_count; i > 0; i--) {
        size_t idx = i - 1;
        // the most significant word is written without zero-padding (still
        // correct - and required - if it happens to be 0, i.e. leading
        // zeros); every other word is padded to its full 16 hex digits so
        // its bit position within the concatenated string stays correct
        const char* format = (idx == word_count - 1) ? "%llx" : "%016llx";
        snprintf(buf, sizeof(buf), format, static_cast<unsigned long long>(words[idx]));
        hex += buf;
    }

    StringRef* hexString = StringRef::createFromASCII(hex.data(), hex.length());
    BigIntRef* magnitude = BigIntRef::create(hexString, 16);
    *result = ToNapi((sign_bit != 0) ? magnitude->negativeValue(env->executionState) : magnitude);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_bigint_int64(napi_env env, napi_value value, int64_t* result, bool* lossless)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isBigInt()) {
        return SetLastError(env, napi_bigint_expected);
    }

    BigIntRef* bigint = v->asBigInt();
    int64_t converted = bigint->toInt64();
    *result = converted;

    if (lossless != nullptr) {
        // toInt64() reduces modulo 2^64 (BF_GET_INT_MOD), so it never fails
        // outright - losslessness instead means "converting `converted` back
        // to a BigInt reproduces the original value"
        *lossless = bigint->equals(BigIntRef::create(converted));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_bigint_uint64(napi_env env, napi_value value, uint64_t* result, bool* lossless)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isBigInt()) {
        return SetLastError(env, napi_bigint_expected);
    }

    BigIntRef* bigint = v->asBigInt();
    uint64_t converted = bigint->toUint64();
    *result = converted;

    if (lossless != nullptr) {
        *lossless = bigint->equals(BigIntRef::create(converted));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_bigint_words(napi_env env, napi_value value, int* sign_bit, size_t* word_count, uint64_t* words)
{
    ValueRef* v = FromNapi(value);
    if (!v->isBigInt()) {
        return SetLastError(env, napi_bigint_expected);
    }
    if (word_count == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // sign_bit == nullptr && words == nullptr is the "how many words do you
    // need" query mode js_native_api.h documents for this function; otherwise
    // both must be provided, matching Node's own V8 implementation
    bool isSizeQuery = (sign_bit == nullptr && words == nullptr);
    if (!isSizeQuery && (sign_bit == nullptr || words == nullptr)) {
        return SetLastError(env, napi_invalid_arg);
    }

    BigIntRef* bigint = v->asBigInt();
    std::string hexString = bigint->toString(16)->toStdUTF8String();

    bool negative = false;
    size_t start = 0;
    if (!hexString.empty() && hexString[0] == '-') {
        negative = true;
        start = 1;
    }
    std::string digits = hexString.substr(start);
    // BigInt(0)'s toString(16) always yields "0" here (no '-' prefix, see
    // BigInt::toString's zero-sign normalization), so this alone identifies
    // the zero-word case
    bool isZero = (digits == "0");
    size_t neededWordCount = isZero ? 0 : ((digits.length() + 15) / 16);

    if (isSizeQuery) {
        *word_count = neededWordCount;
        return napi_ok;
    }

    size_t capacity = *word_count;
    size_t wordsToWrite = std::min(capacity, neededWordCount);

    // fill little-endian: words[0] is the least-significant 64 bits, i.e. the
    // rightmost (up to) 16 hex digits of `digits`
    for (size_t i = 0; i < wordsToWrite; i++) {
        size_t end = digits.length() - i * 16;
        size_t begin = (end >= 16) ? end - 16 : 0;
        std::string chunk = digits.substr(begin, end - begin);
        words[i] = static_cast<uint64_t>(strtoull(chunk.c_str(), nullptr, 16));
    }

    *sign_bit = negative ? 1 : 0;
    *word_count = wordsToWrite;
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
