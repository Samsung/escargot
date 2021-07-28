/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_THREADING)

#include "Escargot.h"
#include "Context.h"
#include "VMInstance.h"
#include "NativeFunctionObject.h"
#include "TypedArrayObject.h"
#include "TypedArrayInlines.h"
#include "SharedArrayBufferObject.h"
#include "CheckedArithmetic.h"

namespace Escargot {

enum class AtomicBinaryOps : uint8_t {
    ADD,
    AND,
    EXCH,
    OR,
    SUB,
    XOR,
};

static Value atomicIntegerOperations(ExecutionState& state, int32_t lnum, int32_t rnum, AtomicBinaryOps op)
{
    int32_t result = 0;
    bool passed = true;

    switch (op) {
    case AtomicBinaryOps::ADD:
        passed = ArithmeticOperations<int32_t, int32_t, int32_t>::add(lnum, rnum, result);
        break;
    case AtomicBinaryOps::AND:
        result = lnum & rnum;
        break;
    case AtomicBinaryOps::EXCH:
        result = rnum;
        break;
    case AtomicBinaryOps::OR:
        result = lnum | rnum;
        break;
    case AtomicBinaryOps::SUB:
        passed = ArithmeticOperations<int32_t, int32_t, int32_t>::sub(lnum, rnum, result);
        break;
    case AtomicBinaryOps::XOR:
        result = lnum ^ rnum;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    };

    if (LIKELY(passed)) {
        return Value(result);
    }

    ASSERT(op == AtomicBinaryOps::ADD || op == AtomicBinaryOps::SUB);
    if (op == AtomicBinaryOps::ADD) {
        return Value(Value::EncodeAsDouble, static_cast<double>(lnum) + static_cast<double>(rnum));
    }
    return Value(Value::EncodeAsDouble, static_cast<double>(lnum) - static_cast<double>(rnum));
}

static Value atomicNumberOperations(ExecutionState& state, double lnum, double rnum, AtomicBinaryOps op)
{
    double result = 0;

    switch (op) {
    case AtomicBinaryOps::ADD:
        result = lnum + rnum;
        break;
    case AtomicBinaryOps::AND:
        result = Value(lnum).toInt32(state) & Value(rnum).toInt32(state);
        break;
    case AtomicBinaryOps::EXCH:
        result = rnum;
        break;
    case AtomicBinaryOps::OR:
        result = Value(lnum).toInt32(state) | Value(rnum).toInt32(state);
        break;
    case AtomicBinaryOps::SUB:
        result = lnum - rnum;
        break;
    case AtomicBinaryOps::XOR:
        result = Value(lnum).toInt32(state) ^ Value(rnum).toInt32(state);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    };

    return Value(result);
}

static Value atomicBigIntOperations(ExecutionState& state, BigInt* lnum, BigInt* rnum, AtomicBinaryOps op)
{
    switch (op) {
    case AtomicBinaryOps::ADD:
        return lnum->addition(state, rnum);
    case AtomicBinaryOps::AND:
        return lnum->bitwiseAnd(state, rnum);
    case AtomicBinaryOps::EXCH:
        return rnum;
    case AtomicBinaryOps::OR:
        return lnum->bitwiseOr(state, rnum);
    case AtomicBinaryOps::SUB:
        return lnum->subtraction(state, rnum);
    case AtomicBinaryOps::XOR:
        return lnum->bitwiseXor(state, rnum);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    };
}

static ArrayBuffer* validateIntegerTypedArray(ExecutionState& state, Value typedArray, bool waitable = false)
{
    ArrayBuffer* buffer = TypedArrayObject::validateTypedArray(state, typedArray);
    TypedArrayObject* TA = typedArray.asObject()->asTypedArrayObject();

    if (waitable) {
        if ((TA->typedArrayType() != TypedArrayType::Int32) && (TA->typedArrayType() != TypedArrayType::BigInt64)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    } else if ((TA->typedArrayType() == TypedArrayType::Uint8Clamped) || (TA->typedArrayType() == TypedArrayType::Float32) || (TA->typedArrayType() == TypedArrayType::Float64)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    return buffer;
}

static size_t validateAtomicAccess(ExecutionState& state, TypedArrayObject* typedArray, Value index)
{
    uint64_t accessIndex = index.toIndex(state);
    size_t length = typedArray->arrayLength();
    if (UNLIKELY(accessIndex == Value::InvalidIndexValue || accessIndex >= (uint64_t)length)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_RangeError);
    }

    ASSERT(accessIndex < std::numeric_limits<size_t>::max());
    size_t elementSize = typedArray->elementSize();
    size_t offset = typedArray->byteOffset();
    return (static_cast<size_t>(accessIndex) * elementSize) + offset;
}

static Value getModifySetValueInBuffer(ExecutionState& state, ArrayBuffer* buffer, size_t indexedPosition, TypedArrayType type, Value v2, AtomicBinaryOps op)
{
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(indexedPosition + elemSize <= buffer->byteLength());

    uint8_t* rawStart = const_cast<uint8_t*>(buffer->data()) + indexedPosition;
    Value rawBytesRead = TypedArrayHelper::rawBytesToNumber(state, type, rawStart);
    Value v1 = rawBytesRead;
    Value val;

    if (v1.isInt32() && v2.isInt32()) {
        val = atomicIntegerOperations(state, v1.asInt32(), v2.asInt32(), op);
    } else if (v1.isNumber() && v2.isNumber()) {
        val = atomicNumberOperations(state, v1.asNumber(), v2.asNumber(), op);
    } else {
        ASSERT(v1.isBigInt() && v2.isBigInt());
        auto lnum = v1.asBigInt();
        auto rnum = v2.asBigInt();
        val = atomicBigIntOperations(state, lnum, rnum, op);
    }

    uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, val, rawBytes);
    memcpy(rawStart, rawBytes, elemSize);

    return rawBytesRead;
}

static Value atomicReadModifyWrite(ExecutionState& state, Value typedArray, Value index, Value value, AtomicBinaryOps op)
{
    ArrayBuffer* buffer = validateIntegerTypedArray(state, typedArray);
    TypedArrayObject* TA = typedArray.asObject()->asTypedArrayObject();
    size_t indexedPosition = validateAtomicAccess(state, TA, index);
    TypedArrayType type = TA->typedArrayType();

    Value v;
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        v = value.toBigInt(state);
    } else {
        v = Value(value.toInteger(state));
    }

