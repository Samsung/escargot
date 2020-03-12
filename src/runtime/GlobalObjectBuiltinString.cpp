/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "StringObject.h"
#include "ErrorObject.h"
#include "RegExpObject.h"
#include "ArrayObject.h"
#include "NativeFunctionObject.h"
#include "parser/Lexer.h"
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#include "IntlCollator.h"
#endif

namespace Escargot {

static Value builtinStringConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    String* s = String::emptyString;
    if (argc > 0) {
        Value value = argv[0];
        if (newTarget.isUndefined() && value.isSymbol()) {
            return value.asSymbol()->symbolDescriptiveString();
        } else {
            s = value.toString(state);
        }
    }
    if (newTarget.isUndefined()) {
        return s;
    }

    // StringCreate(s, ? GetPrototypeFromConstructor(NewTarget, "%StringPrototype%")).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.asObject(), state.context()->globalObject()->stringPrototype());
    StringObject* resultString = new StringObject(state);
    resultString->setPrototype(state, proto);
    resultString->setPrimitiveValue(state, s);

    return resultString;
}

static Value builtinStringToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (thisValue.isObject() && thisValue.asObject()->isStringObject()) {
        return thisValue.asObject()->asStringObject()->primitiveValue();
    }

    if (thisValue.isString())
        return thisValue.toString(state);

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().String.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotString);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
    String* searchStr = argv[0].toString(state);

    Value val;
    if (argc > 1) {
        val = argv[1];
    }
    double pos;
    if (val.isUndefined()) {
        pos = 0;
    } else {
        pos = val.toInteger(state);
    }

    size_t len = str->length();
    size_t start = std::min((size_t)std::max(pos, (double)0), len);
    size_t result = str->find(searchStr, start);
    if (result == SIZE_MAX)
        return Value(-1);
    else
        return Value(result);
}

static Value builtinStringLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let S be ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, lastIndexOf);
    String* searchStr = argv[0].toString(state);

    double numPos;
    if (argc > 1) {
        numPos = argv[1].toNumber(state);
    } else {
        numPos = Value().toNumber(state);
    }

    double pos;
    // If numPos is NaN, let pos be +∞; otherwise, let pos be ToInteger(numPos).
    if (std::isnan(numPos))
        pos = std::numeric_limits<double>::infinity();
    else
        pos = numPos;

    double len = S->length();
    double start = std::min(std::max(pos, 0.0), len);
    size_t result = S->rfind(searchStr, start);
    if (result == SIZE_MAX) {
        return Value(-1);
    }
    return Value(result);
}

static Value builtinStringLocaleCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(S, String, localeCompare);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    String* That = argv[0].toString(state);
    Value locales, options;

    if (argc >= 2) {
        locales = argv[1];
    }
    if (argc >= 3) {
        options = argv[2];
    }

    Object* collator = IntlCollator::create(state, locales, options);

    return Value(IntlCollator::compare(state, collator, S, That));
#else
    String* That = argv[0].toString(state);
    return Value(stringCompare(*S, *That));
#endif
}

static Value builtinStringSubstring(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, substring);
    if (argc == 0) {
        return str;
    } else {
        int len = str->length();
        double doubleStart = argv[0].toNumber(state);
        Value end = argv[1];
        double doubleEnd = (argc < 2 || end.isUndefined()) ? len : end.toNumber(state);
        doubleStart = (std::isnan(doubleStart)) ? 0 : doubleStart;
        doubleEnd = (std::isnan(doubleEnd)) ? 0 : doubleEnd;

        double finalStart = (int)trunc(std::min(std::max(doubleStart, 0.0), (double)len));
        double finalEnd = (int)trunc(std::min(std::max(doubleEnd, 0.0), (double)len));
        size_t from = std::min(finalStart, finalEnd);
        size_t to = std::max(finalStart, finalEnd);
        ASSERT(from <= to);
        if (to - from == 1) {
            char16_t c = str->charAt(from);
            if (c < ESCARGOT_ASCII_TABLE_MAX) {
                return state.context()->staticStrings().asciiTable[c].string();
            }
        }
        return str->substring(from, to);
    }
}

static Value builtinStringMatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (thisValue.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().String.string(), true, state.context()->staticStrings().match.string(), "%s: called on undefined or null");
    }

    Value argument = argv[0];
    RegExpObject* rx;
    Value matcher;

    if (!argument.isUndefinedOrNull()) {
        Value matcher = argument.toObject(state)->getMethod(state, argument, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match));
        if (!matcher.isUndefined()) {
            return Object::call(state, matcher, argument, 1, &thisValue);
        }
    }

    String* S = thisValue.toString(state);

    rx = new RegExpObject(state, argument.isUndefined() ? String::emptyString : argument.toString(state), String::emptyString);
    Value match = rx->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match)).value(state, rx);
    Value parameter[1] = { Value(S) };
    return Object::call(state, match, rx, 1, parameter);
}

