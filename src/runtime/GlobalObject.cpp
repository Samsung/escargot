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
 *  version 2.1 of the License, or (at your option) any later version.
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
#include "ArrayObject.h"
#include "ErrorObject.h"
#include "StringObject.h"
#include "NumberObject.h"
#include "BooleanObject.h"
#include "SymbolObject.h"
#include "BigIntObject.h"
#include "DateObject.h"
#include "NativeFunctionObject.h"
#include "parser/Lexer.h"
#include "parser/ScriptParser.h"
#include "heap/LeakCheckerBridge.h"
#include "EnvironmentRecord.h"
#include "Environment.h"

namespace Escargot {

GlobalObject::GlobalObject(ExecutionState& state)
    : PrototypeObject(state, PrototypeObject::__ForGlobalBuiltin__)
    , m_context(state.context())
#define INIT_BUILTIN_VALUE(builtin, TYPE, objName) \
    , m_##builtin(nullptr)

          GLOBALOBJECT_BUILTIN_ALL_LIST(INIT_BUILTIN_VALUE)
#undef INIT_BUILTIN_VALUE
{
    // m_objectPrototype should be initialized ahead of any other builtins
    m_objectPrototype = Object::createBuiltinObjectPrototype(state);

    Object::setPrototype(state, m_objectPrototype);
    Object::setGlobalIntrinsicObject(state);
}

void GlobalObject::initializeBuiltins(ExecutionState& state)
{
    // m_objectPrototype has been initialized ahead of any other builtins
    ASSERT(!!m_objectPrototype);

    /*
       initialize all global builtin properties by calling initialize##objName method
       Object, Function and other prerequisite builtins are installed ahead
    */
#define DECLARE_BUILTIN_INIT_FUNC(OBJNAME, objName, ARG) \
    initialize##objName(state);

    GLOBALOBJECT_BUILTIN_OBJECT_LIST(DECLARE_BUILTIN_INIT_FUNC, )
#undef DECLARE_BUILTIN_INIT_FUNC
}

Value builtinSpeciesGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return thisValue;
}

#if defined(ESCARGOT_ENABLE_TEST)
static Value builtinIsFunctionAllocatedOnStack(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool result = false;

    Value arg = argv[0];
    if (arg.isObject() && arg.asObject()->isScriptFunctionObject()) {
        result = arg.asObject()->asScriptFunctionObject()->interpretedCodeBlock()->canAllocateEnvironmentOnStack();
    }

    return Value(result);
}

static Value builtinIsBlockAllocatedOnStack(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool result = false;

    Value arg = argv[0];
    auto blockIndex = argv[1].toInt32(state);
    if (arg.isObject() && arg.asObject()->isScriptFunctionObject()) {
        auto bi = arg.asObject()->asScriptFunctionObject()->interpretedCodeBlock()->blockInfo((uint16_t)blockIndex);
        if (bi) {
            result = bi->m_canAllocateEnvironmentOnStack;
        }
    }

    return Value(result);
}
#endif

static Value builtinEval(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    NativeFunctionObject* fn = (NativeFunctionObject*)state.resolveCallee();
    Context* realm = fn->codeBlock()->context();
    return realm->globalObject()->eval(state, argv[0]);
}

ObjectHasPropertyResult GlobalObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    ObjectHasPropertyResult hasResult = Object::hasProperty(state, P);
    if (!hasResult.hasProperty() && UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
        Object* target = getPrototypeObject(state);
        while (target) {
            ObjectHasPropertyResult targetHasProperty = target->hasProperty(state, P);
            if (targetHasProperty.hasProperty()) {
                return hasResult;
            }
            target = target->getPrototypeObject(state);
        }
        Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, P.toPlainValue());
        if (!virtialIdResult.isEmpty()) {
            return ObjectHasPropertyResult(ObjectGetResult(virtialIdResult, true, true, true));
        }
    }

    return hasResult;
}

ObjectGetResult GlobalObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
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
        Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, P.toPlainValue());
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
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::EvalError, checkMSG.asString());
                return Value();
            }
        }
        ScriptParser parser(state.context());
        bool strictFromOutside = false;

        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        size_t stackRemainApprox = currentStackBase - state.stackLimit();
#else
        size_t stackRemainApprox = state.stackLimit() - currentStackBase;
#endif

        Script* script = parser.initializeScript(nullptr, 0, arg.asString(), state.context()->staticStrings().lazyEvalInput().string(), nullptr, false, true, false, false, strictFromOutside, false, false, false, true, stackRemainApprox).scriptThrowsExceptionIfParseError(state);
        // In case of indirect call, use global execution context
        ExecutionState stateForNewGlobal(m_context);
        return script->execute(stateForNewGlobal, true, script->topCodeBlock()->isStrict());
    }
    return arg;
}

