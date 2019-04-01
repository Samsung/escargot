/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "parser/Lexer.h"

namespace Escargot {

static Value builtinStringConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression) {
        Object* thisObject = thisValue.toObject(state);
        StringObject* stringObject = thisObject->asStringObject();
        if (argc == 0) {
            stringObject->setPrimitiveValue(state, String::emptyString);
        } else {
            stringObject->setPrimitiveValue(state, argv[0].toString(state));
        }
        return stringObject;
    } else {
        // called as function
        if (argc == 0)
            return String::emptyString;
        Value value = argv[0];
        // If NewTarget is undefined and Type(value) is Symbol, return SymbolDescriptiveString(value).
        if (value.isSymbol()) {
            return value.asSymbol()->getSymbolDescriptiveString();
        }
        return value.toString(state);
    }
}

static Value builtinStringToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isObject() && thisValue.asObject()->isStringObject()) {
        return thisValue.asObject()->asStringObject()->primitiveValue();
    }

    if (thisValue.isString())
        return thisValue.toString(state);

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().String.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotString);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringLocaleCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(S, String, localeCompare);
    String* That = argv[0].toString(state);
    return Value(stringCompare(*S, *That));
}

static Value builtinStringSubstring(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringSubstr(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringMatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, match);
    Value argument = argv[0];
    RegExpObject* regexp;
    if (argument.isPointerValue() && argument.asPointerValue()->isRegExpObject()) {
        regexp = argument.asPointerValue()->asRegExpObject();
    } else {
        regexp = new RegExpObject(state, argument.isUndefined() ? String::emptyString : argument.toString(state), String::emptyString);
    }

    bool isGlobal = regexp->option() & RegExpObject::Option::Global;
    if (isGlobal) {
        regexp->setLastIndex(state, Value(0));
    }

    RegexMatchResult result;
    bool testResult = regexp->matchNonGlobally(state, str, result, false, 0);
    if (!testResult) {
        if (isGlobal)
            regexp->setLastIndex(state, Value(0));
        return Value(Value::Null);
    }

    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/match
    // if global flag is on, match method returns an Array containing all matched substrings
    if (isGlobal) {
        return regexp->createMatchedArray(state, str, result);
    } else {
        return regexp->createRegExpMatchedArray(state, result, str);
    }
}

#if defined(ENABLE_ICU)
static Value builtinStringNormalize(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    enum NormalizationForm {
        NFC,
        NFD,
        NFKC,
        NFKD
    };

    RESOLVE_THIS_BINDING_TO_STRING(str, String, normalize);
    Value argument = argv[0];
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

static Value builtinStringRepeat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, repeat);
    Value argument = argv[0];
    int32_t repeatCount;
    double count = argument.toInteger(state);
    if (count < 0 || count == std::numeric_limits<double>::infinity()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid count number of String repeat method");
    }
    repeatCount = static_cast<int32_t>(count);
    if (count == 0) {
        return String::emptyString;
    }

    StringBuilder builder;
    for (int i = 0; i < repeatCount; i++) {
        builder.appendString(str);
    }
    return builder.finalize();
}

