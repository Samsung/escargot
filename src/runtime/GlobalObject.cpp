/*
 *  Copyright (C) 2016 Samsung Electronics Co., Ltd. All Rights Reserved
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ErrorObject.h"
#include "StringObject.h"
#include "NumberObject.h"
#include "DateObject.h"
#include "parser/Lexer.h"
#include "parser/ScriptParser.h"
#include "heap/LeakCheckerBridge.h"
#include "EnvironmentRecord.h"
#include "Environment.h"

namespace Escargot {

#ifdef ESCARGOT_SHELL
static Value builtinPrint(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argv[0].isSymbol()) {
        puts(argv[0].asSymbol()->getSymbolDescriptiveString()->toNonGCUTF8StringData().data());
    } else {
        puts(argv[0].toString(state)->toNonGCUTF8StringData().data());
    }
    return Value();
}

static String* builtinHelperFileRead(ExecutionState& state, const char* fileName, AtomicString builtinName)
{
    FILE* fp = fopen(fileName, "r");
    String* src = String::emptyString;
    if (fp) {
        std::string utf8Str;
        Latin1StringDataNonGCStd str;
        char buf[512];
        bool hasNonLatin1Content = false;
        while (fgets(buf, sizeof buf, fp) != NULL) {
            if (!hasNonLatin1Content) {
                const char* source = buf;
                size_t len = strlen(buf);
                int charlen;
                bool valid;
                while (source < buf + len) {
                    char32_t ch = readUTF8Sequence(source, valid, charlen);
                    if (ch > 255) {
                        hasNonLatin1Content = true;
                        fseek(fp, 0, SEEK_SET);
                        break;
                    } else {
                        str += (LChar)ch;
                    }
                }
            } else {
                utf8Str += buf;
            }
        }
        fclose(fp);
        if (hasNonLatin1Content) {
            auto s = utf8StringToUTF16String(utf8Str.data(), utf8Str.length());
            src = new UTF16String(std::move(s));
        } else {
            src = new Char8String(str.data(), str.length());
        }
    } else {
        char msg[1024];
        snprintf(msg, sizeof(msg), "%%s: cannot open file %s", fileName);
        String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, builtinName.string(), msg);
    }
    return src;
}

static Value builtinLoad(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto f = argv[0].toString(state)->toUTF8StringData();
    const char* fileName = f.data();

    String* src = builtinHelperFileRead(state, fileName, state.context()->staticStrings().load);
    Context* context = state.context();
    auto result = context->scriptParser().parse(src, argv[0].toString(state));
    if (!result.m_error) {
        return result.m_script->execute(state, false, false, true);
    } else {
        auto err = result.m_error->message->toUTF8StringData();
        String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
        char msg[1024];
        snprintf(msg, sizeof(msg), "%%s: %s", err.data());
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, globalObjectString, false, state.context()->staticStrings().load.string(), msg);
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinRead(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto f = argv[0].toString(state)->toUTF8StringData();
    const char* fileName = f.data();
    String* src = builtinHelperFileRead(state, fileName, state.context()->staticStrings().read);
    return src;
}

static Value builtinRun(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double startTime = DateObject::currentTime();

    auto f = argv[0].toString(state)->toUTF8StringData();
    const char* fileName = f.data();
    String* src = builtinHelperFileRead(state, fileName, state.context()->staticStrings().run);
    Context* context = state.context();
    auto result = context->scriptParser().parse(src, argv[0].toString(state));
    if (!result.m_error) {
        result.m_script->execute(state, false, false, true);
        return Value(DateObject::currentTime() - startTime);
    } else {
        auto err = result.m_error->message->toUTF8StringData();
        String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
        char msg[1024];
        snprintf(msg, sizeof(msg), "%%s: %s", err.data());
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, globalObjectString, false, state.context()->staticStrings().load.string(), msg);
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#endif

static Value builtinGc(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GC_gcollect_and_unmap();
    return Value();
}

class EvalFunctionObject : public FunctionObject {
public:
    EvalFunctionObject(ExecutionState& state, NativeFunctionInfo info)
        : FunctionObject(state, info)
    {
        m_globalObject = state.context()->globalObject();
    }

    GlobalObject* m_globalObject;
};

static Value builtinEval(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    EvalFunctionObject* fn = (EvalFunctionObject*)state.executionContext()->resolveCallee();
    return fn->m_globalObject->eval(state, argv[0]);
}

ObjectGetResult GlobalObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ObjectGetResult r = Object::getOwnProperty(state, P);
    if (!r.hasValue() && UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
        Object* target = getPrototypeObject(state);
        while (target) {
            auto result = target->getOwnProperty(state, P);
            if (result.hasValue()) {
                return r;
            }
            target = target->getPrototypeObject(state);
        }
        Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, P.toPlainValue(state));
        if (!virtialIdResult.isEmpty())
            return ObjectGetResult(virtialIdResult, true, true, true);
    }
    return r;
}

Value GlobalObject::eval(ExecutionState& state, const Value& arg)
{
    if (arg.isString()) {
        if (UNLIKELY((bool)state.context()->securityPolicyCheckCallback())) {
            Value checkMSG = state.context()->securityPolicyCheckCallback()(state, true);
            if (!checkMSG.isEmpty()) {
                ASSERT(checkMSG.isString());
                ErrorObject* err = ErrorObject::createError(state, ErrorObject::Code::EvalError, checkMSG.asString());
                state.throwException(err);
                return Value();
            }
        }
        ScriptParser parser(state.context());
        const char* s = "eval input";
        ExecutionContext* pec = state.executionContext();
        bool strictFromOutside = false;

        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (state.stackBase() - currentStackBase);
#else
        size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (currentStackBase - state.stackBase());
#endif
        ScriptParser::ScriptParserResult parserResult = parser.parse(StringView(arg.asString(), 0, arg.asString()->length()), String::fromUTF8(s, strlen(s)), nullptr, strictFromOutside, false, stackRemainApprox);
        if (parserResult.m_error) {
            ErrorObject* err = ErrorObject::createError(state, parserResult.m_error->errorCode, parserResult.m_error->message);
            state.throwException(err);
        }
        bool needNewEnv = parserResult.m_script->topCodeBlock()->isStrict();
        // In case of indirect call, use global execution context
        ExecutionState stateForNewGlobal(m_context);
        return parserResult.m_script->execute(stateForNewGlobal, true, needNewEnv, true);
    }
    return arg;
}

Value GlobalObject::evalLocal(ExecutionState& state, const Value& arg, Value thisValue, InterpretedCodeBlock* parentCodeBlock)
{
    if (arg.isString()) {
        if (UNLIKELY((bool)state.context()->securityPolicyCheckCallback())) {
            Value checkMSG = state.context()->securityPolicyCheckCallback()(state, true);
            if (!checkMSG.isEmpty()) {
                ASSERT(checkMSG.isString());
                ErrorObject* err = ErrorObject::createError(state, ErrorObject::Code::EvalError, checkMSG.asString());
                state.throwException(err);
                return Value();
            }
        }
        ScriptParser parser(state.context());
        const char* s = "eval input";
        ExecutionContext* pec = state.executionContext();
        bool isDirectCall = true;
        bool isEvalCodeInFunction = false;
        bool strictFromOutside = state.inStrictMode();
        while (pec) {
            if (pec->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord()) {
                strictFromOutside = pec->inStrictMode();
                DeclarativeEnvironmentRecord* record = pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord();
                if (record->isEvalTarget()) {
                    isEvalCodeInFunction = true;
                    break;
                }

                LexicalEnvironment* env = pec->lexicalEnvironment();

                while (!env->record()->isEvalTarget()) {
                    env = env->outerEnvironment();
                }

                isEvalCodeInFunction = !env->record()->isGlobalEnvironmentRecord();
                break;
            } else if (pec->lexicalEnvironment()->record()->isGlobalEnvironmentRecord()) {
                strictFromOutside = pec->inStrictMode();
                break;
            }
            pec = pec->parent();
        }
        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (state.stackBase() - currentStackBase);
#else
        size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (currentStackBase - state.stackBase());
#endif
        ScriptParser::ScriptParserResult parserResult = parser.parse(StringView(arg.asString(), 0, arg.asString()->length()), String::fromUTF8(s, strlen(s)), parentCodeBlock, strictFromOutside, isEvalCodeInFunction, stackRemainApprox);
        if (parserResult.m_error) {
            ErrorObject* err = ErrorObject::createError(state, parserResult.m_error->errorCode, parserResult.m_error->message);
            state.throwException(err);
        }
        bool needNewEnv = parserResult.m_script->topCodeBlock()->isStrict();
        return parserResult.m_script->executeLocal(state, thisValue, parentCodeBlock, true, needNewEnv);
    }
    return arg;
}

static int parseDigit(char16_t c, int radix)
{
    int digit = -1;

    if (c >= '0' && c <= '9')
        digit = c - '0';
    else if (c >= 'A' && c <= 'Z')
        digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        digit = c - 'a' + 10;

    if (digit >= radix)
        return -1;

    return digit;
}

static const int SizeOfInfinity = 8;

static bool isInfinity(String* str, unsigned p, unsigned length)
{
    return (length - p) >= SizeOfInfinity
        && str->charAt(p) == 'I'
        && str->charAt(p + 1) == 'n'
        && str->charAt(p + 2) == 'f'
        && str->charAt(p + 3) == 'i'
        && str->charAt(p + 4) == 'n'
        && str->charAt(p + 5) == 'i'
        && str->charAt(p + 6) == 't'
        && str->charAt(p + 7) == 'y';
}


static Value builtinParseInt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value ret;

    // 1. Let inputString be ToString(string).
    Value input = argv[0];
    String* s = input.toString(state);

    // 2. Let S be a newly created substring of inputString consisting of the first character that is not a StrWhiteSpaceChar
    //    and all characters following that character. (In other words, remove leading white space.)
    unsigned p = 0;
    unsigned strLen = s->length();

    for (; p < strLen; p++) {
        if (!(EscargotLexer::isWhiteSpaceOrLineTerminator(s->charAt(p))))
            break;
    }

    // 3. Let sign be 1.
    // 4. If S is not empty and the first character of S is a minus sign -, let sign be −1.
    // 5. If S is not empty and the first character of S is a plus sign + or a minus sign -, then remove the first character from S.
    double sign = 1;
    if (p < strLen) {
        if (s->charAt(p) == '+') {
            p++;
        } else if (s->charAt(p) == '-') {
            sign = -1;
            p++;
        }
    }

    // 6. Let R = ToInt32(radix).
    // 7. Let stripPrefix be true.
    // 8. If R ≠ 0, then
    //    b. If R 16, let stripPrefix be false.
    // 9. Else, R = 0
    //    a. Let R = 10.
    // 10. If stripPrefix is true, then
    //     a. If the length of S is at least 2 and the first two characters of S are either “0x” or “0X”, then remove the first two characters from S and let R = 16.
    // 11. If S contains any character that is not a radix-R digit, then let Z be the substring of S consisting of all characters
    //     before the first such character; otherwise, let Z be S.
    int radix = 0;
    if (argc >= 2) {
        radix = argv[1].toInt32(state);
    }
    if ((radix == 0 || radix == 16) && strLen - p >= 2 && s->charAt(p) == '0' && (s->charAt(p + 1) == 'x' || s->charAt(p + 1) == 'X')) {
        radix = 16;
        p += 2;
    }
    if (radix == 0)
        radix = 10;

    // 8.a If R < 2 or R > 36, then return NaN.
    if (radix < 2 || radix > 36)
        return Value(std::numeric_limits<double>::quiet_NaN());

    // 13. Let mathInt be the mathematical integer value that is represented by Z in radix-R notation,
    //     using the letters AZ and az for digits with values 10 through 35. (However, if R is 10 and Z contains more than 20 significant digits,
    //     every significant digit after the 20th may be replaced by a 0 digit, at the option of the implementation;
    //     and if R is not 2, 4, 8, 10, 16, or 32, then mathInt may be an implementation-dependent approximation to the mathematical integer value
    //     that is represented by Z in radix-R notation.)
    // 14. Let number be the Number value for mathInt.
    bool sawDigit = false;
    double number = 0.0;
    while (p < strLen) {
        int digit = parseDigit(s->charAt(p), radix);
        if (digit == -1)
            break;
        sawDigit = true;
        number *= radix;
        number += digit;
        p++;
    }

    // 12. If Z is empty, return NaN.
    if (!sawDigit)
        return Value(std::numeric_limits<double>::quiet_NaN());

    // 15. Return sign × number.
    return Value(sign * number);
}

static Value builtinParseFloat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 1. Let inputString be ToString(string).
    Value input = argv[0];
    String* s = input.toString(state);
    size_t strLen = s->length();

    if (strLen == 1) {
        if (isdigit(s->charAt(0)))
            return Value(s->charAt(0) - '0');
        return Value(std::numeric_limits<double>::quiet_NaN());
    }

    // 2, Let trimmedString be a substring of inputString consisting of the leftmost character
    //    that is not a StrWhiteSpaceChar and all characters to the right of that character.
    //    (In other words, remove leading white space.)
    unsigned p = 0;
    unsigned len = s->length();

    for (; p < len; p++) {
        if (!(EscargotLexer::isWhiteSpaceOrLineTerminator(s->charAt(p))))
            break;
    }

    // empty string
    if (p == len)
        return Value(std::numeric_limits<double>::quiet_NaN());

    char16_t ch = s->charAt(p);
    // HexIntegerLiteral
    if (len - p > 1 && ch == '0' && toupper(s->charAt(p + 1)) == 'X')
        return Value(0);

    // 3. If neither trimmedString nor any prefix of trimmedString satisfies the syntax of
    //    a StrDecimalLiteral (see 9.3.1), return NaN.
    // 4. Let numberString be the longest prefix of trimmedString, which might be trimmedString itself,
    //    that satisfies the syntax of a StrDecimalLiteral.
    // Check the syntax of StrDecimalLiteral
    switch (ch) {
    case 'I':
        if (isInfinity(s, p, len))
            return Value(std::numeric_limits<double>::infinity());
        break;
    case '+':
        if (isInfinity(s, p + 1, len))
            return Value(std::numeric_limits<double>::infinity());
        break;
    case '-':
        if (isInfinity(s, p + 1, len))
            return Value(-std::numeric_limits<double>::infinity());
        break;
    }
    auto u8Str = s->substring(p, len)->toUTF8StringData();
    double number = atof(u8Str.data());
    if (number == 0.0 && !std::signbit(number) && !isdigit(ch) && !(len - p >= 1 && (ch == '.' || ch == '+') && isdigit(s->charAt(p + 1))))
        return Value(std::numeric_limits<double>::quiet_NaN());
    if (number == std::numeric_limits<double>::infinity())
        return Value(std::numeric_limits<double>::quiet_NaN());

    // 5. Return the Number value for the MV of numberString.
    return Value(number);
}

static Value builtinIsFinite(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double num = argv[0].toNumber(state);
    if (std::isnan(num) || num == std::numeric_limits<double>::infinity() || num == -std::numeric_limits<double>::infinity())
        return Value(Value::False);
    else
        return Value(Value::True);
}

static Value builtinIsNaN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double num = argv[0].toNumber(state);
    if (std::isnan(num)) {
        return Value(Value::True);
    }
    return Value(Value::False);
}

/* ES5 15.1.3 URI Handling Functions */

