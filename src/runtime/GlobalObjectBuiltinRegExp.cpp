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
#include "RegExpObject.h"
#include "ArrayObject.h"
#include "GlobalRegExpFunctionObject.h"
#include "NativeFunctionObject.h"

namespace Escargot {

static Value builtinRegExpConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    Value pattern = argv[0];
    Value flags = argv[1];
    String* source = pattern.isUndefined() ? String::emptyString : pattern.toString(state);
    String* option = flags.isUndefined() ? String::emptyString : flags.toString(state);

    // Let patternIsRegExp be IsRegExp(pattern).
    bool patternIsRegExp = argv[0].isObject() && argv[0].asObject()->isRegExp(state);

    if (newTarget.isUndefined()) {
        // Let newTarget be the active function object.
        newTarget = state.resolveCallee();
        // If patternIsRegExp is true and flags is undefined, then
        if (patternIsRegExp && flags.isUndefined()) {
            // Let patternConstructor be Get(pattern, "constructor").
            Value patternConstructor = pattern.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, pattern);
            // If SameValue(patternConstructor, newTarget), then return pattern.
            if (patternConstructor == newTarget) {
                return pattern;
            }
        }
    }

    // If Type(pattern) is Object and pattern has a [[RegExpMatcher]] internal slot, then
    if (pattern.isObject() && pattern.asObject()->isRegExpObject()) {
        RegExpObject* patternRegExp = pattern.asObject()->asRegExpObject();
        // Let P be the value of pattern’s [[OriginalSource]] internal slot.
        source = patternRegExp->source();
        // If flags is undefined, let F be the value of pattern’s [[OriginalFlags]] internal slot.
        // Else, let F be flags.
        option = flags.isUndefined() ? patternRegExp->optionString(state) : flags.toString(state);
    } else if (patternIsRegExp) {
        Value P = pattern.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().source)).value(state, pattern);
        source = P.isUndefined() ? String::emptyString : P.toString(state);
        if (flags.isUndefined()) {
            Value F = pattern.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().flags)).value(state, pattern);
            option = F.isUndefined() ? String::emptyString : F.toString(state);
        }
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.asObject(), state.context()->globalObject()->regexpPrototype());
    RegExpObject* regexp = new RegExpObject(state);
    regexp->setPrototype(state, proto);

    // TODO http://www.ecma-international.org/ecma-262/6.0/index.html#sec-escaperegexppattern
    regexp->init(state, source, option);
    return regexp;
}

static Value builtinRegExpExec(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, exec);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().exec.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    unsigned int option = regexp->option();
    String* str = argv[0].toString(state);
    double lastIndex = 0;
    if (option & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex > str->length()) {
            regexp->setLastIndex(state, Value(0));
            return Value(Value::Null);
        }
    }

    RegexMatchResult result;
    if (regexp->matchNonGlobally(state, str, result, false, lastIndex)) {
        int e = result.m_matchResults[0][0].m_end;
        if (option & RegExpObject::Option::Unicode) {
            char16_t utfRes = str->charAt(e);
            size_t eUTF = str->find(new ASCIIString((const char*)&utfRes), 0);
            if (eUTF >= str->length()) {
                e = str->length();
            } else {
                e = eUTF;
            }
        }

        if (option & (RegExpObject::Option::Sticky | RegExpObject::Option::Global)) {
            regexp->setLastIndex(state, Value(e));
        }

        return regexp->createRegExpMatchedArray(state, result, str);
    }

    if (option & (RegExpObject::Option::Sticky | RegExpObject::Option::Global)) {
        regexp->setLastIndex(state, Value(0));
    }

    return Value(Value::Null);
}

static Value regExpExec(ExecutionState& state, Object* R, String* S)
{
    ASSERT(R->isObject());
    ASSERT(S->isString());
    Value exec = R->get(state, ObjectPropertyName(state.context()->staticStrings().exec)).value(state, R);
    Value arg[1] = { S };
    if (exec.isCallable()) {
        Value result = Object::call(state, exec, R, 1, arg);
        if (result.isNull() || result.isObject()) {
            return result;
        }
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), errorMessage_GlobalObject_ThisNotObject);
    }
    return builtinRegExpExec(state, R, 1, arg, Value());
}

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, test);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    unsigned int option = regexp->option();
    String* str = argv[0].toString(state);
    double lastIndex = 0;
    if (option & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex > str->length()) {
            regexp->setLastIndex(state, Value(0));
            return Value(false);
        }
    }

    if (lastIndex < 0) {
        lastIndex = 0;
    }

    RegexMatchResult result;
    bool testResult = regexp->match(state, str, result, true, lastIndex);
    return Value(testResult);
}

