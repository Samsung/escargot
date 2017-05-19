/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "BooleanObject.h"

namespace Escargot {

static Value builtinBooleanConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    BooleanObject* boolObj;
    bool primitiveVal = (argv[0].isUndefined()) ? false : argv[0].toBoolean(state);
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isBooleanObject()) {
        boolObj = thisValue.asPointerValue()->asObject()->asBooleanObject();
        boolObj->setPrimitiveValue(state, primitiveVal);
        return boolObj;
    } else
        return Value(primitiveVal);
}

static Value builtinBooleanValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isBoolean()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isBooleanObject()) {
        return Value(thisValue.asPointerValue()->asBooleanObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotBoolean);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinBooleanToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isBoolean()) {
        return Value(thisValue.toString(state));
    } else if (thisValue.isObject() && thisValue.asObject()->isBooleanObject()) {
        return Value(thisValue.asPointerValue()->asBooleanObject()->primitiveValue()).toString(state);
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotBoolean);
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installBoolean(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_boolean = new FunctionObject(state, NativeFunctionInfo(strings->Boolean, builtinBooleanConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                       return new BooleanObject(state);
                                   }),
                                   FunctionObject::__ForBuiltin__);
    m_boolean->markThisObjectDontNeedStructureTransitionTable(state);
    m_boolean->setPrototype(state, m_functionPrototype);
    m_booleanPrototype = m_objectPrototype;
    m_booleanPrototype = new BooleanObject(state, false);
    m_booleanPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_booleanPrototype->setPrototype(state, m_objectPrototype);
    m_booleanPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.3.3.2 Boolean.prototype.toString
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinBooleanToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $19.3.3.3 Boolean.prototype.valueOf
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinBooleanValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_boolean->setFunctionPrototype(state, m_booleanPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Boolean),
                      ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