Value GlobalObject::evalLocal(ExecutionState& state, const Value& arg, Value thisValue,
                              InterpretedCodeBlock* parentCodeBlock, bool inWithOperation)
{
    if (arg.isString()) {
        if (UNLIKELY((bool)state.context()->securityPolicyCheckCallback())) {
            Value checkMSG = state.context()->securityPolicyCheckCallback()(state, true);
            if (!checkMSG.isEmpty()) {
                ASSERT(checkMSG.isString());
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::EvalError, checkMSG.asString());
                return Value();
            }
        }
        ScriptParser parser(state.context());
        ExecutionState* current = &state;
        bool isRunningEvalOnFunction = state.resolveCallee();
        bool strictFromOutside = state.inStrictMode();
        while (current != nullptr) {
            if (current->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord()) {
                strictFromOutside = current->inStrictMode();
                break;
            } else if (current->lexicalEnvironment()->record()->isGlobalEnvironmentRecord()) {
                strictFromOutside = current->inStrictMode();
                break;
            }
            current = current->parent();
        }

        bool allowNewTarget = false;
        auto thisEnvironment = state.getThisEnvironment();
        if (thisEnvironment->isDeclarativeEnvironmentRecord() && thisEnvironment->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            allowNewTarget = true;
        }

        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        size_t stackRemainApprox = currentStackBase - state.stackLimit();
#else
        size_t stackRemainApprox = state.stackLimit() - currentStackBase;
#endif

        Script* script = parser.initializeScript(nullptr, 0, arg.asString(), state.context()->staticStrings().lazyEvalInput().string(), parentCodeBlock,
                                                 false, true, isRunningEvalOnFunction, inWithOperation, strictFromOutside, parentCodeBlock->allowSuperCall(),
                                                 parentCodeBlock->allowSuperProperty(), allowNewTarget, true, stackRemainApprox)
                             .scriptThrowsExceptionIfParseError(state);
        return script->executeLocal(state, thisValue, parentCodeBlock, script->topCodeBlock()->isStrict(), isRunningEvalOnFunction);
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


static Value builtinParseInt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value builtinParseFloat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value builtinIsFinite(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double num = argv[0].toNumber(state);
    if (std::isnan(num) || num == std::numeric_limits<double>::infinity() || num == -std::numeric_limits<double>::infinity())
        return Value(Value::False);
    else
        return Value(Value::True);
}

static Value builtinIsNaN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
            char16_t next = uriString->charAt(i + 1);
            char16_t nextnext = uriString->charAt(i + 2);

            // char to hex
            unsigned char b = 0;
            if (!twocharToHexaDecimal(next, nextnext, &b))
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
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
                    ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
                }
                unsigned char octets[4];
                octets[0] = b;

                int j = 1;
                while (j < n) {
                    if (!codeUnitToHexaDecimal(uriString, ++i, &b)) // "%XY" type
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
                    i += 2;
                    octets[j] = b;
                    j++;
                }
                ASSERT(n == 2 || n == 3 || n == 4);
                unsigned int v = 0;
                if (n == 2) {
                    v = (octets[0] & 0x1F) << 6 | (octets[1] & 0x3F);
                    if ((octets[0] == 0xC0) || (octets[0] == 0xC1)) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
                    }
                } else if (n == 3) {
                    v = (octets[0] & 0x0F) << 12 | (octets[1] & 0x3F) << 6 | (octets[2] & 0x3F);
                    if ((0xD800 <= v && v <= 0xDFFF) || ((octets[0] == 0xE0) && ((octets[1] < 0xA0) || (octets[1] > 0xBF)))) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
                    }
                } else if (n == 4) {
                    v = (octets[0] & 0x07) << 18 | (octets[1] & 0x3F) << 12 | (octets[2] & 0x3F) << 6 | (octets[3] & 0x3F);
                    if ((octets[0] == 0xF0) && ((octets[1] < 0x90) || (octets[1] > 0xBF))) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
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

static Value builtinDecodeURI(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0)
        return Value();
    return decode(state, argv[0].toString(state), true, state.context()->staticStrings().decodeURI.string());
}

static Value builtinDecodeURIComponent(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0)
        return Value();
    return decode(state, argv[0].toString(state), false, state.context()->staticStrings().decodeURIComponent.string());
}

// make three code unit "%XY" and append to string builder
using URIBuilderString = Vector<char, GCUtil::gc_malloc_atomic_allocator<char>, ComputeReservedCapacityFunctionWithLog2<>>;
static inline bool convertAndAppendCodeUnit(URIBuilderString* escaped, char16_t ch)
{
    unsigned char dig1 = (ch & 0xF0) >> 4;
    unsigned char dig2 = (ch & 0x0F);
    if (dig1 > 15 || dig2 > 15)
        return false;
    char ch1 = (dig1 <= 9) ? dig1 + '0' : dig1 - 10 + 'A';
    char ch2 = (dig2 <= 9) ? dig2 + '0' : dig2 - 10 + 'A';

    escaped->pushBack('%');
    escaped->pushBack(ch1);
    escaped->pushBack(ch2);
    return true;
}