#if defined(ENABLE_ICU)
static Value builtinStringNormalize(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    enum NormalizationForm {
        NFC,
        NFD,
        NFKC,
        NFKD
    };

    RESOLVE_THIS_BINDING_TO_STRING(str, String, normalize);
    Value argument = Value();
    if (argc > 0) {
        argument = argv[0];
    }
    NormalizationForm form = NFC;
    if (LIKELY(!argument.isUndefined())) {
        String* formString = argument.toString(state);
        if (formString->equals("NFC")) {
            form = NFC;
        } else if (formString->equals("NFD")) {
            form = NFD;
        } else if (formString->equals("NFKC")) {
            form = NFKC;
        } else if (formString->equals("NFKD")) {
            form = NFKD;
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid normalization form");
            return Value();
        }
    }
    if (str->length() == 0) {
        return Value(str);
    }

    auto utf16Str = str->toUTF16StringData();
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = nullptr;
    switch (form) {
    case NFC:
        normalizer = unorm2_getNFCInstance(&status);
        break;
    case NFD:
        normalizer = unorm2_getNFDInstance(&status);
        break;
    case NFKC:
        normalizer = unorm2_getNFKCInstance(&status);
        break;
    case NFKD:
        normalizer = unorm2_getNFKDInstance(&status);
        break;
    default:
        break;
    }
    if (!normalizer || U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "normalization fails");
        return Value();
    }
    int32_t normalizedStringLength = unorm2_normalize(normalizer, (const UChar*)utf16Str.data(), utf16Str.length(), nullptr, 0, &status);

    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        // when normalize fails.
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "normalization fails");
        return Value();
    }
    UTF16StringData ret;
    ret.resizeWithUninitializedValues(normalizedStringLength);
    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, (const UChar*)utf16Str.data(), utf16Str.length(), (UChar*)ret.data(), normalizedStringLength, &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "normalization fails");
        return Value();
    }
    return new UTF16String(std::move(ret));
}
#endif // ENABLE_ICU

static Value builtinStringRepeat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, repeat);
    Value argument = argv[0];
    int32_t repeatCount;
    double count = argument.toInteger(state);
    double newStringLength = str->length() * count;
    if (count < 0 || count == std::numeric_limits<double>::infinity() || newStringLength > STRING_MAXIMUM_LENGTH) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid count number of String repeat method");
    }

    if (newStringLength == 0) {
        return String::emptyString;
    }

    repeatCount = static_cast<int32_t>(count);

    StringBuilder builder;
    for (int i = 0; i < repeatCount; i++) {
        builder.appendString(str);
    }
    return builder.finalize();
}

static Value builtinStringReplace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    ASSERT(argc == 0 || argv != nullptr);
    if (thisValue.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }

    Value searchValue = argv[0];
    Value replaceValue = argv[1];
    bool isReplaceRegExp = searchValue.isPointerValue() && searchValue.asPointerValue()->isRegExpObject();

    if (!searchValue.isUndefinedOrNull()) {
        Value replacer = Object::getMethod(state, searchValue, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().replace));
        if (!replacer.isUndefined()) {
            Value args[2] = { thisValue, replaceValue };
            return Object::call(state, replacer, searchValue, 2, args);
        }
    }
    String* string = thisValue.toString(state);
    String* searchString = searchValue.toString(state);
    bool functionalReplace = replaceValue.isCallable();
    if (!functionalReplace) {
        replaceValue = replaceValue.toString(state);
    }
    size_t pos = string->find(searchString, 0);
    String* matched = searchString;

    //If no occurrences of searchString were found, return string.
    if (pos == SIZE_MAX) {
        return Value(string);
    }
    String* replStr = String::emptyString;
    if (functionalReplace) {
        Value parameters[3] = { Value(matched), Value(pos), Value(string) };
        Value replValue = Object::call(state, replaceValue, Value(), 3, parameters);
        replStr = replValue.toString(state);
    } else {
        StringVector captures;
        replStr = replStr->getSubstitution(state, matched, string, pos, captures, Value(), replaceValue.toString(state));
    }
    size_t tailpos = pos + matched->length();
    StringBuilder builder;
    builder.appendSubString(string, 0, pos);
    builder.appendSubString(replStr, 0, replStr->length());
    builder.appendSubString(string, tailpos, string->length());
    String* newString = builder.finalize(&state);
    return Value(newString);
}

static Value builtinStringSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Call CheckObjectCoercible passing the this value as its argument.
    // Let string be the result of calling ToString, giving it the this value as its argument.
    RESOLVE_THIS_BINDING_TO_STRING(string, String, search);
    Value regexp = argv[0];
    RegExpObject* rx;

    //http://www.ecma-international.org/ecma-262/6.0/#sec-string.prototype.search
    RESOLVE_THIS_BINDING_TO_OBJECT(obj, Object, search);
    if (!(regexp.isUndefinedOrNull())) {
        Value searcher = obj->getMethod(state, regexp, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().search));

        if (!searcher.isUndefined()) {
            Value parameter[1] = { obj };
            return Object::call(state, searcher, regexp, 1, parameter);
        }
    }
    rx = new RegExpObject(state, regexp.isUndefined() ? String::emptyString : regexp.toString(state), String::emptyString);
    Value func = rx->getMethod(state, rx, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().search));
    Value parameter[1] = { Value(string) };
    return Object::call(state, func, rx, 1, parameter);
}