static Value builtinStringReplace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(string, String, replace);
    Value searchValue = argv[0];
    Value replaceValue = argv[1];
    String* replaceString = nullptr;
    bool replaceValueIsFunction = replaceValue.isFunction();
    RegexMatchResult result;

    if (searchValue.isPointerValue() && searchValue.asPointerValue()->isRegExpObject()) {
        RegExpObject* regexp = searchValue.asPointerValue()->asRegExpObject();
        bool isGlobal = regexp->option() & RegExpObject::Option::Global;

        if (isGlobal) {
            regexp->setLastIndex(state, Value(0));
        }
        bool testResult = regexp->matchNonGlobally(state, string, result, false, 0);
        if (testResult) {
            if (isGlobal) {
                regexp->createRegexMatchResult(state, string, result);
            }
        } else {
            regexp->setLastIndex(state, Value(0));
        }
    } else {
        String* searchString = searchValue.toString(state);
        size_t idx = string->find(searchString);
        if (idx != (size_t)-1) {
            std::vector<RegexMatchResult::RegexMatchResultPiece> piece;
            RegexMatchResult::RegexMatchResultPiece p;
            p.m_start = idx;
            p.m_end = idx + searchString->length();
            piece.push_back(std::move(p));
            result.m_matchResults.push_back(std::move(piece));
        }
    }

    // NOTE: replaceValue.toString should be called after searchValue.toString
    if (!replaceValueIsFunction) {
        replaceString = replaceValue.toString(state);
    }

    if (result.m_matchResults.size() == 0) {
        return string;
    }

    if (replaceValueIsFunction) {
        uint32_t matchCount = result.m_matchResults.size();
        Value callee = replaceValue;

        StringBuilder builer;
        builer.appendSubString(string, 0, result.m_matchResults[0][0].m_start);

        for (uint32_t i = 0; i < matchCount; i++) {
            int subLen = result.m_matchResults[i].size();
            Value* arguments;
            // #define ALLOCA(bytes, typenameWithoutPointer, ec) (typenameWithoutPointer*)alloca(bytes)
            arguments = ALLOCA(sizeof(Value) * (subLen + 2), Value, state);
            for (unsigned j = 0; j < (unsigned)subLen; j++) {
                if (result.m_matchResults[i][j].m_start == std::numeric_limits<unsigned>::max())
                    arguments[j] = Value();
                else {
                    StringBuilder argStrBuilder;
                    argStrBuilder.appendSubString(string, result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end);
                    arguments[j] = argStrBuilder.finalize(&state);
                }
            }
            arguments[subLen] = Value((int)result.m_matchResults[i][0].m_start);
            arguments[subLen + 1] = string;
            // 21.1.3.14 (11) it should be called with this as undefined
            String* res = FunctionObject::call(state, callee, Value(), subLen + 2, arguments).toString(state);
            builer.appendSubString(res, 0, res->length());

            if (i < matchCount - 1) {
                builer.appendSubString(string, result.m_matchResults[i][0].m_end, result.m_matchResults[i + 1][0].m_start);
            }
        }
        builer.appendSubString(string, result.m_matchResults[matchCount - 1][0].m_end, string->length());
        return builer.finalize(&state);
    } else {
        ASSERT(replaceString);

        bool hasDollar = false;
        for (size_t i = 0; i < replaceString->length(); i++) {
            if (replaceString->charAt(i) == '$') {
                hasDollar = true;
                break;
            }
        }

        StringBuilder builder;
        if (!hasDollar) {
            // flat replace
            int32_t matchCount = result.m_matchResults.size();
            builder.appendSubString(string, 0, result.m_matchResults[0][0].m_start);
            for (int32_t i = 0; i < matchCount; i++) {
                String* res = replaceString;
                builder.appendString(res);
                if (i < matchCount - 1) {
                    builder.appendSubString(string, result.m_matchResults[i][0].m_end, result.m_matchResults[i + 1][0].m_start);
                }
            }
            builder.appendSubString(string, result.m_matchResults[matchCount - 1][0].m_end, string->length());
        } else {
            // dollar replace
            int32_t matchCount = result.m_matchResults.size();
            builder.appendSubString(string, 0, result.m_matchResults[0][0].m_start);
            for (int32_t i = 0; i < matchCount; i++) {
                for (unsigned j = 0; j < replaceString->length(); j++) {
                    if (replaceString->charAt(j) == '$' && (j + 1) < replaceString->length()) {
                        char16_t c = replaceString->charAt(j + 1);
                        if (c == '$') {
                            builder.appendChar(replaceString->charAt(j));
                        } else if (c == '&') {
                            builder.appendSubString(string, result.m_matchResults[i][0].m_start, result.m_matchResults[i][0].m_end);
                        } else if (c == '\'') {
                            builder.appendSubString(string, result.m_matchResults[i][0].m_end, string->length());
                        } else if (c == '`') {
                            builder.appendSubString(string, 0, result.m_matchResults[i][0].m_start);
                        } else if ('0' <= c && c <= '9') {
                            size_t idx = c - '0';
                            bool usePeek = false;
                            if (j + 2 < replaceString->length()) {
                                int peek = replaceString->charAt(j + 2) - '0';
                                if (0 <= peek && peek <= 9) {
                                    idx *= 10;
                                    idx += peek;
                                    usePeek = true;
                                }
                            }

                            if (idx < result.m_matchResults[i].size() && idx != 0) {
                                builder.appendSubString(string, result.m_matchResults[i][idx].m_start, result.m_matchResults[i][idx].m_end);
                                if (usePeek)
                                    j++;
                            } else {
                                idx = c - '0';
                                if (idx < result.m_matchResults[i].size() && idx != 0) {
                                    builder.appendSubString(string, result.m_matchResults[i][idx].m_start, result.m_matchResults[i][idx].m_end);
                                } else {
                                    builder.appendChar('$');
                                    builder.appendChar(c);
                                }
                            }
                        } else {
                            builder.appendChar('$');
                            builder.appendChar(c);
                        }
                        j++;
                    } else {
                        builder.appendChar(replaceString->charAt(j));
                    }
                }
                if (i < matchCount - 1) {
                    builder.appendSubString(string, result.m_matchResults[i][0].m_end, result.m_matchResults[i + 1][0].m_start);
                }
            }
            builder.appendSubString(string, result.m_matchResults[matchCount - 1][0].m_end, string->length());
        }
        return builder.finalize(&state);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Call CheckObjectCoercible passing the this value as its argument.
    // Let string be the result of calling ToString, giving it the this value as its argument.
    RESOLVE_THIS_BINDING_TO_STRING(string, String, search);
    Value regexp = argv[0];

    RegExpObject* rx;
    if (regexp.isPointerValue() && regexp.asPointerValue()->isRegExpObject()) {
        // If Type(regexp) is Object and the value of the [[Class]] internal property of regexp is "RegExp", then let rx be regexp;
        rx = regexp.asPointerValue()->asRegExpObject();
    } else {
        // Else, let rx be a new RegExp object created as if by the expression new RegExp(regexp) where RegExp is the standard built-in constructor with that name.
        rx = new RegExpObject(state, regexp.isUndefined() ? String::emptyString : regexp.toString(state), String::emptyString);
    }

    // Search the value string from its beginning for an occurrence of the regular expression pattern rx.
    // Let result be a Number indicating the offset within string where the pattern matched, or –1 if there was no match.
    // The lastIndex and global properties of regexp are ignored when performing the search. The lastIndex property of regexp is left unchanged.
    RegexMatchResult result;
    rx->match(state, string, result);
    if (result.m_matchResults.size() != 0) {
        // Return result.
        return Value(result.m_matchResults[0][0].m_start);
    } else {
        return Value(-1);
    }
}

