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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/RegExpObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/NativeFunctionObject.h"
#include "parser/Lexer.h"

namespace Escargot {

static Value builtinRegExpConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value pattern = argv[0];
    Value flags = argv[1];
    String* source = pattern.isUndefined() ? String::emptyString() : pattern.toString(state);
    String* option = flags.isUndefined() ? String::emptyString() : flags.toString(state);

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
        option = flags.isUndefined() ? RegExpObject::computeRegExpOptionString(state, patternRegExp) : flags.toString(state);
    } else if (patternIsRegExp) {
        Value P = pattern.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().source)).value(state, pattern);
        source = P.isUndefined() ? String::emptyString() : P.toString(state);
        if (flags.isUndefined()) {
            Value F = pattern.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().flags)).value(state, pattern);
            option = F.isUndefined() ? String::emptyString() : F.toString(state);
        }
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->regexpPrototype();
    });
    RegExpObject* regexp = new RegExpObject(state, proto, source, option);

    if (newTarget != state.context()->globalObject()->regexp()) {
        regexp->setLegacyFeaturesEnabled(false);
    }

    // TODO http://www.ecma-international.org/ecma-262/6.0/index.html#sec-escaperegexppattern
    return regexp;
}

static Value builtinRegExpExec(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().exec.string(), ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    unsigned int option = regexp->option();
    String* str = argv[0].toString(state);
    uint64_t lastIndex = 0;
    if (option & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex > str->length()) {
            regexp->setLastIndex(state, Value(0));
            return Value(Value::Null);
        }
    } else {
        // dummy get lastIndex
        regexp->computedLastIndex(state);
    }

    RegexMatchResult result;
    if (regexp->matchNonGlobally(state, str, result, false, lastIndex)) {
        int e = result.m_matchResults[0][0].m_end;
        if (option & RegExpObject::Option::Unicode) {
            char16_t utfRes = (static_cast<size_t>(e) == str->length()) ? 0 : str->charAt(e);
            const char* buf = reinterpret_cast<const char*>(&utfRes);
            size_t len = strnlen(buf, 2);
            size_t eUTF = len == 0 ? str->length() : (str->find(buf, len, 0));
            if ((int)eUTF > e || e == (int)str->length()) {
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    return builtinRegExpExec(state, R, 1, arg, nullptr);
}

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    unsigned int option = regexp->option();
    String* str = argv[0].toString(state);
    uint64_t lastIndex = 0;
    if (option & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex > str->length()) {
            regexp->setLastIndex(state, Value(0));
            return Value(false);
        }
    }

    RegexMatchResult result;
    bool testResult = regexp->match(state, str, result, true, lastIndex);
    return Value(testResult);
}

static Value builtinRegExpToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    Object* thisObject = thisValue.asObject();
    StringBuilder builder;
    String* pattern = RegExpObject::regexpSourceValue(state, thisObject);
    Value flagsValue = RegExpObject::regexpFlagsValue(state, thisObject);

    builder.appendString("/", &state);
    builder.appendString(pattern, &state);
    builder.appendString("/", &state);

    if (!flagsValue.isUndefined()) {
        builder.appendString(flagsValue.toString(state), &state);
    } else {
        builder.appendString("\0", &state);
    }

    return builder.finalize(&state);
}