static Value builtinStringSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(obj, Object, split);
    Value separator = argv[0];
    bool isSeparatorRegExp = separator.isPointerValue() && separator.asPointerValue()->isRegExpObject();
    Value limit = argv[1];
    uint64_t lim;
    // If separator is neither undefined nor null, then
    if (!separator.isUndefinedOrNull()) {
        // Let splitter be GetMethod(separator, @@split).
        Value splitter = Object::getMethod(state, separator, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().split));

        // --- Optmize path
        // if splitter is builtin RegExp.prototype.split and separator is RegExpObject
        // we can use old method(ES5) below
        if (isSeparatorRegExp && splitter.isPointerValue() && splitter.asPointerValue() == state.context()->globalObject()->regexpSplitMethod()) {
            // use old method(ES5)
        } else {
            // If splitter is not undefined, then
            if (!splitter.isUndefined()) {
                // Return Call(splitter, separator, <<O, limit>>).
                Value params[2] = { obj, limit };
                return Object::call(state, splitter, separator, 2, params);
            }
        }
    }
    // If limit is undefined, let lim = 2^53 - 1, else let lim = ToLength(limit).
    // NOTE: not using toLength() here since it would return 0 for negative values, which results in incorrect behaviour.
    lim = limit.isUndefined() ? (1ULL << 53) - 1 : limit.toUint32(state);
    // Let R be ToString(separator).
    PointerValue* P;
    if (isSeparatorRegExp) {
        P = separator.asPointerValue()->asRegExpObject();
    } else {
        P = separator.toString(state);
    }

    RESOLVE_THIS_BINDING_TO_STRING(S, String, split);
    ArrayObject* A = new ArrayObject(state);
    Value searcher;

    // Let lengthA = 0.
    size_t lengthA = 0;
    // Let S be the number of elements in S.
    // Let p = 0.
    size_t s = S->length(), p = 0;

    if (lim == 0)
        return A;

    if (separator.isUndefined()) {
        A->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(S, ObjectPropertyDescriptor::AllPresent));
        return A;
    }

    std::function<Value(String*, int, String*)> splitMatchUsingStr;
    splitMatchUsingStr = [](String* S, int q, String* R) -> Value {
        int s = S->length();
        int r = R->length();
        if (q + r > s)
            return Value(false);
        for (int i = 0; i < r; i++)
            if (S->charAt(q + i) != R->charAt(i))
                return Value(false);
        return Value(q + r);
    };
    if (s == 0) {
        bool ret = true;
        if (P->isRegExpObject()) {
            RegexMatchResult result;
            ret = P->asRegExpObject()->matchNonGlobally(state, S, result, false, 0);
        } else {
            Value z = splitMatchUsingStr(S, 0, P->asString());
            if (z.isBoolean()) {
                ret = z.asBoolean();
            }
        }
        if (ret)
            return A;
        A->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(S, ObjectPropertyDescriptor::AllPresent));
        return A;
    }

    size_t q = p;

    // 13
    if (P->isRegExpObject()) {
        RegExpObject* R = P->asRegExpObject();
        while (q != s) {
            RegexMatchResult result;
            bool ret = R->matchNonGlobally(state, S, result, false, (size_t)q);
            if (!ret) {
                break;
            }

            if ((size_t)result.m_matchResults[0][0].m_end == p) {
                q++;
            } else {
                if (result.m_matchResults[0][0].m_start >= S->length())
                    break;

                String* T = S->substring(p, result.m_matchResults[0][0].m_start);
                A->defineOwnProperty(state, ObjectPropertyName(state, Value(lengthA++)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
                if (lengthA == lim)
                    return A;
                p = result.m_matchResults[0][0].m_end;
                R->pushBackToRegExpMatchedArray(state, A, lengthA, lim, result, S);
                if (lengthA == lim)
                    return A;
                q = p;
            }
        }
    } else {
        String* R = P->asString();
        while (q != s) {
            Value e = splitMatchUsingStr(S, q, R);
            if (e == Value(false))
                q++;
            else {
                if ((size_t)e.asInt32() == p)
                    q++;
                else {
                    if (q >= S->length())
                        break;

                    String* T = S->substring(p, q);
                    A->defineOwnProperty(state, ObjectPropertyName(state, Value(lengthA++)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
                    if (lengthA == lim)
                        return A;
                    p = e.asInt32();
                    q = p;
                }
            }
        }
    }

    String* T = S->substring(p, s);
    A->defineOwnProperty(state, ObjectPropertyName(state, Value(lengthA)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
    return A;
}

static Value builtinStringCharCodeAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, charCodeAt);
    int position = argv[0].toInteger(state);
    Value ret;
    const auto& data = str->bufferAccessData();
    if (position < 0 || position >= (int)data.length)
        ret = Value(std::numeric_limits<double>::quiet_NaN());
    else {
        char16_t c;
        if (data.has8BitContent) {
            c = ((LChar*)data.buffer)[position];
        } else {
            c = ((char16_t*)data.buffer)[position];
        }
        ret = Value(c);
    }
    return ret;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-string.prototype.codepointat
static Value builtinStringCodePointAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, codePointAt);
    int position = argv[0].toInteger(state);
    Value ret;
    const auto& data = str->bufferAccessData();
    const int size = (int)data.length;
    if (position < 0 || position >= size)
        return Value();

    char16_t first;
    if (data.has8BitContent) {
        first = ((LChar*)data.buffer)[position];
    } else {
        first = ((char16_t*)data.buffer)[position];
    }

    if (first < 0xD800 || first > 0xDBFF || (position + 1) == size) {
        return Value(first);
    }

    char16_t second;
    if (data.has8BitContent) {
        second = ((LChar*)data.buffer)[position + 1];
    } else {
        second = ((char16_t*)data.buffer)[position + 1];
    }

    if (second < 0xDC00 || second > 0xDFFF) {
        return Value(first);
    }

    int cp = ((first - 0xD800) * 1024) + (second - 0xDC00) + 0x10000;
    return Value(cp);
}

static Value builtinStringCharAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, charAt);

    int64_t position;
    if (argc == 0) {
        position = 0;
    } else if (argc > 0) {
        position = argv[0].toInteger(state);
    } else {
        return Value(String::emptyString);
    }

    const auto& accessData = str->bufferAccessData();

    if (LIKELY(0 <= position && position < (int64_t)accessData.length)) {
        char16_t c;
        if (accessData.has8BitContent) {
            c = ((LChar*)accessData.buffer)[position];
        } else {
            c = ((char16_t*)accessData.buffer)[position];
        }
        if (LIKELY(c < ESCARGOT_ASCII_TABLE_MAX)) {
            return state.context()->staticStrings().asciiTable[c].string();
        } else {
            return String::fromCharCode(c);
        }
    } else {
        return String::emptyString;
    }
}