// [uriReserved] ; / ? : @ & = + $ ,
inline static bool isURIReservedOrSharp(char16_t ch)
{
    return ch == ';' || ch == '/' || ch == '?' || ch == ':' || ch == '@' || ch == '&'
        || ch == '=' || ch == '+' || ch == '$' || ch == ',' || ch == '#';
}

inline static bool isURIAlpha(char16_t ch)
{
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
}

inline static bool isURIMark(char16_t ch)
{
    return (ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*'
            || ch == '\'' || ch == '(' || ch == ')');
}

inline static bool isDecimalDigit(char16_t ch)
{
    return ('0' <= ch && ch <= '9');
}

inline static bool isHexadecimalDigit(char16_t ch)
{
    return isDecimalDigit(ch) || ('A' <= ch && ch <= 'F') || ('a' <= ch && ch <= 'f');
}

inline static bool twocharToHexaDecimal(char16_t ch1, char16_t ch2, unsigned char* res)
{
    if (!isHexadecimalDigit(ch1) || !isHexadecimalDigit(ch2))
        return false;
    *res = (((ch1 & 0x10) ? (ch1 & 0xf) : ((ch1 & 0xf) + 9)) << 4)
        | ((ch2 & 0x10) ? (ch2 & 0xf) : ((ch2 & 0xf) + 9));
    return true;
}