static Value builtinRegExpCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }

    Optional<Object*> proto = thisValue.asObject()->getPrototypeObject(state);
    Context* calleeContext = state.resolveCallee()->codeBlock()->context();

    if (!proto || !proto->isRegExpPrototypeObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }

    bool match = false;
    while (proto) {
        Value c = proto->getOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, proto.value());
        if (c.isFunction()) {
            if (c.asFunction()->codeBlock()->context() == calleeContext) {
                match = true;
                break;
            }
        }
        proto = proto->getPrototypeObject(state);
    }

    if (!match) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Cannot use compile function with another Realm");
    }

    if (argv[0].isObject() && argv[0].asObject()->isRegExpObject()) {
        if (!argv[1].isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Cannot supply flags when constructing one RegExp from another");
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
    String* patternStr = argv[0].isUndefined() ? String::emptyString() : argv[0].toString(state);
    String* flagsStr = argv[1].isUndefined() ? String::emptyString() : argv[1].toString(state);
    retVal->init(state, patternStr, flagsStr);
    return retVal;
}
static Value builtinRegExpSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // $21.2.5.9 RegExp.prototype[@@search]
    Object* rx = thisValue.toObject(state);
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull);
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
    if (flags->find("u") != SIZE_MAX) {
        unicodeMatching = true;
    }

    // If flags contains "y", let newFlags be flags.
    // Else, let newFlags be the string that is the concatenation of flags and "y".
    StringBuilder builder;
    builder.appendString(flags);
    if (flags->find("y") == SIZE_MAX) {
        builder.appendString("y");
    }
    newFlags = builder.finalize();

    // Let splitter be Construct(C, <<rx, newFlags>>).
    Value params[2] = { rx, newFlags };
    Object* splitter = Object::construct(state, C, 2, params).toObject(state);

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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().replace.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull);
    }
    String* str = argv[0].toString(state);
    Value replaceValue = argv[1];
    size_t lengthStr = str->length();
    bool functionalReplace = replaceValue.isCallable();
    bool fullUnicode = false;
    String* replacement = String::emptyString();
    size_t nextSourcePosition = 0;
    StringBuilder builder;

    if (!functionalReplace) {
        replaceValue = replaceValue.toString(state);
    }

    String* flags = rx.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().flags)).value(state, rx).toString(state);

    bool global = flags->contains("g");
    if (global) {
        fullUnicode = flags->contains("u");
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
            uint64_t thisIndex = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, rx).toLength(state);
            uint64_t nextIndex = str->advanceStringIndex(thisIndex, fullUnicode);
            rx.asObject()->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex), Value(nextIndex), rx);
        }
    }

    size_t resultSize = results.size();
    for (size_t i = 0; i < resultSize; i++) {
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
                replacerArgs = ALLOCA(sizeof(Value) * (nCaptures + 3), Value);
            } else {
                replacerArgs = ALLOCA(sizeof(Value) * (nCaptures + 4), Value);
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
                captures.push_back(String::emptyString());
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
            replacement = String::getSubstitution(state, matched, str, position, captures, namedCaptures, replaceValue.toString(state));
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    String* str = argv[0].toString(state);
    ASSERT(str != nullptr);

    // 21.2.5.6.8
    String* flags = rx.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().flags)).value(state, rx).toString(state);
    bool global = flags->contains("g");

    if (!global) {
        return regExpExec(state, rx.asObject(), str);
    }

    bool fullUnicode = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, rx).toBoolean();
    rx.asObject()->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex), Value(0), rx);
    ArrayObject* A = new ArrayObject(state);
    size_t n = 0;

    // 21.2.5.6.8.g.i
    while (true) {
        // 21.2.5.6.8.g.i
        Value result = regExpExec(state, rx.asObject(), str);
        // 21.2.5.6.8.g.iii
        if (result.isNull()) {
            if (n == 0) {
                return Value(Value::Null);
            }
            return A;
        } else {
            // 21.2.5.6.8.g.iv
            Value matchStr = result.asObject()->get(state, ObjectPropertyName(state, Value(0))).value(state, result).toString(state);
            A->defineOwnProperty(state, ObjectPropertyName(state, Value(n).toString(state)), ObjectPropertyDescriptor(Value(matchStr), (ObjectPropertyDescriptor::PresentAttribute::AllPresent)));
            if (matchStr.asString()->length() == 0) {
                // 21.2.5.6.8.g.iv.5
                uint64_t thisIndex = rx.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, rx).toLength(state);
                uint64_t nextIndex = str->advanceStringIndex(thisIndex, fullUnicode);
                rx.asObject()->setThrowsException(state, state.context()->staticStrings().lastIndex, Value(nextIndex), rx);
            }
            n++;
        }
    }
}

static Value builtinRegExpMatchAll(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExp.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    bool global = false;
    bool unicode = false;
    Object* thisObj = thisValue.asObject();
    String* s = argv[0].toString(state);
    String* flags = thisObj->get(state, ObjectPropertyName(state, state.context()->staticStrings().flags)).value(state, thisObj).toString(state);

    Value c = thisObj->speciesConstructor(state, state.context()->globalObject()->regexp());
    Value arguments[] = { thisObj, flags };
    Object* matcher = Object::construct(state, c, 2, arguments).toObject(state);

    size_t lastIndex = thisObj->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, thisObj).toNumber(state);
    matcher->asRegExpObject()->setLastIndex(state, Value(lastIndex));

    if (flags->find("g") != SIZE_MAX) {
        global = true;
    }
    if (flags->find("u") != SIZE_MAX) {
        unicode = true;
    }
    return new RegExpStringIteratorObject(state, global, unicode, matcher->asRegExpObject(), s);
}


