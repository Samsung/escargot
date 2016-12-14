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
            stringObject->setStringData(state, String::emptyString);
        } else {
            stringObject->setStringData(state, argv[0].toString(state));
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
        return str->subString(from, to - from);
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
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
    for (size_t i = 0; i < argc; i++) {
        String* appendStr = argv[i].toString(state);
        str = RopeString::createRopeString(str, appendStr);
    }
    return Value(str);
}

void GlobalObject::installString(ExecutionState& state)
{
    m_string = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().String, builtinStringConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new StringObject(state);
                                  }));
    m_string->markThisObjectDontNeedStructureTransitionTable(state);
    m_string->setPrototype(state, m_functionPrototype);
    // TODO m_string->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_stringPrototype = m_objectPrototype;
    m_stringPrototype = new StringObject(state, String::emptyString);
    m_stringPrototype->setPrototype(state, m_objectPrototype);

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinStringToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    // $21.1.3.4 String.prototype.concat
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinStringConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    // $21.1.3.8 String.prototype.indexOf
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().indexOf),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    // $21.1.3.19 String.prototype.substring
    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().substring),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringSubstring, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().match),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().match, builtinStringMatch, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().replace),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().replace, builtinStringReplace, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().charCodeAt),
                                                        ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().charCodeAt, builtinStringCharCodeAt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_string->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().fromCharCode),
                                               ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().fromCharCode, builtinStringFromCharCode, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_string->setFunctionPrototype(state, m_stringPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().String),
                      ObjectPropertyDescriptorForDefineOwnProperty(m_string, (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
}
}