static Value builtinRegExpToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotObject);
    }

    Object* thisObject = thisValue.asObject();
    StringBuilder builder;
    String* pattern = thisObject->get(state, state.context()->staticStrings().source).value(state, thisObject).toString(state);
    Value flagsValue = thisObject->get(state, state.context()->staticStrings().flags).value(state, thisObject);

    builder.appendString("/");
    builder.appendString(pattern);
    builder.appendString("/");

    if (!flagsValue.isUndefined()) {
        builder.appendString(flagsValue.toString(state));
    } else {
        builder.appendString("\0");
    }

    return builder.finalize(&state);
}

static Value builtinRegExpCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isPointerValue() || !thisValue.asPointerValue()->isObject() || !thisValue.asPointerValue()->asObject()->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "'This' is not a RegExp object");
    }

    if (argv[0].isObject() && argv[0].asObject()->isRegExpObject()) {
        if (!argv[1].isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
        } else {
            RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject();
            RegExpObject* patternRegExp = argv[0].asPointerValue()->asObject()->asRegExpObject();
            if (patternRegExp->regexpOptionString() != nullptr) {
                retVal->init(state, patternRegExp->source(), patternRegExp->regexpOptionString());
            } else {
                retVal->init(state, patternRegExp->source());
            }
            return retVal;
        }
    }

    RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject();
    String* pattern_str = argv[0].isUndefined() ? String::emptyString : argv[0].toString(state);
    String* flags_str = argv[1].isUndefined() ? String::emptyString : argv[1].toString(state);
    retVal->init(state, pattern_str, flags_str);
    return retVal;
}
static Value builtinRegExpSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    // $21.2.5.9 RegExp.prototype[@@search]
    RESOLVE_THIS_BINDING_TO_OBJECT(rx, Object, search);
    String* s = argv[0].toString(state);
    Value previousLastIndex = rx->get(state, ObjectPropertyName(state.context()->staticStrings().lastIndex)).value(state, thisValue);
    if (!previousLastIndex.equalsToByTheSameValueAlgorithm(state, Value(0))) {
        rx->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lastIndex), Value(0), thisValue);
    }
    Value result = regExpExec(state, rx, s);

    Value currentLastIndex = rx->get(state, ObjectPropertyName(state.context()->staticStrings().lastIndex)).value(state, thisValue);
    if (!previousLastIndex.equalsToByTheSameValueAlgorithm(state, currentLastIndex)) {
        rx->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lastIndex), previousLastIndex, thisValue);
    }
    if (result.isNull()) {
        return Value(-1);
    } else {
        return result.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().index)).value(state, thisValue);
    }
}