static Value builtinRegExpOptionGetterHelper(ExecutionState& state, Value thisValue, unsigned int option)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    if (!thisValue.asObject()->isRegExpObject()) {
        if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
            return Value();
        } else {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
        }
    }

    return Value((bool)(thisValue.asObject()->asRegExpObject()->option() & option));
}

static Value builtinRegExpFlagsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "getter called on non-object");
    }

    return Value(RegExpObject::computeRegExpOptionString(state, thisValue.asObject()));
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

static Value builtinRegExpHasIndicesGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::HasIndices);
}

static Value builtinRegExpSourceGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    if (!thisValue.asObject()->isRegExpObject()) {
        if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
            return Value(state.context()->staticStrings().defaultRegExpString.string());
        } else {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
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

static Value builtinRegExpUnicodeSetsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::UnicodeSets);
}


// Legacy RegExp Features (non-standard)
static Value builtinRegExpInputGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || thisValue.asObject() != state.context()->globalObject()->regexp()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }
    auto& status = state.context()->regexpLegacyFeatures();
    if (!status.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::String_InvalidStringLength);
    }
    return status.input;
}

static Value builtinRegExpInputSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || thisValue.asObject() != state.context()->globalObject()->regexp()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);
    }
    state.context()->regexpLegacyFeatures().input = argv[0].toString(state);
    return Value();
}

#define REGEXP_LEGACY_FEATURES(F) \
    F(LastMatch, lastMatch)       \
    F(LastParen, lastParen)       \
    F(LeftContext, leftContext)   \
    F(RightContext, rightContext)

#define REGEXP_LEGACY_DOLLAR_NUMBER_FEATURES(F) \
    F(1)                                        \
    F(2)                                        \
    F(3)                                        \
    F(4)                                        \
    F(5)                                        \
    F(6)                                        \
    F(7)                                        \
    F(8)                                        \
    F(9)

#define DEFINE_LEGACY_FEATURE_GETTER(NAME, name)                                                                                            \
    static Value builtinRegExp##NAME##Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                       \
        if (!thisValue.isObject() || thisValue.asObject() != state.context()->globalObject()->regexp()) {                                   \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);           \
        }                                                                                                                                   \
                                                                                                                                            \
        auto& status = state.context()->regexpLegacyFeatures();                                                                             \
        if (!status.isValid()) {                                                                                                            \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::String_InvalidStringLength);                 \
        }                                                                                                                                   \
        return new StringView(status.name);                                                                                                 \
    }

#define DEFINE_LEGACY_DOLLAR_NUMBER_FEATURE_GETTER(number)                                                                                          \
    static Value builtinRegExpDollar##number##Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                               \
        if (!thisValue.isObject() || thisValue.asObject() != state.context()->globalObject()->regexp()) {                                           \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotRegExpObject);                   \
        }                                                                                                                                           \
        auto& status = state.context()->regexpLegacyFeatures();                                                                                     \
        if (!status.isValid()) {                                                                                                                    \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::String_InvalidStringLength);                         \
        }                                                                                                                                           \
        return (status.dollarCount < number) ? String::emptyString() : new StringView(status.dollars[number - 1]);                                  \
    }

REGEXP_LEGACY_FEATURES(DEFINE_LEGACY_FEATURE_GETTER);
REGEXP_LEGACY_DOLLAR_NUMBER_FEATURES(DEFINE_LEGACY_DOLLAR_NUMBER_FEATURE_GETTER);

// https://tc39.es/ecma262/#prod-SyntaxCharacter
static bool isSyntaxChar(char32_t cp)
{
    switch (cp) {
    case '^':
    case '$':
    case '\\':
    case '.':
    case '*':
    case '+':
    case '?':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
        return true;
    default:
        return false;
    }
}

static void utf16EncodeCodePoint(ExecutionState& state, char32_t cp, LargeStringBuilder& builder)
{
    auto r = utf16EncodeCodePoint(cp);
    if (r.second) {
        char16_t cu1 = floor((cp - 65536) / 1024) + 0xD800;
        char16_t cu2 = ((cp - 65536) % 1024) + 0xDC00;

        builder.appendChar(cu1, &state);
        builder.appendChar(cu2, &state);
    } else {
        builder.appendChar(r.first, &state);
    }
}

