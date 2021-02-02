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

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#include "IntlNumberFormat.h"
#endif

namespace Escargot {

static Value builtinBigIntConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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
        return new BigInt(state.context()->vmInstance(), (int64_t)prim.asNumber());
    } else {
        // Otherwise, return ? ToBigInt(value).
        return argv[0].toBigInt(state);
    }
}

static Value builtinBigIntAsUintN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let bits be ? ToIndex(bits).
    auto bits = argv[0].toIndex(state);
    if (bits == Value::InvalidIndexValue) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::CanNotConvertValueToIndex);
    }
    // Let bigint be ? ToBigInt(bigint).
    BigInt* bigint = argv[1].toBigInt(state);
    // Return a BigInt representing bigint modulo 2bits.
    bf_t mask, r;
    bf_init(state.context()->vmInstance()->bfContext(), &mask);
    bf_init(state.context()->vmInstance()->bfContext(), &r);
    bf_set_ui(&mask, 1);
    bf_mul_2exp(&mask, bits, BF_PREC_INF, BF_RNDZ);
    bf_add_si(&mask, &mask, -1, BF_PREC_INF, BF_RNDZ);
    bf_logic_and(&r, bigint->bf(), &mask);
    bf_delete(&mask);

    return new BigInt(state.context()->vmInstance(), r);
}

static Value builtinBigIntAsIntN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let bits be ? ToIndex(bits).
    auto bits = argv[0].toIndex(state);
    if (bits == Value::InvalidIndexValue) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::CanNotConvertValueToIndex);
    }
    // Let bigint be ? ToBigInt(bigint).
    BigInt* bigint = argv[1].toBigInt(state);
    // Return a BigInt representing bigint modulo 2bits.
    bf_t mask, r;
    bf_init(state.context()->vmInstance()->bfContext(), &mask);
    bf_init(state.context()->vmInstance()->bfContext(), &r);
    bf_set_ui(&mask, 1);
    bf_mul_2exp(&mask, bits, BF_PREC_INF, BF_RNDZ);
    bf_add_si(&mask, &mask, -1, BF_PREC_INF, BF_RNDZ);
    bf_logic_and(&r, bigint->bf(), &mask);
    if (bits != 0) {
        bf_set_ui(&mask, 1);
        bf_mul_2exp(&mask, bits - 1, BF_PREC_INF, BF_RNDZ);
        if (bf_cmpu(&r, &mask) >= 0) {
            bf_set_ui(&mask, 1);
            bf_mul_2exp(&mask, bits, BF_PREC_INF, BF_RNDZ);
            bf_sub(&r, &r, &mask, BF_PREC_INF, BF_RNDZ);
        }
    }
    bf_delete(&mask);

    return new BigInt(state.context()->vmInstance(), r);
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
        return Value();                                                                                                                                                                                                                                      \
    }

static Value builtinBigIntToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_BIGINT(S, BigInt, toString);

    double radix = 10;
    if (argc > 0 && !argv[0].isUndefined()) {
        radix = argv[0].toInteger(state);
        if (radix < 2 || radix > 36) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_RadixInvalidRange);
        }
    }

    return S->toString(radix);
}

static Value builtinBigIntValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_BIGINT(S, BigInt, valueOf);
    return Value(S);
}

static Value builtinBigIntToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_BIGINT(thisObject, BigInt, toLocaleString);

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    Value locales = argc > 0 ? argv[0] : Value();
    Value options = argc > 1 ? argv[1] : Value();
    Object* numberFormat = IntlNumberFormat::create(state, state.context(), locales, options);
    auto result = IntlNumberFormat::format(state, numberFormat, thisObject->toString());

    return new UTF16String(result.data(), result.length());
#else

    ObjectGetResult toStrFuncGetResult = state.context()->globalObject()->bigIntProxyObject()->get(state, ObjectPropertyName(state.context()->staticStrings().toString));
    if (toStrFuncGetResult.hasValue()) {
        Value toStrFunc = toStrFuncGetResult.value(state, thisObject);
        if (toStrFunc.isCallable()) {
            // toLocaleString() ignores the first argument, unlike toString()
            return Object::call(state, toStrFunc, thisObject, 0, argv);
        }
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().BigInt.string(), true, state.context()->staticStrings().toLocaleString.string(), ErrorObject::Messages::GlobalObject_ToLocaleStringNotCallable);
    RELEASE_ASSERT_NOT_REACHED();
    return Value();
#endif
}

void GlobalObject::installBigInt(ExecutionState& state)
{
    m_bigInt = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().BigInt, builtinBigIntConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_bigInt->setGlobalIntrinsicObject(state);

    m_bigInt->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().asUintN),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asUintN, builtinBigIntAsUintN, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_bigInt->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().asIntN),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asIntN, builtinBigIntAsIntN, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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

    m_bigIntPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toLocaleString),
                                                        ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleString, builtinBigIntToLocaleString, 0, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_bigInt->setFunctionPrototype(state, m_bigIntPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().BigInt),
                      ObjectPropertyDescriptor(m_bigInt, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