static Value builtinStringFromCharCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (argc == 1) {
        char16_t c = argv[0].toUint32(state) & 0xFFFF;
        if (c < ESCARGOT_ASCII_TABLE_MAX)
            return state.context()->staticStrings().asciiTable[c].string();
        return String::fromCharCode(c);
    } else {
        StringBuilder builder;
        for (size_t i = 0; i < argc; i++) {
            builder.appendChar((char16_t)argv[i].toInteger(state));
        }
        return builder.finalize(&state);
    }
    return Value();
}

static Value builtinStringFromCodePoint(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    StringBuilder builder;
    for (size_t nextIndex = 0; nextIndex < argc; nextIndex++) {
        Value next = argv[nextIndex];
        double nextCP = next.toNumber(state);
        double toIntegerNexCP = next.toInteger(state);

        if (nextCP != toIntegerNexCP || nextCP < 0 || nextCP > 0x10FFFF) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid code point");
        }

        uint32_t cp = (uint32_t)nextCP;

        if (cp <= 65535) {
            builder.appendChar((char16_t)cp);
        } else {
            char16_t cu1 = floor((cp - 65536) / 1024) + 0xD800;
            char16_t cu2 = ((cp - 65536) % 1024) + 0xDC00;

            builder.appendChar(cu1);
            builder.appendChar(cu2);
        }
    }

    return builder.finalize(&state);
}

static Value builtinStringConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, concat);
    for (size_t i = 0; i < argc; i++) {
        String* appendStr = argv[i].toString(state);
        str = RopeString::createRopeString(str, appendStr, &state);
    }
    return Value(str);
}

static Value builtinStringSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, slice);
    size_t len = str->length();
    double start = argv[0].toInteger(state);
    double end = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    int from = (start < 0) ? std::max(len + start, 0.0) : std::min(start, (double)len);
    int to = (end < 0) ? std::max(len + end, 0.0) : std::min(end, (double)len);
    int span = std::max(to - from, 0);
    return str->substring(from, from + span);
}

static Value builtinStringToLowerCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLowerCase);
    if (str->has8BitContent()) {
        Latin1StringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const LChar* buf = str->characters8();
        bool result = true;
        for (size_t i = 0; i < len; i++) {
#if defined(ENABLE_ICU)
            char32_t u2 = u_tolower(buf[i]);
#else
            char32_t u2 = tolower(buf[i]);
#endif
            if (UNLIKELY(u2 > 255)) {
                result = false;
                break;
            }
            newStr[i] = u2;
        }
        if (result)
            return new Latin1String(std::move(newStr));
    }

    size_t len = str->length();
    UTF16StringData newStr;
    if (str->has8BitContent()) {
        const LChar* buf = str->characters8();
        newStr.resizeWithUninitializedValues(len);
        for (size_t i = 0; i < len; i++) {
            newStr[i] = buf[i];
        }
    } else
        newStr = UTF16StringData(str->characters16(), len);
    char16_t* buf = newStr.data();
    for (size_t i = 0; i < len;) {
        char32_t c;
        size_t iBefore = i;
        U16_NEXT(buf, i, len, c);

#if defined(ENABLE_ICU)
        c = u_tolower(c);
#else
        c = tolower(c);
#endif
        if (c <= 0x10000) {
            char16_t c2 = (char16_t)c;
            buf[iBefore] = c2;
        } else {
            buf[iBefore] = (char16_t)(0xD800 + ((c - 0x10000) >> 10));
            buf[iBefore + 1] = (char16_t)(0xDC00 + ((c - 0x10000) & 1023));
        }
    }
    return new UTF16String(std::move(newStr));
}

static Value builtinStringToUpperCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toUpperCase);
    if (str->has8BitContent()) {
        Latin1StringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const LChar* buf = str->characters8();
        bool result = true;
        for (size_t i = 0; i < len; i++) {
#if defined(ENABLE_ICU)
            char32_t u2 = u_toupper(buf[i]);
#else
            char32_t u2 = toupper(buf[i]);
#endif
            if (UNLIKELY(u2 > 255)) {
                result = false;
                break;
            }
            newStr[i] = u2;
        }
        if (result)
            return new Latin1String(std::move(newStr));
    }

    size_t len = str->length();
    UTF16StringData newStr;
    if (str->has8BitContent()) {
        const LChar* buf = str->characters8();
        newStr.resizeWithUninitializedValues(len);
        for (size_t i = 0; i < len; i++) {
            newStr[i] = buf[i];
        }
    } else
        newStr = UTF16StringData(str->characters16(), len);
    char16_t* buf = newStr.data();
    for (size_t i = 0; i < len;) {
        char32_t c;
        size_t iBefore = i;
        U16_NEXT(buf, i, len, c);
#if defined(ENABLE_ICU)
        c = u_toupper(c);
#else
        c = toupper(c);
#endif
        if (c <= 0x10000) {
            char16_t c2 = (char16_t)c;
            buf[iBefore] = c2;
        } else {
            buf[iBefore] = (char16_t)(0xD800 + ((c - 0x10000) >> 10));
            buf[iBefore + 1] = (char16_t)(0xDC00 + ((c - 0x10000) & 1023));
        }
    }
    return new UTF16String(std::move(newStr));
}

static Value builtinStringToLocaleLowerCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLocaleLowerCase);
    return builtinStringToLowerCase(state, thisValue, argc, argv, newTarget);
}

static Value builtinStringToLocaleUpperCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLocaleUpperCase);
    return builtinStringToUpperCase(state, thisValue, argc, argv, newTarget);
}

