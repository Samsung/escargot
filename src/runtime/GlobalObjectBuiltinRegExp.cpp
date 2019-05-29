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
#include "RegExpObject.h"
#include "ArrayObject.h"
#include "GlobalRegExpFunctionObject.h"

namespace Escargot {

static Value builtinRegExpConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value pattern = argv[0];
    Value flags = argv[1];
    String* source = pattern.isUndefined() ? String::emptyString : pattern.toString(state);
    String* option = flags.isUndefined() ? String::emptyString : flags.toString(state);
    // Let patternIsRegExp be IsRegExp(pattern).
    bool patternIsRegExp = argv[0].isObject() && argv[0].asObject()->isRegExpObject(state);
    if (patternIsRegExp) {
        // If patternIsRegExp is true and flags is undefined, then
        if (flags.isUndefined()) {
            // Let patternConstructor be Get(pattern, "constructor").
            Value patternConstructor = pattern.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, pattern);
            // If SameValue(patternConstructor, newTarget), then return pattern.
            if (patternConstructor == state.executionContext()->resolveCallee())
                return pattern;
        }
#ifndef ESCARGOT_ENABLE_ES2015
        else
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
    }
#else
    }
    // If Type(pattern) is Object and pattern has a [[RegExpMatcher]] internal slot, then
    if (pattern.isObject() == true && pattern.asObject()->isRegExpObject(state) == true) {
        RegExpObject* patternRegExp = pattern.asObject()->asRegExpObject(state);
        // Let P be the value of pattern’s [[OriginalSource]] internal slot.
        source = patternRegExp->source();
        // If flags is undefined, let F be the value of pattern’s [[OriginalFlags]] internal slot.
        // Else, let F be flags.
        option = flags.isUndefined() ? patternRegExp->optionString(state) : flags.toString(state);
    }
#endif /* !ESCARGOT_ENABLE_ES2015 */

    RegExpObject* regexp;
    if (isNewExpression && thisValue.isObject() == true && thisValue.asObject()->isRegExpObject(state) == true) {
        regexp = thisValue.asPointerValue()->asObject()->asRegExpObject(state);
    } else {
        regexp = new RegExpObject(state);
    }

    // TODO http://www.ecma-international.org/ecma-262/6.0/index.html#sec-escaperegexppattern
    regexp->init(state, source, option);
    return regexp;
}

static Value builtinRegExpExec(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, exec);
    if (thisObject->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().exec.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject(state);
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
        if (option & (RegExpObject::Option::Global | RegExpObject::Option::Sticky))
            regexp->setLastIndex(state, Value(e));

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

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, test);
    if (thisObject->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject(state);
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

    RegexMatchResult result;
    bool testResult = regexp->match(state, str, result, true, lastIndex);
    return Value(testResult);
}

static Value builtinRegExpToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinRegExpCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isPointerValue() == false || thisValue.asPointerValue()->isObject() == false || thisValue.asPointerValue()->asObject()->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "'This' is not a RegExp object");
    }

    if (argv[0].isObject() == true && argv[0].asObject()->isRegExpObject(state) == true) {
        if (argv[1].isUndefined() == false) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
        } else {
            RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject(state);
            RegExpObject* patternRegExp = argv[0].asPointerValue()->asObject()->asRegExpObject(state);
            retVal->initWithOption(state, patternRegExp->source(), patternRegExp->option());
            return retVal;
        }
    }

    RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject(state);
    String* pattern_str = argv[0].isUndefined() ? String::emptyString : argv[0].toString(state);
    String* flags_str = argv[1].isUndefined() ? String::emptyString : argv[1].toString(state);
    retVal->init(state, pattern_str, flags_str);

    return retVal;
}
static Value builtinRegExpSearch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // $21.2.5.9 RegExp.prototype[@@search]
    RESOLVE_THIS_BINDING_TO_OBJECT(rx, Object, search);
    String* s = argv[0].toString(state);
    Value previousLastIndex = rx->get(state, ObjectPropertyName(state.context()->staticStrings().lastIndex)).value(state, thisValue);

    bool status = rx->set(state, ObjectPropertyName(state.context()->staticStrings().lastIndex), Value(0), thisValue);
    Value result = builtinRegExpExec(state, rx, 1, argv, true);
    status = rx->set(state, ObjectPropertyName(state.context()->staticStrings().lastIndex), previousLastIndex, thisValue);
    if (result.isUndefinedOrNull()) {
        return Value(-1);
    } else {
        return result.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().index)).value(state, thisValue);
    }
}