// $21.2.5.11 RegExp.prototype[@@split]
static Value builtinRegExpSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    Object* rx = thisValue.asObject();
    String* S = argv[0].toString(state);

    // Let C be SpeciesConstructor(rx, %RegExp%).
    Value C = rx->speciesConstructor(state, state.context()->globalObject()->regexp());

    // Let flags be ToString(Get(rx, "flags")).
    String* flags = rx->get(state, ObjectPropertyName(state.context()->staticStrings().flags)).value(state, rx).toString(state);
    bool unicodeMatching = false;

    String* newFlags;
    // If flags contains "u", let unicodeMatching be true.
    if (flags->find(String::fromASCII("u")) != SIZE_MAX) {
        unicodeMatching = true;
    }

    // If flags contains "y", let newFlags be flags.
    // Else, let newFlags be the string that is the concatenation of flags and "y".
    StringBuilder builder;
    builder.appendString(flags);
    if (flags->find(String::fromASCII("y")) == SIZE_MAX) {
        builder.appendString("y");
    }
    newFlags = builder.finalize();

    // Let splitter be Construct(C, <<rx, newFlags>>).
    Value params[2] = { rx, newFlags };
    Object* splitter = Object::construct(state, C, 2, params);

    // Let A be ArrayCreate(0).
    ArrayObject* A = new ArrayObject(state);

    // Let lengthA be 0.
    size_t lengthA = 0;
    // If limit is undefined, let lim be 2^53–1; else let lim be ToLength(limit).
    // Note: using toUint32() here since toLength() returns 0 for negative values, which leads to incorrect behavior.
    uint64_t lim = argc > 1 && !argv[1].isUndefined() ? argv[1].toUint32(state) : (1ULL << 53) - 1;

    // Let size be the number of elements in S.
    size_t size = S->length();
    // Let p be 0.
    size_t p = 0;
    // If lim = 0, return A.
    if (lim == 0) {
        return A;
    }

    Value z;
    // If size = 0, then
    if (size == 0) {
        // Let z be RegExpExec(splitter, S).

        Value z = regExpExec(state, splitter, S);
        // If z is not null, return A.
        if (!z.isNull()) {
            return A;
        }
        // Perform CreateDataProperty(A, "0", S).
        A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(S, (ObjectPropertyDescriptor::AllPresent)));
        // Return A.
        return A;
    }

    // Let q be p.
    size_t q = p;
    // Repeat, while q < size.
    while (q < size) {
        // Let setStatus be Set(splitter, "lastIndex", q, true).
        splitter->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lastIndex), Value(q), splitter);
        // Let z be RegExpExec(splitter, S).
        Value z = regExpExec(state, splitter, S);
        // If z is null, let q be AdvanceStringIndex(S, q, unicodeMatching).
        if (z.isNull()) {
            q = S->advanceStringIndex(q, unicodeMatching);
        } else {
            // Else z is not null,
            // Let e be ToLength(Get(splitter, "lastIndex")).
            size_t e = splitter->get(state, ObjectPropertyName(state.context()->staticStrings().lastIndex)).value(state, splitter).toLength(state);
            if (e > size) {
                e = size;
            }
            // If e = p, let q be AdvanceStringIndex(S, q, unicodeMatching).
            if (e == p) {
                q = S->advanceStringIndex(q, unicodeMatching);
            } else {
                // Else e != p
                size_t matchStart = z.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().index)).value(state, z).toNumber(state);

                if (matchStart >= S->length()) {
                    matchStart = p;
                }
                // Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through q (exclusive).
                String* T = S->substring(p, matchStart);
                // Perform CreateDataProperty(A, ToString(lengthA), T).
                A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(lengthA).toString(state)), ObjectPropertyDescriptor(T, (ObjectPropertyDescriptor::AllPresent)));
                // Let lengthA be lengthA + 1.
                lengthA++;
                // If lengthA = lim, return A.
                if (lengthA == lim) {
                    return A;
                }
                // Let p be e.
                p = e;
                // Let numberOfCaptures be ToLength(Get(z, "length")).
                size_t numberOfCaptures = z.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, z).toLength(state);
                // Let numberOfCaptures be max(numberOfCaptures - 1, 0).
                numberOfCaptures = std::max(numberOfCaptures - 1, (size_t)0);
                if (numberOfCaptures == SIZE_MAX) {
                    numberOfCaptures = 0;
                }
                // Let i be 1.
                // Repeat, while i <= numberOfCaptures.
                for (size_t i = 1; i <= numberOfCaptures; i++) {
                    // Let nextCapture be Get(z, ToString(i)).
                    Value nextCapture = z.asObject()->get(state, ObjectPropertyName(state, Value(i).toString(state))).value(state, z);
                    // Perform CreateDataProperty(A, ToString(lengthA), nextCapture).
                    A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(lengthA).toString(state)), ObjectPropertyDescriptor(nextCapture, ObjectPropertyDescriptor::AllPresent));
                    // Let lengthA be lengthA + 1.
                    lengthA++;
                    // If lengthA = lim, return A.
                    if (lengthA == lim) {
                        return A;
                    }
                }
                // Let q be p.
                q = p;
            }
        }
    }

    // Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through size (exclusive).
    String* T = S->substring(p, size);
    // Perform CreateDataProperty(A, ToString(lengthA), T ).
    A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(lengthA).toString(state)), ObjectPropertyDescriptor(T, ObjectPropertyDescriptor::AllPresent));
    // Return A.
    return A;
}

