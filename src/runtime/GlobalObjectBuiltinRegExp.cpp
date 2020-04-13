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
#include "NativeFunctionObject.h"

namespace Escargot {

static Value builtinRegExpConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value pattern = argv[0];
    Value flags = argv[1];
    String* source = pattern.isUndefined() ? String::emptyString : pattern.toString(state);
    String* option = flags.isUndefined() ? String::emptyString : flags.toString(state);

    // Let patternIsRegExp be IsRegExp(pattern).
    bool patternIsRegExp = argv[0].isObject() && argv[0].asObject()->isRegExp(state);

    if (!newTarget.hasValue()) {
        // Let newTarget be the active function object.
        newTarget = state.resolveCallee();
        // If patternIsRegExp is true and flags is undefined, then
        if (patternIsRegExp && flags.isUndefined()) {
            // Let patternConstructor be Get(pattern, "constructor").
            Value patternConstructor = pattern.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, pattern);
            // If SameValue(patternConstructor, newTarget), then return pattern.
            if (patternConstructor.isObject() && patternConstructor.asObject() == newTarget.value()) {
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

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->regexpPrototype();
    });
    RegExpObject* regexp = new RegExpObject(state, proto, source, option);

    // TODO http://www.ecma-international.org/ecma-262/6.0/index.html#sec-escaperegexppattern
    return regexp;
}

static Value builtinRegExpExec(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, exec);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().exec.string(), ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    return builtinRegExpExec(state, R, 1, arg, nullptr);
}

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, test);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
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

static Value builtinRegExpToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
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

static Value builtinRegExpCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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
static Value builtinRegExpSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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
static Value builtinRegExpSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull);
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

static Value builtinRegExpReplace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argc == 0 || argv != nullptr);
    Value rx = thisValue;
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull);
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

static Value builtinRegExpMatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value rx = thisValue;

    if (!rx.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
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

static Value builtinRegExpOptionGetterHelper(ExecutionState& state, Value thisValue, unsigned int option)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    if (!thisValue.asObject()->isRegExpObject()) {
        if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
            return Value();
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
        }
    }

    return Value((bool)(thisValue.asObject()->asRegExpObject()->option() & option));
}

static Value builtinRegExpFlagsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-object");
    }

    return Value(thisValue.asObject()->optionString(state));
}

static Value builtinRegExpGlobalGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Global);
}

static Value builtinRegExpDotAllGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::DotAll);
}

static Value builtinRegExpIgnoreCaseGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::IgnoreCase);
}

static Value builtinRegExpMultiLineGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::MultiLine);
}

static Value builtinRegExpSourceGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    if (!thisValue.asObject()->isRegExpObject()) {
        if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
            return Value(state.context()->staticStrings().defaultRegExpString.string());
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
        }
    }

    return Value(thisValue.asObject()->asRegExpObject()->source());
}

static Value builtinRegExpStickyGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Sticky);
}

static Value builtinRegExpUnicodeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Unicode);
}

// For non-standard, read-only properties of RegExp
static Value builtinRegExpInputGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return state.resolveCallee()->codeBlock()->context()->regexpStatus().input;
}

static Value builtinRegExpInputSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    state.resolveCallee()->codeBlock()->context()->regexpStatus().input = argv[0].toString(state);
    return Value();
}

static Value builtinRegExpLastMatchGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(new StringView(state.resolveCallee()->codeBlock()->context()->regexpStatus().lastMatch));
}

static Value builtinRegExpLastParenGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(new StringView(state.resolveCallee()->codeBlock()->context()->regexpStatus().lastParen));
}

static Value builtinRegExpLeftContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(new StringView(state.resolveCallee()->codeBlock()->context()->regexpStatus().leftContext));
}

static Value builtinRegExpRightContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(new StringView(state.resolveCallee()->codeBlock()->context()->regexpStatus().rightContext));
}

