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

// Exercises NapiSymbolBigInt.cpp (napi_create_symbol + the BigInt slice of
// js_native_api.h) directly, with no addon .so involved - every call happens
// inside a single Evaluator::execute callback (same "own call frame"
// discipline as testnapi.cpp's dlopen()-free tests, e.g. Napi.ReferenceRefUnref).

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstring>

TEST(Napi, Symbol)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            napi_value description = ToNapi(StringRef::createFromASCII("mySymbol"));
            napi_value symbolValue = nullptr;
            if (napi_create_symbol(env, description, &symbolValue) != napi_ok) {
                return ValueRef::create(false);
            }

            ValueRef* symbol = FromNapi(symbolValue);
            if (!symbol->isSymbol()) {
                return ValueRef::create(false);
            }

            napi_valuetype type;
            napi_typeof(env, symbolValue, &type);
            if (type != napi_symbol) {
                return ValueRef::create(false);
            }

            bool descOk = symbol->asSymbol()->descriptionString()->equalsWithASCIIString("mySymbol", strlen("mySymbol"));

            // a symbol created with no description (napi_value == nullptr) is
            // still a valid, independent symbol value
            napi_value noDescSymbolValue = nullptr;
            bool noDescOk = napi_create_symbol(env, nullptr, &noDescSymbolValue) == napi_ok
                && FromNapi(noDescSymbolValue)->isSymbol()
                // every napi_create_symbol call mints a fresh, distinct symbol
                // (like `Symbol()` in JS), so its GC pointer must differ from
                // the first one's
                && FromNapi(noDescSymbolValue) != symbol;

            return ValueRef::create(descOk && noDescOk);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, BigIntInt64RoundTrip)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            napi_value bigintValue = nullptr;
            if (napi_create_bigint_int64(env, -123456789012345LL, &bigintValue) != napi_ok) {
                return ValueRef::create(false);
            }

            napi_valuetype type;
            napi_typeof(env, bigintValue, &type);
            if (type != napi_bigint) {
                return ValueRef::create(false);
            }

            int64_t out = 0;
            bool lossless = false;
            if (napi_get_value_bigint_int64(env, bigintValue, &out, &lossless) != napi_ok) {
                return ValueRef::create(false);
            }

            return ValueRef::create(out == -123456789012345LL && lossless);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, BigIntUint64RoundTrip)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            const uint64_t kValue = 18446744073709551615ULL; // UINT64_MAX
            napi_value bigintValue = nullptr;
            if (napi_create_bigint_uint64(env, kValue, &bigintValue) != napi_ok) {
                return ValueRef::create(false);
            }

            napi_valuetype type;
            napi_typeof(env, bigintValue, &type);
            if (type != napi_bigint) {
                return ValueRef::create(false);
            }

            uint64_t out = 0;
            bool lossless = false;
            if (napi_get_value_bigint_uint64(env, bigintValue, &out, &lossless) != napi_ok) {
                return ValueRef::create(false);
            }
            bool uint64Ok = (out == kValue) && lossless;

            // the same value read back through the *signed* accessor cannot
            // be represented losslessly (it does not fit in an int64_t)
            int64_t signedOut = 0;
            bool signedLossless = true;
            napi_get_value_bigint_int64(env, bigintValue, &signedOut, &signedLossless);
            bool signedLossyOk = !signedLossless;

            return ValueRef::create(uint64Ok && signedLossyOk);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

TEST(Napi, BigIntCreateWordsSingleWordRoundTrip)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            uint64_t words[1] = { 0xDEADBEEFCAFEBABEULL };
            napi_value bigintValue = nullptr;
            if (napi_create_bigint_words(env, /* sign_bit */ 0, /* word_count */ 1, words, &bigintValue) != napi_ok) {
                return ValueRef::create(false);
            }

            napi_valuetype type;
            napi_typeof(env, bigintValue, &type);
            if (type != napi_bigint) {
                return ValueRef::create(false);
            }

            // round trip through napi_get_value_bigint_uint64
            uint64_t asUint64 = 0;
            bool lossless = false;
            napi_get_value_bigint_uint64(env, bigintValue, &asUint64, &lossless);
            bool uint64Ok = (asUint64 == words[0]) && lossless;

            // size-query mode: sign_bit == nullptr && words == nullptr
            size_t neededWordCount = 0;
            if (napi_get_value_bigint_words(env, bigintValue, nullptr, &neededWordCount, nullptr) != napi_ok || neededWordCount != 1) {
                return ValueRef::create(false);
            }

            // full decomposition
            int signBit = -1;
            size_t wordCount = 4;
            uint64_t outWords[4] = { 1, 1, 1, 1 };
            napi_get_value_bigint_words(env, bigintValue, &signBit, &wordCount, outWords);
            bool wordsOk = (signBit == 0) && (wordCount == 1) && (outWords[0] == words[0]);

            // negative single-word value: create then decompose then recreate
            uint64_t negWords[1] = { 42 };
            napi_value negBigintValue = nullptr;
            napi_create_bigint_words(env, /* sign_bit */ 1, 1, negWords, &negBigintValue);

            int64_t negAsInt64 = 0;
            bool negLossless = false;
            napi_get_value_bigint_int64(env, negBigintValue, &negAsInt64, &negLossless);
            bool negCreateOk = (negAsInt64 == -42) && negLossless;

            int negSignBit = -1;
            size_t negWordCount = 4;
            uint64_t negOutWords[4] = { 1, 1, 1, 1 };
            napi_get_value_bigint_words(env, negBigintValue, &negSignBit, &negWordCount, negOutWords);
            bool negDecomposeOk = (negSignBit == 1) && (negWordCount == 1) && (negOutWords[0] == 42ULL);

            return ValueRef::create(uint64Ok && wordsOk && negCreateOk && negDecomposeOk);
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
    EXPECT_TRUE(result.result->asBoolean());
}

#endif // ENABLE_NAPI