static Value builtinStringSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 1, 2, 3
    RESOLVE_THIS_BINDING_TO_OBJECT(obj, Object, split);
    ArrayObject* A = new ArrayObject(state);
    Value separator = argv[0];
    Value limit = argv[1];
    Value searcher;

    // 21.1.3.17.3
    if(!separator.isUndefinedOrNull()) {
      Value splitter = obj->getMethod(state, separator, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().split));
      if(!searcher.isUndefined()) {
          Value params[2] = {obj, limit};
          return FunctionObject::call(state, splitter, separator, 2, params);
      }
    }

    // 4, 5
    RESOLVE_THIS_BINDING_TO_STRING(S, String, split);
    size_t lengthA = 0;
    size_t lim;
    if (argv[1].isUndefined()) {
        lim = Value::InvalidIndexValue - 1;
    } else {
        lim = argv[1].toUint32(state);
    }

    // 6, 7
    size_t s = S->length(), p = 0;

    // 8
    PointerValue* P;
    if (separator.isPointerValue() && separator.asPointerValue()->isRegExpObject()) {
        P = separator.asPointerValue()->asRegExpObject();
    } else {
        P = separator.toString(state);
    }

    // 9
    if (lim == 0)
        return A;

    // 10
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
    // 11
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

    // 12
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

    // 14, 15, 16
    String* T = S->substring(p, s);
    A->defineOwnProperty(state, ObjectPropertyName(state, Value(lengthA)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
    return A;
}

static Value builtinStringCharCodeAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringCharAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringFromCharCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, concat);
    for (size_t i = 0; i < argc; i++) {
        String* appendStr = argv[i].toString(state);
        str = RopeString::createRopeString(str, appendStr, &state);
    }
    return Value(str);
}

static Value builtinStringSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringToLowerCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLowerCase);
    if (str->has8BitContent()) {
        Latin1StringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const LChar* buf = str->characters8();
        bool result = true;
        for (size_t i = 0; i < len; i++) {
            char32_t u2 = u_tolower(buf[i]);
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
        c = u_tolower(c);
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

static Value builtinStringToUpperCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toUpperCase);
    if (str->has8BitContent()) {
        Latin1StringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const LChar* buf = str->characters8();
        bool result = true;
        for (size_t i = 0; i < len; i++) {
            char32_t u2 = u_toupper(buf[i]);
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
        c = u_toupper(c);
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

static Value builtinStringToLocaleLowerCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLocaleLowerCase);
    return builtinStringToLowerCase(state, thisValue, argc, argv, isNewExpression);
}

static Value builtinStringToLocaleUpperCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLocaleUpperCase);
    return builtinStringToUpperCase(state, thisValue, argc, argv, isNewExpression);
}

static Value builtinStringTrim(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, trim);
    int64_t s, e;
    for (s = 0; s < (int64_t)str->length(); s++) {
        if (!EscargotLexer::isWhiteSpaceOrLineTerminator((*str)[s]))
            break;
    }
    for (e = ((int64_t)str->length()) - 1; e >= s; e--) {
        if (!EscargotLexer::isWhiteSpaceOrLineTerminator((*str)[e]))
            break;
    }
    return new StringView(str, s, e + 1);
}