#define DEFINE_GETTER(number)                                                                                                                       \
    static Value builtinRegExpDollar##number##Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                               \
        auto& status = state.resolveCallee()->codeBlock()->context()->regexpStatus();                                                               \
        if (status.dollarCount < number) {                                                                                                          \
            return Value(String::emptyString);                                                                                                      \
        }                                                                                                                                           \
        return Value(new StringView(status.dollars[number - 1]));                                                                                   \
    }

DEFINE_GETTER(1)
DEFINE_GETTER(2)
DEFINE_GETTER(3)
DEFINE_GETTER(4)
DEFINE_GETTER(5)
DEFINE_GETTER(6)
DEFINE_GETTER(7)
DEFINE_GETTER(8)
DEFINE_GETTER(9)

#undef DEFINE_GETTER

void GlobalObject::installRegExp(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_regexp = new NativeFunctionObject(state, NativeFunctionInfo(strings->RegExp, builtinRegExpConstructor, 2), NativeFunctionObject::__ForBuiltinConstructor__);
    m_regexp->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexp->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    // For non-standard, read-only properties of RegExp
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpInputGetter, 0, NativeFunctionInfo::Strict)),
            new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinRegExpInputSetter, 1, NativeFunctionInfo::Strict)));
        m_regexp->defineOwnProperty(state, strings->input, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent)));
        m_regexp->defineOwnProperty(state, strings->$_, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLastMatchGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->defineOwnProperty(state, strings->lastMatch, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent)));
        m_regexp->defineOwnProperty(state, strings->$Ampersand, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLastParenGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->defineOwnProperty(state, strings->lastParen, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent)));
        m_regexp->defineOwnProperty(state, strings->$PlusSign, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLeftContextGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->defineOwnProperty(state, strings->leftContext, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent)));
        m_regexp->defineOwnProperty(state, strings->$GraveAccent, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpRightContextGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->defineOwnProperty(state, strings->rightContext, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent)));
        m_regexp->defineOwnProperty(state, strings->$Apostrophe, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    }

#define DEFINE_ATTR(number)                                                                                                                                                              \
    {                                                                                                                                                                                    \
        JSGetterSetter gs(                                                                                                                                                               \
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpDollar##number##Getter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));            \
        m_regexp->defineOwnProperty(state, strings->$##number, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent))); \
    }

    DEFINE_ATTR(1)
    DEFINE_ATTR(2)
    DEFINE_ATTR(3)
    DEFINE_ATTR(4)
    DEFINE_ATTR(5)
    DEFINE_ATTR(6)
    DEFINE_ATTR(7)
    DEFINE_ATTR(8)
    DEFINE_ATTR(9)

#undef DEFINE_ATTR

    m_regexpPrototype = new Object(state);
    m_regexpPrototype->setGlobalIntrinsicObject(state, true);

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getFlags, builtinRegExpFlagsGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->flags), desc);
    }


    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getGlobal, builtinRegExpGlobalGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->global), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getDotAll, builtinRegExpDotAllGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->dotAll), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getIgnoreCase, builtinRegExpIgnoreCaseGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->ignoreCase), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getMultiline, builtinRegExpMultiLineGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->multiline), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getSource, builtinRegExpSourceGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->source), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getSticky, builtinRegExpStickyGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->sticky), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getUnicode, builtinRegExpUnicodeGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, strings->unicode), desc);
    }

    m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexp->setFunctionPrototype(state, m_regexpPrototype);

    // $21.2.5.2 RegExp.prototype.exec
    m_regexpExecMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->exec, builtinRegExpExec, 1, NativeFunctionInfo::Strict));
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->exec),
                                                        ObjectPropertyDescriptor(m_regexpExecMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
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
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolSearch, builtinRegExpSearch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.11 RegExp.prototype[@@split]
    m_regexpSplitMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolSplit, builtinRegExpSplit, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().split),
                                                        ObjectPropertyDescriptor(m_regexpSplitMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.8 RegExp.prototype [@@replace]
    m_regexpReplaceMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolReplace, builtinRegExpReplace, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().replace),
                                                        ObjectPropertyDescriptor(m_regexpReplaceMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.6 RegExp.prototype[@@match]
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolMatch, builtinRegExpMatch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->RegExp),
                      ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