enum StringTrimWhere : unsigned {
    START,
    END,
    STARTEND
};

static Value builtinStringTrimString(ExecutionState& state, Value thisValue, StringTrimWhere where)
{
    // Let str be ? RequireObjectCoercible(string).
    // Let S be ? ToString(str).
    RESOLVE_THIS_BINDING_TO_STRING(str, String, trim);

    int64_t stringLength = str->length();
    int64_t s = 0;
    int64_t e = stringLength - 1;

    // If where is "start", let T be the String value that is a copy of S with leading white space removed.
    // Trim beginning if start, or start+end
    if (where == START || where == STARTEND) {
        for (s = 0; s < stringLength; s++) {
            if (!EscargotLexer::isWhiteSpaceOrLineTerminator((*str)[s]))
                break;
        }
    }
    // Else if where is "end", let T be the String value that is a copy of S with trailing white space removed.
    // Trim ending if end or start+end
    if (where == END || where == STARTEND) {
        for (e = stringLength - 1; e >= s; e--) {
            if (!EscargotLexer::isWhiteSpaceOrLineTerminator((*str)[e]))
                break;
        }
    }
    // Return T.
    return new StringView(str, s, e + 1);
}

static Value builtinStringTrim(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let S be this value.
    // Return ? TrimString(S, "start+end").
    return builtinStringTrimString(state, thisValue, STARTEND);
}

static Value builtinStringTrimStart(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let S be this value.
    // Return ? TrimString(S, "start").
    return builtinStringTrimString(state, thisValue, START);
}

static Value builtinStringTrimEnd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let S be this value.
    // Return ? TrimString(S, "end").
    return builtinStringTrimString(state, thisValue, END);
}

static Value builtinStringValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (thisValue.isString()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isStringObject()) {
        return Value(thisValue.asPointerValue()->asStringObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotString);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringStartsWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, startsWith);
    Value searchString = argv[0];
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.

    if (searchString.isObject() && searchString.asObject()->isRegExp(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't use RegExp with startsWith");
    }
    // Let searchStr be ? ToString(searchString).
    String* searchStr = searchString.toString(state);
    // Let pos be ? ToInteger(position). (If position is undefined, this step produces the value 0.)
    double pos = 0;
    if (argc >= 2) {
        pos = argv[1].toInteger(state);
    }

    // Let len be the number of elements in S.
    double len = S->length();
    // Let start be min(max(pos, 0), len).
    double start = std::min(std::max(pos, 0.0), len);
    // Let searchLength be the number of elements in searchStr.
    double searchLength = searchStr->length();
    // If searchLength+start is greater than len, return false.
    if (searchLength + start > len) {
        return Value(false);
    }
    // If the sequence of elements of S starting at start of length searchLength is the same as the full element sequence of searchStr, return true.
    // Otherwise, return false.
    const auto& srcData = S->bufferAccessData();
    const auto& src2Data = searchStr->bufferAccessData();

    for (size_t i = 0; i < src2Data.length; i++) {
        if (srcData.charAt(i + start) != src2Data.charAt(i)) {
            return Value(false);
        }
    }

    return Value(true);
}

static Value builtinStringEndsWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, endsWith);
    Value searchString = argv[0];
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.
    if (searchString.isObject() && searchString.asObject()->isRegExp(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't use RegExp with endsWith");
    }
    // Let len be the number of elements in S.
    double len = S->length();

    // Let searchStr be ? ToString(searchString).
    String* searchStr = searchString.toString(state);
    // If endPosition is undefined, let pos be len, else let pos be ? ToInteger(endPosition).
    double pos = 0;
    if (argc >= 2 && !argv[1].isUndefined()) {
        pos = argv[1].toInteger(state);
    } else {
        pos = len;
    }

    // Let end be min(max(pos, 0), len).
    double end = std::min(std::max(pos, 0.0), len);
    // Let searchLength be the number of elements in searchStr.
    double searchLength = searchStr->length();
    // Let start be end - searchLength.
    double start = end - searchLength;
    // If start is less than 0, return false.
    if (start < 0) {
        return Value(false);
    }
    // If the sequence of elements of S starting at start of length searchLength is the same as the full element sequence of searchStr, return true.
    const auto& srcData = S->bufferAccessData();
    const auto& src2Data = searchStr->bufferAccessData();
    for (size_t i = 0; i < searchLength; i++) {
        if (srcData.charAt(i + start) != src2Data.charAt(i)) {
            return Value(false);
        }
    }

    return Value(true);
}