// $21.2.5.11 RegExp.prototype[@@split]
static Value builtinRegExpSplit(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(rx, Object, split);
    String* S = argv[0].toString(state);

    // Let C be SpeciesConstructor(rx, %RegExp%).
    Value C = rx->speciesConstructor(state, state.context()->globalObject()->regexp());

    // Let flags be ToString(Get(rx, "flags")).
    String* flags = rx->asRegExpObject(state)->optionString(state);
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
    if (flags->find(String::fromASCII("g")) == SIZE_MAX) {
        // Must use global, because later we use lastIndex, which will not get adjusted without the global flag.
        builder.appendString("g");
    }
    newFlags = builder.finalize();

    // Let splitter be Construct(C, <<rx, newFlags>>).
    Value params[2] = { rx->asRegExpObject(state)->source(), newFlags };
    RegExpObject* splitter = C.asFunction()->newInstance(state, 2, params)->asRegExpObject(state);

    // Let A be ArrayCreate(0).
    ArrayObject* A = new ArrayObject(state);

    // Let lengthA be 0.
    size_t lengthA = 0;
    // If limit is undefined, let lim be 2^53–1; else let lim be ToLength(limit).
    // Note: using toUint32() here since toLength() returns 0 for negative values, which leads to incorrect behavior.
    size_t lim = argc > 1 && !argv[1].isUndefined() ? argv[1].toUint32(state) : (1ULL << 53) - 1;

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
        Value execFunction = Object::getMethod(state, splitter, ObjectPropertyName(state.context()->staticStrings().exec));
        // Let z be RegExpExec(splitter, S).
        Value arg[1] = { S };
        z = FunctionObject::call(state, execFunction, splitter, 1, arg);
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
        Value execFunction = Object::getMethod(state, splitter, ObjectPropertyName(state.context()->staticStrings().exec));
        Value arg[1] = { S };
        z = FunctionObject::call(state, execFunction, splitter, 1, arg);
        // If z is null, let q be AdvanceStringIndex(S, q, unicodeMatching).
        if (z.isNull()) {
            q = S->advanceStringIndex(q, unicodeMatching);
        } else {
            // Else z is not null,
            // Let e be ToLength(Get(splitter, "lastIndex")).
            size_t e = splitter->get(state, ObjectPropertyName(state.context()->staticStrings().lastIndex)).value(state, splitter).toLength(state);
            // If e = p, let q be AdvanceStringIndex(S, q, unicodeMatching).
            if (e == p) {
                q = S->advanceStringIndex(q, unicodeMatching);
            } else {
                // Else e != p
                size_t matchStart = z.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().index)).value(state, z).toNumber(state);
                if (matchStart >= S->length()) {
                    break;
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

GlobalRegExpFunctionObject::GlobalRegExpFunctionObject(ExecutionState& state)
    : FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().RegExp, builtinRegExpConstructor, 2, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                         return new RegExpObject(state, String::emptyString, String::emptyString);
                     }),
                     FunctionObject::__ForBuiltin__)
{
    initInternalProperties(state);
}

#ifdef ESCARGOT_ENABLE_ES2015

static Value builtinRegExpOptionGetterHelper(ExecutionState& state, Value thisValue, unsigned int option)
{
    if (thisValue.isObject() == false || thisValue.asObject()->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }

    // ES2017 compatibility
    if (thisValue.asObject() == state.context()->globalObject()->regexpPrototype()) {
        return Value();
    }

    return Value((bool)(thisValue.asObject()->asRegExpObject(state)->option() & option));
}

static Value builtinRegExpFlagsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isObject() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-object");
    }

    return Value(thisValue.asObject()->optionString(state));
}

static Value builtinRegExpGlobalGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Global);
}

static Value builtinRegExpIgnoreCaseGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::IgnoreCase);
}

static Value builtinRegExpMultiLineGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::MultiLine);
}

static Value builtinRegExpSourceGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isObject() == false || thisValue.asObject()->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }

    return Value(thisValue.asObject()->asRegExpObject(state)->source());
}

static Value builtinRegExpStickyGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Sticky);
}

static Value builtinRegExpUnicodeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return builtinRegExpOptionGetterHelper(state, thisValue, RegExpObject::Option::Unicode);
}

#endif

void GlobalObject::installRegExp(ExecutionState& state)
{
    m_regexp = new GlobalRegExpFunctionObject(state);
    m_regexp->markThisObjectDontNeedStructureTransitionTable(state);
    m_regexp->setPrototype(state, m_functionPrototype);

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, nullptr, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexp->defineOwnProperty(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_regexpPrototype = m_objectPrototype;
    m_regexpPrototype = new RegExpObjectPrototype(state);
    m_regexpPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_regexpPrototype->setPrototype(state, m_objectPrototype);

#ifdef ESCARGOT_ENABLE_ES2015
    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getFlags, builtinRegExpFlagsGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().flags), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getGlobal, builtinRegExpGlobalGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().global), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getIgnoreCase, builtinRegExpIgnoreCaseGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().ignoreCase), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getMultiline, builtinRegExpMultiLineGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().multiline), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSource, builtinRegExpSourceGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().source), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSticky, builtinRegExpStickyGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().sticky), desc);
    }

    {
        Value getter = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getUnicode, builtinRegExpUnicodeGetter, 0, nullptr, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().unicode), desc);
    }

#endif

    m_regexpPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_regexp->setFunctionPrototype(state, m_regexpPrototype);

    const StaticStrings* strings = &state.context()->staticStrings();
    // $21.2.5.2 RegExp.prototype.exec
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->exec),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->exec, builtinRegExpExec, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.13 RegExp.prototype.test
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->test),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->test, builtinRegExpTest, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.14 RegExp.prototype.toString
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinRegExpToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $B.2.5.1 RegExp.prototype.compile
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->compile),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->compile, builtinRegExpCompile, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $21.2.5.9 RegExp.prototype[@@search]
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().search),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSearch, builtinRegExpSearch, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $21.2.5.11 RegExp.prototype[@@split]
    m_regexpPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().split),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().symbolSplit, builtinRegExpSplit, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().RegExp),
                      ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