inline static bool codeUnitToHexaDecimal(String* str, size_t start, unsigned char* res)
{
    ASSERT(str && str->length() > start + 2);
    if (str->charAt(start) != '%')
        return false;
    bool succeed = twocharToHexaDecimal(str->charAt(start + 1), str->charAt(start + 2), res);
    // The two most significant bits of res should be 10.
    return succeed && (*res & 0xC0) == 0x80;
}

static Value decode(ExecutionState& state, String* uriString, bool noComponent, String* funcName)
{
    String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
    StringBuilder unescaped;
    size_t strLen = uriString->length();

    for (size_t i = 0; i < strLen; i++) {
        char16_t t = uriString->charAt(i);
        if (t != '%') {
            unescaped.appendChar(t);
        } else {
            size_t start = i;
            if (i + 2 >= strLen)
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
            char16_t next = uriString->charAt(i + 1);
            char16_t nextnext = uriString->charAt(i + 2);

            // char to hex
            unsigned char b = 0;
            if (!twocharToHexaDecimal(next, nextnext, &b))
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
            i += 2;

            // most significant bit in b is 0
            if (!(b & 0x80)) {
                // let C be the character with code unit value B.
                // if C is not in reservedSet, then let S be the String containing only the character C.
                // else, C is in reservedSet, Let S be the substring of string from position start to position k included.
                const char16_t c = b & 0x7f;
                if (noComponent && isURIReservedOrSharp(c)) {
                    unescaped.appendSubString(uriString, start, start + 3);
                } else {
                    unescaped.appendChar(c);
                }
            } else { // most significant bit in b is 1
                unsigned char b_tmp = b;
                int n = 1;
                while (n < 5) {
                    b_tmp <<= 1;
                    if ((b_tmp & 0x80) == 0) {
                        break;
                    }
                    n++;
                }
                if (n == 1 || n == 5 || (i + (3 * (n - 1)) >= strLen)) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
                }
                unsigned char octets[4];
                octets[0] = b;

                int j = 1;
                while (j < n) {
                    if (!codeUnitToHexaDecimal(uriString, ++i, &b)) // "%XY" type
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
                    i += 2;
                    octets[j] = b;
                    j++;
                }
                ASSERT(n == 2 || n == 3 || n == 4);
                unsigned int v = 0;
                if (n == 2) {
                    v = (octets[0] & 0x1F) << 6 | (octets[1] & 0x3F);
                    if ((octets[0] == 0xC0) || (octets[0] == 0xC1)) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
                    }
                } else if (n == 3) {
                    v = (octets[0] & 0x0F) << 12 | (octets[1] & 0x3F) << 6 | (octets[2] & 0x3F);
                    if ((0xD800 <= v && v <= 0xDFFF) || ((octets[0] == 0xE0) && ((octets[1] < 0xA0) || (octets[1] > 0xBF)))) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
                    }
                } else if (n == 4) {
                    v = (octets[0] & 0x07) << 18 | (octets[1] & 0x3F) << 12 | (octets[2] & 0x3F) << 6 | (octets[3] & 0x3F);
                    if ((octets[0] == 0xF0) && ((octets[1] < 0x90) || (octets[1] > 0xBF))) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
                    }
                }
                if (v >= 0x10000) {
                    const char16_t l = (((v - 0x10000) & 0x3ff) + 0xdc00);
                    const char16_t h = ((((v - 0x10000) >> 10) & 0x3ff) + 0xd800);
                    unescaped.appendChar(h);
                    unescaped.appendChar(l);
                } else {
                    const char16_t l = v & 0xFFFF;
                    unescaped.appendChar(l);
                }
            }
        }
    }
    return unescaped.finalize(&state);
}