static Value encode(ExecutionState& state, String* uriString, bool noComponent, String* funcName)
{
    String* globalObjectString = state.context()->staticStrings().GlobalObject.string();
    auto bad = uriString->bufferAccessData();

    URIBuilderString escaped;
    escaped.reserve(bad.length * 1.25);

    for (size_t i = 0; i < bad.length; i++) {
        char16_t t = bad.charAt(i);
        if (isDecimalDigit(t) || isURIAlpha(t) || isURIMark(t)
            || (noComponent && isURIReservedOrSharp(t))) {
            escaped.pushBack((char)t);
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
            if (i + 1 < bad.length && 0xDC00 <= bad.charAt(i + 1) && bad.charAt(i + 1) <= 0xDFFF) {
                int index = (t - 0xD800) * 0x400 + (bad.charAt(i + 1) - 0xDC00) + 0x10000;
                convertAndAppendCodeUnit(&escaped, 0x00F0 + (index & 0x1C0000) / 0x40000);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x3F000) / 0x1000);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x0FC0) / 0x0040);
                convertAndAppendCodeUnit(&escaped, 0x0080 + (index & 0x003F));
                i++;
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
            }
        } else if (0xDC00 <= t && t <= 0xDFFF) {
            ErrorObject::throwBuiltinError(state, ErrorObject::URIError, globalObjectString, false, funcName, ErrorObject::Messages::GlobalObject_MalformedURI);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    size_t newLen = escaped.size();
    auto ptr = escaped.takeBuffer();
    return new ASCIIString(ptr, newLen, ASCIIString::FromGCBufferTag);
}

static Value builtinEncodeURI(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0)
        return Value();
    return encode(state, argv[0].toString(state), true, state.context()->staticStrings().encodeURI.string());
}

static Value builtinEncodeURIComponent(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0)
        return Value();
    return encode(state, argv[0].toString(state), false, state.context()->staticStrings().encodeURIComponent.string());
}

void char2hex(char dec, URIBuilderString& result)
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
    char dig1_appended = static_cast<char>(dig1);
    char dig2_appended = static_cast<char>(dig2);
    result.pushBack(dig1_appended);
    result.pushBack(dig2_appended);
}

void char2hex4digit(char16_t dec, URIBuilderString& result)
{
    char dig[4];
    ASCIIStringDataNonGCStd r;
    for (int i = 0; i < 4; i++) {
        dig[i] = (dec & (0xF000 >> i * 4)) >> (12 - i * 4);
        if (dig[i] <= 9)
            dig[i] += 48; // 0, 48inascii
        if (10 <= dig[i] && dig[i] <= 15)
            dig[i] += 65 - 10; // a, 97inascii
        result.pushBack(dig[i]);
    }
}

static Value builtinEscape(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    String* str = argv[0].toString(state);
    auto bad = str->bufferAccessData();
    size_t length = bad.length;
    URIBuilderString R;

    size_t len = 0;
    for (size_t i = 0; i < length; i++) {
        char16_t t = bad.charAt(i);
        if ((48 <= t && t <= 57) // DecimalDigit
            || (65 <= t && t <= 90) // uriAlpha - upper case
            || (97 <= t && t <= 122) // uriAlpha - lower case
            || t == '@' || t == '*' || t == '_' || t == '+' || t == '-' || t == '.' || t == '/') {
            R.push_back(t);
            len += 1;
        } else if (t < 256) {
            // %xy
            R.pushBack('%');
            char2hex(t, R);

            len = R.size();
        } else {
            // %uwxyz
            R.pushBack('%');
            R.pushBack('u');
            char2hex4digit(t, R);

            len = R.size();
        }

        if (UNLIKELY(len > STRING_MAXIMUM_LENGTH)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::String_InvalidStringLength);
        }
    }

    size_t newLen = R.size();
    auto ptr = R.takeBuffer();
    return new ASCIIString(ptr, newLen, ASCIIString::FromGCBufferTag);
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
static Value builtinUnescape(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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
        return new ASCIIString(std::move(data));
    }
}

static Value builtinArrayToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // this builtin function is shared both for Array and TypedArray (prototype.toString property)
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, toString);
    Value toString = thisObject->get(state, state.context()->staticStrings().join).value(state, thisObject);
    if (!toString.isCallable()) {
        toString = state.context()->globalObject()->objectPrototypeToString();
    }
    return Object::call(state, toString, thisObject, 0, nullptr);
}