static Value builtinStringValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isString()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isStringObject()) {
        return Value(thisValue.asPointerValue()->asStringObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotString);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringStartsWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    String* S = Value(thisValue.toObject(state)).toString(state);
    Value searchString = argv[0];
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.
    if (searchString.isObject() && searchString.asObject()->isRegExpObject()) {
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

static Value builtinStringEndsWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    String* S = Value(thisValue.toObject(state)).toString(state);
    Value searchString = argv[0];
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.
    if (searchString.isObject() && searchString.asObject()->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't use RegExp with endsWith");
    }
    // Let len be the number of elements in S.
    double len = S->length();

    // Let searchStr be ? ToString(searchString).
    String* searchStr = searchString.toString(state);
    // If endPosition is undefined, let pos be len, else let pos be ? ToInteger(endPosition).
    double pos = 0;
    if (argc >= 2) {
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
static Value builtinStringRaw(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinStringIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? RequireObjectCoercible(this value).
    // Let S be ? ToString(O).
    String* S = Value(thisValue.toObject(state)).toString(state);
    // Let isRegExp be ? IsRegExp(searchString).
    // If isRegExp is true, throw a TypeError exception.
    Value searchString = argv[0];
    if (searchString.isObject() && searchString.asObject()->isRegExpObject()) {
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

static Value builtinStringIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorObject() || !thisValue.asObject()->asIteratorObject()->isStringIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().StringIterator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }
    StringIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asStringIteratorObject();
    return iter->next(state);
}

static Value builtinStringIterator(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? RequireObjectCoercible(this value).
    Value O = thisValue.toObject(state);
    // Let S be ? ToString(O).
    String* S = O.toString(state);
    // Return CreateStringIterator(S).
    return new StringIteratorObject(state, S);
}

void GlobalObject::installString(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_string = new FunctionObject(state, NativeFunctionInfo(strings->String, builtinStringConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                      return new StringObject(state);
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_string->markThisObjectDontNeedStructureTransitionTable(state);
    m_string->setPrototype(state, m_functionPrototype);
    m_stringPrototype = m_objectPrototype;
    m_stringPrototype = new StringObject(state, String::emptyString);
    m_stringPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_stringPrototype->setPrototype(state, m_objectPrototype);
    m_string->setFunctionPrototype(state, m_stringPrototype);
    m_stringPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinStringToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.4 String.prototype.concat
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->concat),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->concat, builtinStringConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.8 String.prototype.indexOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->indexOf),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinStringIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->lastIndexOf),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinStringLastIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->localeCompare),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->localeCompare, builtinStringLocaleCompare, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.16 String.prototype.slice
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->slice, builtinStringSlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.19 String.prototype.substring
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->substring),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->substring, builtinStringSubstring, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->substr),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->substr, builtinStringSubstr, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->match),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->match, builtinStringMatch, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

#if defined(ENABLE_ICU)
    // The length property of the normalize method is 0.
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->normalize),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->normalize, builtinStringNormalize, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
#endif // ENABLE_ICU

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->repeat),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->repeat, builtinStringRepeat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->replace),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->replace, builtinStringReplace, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->search),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->search, builtinStringSearch, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->split),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->split, builtinStringSplit, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->charCodeAt),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->charCodeAt, builtinStringCharCodeAt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->charAt),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->charAt, builtinStringCharAt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLowerCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toLowerCase, builtinStringToLowerCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toUpperCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toUpperCase, builtinStringToUpperCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleLowerCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toLocaleLowerCase, builtinStringToLocaleLowerCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleUpperCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toLocaleUpperCase, builtinStringToLocaleUpperCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->trim),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->trim, builtinStringTrim, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.26 String.prototype.valueOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinStringValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES6 builtins
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->startsWith),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->startsWith, builtinStringStartsWith, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->endsWith),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->endsWith, builtinStringEndsWith, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->includes),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->includes, builtinStringIncludes, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.iterator]")), builtinStringIterator, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->fromCharCode),
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->fromCharCode, builtinStringFromCharCode, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->raw),
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->fromCharCode, builtinStringRaw, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->setFunctionPrototype(state, m_stringPrototype);

    m_stringIteratorPrototype = m_iteratorPrototype;
    m_stringIteratorPrototype = new StringIteratorObject(state, nullptr);

    m_stringIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinStringIteratorNext, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                                ObjectPropertyDescriptor(Value(String::fromASCII("String Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->String),
                      ObjectPropertyDescriptor(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