// ( template, ...substitutions )
static Value builtinStringRaw(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    Value argTemplate = argv[0];
    // Let substitutions be a List consisting of all of the arguments passed to this function, starting with the second argument. If fewer than two arguments were passed, the List is empty.
    // Let numberOfSubstitutions be the number of elements in substitutions.
    size_t numberOfSubstitutions;
    if (argc < 2) {
        numberOfSubstitutions = 0;
    } else {
        numberOfSubstitutions = argc - 1;
    }

    // Let cooked be ? ToObject(template).
    Object* cooked = argTemplate.toObject(state);
    // Let raw be ? ToObject(? Get(cooked, "raw")).
    Object* raw = cooked->get(state, ObjectPropertyName(state.context()->staticStrings().raw)).value(state, cooked).toObject(state);
    // Let literalSegments be ? ToLength(? Get(raw, "length")).
    double literalSegments = raw->lengthES6(state);
    // If literalSegments ≤ 0, return the empty string.
    if (literalSegments <= 0) {
        return String::emptyString;
    }
    // Let stringElements be a new empty List.
    StringBuilder stringElements;
    // Let nextIndex be 0.
    size_t nextIndex = 0;
    // Repeat
    while (true) {
        // Let nextKey be ! ToString(nextIndex).
        // Let nextSeg be ? ToString(? Get(raw, nextKey)).
        String* nextSeg = raw->get(state, ObjectPropertyName(state, Value(nextIndex))).value(state, raw).toString(state);
        // Append in order the code unit elements of nextSeg to the end of stringElements.
        for (size_t i = 0; i < nextSeg->length(); i++) {
            stringElements.appendChar(nextSeg->charAt(i));
        }
        // If nextIndex + 1 = literalSegments, then
        if (nextIndex + 1 == literalSegments) {
            // Return the String value whose code units are, in order, the elements in the List stringElements. If stringElements has no elements, the empty string is returned.
            return stringElements.finalize(&state);
        }
        Value next;
        // If nextIndex < numberOfSubstitutions, let next be substitutions[nextIndex].
        if (nextIndex < numberOfSubstitutions) {
            next = argv[nextIndex + 1];
        } else {
            // Else, let next be the empty String.
            next = String::emptyString;
        }
        // Let nextSub be ? ToString(next).
        String* nextSub = next.toString(state);
        // Append in order the code unit elements of nextSub to the end of stringElements.
        stringElements.appendString(nextSub);
        // Let nextIndex be nextIndex + 1.
        nextIndex++;
    }
}

// https://www.ecma-international.org/ecma-262/8.0/#sec-string.prototype.padstart
// 21.1.3.14String.prototype.padStart( maxLength [ , fillString ] )
static Value builtinStringPadStart(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, padStart);

    // Let intMaxLength be ? ToLength(maxLength).
    // Let stringLength be the number of elements in S.
    uint64_t intMaxLength = 0;
    if (argc >= 1) {
        intMaxLength = argv[0].toLength(state);
    }
    uint64_t stringLength = S->length();

    // If intMaxLength is not greater than stringLength, return S.
    if (intMaxLength <= stringLength) {
        return S;
    }

    // If fillString is undefined, let filler be a String consisting solely of the code unit 0x0020 (SPACE).
    // Else, let filler be ? ToString(fillString).
    String* filler;
    if (argc >= 2 && (!argv[1].isUndefined())) {
        filler = argv[1].toString(state);
    } else {
        filler = state.context()->staticStrings().asciiTable[0x20].string();
    }

    // If filler is the empty String, return S.
    if (filler->length() == 0) {
        return S;
    }

    // Let fillLen be intMaxLength - stringLength.
    uint64_t fillLen = intMaxLength - stringLength;

    // Let truncatedStringFiller be a new String value consisting of repeated concatenations of filler truncated to length fillLen.
    StringBuilder sb;
    while (sb.contentLength() < fillLen) {
        sb.appendString(filler);
    }

    // Build the string, than truncate the characters over fillLen
    String* truncatedStringFiller = sb.finalize(&state);
    truncatedStringFiller = truncatedStringFiller->substring(0, fillLen);

    // Return a new String value computed by the concatenation of truncatedStringFiller and S.
    sb.appendString(truncatedStringFiller);
    sb.appendString(S);
    return sb.finalize(&state);
}