static Value builtinRegExpReplace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    ASSERT(argc == 0 || argv != nullptr);
    Value rx = thisValue;
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    String* str = argv[0].toString(state);
    Value replaceValue = argv[1];
    size_t lengthStr = str->length();
    bool functionalReplace = replaceValue.isCallable();
    bool fullUnicode = false;
    String* replacement = String::emptyString;
    size_t nextSourcePosition = 0;
    StringBuilder builder;

    if (!functionalReplace) {
        replaceValue = replaceValue.toString(state);
    }
    bool global = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().global)).value(state, rx).toBoolean(state);
    if (global) {
        fullUnicode = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, rx).toBoolean(state);
        rx.asObject()->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex), Value(0), rx);
    }
    ValueVectorWithInlineStorage results;
    Value strValue = Value(str);
    while (true) {
        Value result = regExpExec(state, rx.toObject(state), str);
        if (result.isNull()) {
            break;
        }
        results.push_back(result);
        if (!global) {
            break;
        }
        Value matchStr = result.asObject()->get(state, ObjectPropertyName(state, Value(0))).value(state, result);
        if (matchStr.toString(state)->length() == 0 && global) {
            size_t thisIndex = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, rx).toLength(state);
            size_t nextIndex = str->advanceStringIndex(thisIndex, fullUnicode);
            rx.asObject()->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex), Value(nextIndex), rx);
        }
    }

    size_t resultSize = results.size();
    for (uint i = 0; i < resultSize; i++) {
        Object* result = results[i].toObject(state);
        size_t nCaptures = result->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, result).toLength(state) - 1;

        if (nCaptures == SIZE_MAX) {
            nCaptures = 0;
        }
        String* matched = result->get(state, ObjectPropertyName(state, Value(0))).value(state, result).toString(state);
        size_t matchLength = matched->length();
        size_t position = result->get(state, ObjectPropertyName(state.context()->staticStrings().index)).value(state, result).toInteger(state);

        if (position > lengthStr) {
            position = lengthStr;
        }
        if (position == SIZE_MAX) {
            position = 0;
        }
        size_t n = 1;
        StringVector captures;
        Value* replacerArgs = nullptr;
        Value namedCaptures = result->get(state, ObjectPropertyName(state, state.context()->staticStrings().groups)).value(state, result);
        size_t replacerArgsSize = nCaptures + 3;
        if (functionalReplace) {
            if (namedCaptures.isUndefined()) {
                replacerArgs = ALLOCA(sizeof(Value) * (nCaptures + 3), Value, state);
            } else {
                replacerArgs = ALLOCA(sizeof(Value) * (nCaptures + 4), Value, state);
                replacerArgsSize++;
                replacerArgs[nCaptures + 3] = namedCaptures;
            }
            replacerArgs[0] = matched;
        }
        while (n <= nCaptures) {
            Value capN = result->get(state, ObjectPropertyName(state, Value(n))).value(state, result);
            if (!capN.isUndefined()) {
                captures.push_back(capN.toString(state));
            } else {
                captures.push_back(String::emptyString);
            }
            if (functionalReplace) {
                replacerArgs[n] = capN;
            }
            n++;
        }
        if (functionalReplace) {
            replacerArgs[nCaptures + 1] = Value((size_t)position);
            replacerArgs[nCaptures + 2] = Value(str);

            replacement = Object::call(state, replaceValue, Value(), replacerArgsSize, replacerArgs).toString(state);
        } else {
            replacement = replacement->getSubstitution(state, matched, str, position, captures, namedCaptures, replaceValue.toString(state));
        }
        if (position >= nextSourcePosition) {
            builder.appendSubString(str, nextSourcePosition, position);
            builder.appendSubString(replacement, 0, replacement->length());
            nextSourcePosition = position + matchLength;
        }
    }
    if (nextSourcePosition >= lengthStr) {
        return Value(builder.finalize(&state));
    }
    builder.appendSubString(str, nextSourcePosition, str->length());
    return Value(builder.finalize(&state));
}

static Value builtinRegExpMatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    Value rx = thisValue;

    if (!rx.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), errorMessage_GlobalObject_ThisNotObject);
    }

    String* str = argv[0].toString(state);
    Value arg[1] = { str };
    ASSERT(str != nullptr);

    //21.2.5.6.8
    bool global = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().global)).value(state, rx).toBoolean(state);

    if (!global) {
        return regExpExec(state, rx.asObject(), str);
    }

    bool fullUnicode = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, rx).toBoolean(state);
    rx.asObject()->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex), Value(0), rx);
    ArrayObject* A = new ArrayObject(state);
    size_t n = 0;

    //21.2.5.6.8.g.i
    while (true) {
        //21.2.5.6.8.g.i
        Value result = regExpExec(state, rx.asObject(), str);
        //21.2.5.6.8.g.iii
        if (result.isNull()) {
            if (n == 0) {
                return Value(Value::Null);
            }
            return A;
        } else {
            //21.2.5.6.8.g.iv
            Value matchStr = result.asObject()->get(state, ObjectPropertyName(state, Value(0))).value(state, result).toString(state);
            A->defineOwnProperty(state, ObjectPropertyName(state, Value(n).toString(state)), ObjectPropertyDescriptor(Value(matchStr), (ObjectPropertyDescriptor::PresentAttribute::AllPresent)));
            if (matchStr.asString()->length() == 0) {
                //21.2.5.6.8.g.iv.5
                size_t thisIndex = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, rx).toLength(state);
                size_t nextIndex = str->advanceStringIndex(thisIndex, fullUnicode);
                rx.asObject()->setThrowsException(state, state.context()->staticStrings().lastIndex, Value(nextIndex), rx);
            }
            n++;
        }
    }
}

