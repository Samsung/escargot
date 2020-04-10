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
#include "BooleanObject.h"
#include "NativeFunctionObject.h"

namespace Escargot {

static Value builtinBooleanConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool primitiveVal = argv[0].toBoolean(state);
    if (!newTarget.hasValue()) {
        return Value(primitiveVal);
    } else {
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
            return constructorRealm->globalObject()->booleanPrototype();
        });
        BooleanObject* boolObj = new BooleanObject(state, proto, primitiveVal);
        return boolObj;
    }
}

static Value builtinBooleanValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (thisValue.isBoolean()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isBooleanObject()) {
        return Value(thisValue.asPointerValue()->asBooleanObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotBoolean);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinBooleanToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (thisValue.isBoolean()) {
        return Value(thisValue.toString(state));
    } else if (thisValue.isObject() && thisValue.asObject()->isBooleanObject()) {
        return Value(thisValue.asPointerValue()->asBooleanObject()->primitiveValue()).toString(state);
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotBoolean);
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installBoolean(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_boolean = new NativeFunctionObject(state, NativeFunctionInfo(strings->Boolean, builtinBooleanConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_boolean->setGlobalIntrinsicObject(state);

    m_booleanPrototype = new BooleanObject(state, m_objectPrototype, false);
    m_booleanPrototype->setGlobalIntrinsicObject(state);
    m_booleanPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.3.3.2 Boolean.prototype.toString
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinBooleanToString, 0, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $19.3.3.3 Boolean.prototype.valueOf
    m_booleanPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinBooleanValueOf, 0, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_boolean->setFunctionPrototype(state, m_booleanPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Boolean),
                      ObjectPropertyDescriptor(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