// https://www.ecma-international.org/ecma-262/8.0/#sec-string.prototype.padend
// 21.1.3.13String.prototype.padEnd( maxLength [ , fillString ] )
static Value builtinStringPadEnd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, padStart);

    // Let intMaxLength be ? ToLength(maxLength).
    // Let stringLength be the number of elements in S.
    uint64_t intMaxLength = 0;
    if (argc >= 1) {
        intMaxLength = argv[0].toLength(state);
    }
    uint64_t stringLength = S->length();

    // If intMaxLength is not greater than stringLength, return S.
    if (intMaxLength <= stringLength) {
        return S;
    }

    // If fillString is undefined, let filler be a String consisting solely of the code unit 0x0020 (SPACE).
    // Else, let filler be ? ToString(fillString).
    String* filler;
    if (argc >= 2 && (!argv[1].isUndefined())) {
        filler = argv[1].toString(state);
    } else {
        filler = state.context()->staticStrings().asciiTable[0x20].string();
    }

    // If filler is the empty String, return S.
    if (filler->length() == 0) {
        return S;
    }

    // Let fillLen be intMaxLength - stringLength.
    uint64_t fillLen = intMaxLength - stringLength;

    // Let truncatedStringFiller be a new String value consisting of repeated concatenations of filler truncated to length fillLen.
    StringBuilder sb;
    while (sb.contentLength() < fillLen) {
        sb.appendString(filler);
    }

    // Build the string, than truncate the characters over fillLen
    String* truncatedStringFiller = sb.finalize(&state);
    truncatedStringFiller = truncatedStringFiller->substring(0, fillLen);

    // Return a new String value computed by the concatenation of S and truncatedStringFiller.
    sb.appendString(S);
    sb.appendString(truncatedStringFiller);
    return sb.finalize(&state);
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-createhtml
// Runtime Semantics: CreateHTML ( string, tag, attribute, value )
static String* createHTML(ExecutionState& state, Value string, String* tag, String* attribute, Value value, AtomicString methodName)
{
    // Let str be RequireObjectCoercible(string).
    // Let S be ToString(str).
    // ReturnIfAbrupt(S).
    if (string.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().String.string(), true, methodName.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    String* S = string.toString(state);

    // Let p1 be the String value that is the concatenation of "<" and tag.
    StringBuilder sb;
    sb.appendChar('<');
    sb.appendString(tag);
    String* p1 = sb.finalize(&state);
    // If attribute is not the empty String, then
    if (attribute->length()) {
        // Let V be ToString(value).
        String* V = value.toString(state);
        // ReturnIfAbrupt(V).
        // Let escapedV be the String value that is the same as V except that each occurrence of the code unit 0x0022 (QUOTATION MARK) in V has been replaced with the six code unit sequence "&quot;".
        StringBuilder sb;
        for (size_t i = 0; i < V->length(); i++) {
            char16_t ch = V->charAt(i);
            if (ch == 0x22) {
                sb.appendString("&quot;");
            } else {
                sb.appendChar(ch);
            }
        }
        String* escapedV = sb.finalize(&state);

        // Let p1 be the String value that is the concatenation of the following String values:
        // The String value of p1
        // Code unit 0x0020 (SPACE)
        // The String value of attribute
        // Code unit 0x003D (EQUALS SIGN)
        // Code unit 0x0022 (QUOTATION MARK)
        // The String value of escapedV
        // Code unit 0x0022 (QUOTATION MARK)
        sb.appendString(p1);
        sb.appendChar((char)0x20);
        sb.appendString(attribute);
        sb.appendChar((char)0x3d);
        sb.appendChar((char)0x22);
        sb.appendString(escapedV);
        sb.appendChar((char)0x22);
        p1 = sb.finalize(&state);
    }
    // Let p2 be the String value that is the concatenation of p1 and ">".
    // Let p3 be the String value that is the concatenation of p2 and S.
    // Let p4 be the String value that is the concatenation of p3, "</", tag, and ">".
    // Return p4.
    sb.appendString(p1);
    sb.appendChar('>');
    sb.appendString(S);
    sb.appendString("</");
    sb.appendString(tag);
    sb.appendChar('>');
    return sb.finalize(&state);
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-additional-properties-of-the-string.prototype-object

static Value builtinStringSubstr(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, substr);
    if (argc < 1) {
        return str;
    }
    double intStart = argv[0].toInteger(state);
    double end;
    if (argc > 1) {
        if (argv[1].isUndefined()) {
            end = std::numeric_limits<double>::infinity();
        } else
            end = argv[1].toInteger(state);
    } else {
        end = std::numeric_limits<double>::infinity();
    }
    double size = str->length();
    if (intStart < 0)
        intStart = std::max(size + intStart, 0.0);
    double resultLength = std::min(std::max(end, 0.0), size - intStart);
    if (resultLength <= 0)
        return String::emptyString;

    return str->substring(intStart, intStart + resultLength);
}


#define DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fnName, P0, P1, P2)                                                        \
    static Value builtinString##fnName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget) \
    {                                                                                                                     \
        return createHTML(state, thisValue, P0, P1, P2, state.context()->staticStrings().fnName);                         \
    }

// String.prototype.anchor (name)
// Return CreateHTML(S, "a", "name", name).
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(anchor, state.context()->staticStrings().asciiTable[(size_t)'a'].string(), state.context()->staticStrings().name.string(), argv[0])
// String.prototype.big ()
// Return CreateHTML(S, "big", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(big, state.context()->staticStrings().big.string(), String::emptyString, String::emptyString)
// String.prototype.blink ()
// Return CreateHTML(S, "blink", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(blink, state.context()->staticStrings().blink.string(), String::emptyString, String::emptyString)
// String.prototype.bold ()
// Return CreateHTML(S, "b", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(bold, state.context()->staticStrings().asciiTable[(size_t)'b'].string(), String::emptyString, String::emptyString)
// String.prototype.fixed ()
// Return CreateHTML(S, "tt", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fixed, String::fromASCII("tt"), String::emptyString, String::emptyString)
// String.prototype.fontcolor (color)
// Return CreateHTML(S, "font", "color", color).
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fontcolor, String::fromASCII("font"), String::fromASCII("color"), argv[0])
// String.prototype.fontsize (size)
// Return CreateHTML(S, "font", "size", size).
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fontsize, String::fromASCII("font"), state.context()->staticStrings().size.string(), argv[0])
// String.prototype.italics ()
// Return CreateHTML(S, "i", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(italics, state.context()->staticStrings().asciiTable[(size_t)'i'].string(), String::emptyString, String::emptyString)
// String.prototype.link (url)
// Return CreateHTML(S, "a", "href", url).
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(link, state.context()->staticStrings().asciiTable[(size_t)'a'].string(), String::fromASCII("href"), argv[0])
// String.prototype.small ()
// Return CreateHTML(S, "small", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(small, state.context()->staticStrings().small.string(), String::emptyString, String::emptyString)
// String.prototype.strike ()
// Return CreateHTML(S, "strike", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(strike, state.context()->staticStrings().strike.string(), String::emptyString, String::emptyString)
// String.prototype.sub ()
// Return CreateHTML(S, "sub", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(sub, state.context()->staticStrings().sub.string(), String::emptyString, String::emptyString)
// String.prototype.sup ()
// Return CreateHTML(S, "sup", "", "").
DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(sup, state.context()->staticStrings().sup.string(), String::emptyString, String::emptyString)

#undef DEFINE_STRING_ADDITIONAL_HTML_FUNCTION

static Value builtinStringIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, includes);
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.
    Value searchString = argv[0];
    if (searchString.isObject() && searchString.asObject()->isRegExp(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't use RegExp with includes");
    }

    // Let searchStr be ? ToString(searchString).
    String* searchStr = searchString.toString(state);

    // Let pos be ? ToInteger(position). (If position is undefined, this step produces the value 0.)
    double pos = 0;
    if (argc >= 2) {
        pos = argv[1].toInteger(state);
    }

    // Let len be the number of elements in S.
    double len = S->length();

    // Let start be min(max(pos, 0), len).
    double start = std::min(std::max(pos, 0.0), len);
    // Let searchLen be the number of elements in searchStr.
    size_t searchLen = searchStr->length();
    // If there exists any integer k not smaller than start such that k + searchLen is not greater than len, and for all nonnegative integers j less than searchLen, the code unit at index k+j of S is the same as the code unit at index j of searchStr, return true; but if there is no such integer k, return false.
    auto ret = S->find(searchStr, start);
    if (ret == SIZE_MAX) {
        return Value(false);
    }
    return Value(true);
}