    return getModifySetValueInBuffer(state, buffer, indexedPosition, type, v, op);
}

static Value builtinAtomicsAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::ADD);
}

static Value builtinAtomicsAnd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::AND);
}

static Value builtinAtomicsCompareExchange(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ArrayBuffer* buffer = validateIntegerTypedArray(state, argv[0]);
    TypedArrayObject* TA = argv[0].asObject()->asTypedArrayObject();
    size_t indexedPosition = validateAtomicAccess(state, TA, argv[1]);
    TypedArrayType type = TA->typedArrayType();

    Value expected;
    Value replacement;
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        expected = argv[2].toBigInt(state);
        replacement = argv[3].toBigInt(state);
    } else {
        expected = Value(argv[2].toInteger(state));
        replacement = Value(argv[3].toInteger(state));
    }

    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(indexedPosition + elemSize <= buffer->byteLength());
    uint8_t* expectedBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, expected, expectedBytes);

    // TODO SharedArrayBuffer case
    uint8_t* rawStart = const_cast<uint8_t*>(buffer->data()) + indexedPosition;
    Value rawBytesRead = TypedArrayHelper::rawBytesToNumber(state, type, rawStart);

    bool isByteListEqual = true;
    for (size_t i = 0; i < elemSize; i++) {
        if (rawStart[i] != expectedBytes[i]) {
            isByteListEqual = false;
            break;
        }
    }
    if (isByteListEqual) {
        uint8_t* replacementBytes = ALLOCA(8, uint8_t, state);
        TypedArrayHelper::numberToRawBytes(state, type, replacement, replacementBytes);
        memcpy(rawStart, replacementBytes, elemSize);
    }

    return rawBytesRead;
}

static Value builtinAtomicsExchange(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::EXCH);
}

static Value builtinAtomicsLoad(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ArrayBuffer* buffer = validateIntegerTypedArray(state, argv[0]);
    TypedArrayObject* TA = argv[0].asObject()->asTypedArrayObject();
    size_t indexedPosition = validateAtomicAccess(state, TA, argv[1]);
    TypedArrayType type = TA->typedArrayType();

    return buffer->getValueFromBuffer(state, indexedPosition, type);
}

static Value builtinAtomicsOr(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::OR);
}

static Value builtinAtomicsStore(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ArrayBuffer* buffer = validateIntegerTypedArray(state, argv[0]);
    TypedArrayObject* TA = argv[0].asObject()->asTypedArrayObject();
    size_t indexedPosition = validateAtomicAccess(state, TA, argv[1]);
    TypedArrayType type = TA->typedArrayType();

    Value value = argv[2];
    Value v;
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        v = value.toBigInt(state);
    } else {
        v = Value(value.toInteger(state));
    }

    buffer->setValueInBuffer(state, indexedPosition, type, v);
    return v;
}

static Value builtinAtomicsSub(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::SUB);
}

static Value builtinAtomicsXor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::XOR);
}

void GlobalObject::installAtomics(ExecutionState& state)
{
    m_atomics = new Object(state);
    m_atomics->setGlobalIntrinsicObject(state);

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                ObjectPropertyDescriptor(Value(state.context()->staticStrings().Atomics.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().add),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinAtomicsAdd, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringAnd),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringAnd, builtinAtomicsAnd, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().compareExchange),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().compareExchange, builtinAtomicsCompareExchange, 4, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().exchange),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().exchange, builtinAtomicsExchange, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().load),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().load, builtinAtomicsLoad, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringOr),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringOr, builtinAtomicsOr, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().store),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().store, builtinAtomicsStore, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sub),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sub, builtinAtomicsSub, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringXor),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringXor, builtinAtomicsXor, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Atomics),
                      ObjectPropertyDescriptor(m_atomics, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot

#endif
