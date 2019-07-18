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
#include "BooleanObject.h"

namespace Escargot {

static Value builtinBooleanConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    ASSERT(thisValue.isEmpty());
    bool primitiveVal = (argv[0].isUndefined()) ? false : argv[0].toBoolean(state);
    if (isNewExpression) {
        BooleanObject* boolObj = new BooleanObject(state);
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
    m_boolean = new FunctionObject(state, NativeFunctionInfo(strings->Boolean, builtinBooleanConstructor, 1), FunctionObject::__ForBuiltin__);
    m_boolean->markThisObjectDontNeedStructureTransitionTable(state);
    m_boolean->setPrototype(state, m_functionPrototype);
    m_booleanPrototype = m_objectPrototype;
    m_booleanPrototype = new BooleanObject(state, false);
    m_booleanPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_booleanPrototype->setPrototype(state, m_objectPrototype);
    m_booleanPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.3.3.2 Boolean.prototype.toString
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinBooleanToString, 0, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $19.3.3.3 Boolean.prototype.valueOf
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinBooleanValueOf, 0, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_boolean->setFunctionPrototype(state, m_booleanPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Boolean),
                      ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
