#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"
#include "ErrorObject.h"
#include "RegExpObject.h"
#include "ArrayObject.h"

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
    size_t pos;
    if (val.isUndefined()) {
        pos = 0;
    } else {
        pos = val.toInteger(state);
    }

    size_t len = str->length();
    size_t start = std::min(std::max(pos, (size_t)0), len);
    size_t result = str->find(searchStr, start);
    if (result == SIZE_MAX)
        return Value(-1);
    else
        return Value(result);
}

static Value builtinStringSubstring(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
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
        return str->subString(from, to);
    }
}

static Value builtinStringMatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, match);
    Value argument = argv[0];
    RegExpObject* regexp;
    if (argument.isPointerValue() && argument.asPointerValue()->isRegExpObject()) {
        regexp = argument.asPointerValue()->asRegExpObject();
    } else {
        regexp = new RegExpObject(state, argument.toString(state), String::emptyString);
    }

    (void)regexp->lastIndex().toInteger(state);
    bool isGlobal = regexp->option() & RegExpObject::Option::Global;
    if (isGlobal) {
        regexp->setLastIndex(Value(0));
    }

    RegexMatchResult result;
    bool testResult = regexp->matchNonGlobally(state, str, result, false, 0);
    if (!testResult) {
        regexp->setLastIndex(Value(0));
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

static Value builtinStringReplace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(string, String, replace);
    Value searchValue = argv[0];
    Value replaceValue = argv[1];
    String* replaceString = nullptr;
    bool replaceValueIsFunction = replaceValue.isFunction();
    if (!replaceValueIsFunction)
        replaceString = replaceValue.toString(state);
    RegexMatchResult result;

    if (searchValue.isPointerValue() && searchValue.asPointerValue()->isRegExpObject()) {
        RegExpObject* regexp = searchValue.asPointerValue()->asRegExpObject();
        bool isGlobal = regexp->option() & RegExpObject::Option::Global;

        if (isGlobal) {
            regexp->setLastIndex(Value(0));
        }
        bool testResult = regexp->matchNonGlobally(state, string, result, false, 0);
        if (testResult) {
            if (isGlobal) {
                regexp->createRegexMatchResult(state, string, result);
            }
        } else {
            regexp->setLastIndex(Value(0));
        }
    } else {
        String* searchString = searchValue.toString(state);
        size_t idx = string->find(searchString);
        if (idx != (size_t)-1) {
            Vector<RegexMatchResult::RegexMatchResultPiece, gc_malloc_pointer_free_allocator<RegexMatchResult::RegexMatchResultPiece>> piece;
            RegexMatchResult::RegexMatchResultPiece p;
            p.m_start = idx;
            p.m_end = idx + searchString->length();
            piece.push_back(std::move(p));
            result.m_matchResults.push_back(std::move(piece));
        }
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
                    arguments[j] = argStrBuilder.finalize();
                }
            }
            arguments[subLen] = Value((int)result.m_matchResults[i][0].m_start);
            arguments[subLen + 1] = string;
            // 21.1.3.14 (11) it should be called with this as undefined
            String* res = FunctionObject::call(callee, state, Value(), subLen + 2, arguments).toString(state);
            builer.appendSubString(res, 0, res->length());

            if (i < matchCount - 1) {
                builer.appendSubString(string, result.m_matchResults[i][0].m_end, result.m_matchResults[i + 1][0].m_start);
            }
        }
        builer.appendSubString(string, result.m_matchResults[matchCount - 1][0].m_end, string->length());
        return builer.finalize();
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
                            int peek = replaceString->charAt(j + 2) - '0';
                            bool usePeek = false;
                            if (0 <= peek && peek <= 9) {
                                idx *= 10;
                                idx += peek;
                                usePeek = true;
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
        return builder.finalize();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 1, 2, 3
    RESOLVE_THIS_BINDING_TO_STRING(S, String, split);
    ArrayObject* A = new ArrayObject(state);

    // 4, 5
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
    Value separator = argv[0];
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

                String* T = S->subString(p, result.m_matchResults[0][0].m_start);
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

                    String* T = S->subString(p, q);
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
    String* T = S->subString(p, s);
    A->defineOwnProperty(state, ObjectPropertyName(state, Value(lengthA)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
    return A;
}

static Value builtinStringCharCodeAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
    int position = argv[0].toInteger(state);
    Value ret;
    if (position < 0 || position >= (int)str->length())
        ret = Value(std::numeric_limits<double>::quiet_NaN());
    else
        ret = Value(str->charAt(position));
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

    if (LIKELY(0 <= position && position < (int64_t)str->length())) {
        char16_t c = str->charAt(position);
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
        return builder.finalize();
    }
    return Value();
}

static Value builtinStringConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, concat);
    for (size_t i = 0; i < argc; i++) {
        String* appendStr = argv[i].toString(state);
        str = RopeString::createRopeString(str, appendStr);
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
    return str->subString(from, from + span);
}

static Value builtinStringToLowerCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toLowerCase);
    if (str->hasASCIIContent()) {
        ASCIIStringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const char* buf = str->characters8();
        for (size_t i = 0; i < str->length(); i++) {
            newStr[i] = u_tolower(buf[i]);
        }
        return new ASCIIString(std::move(newStr));
    } else {
        size_t len = str->length();
        UTF16StringData newStr(str->characters16(), len);
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
}

static Value builtinStringToUpperCase(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, toUpperCase);
    if (str->hasASCIIContent()) {
        ASCIIStringData newStr;
        size_t len = str->length();
        newStr.resizeWithUninitializedValues(len);
        const char* buf = str->characters8();
        for (size_t i = 0; i < str->length(); i++) {
            newStr[i] = u_toupper(buf[i]);
        }
        return new ASCIIString(std::move(newStr));
    } else {
        size_t len = str->length();
        UTF16StringData newStr(str->characters16(), len);
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
}

void GlobalObject::installString(ExecutionState& state)
{
    m_string = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().String, builtinStringConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new StringObject(state);
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_string->markThisObjectDontNeedStructureTransitionTable(state);
    m_string->setPrototype(state, m_functionPrototype);
    m_stringPrototype = m_objectPrototype;
    m_stringPrototype = new StringObject(state, String::emptyString);
    m_stringPrototype->setPrototype(state, m_objectPrototype);

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinStringToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.4 String.prototype.concat
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinStringConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.8 String.prototype.indexOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().indexOf),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.16 String.prototype.slice
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().slice),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().slice, builtinStringSlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.1.3.19 String.prototype.substring
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().substring),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringSubstring, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().match),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().match, builtinStringMatch, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().replace),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().replace, builtinStringReplace, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().split),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().split, builtinStringSplit, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().charCodeAt),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().charCodeAt, builtinStringCharCodeAt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().charAt),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().charAt, builtinStringCharAt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toLowerCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLowerCase, builtinStringToLowerCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toUpperCase),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toUpperCase, builtinStringToUpperCase, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().fromCharCode),
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().fromCharCode, builtinStringFromCharCode, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_string->setFunctionPrototype(state, m_stringPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().String),
                      ObjectPropertyDescriptor(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