Value builtinGenericIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isGenericIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, String::fromASCII("Iterator"), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    IteratorObject* iter = thisValue.asObject()->asIteratorObject();
    return iter->next(state);
}

void GlobalObject::initializeOthers(ExecutionState& state)
{
    // Other prerequisite builtins should be installed at the start time
    installOthers(state);
}

void GlobalObject::installOthers(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    defineOwnProperty(state, strings->Infinity, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::infinity()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));
    defineOwnProperty(state, strings->NaN, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::quiet_NaN()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));
    defineOwnProperty(state, strings->undefined, ObjectPropertyDescriptor(Value(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // $18.2.1 eval (x)
    m_eval = new NativeFunctionObject(state,
                                      NativeFunctionInfo(strings->eval, builtinEval, 1, NativeFunctionInfo::Strict));

    defineOwnProperty(state, ObjectPropertyName(strings->eval),
                      ObjectPropertyDescriptor(m_eval, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $18.2.2 isFinite(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isFinite),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->isFinite, builtinIsFinite, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $18.2.3 isNaN(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isNaN),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->isNaN, builtinIsNaN, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    defineOwnProperty(state, ObjectPropertyName(strings->encodeURI),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->encodeURI, builtinEncodeURI, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->decodeURI),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->decodeURI, builtinDecodeURI, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->encodeURIComponent),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->encodeURIComponent, builtinEncodeURIComponent, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->decodeURIComponent),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->decodeURIComponent, builtinDecodeURIComponent, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->escape),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->escape, builtinEscape, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->unescape),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->unescape, builtinUnescape, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // shared builtin methods
    m_parseInt = new NativeFunctionObject(state, NativeFunctionInfo(strings->parseInt, builtinParseInt, 2, NativeFunctionInfo::Strict));
    defineOwnProperty(state, ObjectPropertyName(strings->parseInt),
                      ObjectPropertyDescriptor(m_parseInt,
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_parseFloat = new NativeFunctionObject(state, NativeFunctionInfo(strings->parseFloat, builtinParseFloat, 1, NativeFunctionInfo::Strict));
    defineOwnProperty(state, ObjectPropertyName(strings->parseFloat),
                      ObjectPropertyDescriptor(m_parseFloat,
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayToString = new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinArrayToString, 0, NativeFunctionInfo::Strict));

    // shared builtin objects
    m_asyncIteratorPrototype = new PrototypeObject(state);
    m_asyncIteratorPrototype->setGlobalIntrinsicObject(state, true);
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asynciteratorprototype-asynciterator
    m_asyncIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().asyncIterator),
                                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.asyncIterator]")), builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)),
                                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iteratorPrototype = new PrototypeObject(state);
    m_iteratorPrototype->setGlobalIntrinsicObject(state, true);
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-%iteratorprototype%-@@iterator
    m_iteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.iterator]")), builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)),
                                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_genericIteratorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_genericIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_genericIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinGenericIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_genericIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                                 ObjectPropertyDescriptor(Value(String::fromASCII("Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

#if defined(ESCARGOT_ENABLE_TEST)
    AtomicString isFunctionAllocatedOnStackFunctionName(state, "isFunctionAllocatedOnStack");
    defineOwnProperty(state, ObjectPropertyName(isFunctionAllocatedOnStackFunctionName),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->run, builtinIsFunctionAllocatedOnStack, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    AtomicString isBlockAllocatedOnStackFunctionName(state, "isBlockAllocatedOnStack");
    defineOwnProperty(state, ObjectPropertyName(isBlockAllocatedOnStackFunctionName),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(strings->run, builtinIsBlockAllocatedOnStack, 2, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif

#ifdef PROFILE_BDWGC
    AtomicString dumpBackTrace(state, "dumpBackTrace");
    defineOwnProperty(state, ObjectPropertyName(dumpBackTrace),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(dumpBackTrace, builtinDumpBackTrace, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString registerLeakCheck(state, "registerLeakCheck");
    defineOwnProperty(state, ObjectPropertyName(registerLeakCheck),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(registerLeakCheck, builtinRegisterLeakCheck, 2, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString setPhaseName(state, "setPhaseName");
    defineOwnProperty(state, ObjectPropertyName(setPhaseName),
                      ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                        NativeFunctionInfo(setPhaseName, builtinSetGCPhaseName, 1, NativeFunctionInfo::Strict)),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif

    // https://tc39.es/ecma262/#sec-globalthis
    // The initial value of globalThis is the well-known intrinsic object %GlobalThisValue%.
    // This property has the attributes { [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true }.
    defineOwnProperty(state, ObjectPropertyName(strings->globalThis),
                      ObjectPropertyDescriptor(this, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

} // namespace Escargot