static bool isOtherPunctuators(char32_t cp)
{
    // ",-=<>#&!%:;@~'`" and the code unit 0x0022 (QUOTATION MARK).
    switch (cp) {
    case ',':
    case '-':
    case '=':
    case '<':
    case '>':
    case '#':
    case '&':
    case '!':
    case '%':
    case ':':
    case ';':
    case '@':
    case '~':
    case '\'':
    case '`':
    case 0x22:
        return true;
    default:
        return false;
    }
}

static std::pair<std::array<char, 12>, size_t> itoa(char32_t v)
{
    std::array<char, 12> result;
    char* tp = result.data();
    while (v || tp == result.data()) {
        int i = v % 16;
        v /= 16;
        if (i < 10) {
            *tp++ = i + '0';
        } else {
            *tp++ = i + 'a' - 10;
        }
    }

    return std::make_pair(result, tp - result.data());
}

// https://tc39.es/ecma262/#sec-unicodeescape
static void unicodeEscape(ExecutionState& state, char32_t cp, LargeStringBuilder& builder)
{
    ASSERT(cp <= 0x10FFFF);
    builder.appendChar(static_cast<char>(0x5C), &state);
    builder.appendChar('u', &state);

    auto a = itoa(cp);
    int pad = 4 - a.second;
    while (pad > 0) {
        builder.appendChar('0', &state);
        pad--;
    }

    for (size_t i = 0; i < a.second; i++) {
        builder.appendChar(a.first.at(a.second - i - 1), &state);
    }
}