static Value builtinDecodeURI(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0)
        return Value();
    return decode(state, argv[0].toString(state), true, state.context()->staticStrings().decodeURI.string());
}

static Value builtinDecodeURIComponent(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0)
        return Value();
    return decode(state, argv[0].toString(state), false, state.context()->staticStrings().decodeURIComponent.string());
}

// make three code unit "%XY" and append to string builder
static inline bool convertAndAppendCodeUnit(StringBuilder* builder, char16_t ch)
{
    unsigned char dig1 = (ch & 0xF0) >> 4;
    unsigned char dig2 = (ch & 0x0F);
    if (dig1 > 15 || dig2 > 15)
        return false;
    char ch1 = (dig1 <= 9) ? dig1 + '0' : dig1 - 10 + 'A';
    char ch2 = (dig2 <= 9) ? dig2 + '0' : dig2 - 10 + 'A';

    builder->appendChar('%');
    builder->appendChar(ch1);
    builder->appendChar(ch2);
    return true;
}

static Value encode(ExecutionState& state, String* uriString, bool noComponent, String* funcName)
{
    String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
    int strLen = uriString->length();

    StringBuilder escaped;
    for (int i = 0; i < strLen; i++) {
        char16_t t = uriString->charAt(i);
        if (isDecimalDigit(t) || isURIAlpha(t) || isURIMark(t)
            || (noComponent && isURIReservedOrSharp(t))) {
            escaped.appendChar(uriString->charAt(i));
        } else if (t <= 0x007F) {
            convertAndAppendCodeUnit(&escaped, t);
        } else if (0x0080 <= t && t <= 0x07FF) {
            convertAndAppendCodeUnit(&escaped, 0x00C0 + (t & 0x07C0) / 0x0040);
            convertAndAppendCodeUnit(&escaped, 0x0080 + (t & 0x003F));
        } else if ((0x0800 <= t && t <= 0xD7FF)
                   || (0xE000 <= t /* && t <= 0xFFFF*/)) {
            convertAndAppendCodeUnit(&escaped, 0x00E0 + (t & 0xF000) / 0x1000);
            convertAndAppendCodeUnit(&escaped, 0x0080 + (t & 0x0FC0) / 0x0040);
            convertAndAppendCodeUnit(&escaped, 0x0080 + (t & 0x003F));
        } else if (0xD800 <= t && t <= 0xDBFF) {
            if (i + 1 < strLen && 0xDC00 <= uriString->charAt(i + 1) && uriString->charAt(i + 1) <= 0xDFFF) {
                int index = (t - 0xD800) * 0x400 + (uriString->charAt(i + 1) - 0xDC00) + 0x10000;
                convertAndAppendCodeUnit(&escaped, 0x00F0 + (index & 0x1C0000) / 0x40000);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x3F000) / 0x1000);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x0FC0) / 0x0040);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x003F));
                i++;
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
            }
        } else if (0xDC00 <= t && t <= 0xDFFF) {
            ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, errorMessage_GlobalObject_MalformedURI);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    return escaped.finalize(&state);
}

