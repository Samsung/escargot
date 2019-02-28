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
    bool patternIsRegExp = argv[0].isObject() && argv[0].asObject()->isRegExpObject();
    if (patternIsRegExp) {
        if (argv[1].isUndefined())
            return argv[0];
        else // TODO(ES6) else part is only for es5
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
    }
    RegExpObject* regexp;
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isRegExpObject()) {
        regexp = thisValue.asPointerValue()->asObject()->asRegExpObject();
    } else {
        regexp = new RegExpObject(state);
    }
    String* source = argv[0].isUndefined() ? String::emptyString : argv[0].toString(state);
    String* option = argv[1].isUndefined() ? String::emptyString : argv[1].toString(state);

    // TODO http://www.ecma-international.org/ecma-262/6.0/index.html#sec-escaperegexppattern
    regexp->init(state, source, option);
    return regexp;
}

static Value builtinRegExpExec(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, exec);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().exec.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    String* str = argv[0].toString(state);
    double lastIndex = 0;
    if (regexp->option() & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex < 0 || lastIndex > str->length()) {
            regexp->setLastIndex(state, Value(0));
            return Value(Value::Null);
        }
    }

    RegexMatchResult result;
    if (regexp->matchNonGlobally(state, str, result, false, lastIndex)) {
        // TODO(ES6): consider Sticky and Unicode
        if (regexp->option() & RegExpObject::Option::Global)
            regexp->setLastIndex(state, Value(result.m_matchResults[0][0].m_end));
        return regexp->createRegExpMatchedArray(state, result, str);
    }

    return Value(Value::Null);
}

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, RegExp, test);
    if (!thisObject->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().RegExp.string(), true, state.context()->staticStrings().test.string(), errorMessage_GlobalObject_ThisNotRegExpObject);
    }
    RegExpObject* regexp = thisObject->asRegExpObject();
    String* str = argv[0].toString(state);

    double lastIndex = 0;
    if (regexp->option() & (RegExpObject::Global | RegExpObject::Sticky)) {
        lastIndex = regexp->computedLastIndex(state);
        if (lastIndex < 0 || lastIndex > str->length()) {
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
    if (argv[0].isObject() && argv[0].asObject()->isRegExpObject()) {
        if (!argv[1].isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
        } else {
            RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject();
            RegExpObject* patternRegExp = argv[0].asPointerValue()->asObject()->asRegExpObject();
            retVal->initWithOption(state, patternRegExp->source(), patternRegExp->option());
            return retVal;
        }
    }

    RegExpObject* retVal = thisValue.asPointerValue()->asObject()->asRegExpObject();
    String* pattern_str = argv[0].isUndefined() ? String::emptyString : argv[0].toString(state);
    String* flags_str = argv[1].isUndefined() ? String::emptyString : argv[1].toString(state);
    retVal->init(state, pattern_str, flags_str);

    return retVal;
}

GlobalRegExpFunctionObject::GlobalRegExpFunctionObject(ExecutionState& state)
    : FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().RegExp, builtinRegExpConstructor, 2, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                         return new RegExpObject(state, String::emptyString, String::emptyString);
                     }),
                     FunctionObject::__ForBuiltin__)
{
    initInternalProperties(state);
}

void GlobalObject::installRegExp(ExecutionState& state)
{
    m_regexp = new GlobalRegExpFunctionObject(state);
    m_regexp->markThisObjectDontNeedStructureTransitionTable(state);
    m_regexp->setPrototype(state, m_functionPrototype);

    m_regexpPrototype = m_objectPrototype;
    m_regexpPrototype = new RegExpObject(state);
    m_regexpPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_regexpPrototype->setPrototype(state, m_objectPrototype);

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

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().RegExp),
                      ObjectPropertyDescriptor(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