// https://tc39.es/ecma262/#sec-encodeforregexpescape
static void encodeForRegExpEscape(ExecutionState& state, char32_t cp, LargeStringBuilder& builder)
{
    // If cp is matched by SyntaxCharacter or cp is U+002F (SOLIDUS), then
    if (isSyntaxChar(cp) || cp == 0x02f) {
        // a. Return the string-concatenation of 0x005C (REVERSE SOLIDUS) and UTF16EncodeCodePoint(cp).
        builder.appendChar(static_cast<char>(0x5C), &state);
        utf16EncodeCodePoint(state, cp, builder);
        return;
    } else if (cp >= 0x09 && cp <= 0x0d) {
        // 2. Else if cp is a code point listed in the “Code Point” column of Table 67, then
        // a. Return the string-concatenation of 0x005C (REVERSE SOLIDUS) and the string in the “ControlEscape” column of the row whose “Code Point” column contains cp.
        builder.appendChar(static_cast<char>(0x5C), &state);
        switch (cp) {
        case 0x9:
            builder.appendChar('t', &state);
            break;
        case 0xa:
            builder.appendChar('n', &state);
            break;
        case 0xb:
            builder.appendChar('v', &state);
            break;
        case 0xc:
            builder.appendChar('f', &state);
            break;
        case 0xd:
            builder.appendChar('r', &state);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return;
    }

    // 3. Let otherPunctuators be the string-concatenation of ",-=<>#&!%:;@~'`" and the code unit 0x0022 (QUOTATION MARK).
    // 4. Let toEscape be StringToCodePoints(otherPunctuators).
    // 5. If toEscape contains cp, cp is matched by either WhiteSpace or LineTerminator, or cp has the same numeric value as a leading surrogate or trailing surrogate, then
    if (isOtherPunctuators(cp) || EscargotLexer::isWhiteSpaceOrLineTerminator(cp) || U16_IS_LEAD(cp) || U16_IS_TRAIL(cp)) {
        // a. Let cpNum be the numeric value of cp.
        // b. If cpNum ≤ 0xFF, then
        if (cp < 0xff) {
            // i. Let hex be Number::toString(𝔽(cpNum), 16).
            // ii. Return the string-concatenation of the code unit 0x005C (REVERSE SOLIDUS), "x", and StringPad(hex, 2, "0", start).
            auto cd = charToHexCode(static_cast<char>(cp), false);
            builder.appendChar(static_cast<char>(0x5C), &state);
            builder.appendChar('x', &state);
            if (cd.first) {
                builder.appendChar(cd.first, &state);
            } else {
                // check
                builder.appendChar('0', &state);
            }
            builder.appendChar(cd.second, &state);
            return;
        }
        // c. Let escaped be the empty String.
        // d. Let codeUnits be UTF16EncodeCodePoint(cp).
        // e. For each code unit cu of codeUnits, do
        // i. Set escaped to the string-concatenation of escaped and UnicodeEscape(cu).
        // f. Return escaped.

        auto r = utf16EncodeCodePoint(cp);
        if (r.second) {
            unicodeEscape(state, r.first, builder);
            unicodeEscape(state, r.second, builder);
        } else {
            unicodeEscape(state, r.first, builder);
        }
        return;
    }

    // 6. Return UTF16EncodeCodePoint(cp).
    utf16EncodeCodePoint(state, cp, builder);
}

// https://tc39.es/ecma262/#sec-regexp.escape
static Value builtinRegExpEscape(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 1. If S is not a String, throw a TypeError exception.
    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "First parameter must be String");
    }
    String* S = argv[0].asString();
    // 2. Let escaped be the empty String.
    // 3. Let cpList be StringToCodePoints(S).
    // 4. For each code point cp of cpList, do
    size_t index = 0;
    LargeStringBuilder builder;
    while (index < S->length()) {
        auto cp = S->codePointAt(index);
        // a. If escaped is the empty String and cp is matched by either DecimalDigit or AsciiLetter, then
        if ((builder.contentLength() == 0) && cp.codePoint < 128 && isASCIIAlphanumeric(cp.codePoint)) {
            // i. NOTE: Escaping a leading digit ensures that output corresponds with pattern text which may be used after a \0 character escape or a DecimalEscape such as \1 and still match S rather than be interpreted as an extension of the preceding escape sequence. Escaping a leading ASCII letter does the same for the context after \c.
            // ii. Let numericValue be the numeric value of cp.
            char numericValue = static_cast<char>(cp.codePoint);
            // iii. Let hex be Number::toString(𝔽(numericValue), 16).
            // iv. Assert: The length of hex is 2.
            // v. Set escaped to the string-concatenation of the code unit 0x005C (REVERSE SOLIDUS), "x", and hex.
            auto cd = charToHexCode(numericValue, false);
            builder.appendChar(static_cast<char>(0x5C), &state);
            builder.appendChar('x', &state);
            builder.appendChar(cd.first, &state);
            builder.appendChar(cd.second, &state);
        } else {
            // b. Else,
            // i. Set escaped to the string-concatenation of escaped and EncodeForRegExpEscape(cp).
            encodeForRegExpEscape(state, cp.codePoint, builder);
        }


        index += cp.codeUnitCount;
    }
    // 5. Return escaped.
    return builder.finalize(&state);
}

static Value builtinRegExpStringIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExpStringIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().RegExpStringIterator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }
    RegExpStringIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asRegExpStringIteratorObject();
    return iter->next(state);
}

std::pair<Value, bool> RegExpStringIteratorObject::advance(ExecutionState& state)
{
    if (m_isDone) {
        return std::make_pair(Value(), true);
    }
    RegExpObject* r = m_regexp;
    String* s = m_string;
    bool global = m_isGlobal;
    bool unicode = m_isUnicode;
    Value match = regExpExec(state, r, s);

    if (match.isNull()) {
        m_isDone = true;
        return std::make_pair(Value(), true);
    }
    if (global) {
        String* matchStr = match.asObject()->get(state, ObjectPropertyName(state, Value(0))).value(state, match).toString(state);
        if (matchStr->length() == 0) {
            // 21.2.5.6.8.g.iv.5
            uint64_t thisIndex = r->get(state, ObjectPropertyName(state, state.context()->staticStrings().lastIndex)).value(state, r).toLength(state);
            uint64_t nextIndex = s->advanceStringIndex(thisIndex, unicode);
            r->setThrowsException(state, state.context()->staticStrings().lastIndex, Value(nextIndex), r);
        }
        return std::make_pair(match, false);
    }
    m_isDone = true;
    return std::make_pair(match, false);
}