static Value builtinEncodeURI(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0)
        return Value();
    return encode(state, argv[0].toString(state), true, state.context()->staticStrings().encodeURI.string());
}

static Value builtinEncodeURIComponent(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0)
        return Value();
    return encode(state, argv[0].toString(state), false, state.context()->staticStrings().encodeURIComponent.string());
}


ASCIIStringDataNonGCStd char2hex(char dec)
{
    unsigned char dig1 = (dec & 0xF0) >> 4;
    unsigned char dig2 = (dec & 0x0F);
    if (dig1 <= 9)
        dig1 += 48; // 0, 48inascii
    if (10 <= dig1 && dig1 <= 15)
        dig1 += 65 - 10; // a, 97inascii
    if (dig2 <= 9)
        dig2 += 48;
    if (10 <= dig2 && dig2 <= 15)
        dig2 += 65 - 10;
    ASCIIStringDataNonGCStd r;
    char dig1_appended = static_cast<char>(dig1);
    char dig2_appended = static_cast<char>(dig2);
    r.append(&dig1_appended, 1);
    r.append(&dig2_appended, 1);
    return r;
}
ASCIIStringDataNonGCStd char2hex4digit(char16_t dec)
{
    char dig[4];
    ASCIIStringDataNonGCStd r;
    for (int i = 0; i < 4; i++) {
        dig[i] = (dec & (0xF000 >> i * 4)) >> (12 - i * 4);
        if (dig[i] <= 9)
            dig[i] += 48; // 0, 48inascii
        if (10 <= dig[i] && dig[i] <= 15)
            dig[i] += 65 - 10; // a, 97inascii
        r.append(&dig[i], 1);
    }
    return r;
}


