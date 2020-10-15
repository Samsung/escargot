/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "BigIntObject.h"
#include "NativeFunctionObject.h"

namespace Escargot {

Value builtinBigIntConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is not undefined, throw a TypeError exception.
    if (newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "illegal constructor BigInt");
    }
    // Let prim be ? ToPrimitive(value, hint Number).
    Value prim = argv[0].toPrimitive(state, Value::PreferNumber);
    // If Type(prim) is Number, return ? NumberToBigInt(prim).
    if (prim.isNumber()) {
        // NumberToBigInt(prim)
        // If IsInteger(number) is false, throw a RangeError exception.
        if (!prim.isInteger(state)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "The value you input to BigInt constructor is not integer");
        }
        // Return the BigInt value that represents the mathematical value of number.
        return new BigInt(state.context()->vmInstance(), prim.asNumber());
    } else {
        // Otherwise, return ? ToBigInt(value).
        return argv[0].toBigInt(state);
    }
}

// The abstract operation thisBigIntValue(value) performs the following steps:
// If Type(value) is BigInt, return value.
// If Type(value) is Object and value has a [[BigIntData]] internal slot, then
// Assert: Type(value.[[BigIntData]]) is BigInt.
// Return value.[[BigIntData]].
// Throw a TypeError exception.
#define RESOLVE_THIS_BINDING_TO_BIGINT(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                           \
    BigInt* NAME = nullptr;                                                                                                                                                                                                                                  \
    if (thisValue.isObject()) {                                                                                                                                                                                                                              \
        if (!thisValue.asObject()->isBigIntObject()) {                                                                                                                                                                                                       \
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
        }                                                                                                                                                                                                                                                    \
        NAME = thisValue.asObject()->asBigIntObject()->primitiveValue();                                                                                                                                                                                     \
    } else if (thisValue.isBigInt()) {                                                                                                                                                                                                                       \
        NAME = thisValue.asBigInt();                                                                                                                                                                                                                         \
    } else {                                                                                                                                                                                                                                                 \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);     \
    }

Value builtinBigIntToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_BIGINT(S, BigInt, toString);

    double radix = 10;
    if (argc > 0 && !argv[0].isUndefined()) {
        radix = argv[0].toInteger(state);
        if (radix < 2 || radix > 36) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_RadixInvalidRange);
        }
    }

    return S->toString(state, radix);
}

Value builtinBigIntValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_BIGINT(S, BigInt, valueOf);
    return Value(S);
}

void GlobalObject::installBigInt(ExecutionState& state)
{
    m_bigInt = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().BigInt, builtinBigIntConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_bigInt->setGlobalIntrinsicObject(state);

    m_bigIntPrototype = new Object(state);
    m_bigIntPrototype->setGlobalIntrinsicObject(state, true);

    m_bigIntPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_bigInt, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_bigIntPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                        ObjectPropertyDescriptor(state.context()->staticStrings().BigInt.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_bigIntPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinBigIntToString, 0, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_bigIntPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().valueOf),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().valueOf, builtinBigIntValueOf, 0, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_bigInt->setFunctionPrototype(state, m_bigIntPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().BigInt),
                      ObjectPropertyDescriptor(m_bigInt, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