void GlobalObject::initializeRegExp(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->regexp(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().RegExp), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installRegExp(ExecutionState& state)
{
    ASSERT(!!m_iteratorPrototype);

    const StaticStrings* strings = &state.context()->staticStrings();

    m_regexp = new NativeFunctionObject(state, NativeFunctionInfo(strings->RegExp, builtinRegExpConstructor, 2), NativeFunctionObject::__ForBuiltinConstructor__);
    m_regexp->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexp->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    // Legacy RegExp Features (non-standard)
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpInputGetter, 0, NativeFunctionInfo::Strict)),
            new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinRegExpInputSetter, 1, NativeFunctionInfo::Strict)));
        m_regexp->directDefineOwnProperty(state, strings->input, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
        m_regexp->directDefineOwnProperty(state, strings->$_, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLastMatchGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->directDefineOwnProperty(state, strings->lastMatch, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
        m_regexp->directDefineOwnProperty(state, strings->$Ampersand, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLastParenGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->directDefineOwnProperty(state, strings->lastParen, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
        m_regexp->directDefineOwnProperty(state, strings->$PlusSign, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpLeftContextGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->directDefineOwnProperty(state, strings->leftContext, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
        m_regexp->directDefineOwnProperty(state, strings->$GraveAccent, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpRightContextGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        m_regexp->directDefineOwnProperty(state, strings->rightContext, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
        m_regexp->directDefineOwnProperty(state, strings->$Apostrophe, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    }

#define DEFINE_LEGACY_DOLLAR_NUMBER_ATTR(number)                                                                                                                                                 \
    {                                                                                                                                                                                            \
        JSGetterSetter gs(                                                                                                                                                                       \
            new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinRegExpDollar##number##Getter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));                    \
        m_regexp->directDefineOwnProperty(state, strings->$##number, ObjectPropertyDescriptor(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent))); \
    }

    REGEXP_LEGACY_DOLLAR_NUMBER_FEATURES(DEFINE_LEGACY_DOLLAR_NUMBER_ATTR);


    {
        auto fn = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().escape, builtinRegExpEscape, 1, NativeFunctionInfo::Strict));
        m_regexp->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().escape),
                                          ObjectPropertyDescriptor(fn,
                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    }

    m_regexpPrototype = new RegExpPrototypeObject(state);
    m_regexpPrototype->setGlobalIntrinsicObject(state, true);

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getFlags, builtinRegExpFlagsGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->flags), desc);
    }


    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getGlobal, builtinRegExpGlobalGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->global), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getDotAll, builtinRegExpDotAllGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->dotAll), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getIgnoreCase, builtinRegExpIgnoreCaseGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->ignoreCase), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getMultiline, builtinRegExpMultiLineGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->multiline), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getHasIndices, builtinRegExpHasIndicesGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->hasIndices), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getSource, builtinRegExpSourceGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->source), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getSticky, builtinRegExpStickyGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->sticky), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getUnicode, builtinRegExpUnicodeGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->unicode), desc);
    }

    {
        Value getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->getUnicodeSets, builtinRegExpUnicodeSetsGetter, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->unicodeSets), desc);
    }

    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexp->setFunctionPrototype(state, m_regexpPrototype);

    // $21.2.5.2 RegExp.prototype.exec
    m_regexpExecMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->exec, builtinRegExpExec, 1, NativeFunctionInfo::Strict));
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->exec),
                                               ObjectPropertyDescriptor(m_regexpExecMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.13 RegExp.prototype.test
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->test),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->test, builtinRegExpTest, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.14 RegExp.prototype.toString
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinRegExpToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $B.2.5.1 RegExp.prototype.compile
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->compile),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->compile, builtinRegExpCompile, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.9 RegExp.prototype[@@search]
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().search),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolSearch, builtinRegExpSearch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.11 RegExp.prototype[@@split]
    m_regexpSplitMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolSplit, builtinRegExpSplit, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().split),
                                               ObjectPropertyDescriptor(m_regexpSplitMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.8 RegExp.prototype [@@replace]
    m_regexpReplaceMethod = new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolReplace, builtinRegExpReplace, 2, NativeFunctionInfo::Strict));
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().replace),
                                               ObjectPropertyDescriptor(m_regexpReplaceMethod, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.6 RegExp.prototype[@@match]
    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolMatch, builtinRegExpMatch, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexpPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().matchAll),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->symbolMatchAll, builtinRegExpMatchAll, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexpStringIteratorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_regexpStringIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_regexpStringIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinRegExpStringIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexpStringIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                             ObjectPropertyDescriptor(Value(String::fromASCII("RegExp String Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(strings->RegExp),
                        ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