static Value builtinEscape(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    String* str = argv[0].toString(state);
    size_t length = str->length();
    ASCIIStringDataNonGCStd R;

    size_t len = 0;
    for (size_t i = 0; i < length; i++) {
        char16_t t = str->charAt(i);
        if ((48 <= t && t <= 57) // DecimalDigit
            || (65 <= t && t <= 90) // uriAlpha - upper case
            || (97 <= t && t <= 122) // uriAlpha - lower case
            || t == '@' || t == '*' || t == '_' || t == '+' || t == '-' || t == '.' || t == '/') {
            R.push_back(t);
            len += 1;
        } else if (t < 256) {
            // %xy
            R.append("%");
            R.append(char2hex(t));

            len = R.length();
        } else {
            // %uwxyz
            R.append("%u");
            R.append(char2hex4digit(t));

            len = R.length();
        }

        if (UNLIKELY(len > STRING_MAXIMUM_LENGTH)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
        }
    }

    return new Char8String(R.data(), R.size());
}

char16_t hex2char(char16_t first, char16_t second)
{
    char16_t dig1 = first;
    char16_t dig2 = second;
    if (48 <= dig1 && dig1 <= 57)
        dig1 -= 48;
    if (65 <= dig1 && dig1 <= 70)
        dig1 -= 65 - 10;
    if (97 <= dig1 && dig1 <= 102)
        dig1 -= 97 - 10;
    if (48 <= dig2 && dig2 <= 57)
        dig2 -= 48;
    if (65 <= dig2 && dig2 <= 70)
        dig2 -= 65 - 10;
    if (97 <= dig2 && dig2 <= 102)
        dig2 -= 97 - 10;
    char16_t dec = dig1 << 4;
    dec |= dig2;
    return dec;
}
static Value builtinUnescape(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    String* str = argv[0].toString(state);
    size_t length = str->length();
    UTF16StringDataNonGCStd R;
    bool unescapeValue = false;
    bool gotNonASCIIString = false;

    for (size_t i = 0; i < length; i++) {
        char16_t first = str->charAt(i);
        if (first == '%') {
            if (length - i >= 6) {
                char16_t second = str->charAt(i + 1);
                char16_t third = str->charAt(i + 2);
                if (second == 'u') {
                    char16_t fourth = str->charAt(i + 3);
                    char16_t fifth = str->charAt(i + 4);
                    char16_t sixth = str->charAt(i + 5);
                    // hex dig check
                    if (((48 <= third && third <= 57) || (65 <= third && third <= 70) || (97 <= third && third <= 102))
                        && ((48 <= fourth && fourth <= 57) || (65 <= fourth && fourth <= 70) || (97 <= fourth && fourth <= 102))
                        && ((48 <= fifth && fifth <= 57) || (65 <= fifth && fifth <= 70) || (97 <= fifth && fifth <= 102))
                        && ((48 <= sixth && sixth <= 57) || (65 <= sixth && sixth <= 70) || (97 <= sixth && sixth <= 102))) {
                        char16_t l = hex2char(third, fourth) << 8;
                        l |= hex2char(fifth, sixth);
                        if (l > 128)
                            gotNonASCIIString = true;
                        R.append(&l, 1);
                        i += 5;
                        unescapeValue = true;
                    }
                } else if (((48 <= second && second <= 57) || (65 <= second && second <= 70) || (97 <= second && second <= 102))
                           && ((48 <= third && third <= 57) || (65 <= third && third <= 70) || (97 <= third && third <= 102))) {
                    char16_t l = hex2char(second, third);
                    if (l > 128)
                        gotNonASCIIString = true;
                    R.append(&l, 1);
                    i += 2;
                    unescapeValue = true;
                }
            } else if (length - i >= 3) {
                char16_t second = str->charAt(i + 1);
                char16_t third = str->charAt(i + 2);
                if (((48 <= second && second <= 57) || (65 <= second && second <= 70) || (97 <= second && second <= 102))
                    && ((48 <= third && third <= 57) || (65 <= third && third <= 70) || (97 <= third && third <= 102))) {
                    char16_t l = hex2char(second, third);
                    if (l > 128)
                        gotNonASCIIString = true;
                    R.append(&l, 1);
                    i += 2;
                    unescapeValue = true;
                }
            }
        }
        if (!unescapeValue) {
            char16_t l = str->charAt(i);
            if (l > 128)
                gotNonASCIIString = true;
            R.append(&l, 1);
        }
        unescapeValue = false;
    }

    if (gotNonASCIIString) {
        return new UTF16String(R.data(), R.length());
    } else {
        ASCIIStringData data;
        data.resizeWithUninitializedValues(R.length());
        for (size_t i = 0; i < data.length(); i++) {
            data[i] = R[i];
        }
        return new Char8String(std::move(data));
    }
}