GlobalRegExpFunctionObject::GlobalRegExpFunctionObject(ExecutionState& state)
    : NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().RegExp, builtinRegExpConstructor, 2), NativeFunctionObject::__ForBuiltinConstructor__)
{
    initInternalProperties(state);
}

static Value builtinRegExpOptionGetterHelper(ExecutionState& state, Value thisValue, unsigned int option)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }

    // ES2017 compatibility
    if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
        return Value();
    }

    return Value((bool)(thisValue.asObject()->asRegExpObject()->option() & option));
}

static Value builtinRegExpFlagsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-object");
    }

    return Value(thisValue.asObject()->optionString(state));
}

static Value builtinRegExpGlobalGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Global);
}

static Value builtinRegExpDotAllGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::DotAll);
}

static Value builtinRegExpIgnoreCaseGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::IgnoreCase);
}

static Value builtinRegExpMultiLineGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::MultiLine);
}

static Value builtinRegExpSourceGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }

    return Value(thisValue.asObject()->asRegExpObject()->source());
}

static Value builtinRegExpStickyGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Sticky);
}

static Value builtinRegExpUnicodeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Unicode);
}

class RegExpObjectPrototype : public RegExpObject {
public:
    RegExpObjectPrototype(ExecutionState& state)
        : RegExpObject(state, false)
    {
    }

    virtual bool isRegExpPrototypeObject() const override
    {
        return true;
    }

    virtual const char* internalClassProperty(ExecutionState& state) override
    {
        return "Object";
    }
};


void GlobalObject::installRegExp(ExecutionState& state)
{
    m_regexp = new GlobalRegExpFunctionObject(state);
    m_regexp->markThisObjectDontNeedStructureTransitionTable();
    m_regexp->setPrototype(state, m_functionPrototype);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexp->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_regexpPrototype = m_objectPrototype;
    m_regexpPrototype = new RegExpObjectPrototype(state);
    m_regexpPrototype->markThisObjectDontNeedStructureTransitionTable();
    m_regexpPrototype->setPrototype(state, m_objectPrototype);
    m_regexpPrototype->deleteOwnProperty(state, state.context()->staticStrings().lastIndex);

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getFlags, builtinRegExpFlagsGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().flags), desc);
    }


    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getGlobal, builtinRegExpGlobalGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().global), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getDotAll, builtinRegExpDotAllGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().dotAll), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getIgnoreCase, builtinRegExpIgnoreCaseGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().ignoreCase), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getMultiline, builtinRegExpMultiLineGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().multiline), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSource, builtinRegExpSourceGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().source), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSticky, builtinRegExpStickyGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().sticky), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getUnicode, builtinRegExpUnicodeGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().unicode), desc);
    }

    m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexp->setFunctionPrototype(state, m_regexpPrototype);

    const StaticStrings* strings = &state.context()->staticStrings();
    // $21.2.5.2 RegExp.prototype.exec
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->exec),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->exec, builtinRegExpExec, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.13 RegExp.prototype.test
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->test),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->test, builtinRegExpTest, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.14 RegExp.prototype.toString
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinRegExpToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $B.2.5.1 RegExp.prototype.compile
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->compile),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->compile, builtinRegExpCompile, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.9 RegExp.prototype[@@search]
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().search),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().symbolSearch, builtinRegExpSearch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.11 RegExp.prototype[@@split]
    m_regexpSplitMethod = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().symbolSplit, builtinRegExpSplit, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().split),
                                                        ObjectPropertyDescriptor(m_regexpSplitMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.8 RegExp.prototype [@@replace]
    m_regexpReplaceMethod = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().symbolReplace, builtinRegExpReplace, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().replace),
                                                        ObjectPropertyDescriptor(m_regexpReplaceMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.6 RegExp.prototype[@@match]
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().symbolMatch, builtinRegExpMatch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().RegExp),
                      ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