static Value builtinStringIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorObject() || !thisValue.asObject()->asIteratorObject()->isStringIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().StringIterator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }
    StringIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asStringIteratorObject();
    return iter->next(state);
}

static Value builtinStringIterator(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    RESOLVE_THIS_BINDING_TO_STRING(S, String, iterator);
    // Return CreateStringIterator(S).
    return new StringIteratorObject(state, S);
}

void GlobalObject::installString(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_string = new NativeFunctionObject(state, NativeFunctionInfo(strings->String, builtinStringConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_string->markThisObjectDontNeedStructureTransitionTable();
    m_string->setPrototype(state, m_functionPrototype);
    m_stringPrototype = m_objectPrototype;
    m_stringPrototype = new StringObject(state, String::emptyString);
    m_stringPrototype->markThisObjectDontNeedStructureTransitionTable();
    m_stringPrototype->setPrototype(state, m_objectPrototype);
    m_string->setFunctionPrototype(state, m_stringPrototype);
    m_stringPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinStringToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.4 String.prototype.concat
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->concat),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->concat, builtinStringConcat, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.8 String.prototype.indexOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->indexOf),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinStringIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->lastIndexOf),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinStringLastIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->localeCompare),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->localeCompare, builtinStringLocaleCompare, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.16 String.prototype.slice
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinStringSlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.19 String.prototype.substring
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->substring),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->substring, builtinStringSubstring, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->substr),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->substr, builtinStringSubstr, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->match),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->match, builtinStringMatch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

#if defined(ENABLE_ICU)
    // The length property of the normalize method is 0.
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->normalize),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->normalize, builtinStringNormalize, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
#endif // ENABLE_ICU

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->repeat),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->repeat, builtinStringRepeat, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->replace),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->replace, builtinStringReplace, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->search),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->search, builtinStringSearch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->split),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->split, builtinStringSplit, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->charCodeAt),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->charCodeAt, builtinStringCharCodeAt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->codePointAt),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->codePointAt, builtinStringCodePointAt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->charAt),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->charAt, builtinStringCharAt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLowerCase),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLowerCase, builtinStringToLowerCase, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toUpperCase),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toUpperCase, builtinStringToUpperCase, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleLowerCase),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleLowerCase, builtinStringToLocaleLowerCase, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleUpperCase),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleUpperCase, builtinStringToLocaleUpperCase, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trim),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->trim, builtinStringTrim, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->padStart),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->padStart, builtinStringPadStart, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->padEnd),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->padEnd, builtinStringPadEnd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* trimStart = new NativeFunctionObject(state, NativeFunctionInfo(strings->trimStart, builtinStringTrimStart, 0, NativeFunctionInfo::Strict));
    FunctionObject* trimEnd = new NativeFunctionObject(state, NativeFunctionInfo(strings->trimEnd, builtinStringTrimEnd, 0, NativeFunctionInfo::Strict));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trimStart),
                                                        ObjectPropertyDescriptor(trimStart, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trimEnd),
                                                        ObjectPropertyDescriptor(trimEnd, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trimRight),
                                                        ObjectPropertyDescriptor(trimEnd, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trimLeft),
                                                        ObjectPropertyDescriptor(trimStart, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.26 String.prototype.valueOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinStringValueOf, 0, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES6 builtins
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->startsWith),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->startsWith, builtinStringStartsWith, 1, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->endsWith),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->endsWith, builtinStringEndsWith, 1, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->includes),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->includes, builtinStringIncludes, 1, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.iterator]")), builtinStringIterator, 0, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

#define DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fnName, argLength)                                                                                                                                                    \
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->fnName),                                                                                                                  \
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fnName, builtinString##fnName, argLength, NativeFunctionInfo::Strict)), \
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // String.prototype.anchor (name)
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(anchor, 1)
    // String.prototype.big ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(big, 0)
    // String.prototype.blink ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(blink, 0)
    // String.prototype.bold ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(bold, 0)
    // String.prototype.fixed ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fixed, 0)
    // String.prototype.fontcolor (color)
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fontcolor, 1)
    // String.prototype.fontsize (size)
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(fontsize, 1)
    // String.prototype.italics ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(italics, 0)
    // String.prototype.link (url)
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(link, 1)
    // String.prototype.small ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(small, 0)
    // String.prototype.strike ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(strike, 0)
    // String.prototype.sub ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(sub, 0)
    // String.prototype.sup ()
    DEFINE_STRING_ADDITIONAL_HTML_FUNCTION(sup, 0)

#undef DEFINE_STRING_ADDITIONAL_HTML_FUNCTION

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->fromCharCode),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fromCharCode, builtinStringFromCharCode, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->fromCodePoint),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fromCodePoint, builtinStringFromCodePoint, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->raw),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fromCharCode, builtinStringRaw, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->setFunctionPrototype(state, m_stringPrototype);

    m_stringIteratorPrototype = m_iteratorPrototype;
    m_stringIteratorPrototype = new StringIteratorObject(state, nullptr);

    m_stringIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinStringIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                                ObjectPropertyDescriptor(Value(String::fromASCII("String Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->String),
                      ObjectPropertyDescriptor(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