// Object.prototype.__defineGetter__ ( P, getter )
static Value builtinDefineGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // If IsCallable(getter) is false, throw a TypeError exception.
    if (!argv[1].isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, String::emptyString, true, state.context()->staticStrings().__defineGetter__.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    // Let desc be PropertyDescriptor{[[Get]]: getter, [[Enumerable]]: true, [[Configurable]]: true}.
    ObjectPropertyDescriptor desc(JSGetterSetter(argv[1].asFunction(), Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));

    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Perform ? DefinePropertyOrThrow(O, key, desc).
    // https://tc39.github.io/ecma262/#sec-object.prototype.__defineGetter__
    // says we should use 'Object::defineOwnPropertyThrowsException'
    // but, other engines not follow that clause
    O->defineOwnProperty(state, key, desc);

    // Return undefined.
    return Value();
}

// Object.prototype.__defineSetter__ ( P, getter )
static Value builtinDefineSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // If IsCallable(getter) is false, throw a TypeError exception.
    if (!argv[1].isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, String::emptyString, true, state.context()->staticStrings().__defineSetter__.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    // Let desc be PropertyDescriptor{[[Get]]: getter, [[Enumerable]]: true, [[Configurable]]: true}.
    ObjectPropertyDescriptor desc(JSGetterSetter(Value(Value::EmptyValue), argv[1].asFunction()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));

    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Perform ? DefinePropertyOrThrow(O, key, desc).
    // https://tc39.github.io/ecma262/#sec-object.prototype.__defineSetter__
    // says we should use 'Object::defineOwnPropertyThrowsException'
    // but, other engines not follow that clause
    O->defineOwnProperty(state, key, desc);

    // Return undefined.
    return Value();
}

// Object.prototype.__lookupGetter__ ( P, getter )
static Value builtinLookupGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Repeat,
    while (O) {
        // Let desc be ? O.[[GetOwnProperty]](key).
        auto desc = O->getOwnProperty(state, key);

        // If desc is not undefined, then
        if (desc.hasValue()) {
            // If IsAccessorDescriptor(desc) is true, return desc.[[Get]].
            if (!desc.isDataProperty() && desc.jsGetterSetter()->hasGetter()) {
                return desc.jsGetterSetter()->getter();
            }
            // Return undefined.
            return Value();
        }
        // Set O to ? O.[[GetPrototypeOf]]().
        O = O->getPrototypeObject(state);
        // If O is null, return undefined.
    }
    return Value();
}

// Object.prototype.__lookupSetter__ ( P, getter )
static Value builtinLookupSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Repeat,
    while (O) {
        // Let desc be ? O.[[GetOwnProperty]](key).
        auto desc = O->getOwnProperty(state, key);

        // If desc is not undefined, then
        if (desc.hasValue()) {
            // If IsAccessorDescriptor(desc) is true, return desc.[[Set]].
            if (!desc.isDataProperty() && desc.jsGetterSetter()->hasSetter()) {
                return desc.jsGetterSetter()->setter();
            }
            // Return undefined.
            return Value();
        }
        // Set O to ? O.[[GetPrototypeOf]]().
        O = O->getPrototypeObject(state);
        // If O is null, return undefined.
    }
    return Value();
}

void GlobalObject::installOthers(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    defineOwnProperty(state, strings->Infinity, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::infinity()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));
    defineOwnProperty(state, strings->NaN, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::quiet_NaN()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));
    defineOwnProperty(state, strings->undefined, ObjectPropertyDescriptor(Value(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // $18.2.1 eval (x)
    m_eval = new EvalFunctionObject(state,
                                    NativeFunctionInfo(strings->eval, builtinEval, 1, nullptr, NativeFunctionInfo::Strict));
    defineOwnProperty(state, ObjectPropertyName(strings->eval),
                      ObjectPropertyDescriptor(m_eval, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $18.2.2 isFinite(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isFinite),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->isFinite, builtinIsFinite, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $18.2.3 isNaN(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isNaN),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->isNaN, builtinIsNaN, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* parseInt = new FunctionObject(state, NativeFunctionInfo(strings->parseInt, builtinParseInt, 2, nullptr, NativeFunctionInfo::Strict));
    defineOwnProperty(state, ObjectPropertyName(strings->parseInt),
                      ObjectPropertyDescriptor(parseInt,
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_number->defineOwnProperty(state, ObjectPropertyName(strings->parseInt),
                                ObjectPropertyDescriptor(parseInt,
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* parseFloat = new FunctionObject(state, NativeFunctionInfo(strings->parseFloat, builtinParseFloat, 1, nullptr, NativeFunctionInfo::Strict));
    defineOwnProperty(state, ObjectPropertyName(strings->parseFloat),
                      ObjectPropertyDescriptor(parseFloat,
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_number->defineOwnProperty(state, ObjectPropertyName(strings->parseFloat),
                                ObjectPropertyDescriptor(parseFloat,
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->encodeURI),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->encodeURI, builtinEncodeURI, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->decodeURI),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->decodeURI, builtinDecodeURI, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->encodeURIComponent),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->encodeURIComponent, builtinEncodeURIComponent, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->decodeURIComponent),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->decodeURIComponent, builtinDecodeURIComponent, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->escape),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->escape, builtinEscape, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->unescape),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->unescape, builtinUnescape, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings->__defineGetter__),
                                         ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                     NativeFunctionInfo(strings->__defineGetter__, builtinDefineGetter, 2, nullptr, 0)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings->__defineSetter__),
                                         ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                     NativeFunctionInfo(strings->__defineSetter__, builtinDefineSetter, 2, nullptr, 0)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings->__lookupGetter__),
                                         ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                     NativeFunctionInfo(strings->__lookupGetter__, builtinLookupGetter, 1, nullptr, 0)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings->__lookupSetter__),
                                         ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                     NativeFunctionInfo(strings->__lookupSetter__, builtinLookupSetter, 1, nullptr, 0)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

#ifdef ESCARGOT_SHELL
    defineOwnProperty(state, ObjectPropertyName(strings->print),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->print, builtinPrint, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    defineOwnProperty(state, ObjectPropertyName(strings->load),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->load, builtinLoad, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    defineOwnProperty(state, ObjectPropertyName(strings->read),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->read, builtinRead, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    defineOwnProperty(state, ObjectPropertyName(strings->run),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->run, builtinRun, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
/*
    defineOwnProperty(state, ObjectPropertyName(strings->dbgBreak),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->dbgBreak, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression) -> Value {
                                                                      puts("dbgBreak");
                                                                      return Value();
                                                                  },
                                                                                     0, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
*/
#endif

    defineOwnProperty(state, ObjectPropertyName(strings->gc),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->gc, builtinGc, 0, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

#ifdef PROFILE_BDWGC
    AtomicString dumpBackTrace(state, "dumpBackTrace");
    defineOwnProperty(state, ObjectPropertyName(dumpBackTrace),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(dumpBackTrace, builtinDumpBackTrace, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString registerLeakCheck(state, "registerLeakCheck");
    defineOwnProperty(state, ObjectPropertyName(registerLeakCheck),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(registerLeakCheck, builtinRegisterLeakCheck, 2, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString setPhaseName(state, "setPhaseName");
    defineOwnProperty(state, ObjectPropertyName(setPhaseName),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(setPhaseName, builtinSetGCPhaseName, 1, nullptr, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif

    m_stringProxyObject = new StringObject(state);
    m_numberProxyObject = new NumberObject(state);
}

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
static String* icuLocaleToBCP47Tag(String* string)
{
    StringBuilder sb;
    for (size_t i = 0; i < string->length(); i++) {
        char16_t ch = string->charAt(i);
        if (ch == '_')
            ch = '-';
        sb.appendChar(ch);
    }
    return sb.finalize();
}

const Vector<String*, gc_allocator<String*>>& GlobalObject::intlCollatorAvailableLocales()
{
    if (m_intlCollatorAvailableLocales.size() == 0) {
        auto count = ucol_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String* locale = String::fromASCII(ucol_getAvailable(i));
            locale = icuLocaleToBCP47Tag(locale);
            m_intlCollatorAvailableLocales.pushBack(locale);
        }
    }
    return m_intlCollatorAvailableLocales;
}

const Vector<String*, gc_allocator<String*>>& GlobalObject::intlDateTimeFormatAvailableLocales()
{
    if (m_intlDateTimeFormatAvailableLocales.size() == 0) {
        auto count = udat_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String* locale = String::fromASCII(udat_getAvailable(i));
            locale = icuLocaleToBCP47Tag(locale);
            m_intlDateTimeFormatAvailableLocales.pushBack(locale);
        }
    }
    return m_intlDateTimeFormatAvailableLocales;
}

const Vector<String*, gc_allocator<String*>>& GlobalObject::intlNumberFormatAvailableLocales()
{
    if (m_intlNumberFormatAvailableLocales.size() == 0) {
        auto count = unum_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String* locale = String::fromASCII(unum_getAvailable(i));
            locale = icuLocaleToBCP47Tag(locale);
            m_intlNumberFormatAvailableLocales.pushBack(locale);
        }
    }
    return m_intlNumberFormatAvailableLocales;
}
#endif
}
