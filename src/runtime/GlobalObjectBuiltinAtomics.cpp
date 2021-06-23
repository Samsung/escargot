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
#include "SharedArrayBufferObject.h"
#include "TypedArrayObject.h"
#include "TypedArrayInlines.h"
#include "CheckedArithmetic.h"

namespace Escargot {

enum class AtomicOps : uint8_t {
    ADD,
    AND,
    OR,
    SUB,
    XOR,
};

static Value atomicIntegerOperations(ExecutionState& state, int32_t lnum, int32_t rnum, AtomicOps op)
{
    int32_t result = 0;
    bool passed = true;

    switch (op) {
    case AtomicOps::ADD:
        passed = ArithmeticOperations<int32_t, int32_t, int32_t>::add(lnum, rnum, result);
        break;
    case AtomicOps::AND:
        result = lnum & rnum;
        break;
    case AtomicOps::OR:
        result = lnum | rnum;
        break;
    case AtomicOps::SUB:
        passed = ArithmeticOperations<int32_t, int32_t, int32_t>::sub(lnum, rnum, result);
        break;
    case AtomicOps::XOR:
        result = lnum ^ rnum;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    };

    if (LIKELY(passed)) {
        return Value(result);
    }

    ASSERT(op == AtomicOps::ADD || op == AtomicOps::SUB);
    if (op == AtomicOps::ADD) {
        return Value(Value::EncodeAsDouble, static_cast<double>(lnum) + static_cast<double>(rnum));
    }
    return Value(Value::EncodeAsDouble, static_cast<double>(lnum) - static_cast<double>(rnum));
}

static Value atomicNumberOperations(ExecutionState& state, double lnum, double rnum, AtomicOps op)
{
    double result = 0;

    switch (op) {
    case AtomicOps::ADD:
        result = lnum + rnum;
        break;
    case AtomicOps::AND:
        result = Value(lnum).toInt32(state) & Value(rnum).toInt32(state);
        break;
    case AtomicOps::OR:
        result = Value(lnum).toInt32(state) | Value(rnum).toInt32(state);
        break;
    case AtomicOps::SUB:
        result = lnum - rnum;
        break;
    case AtomicOps::XOR:
        result = Value(lnum).toInt32(state) ^ Value(rnum).toInt32(state);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    };

    return Value(result);
}

static Value atomicBigIntOperations(ExecutionState& state, BigInt* lnum, BigInt* rnum, AtomicOps op)
{
    switch (op) {
    case AtomicOps::ADD:
        return lnum->addition(state, rnum);
    case AtomicOps::AND:
        return lnum->bitwiseAnd(state, rnum);
    case AtomicOps::OR:
        return lnum->bitwiseOr(state, rnum);
    case AtomicOps::SUB:
        return lnum->subtraction(state, rnum);
    case AtomicOps::XOR:
        return lnum->bitwiseXor(state, rnum);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    };
}

static SharedArrayBufferObject* validateSharedIntegerTypedArray(ExecutionState& state, Value typedArray, bool waitable = false)
{
    if (UNLIKELY(!typedArray.isObject() || !typedArray.asObject()->isTypedArrayObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotTypedArrayObject);
    }
    TypedArrayObject* TA = typedArray.asObject()->asTypedArrayObject();

    if (waitable) {
        if ((TA->typedArrayType() != TypedArrayType::Int32) && (TA->typedArrayType() != TypedArrayType::BigInt64)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    } else if ((TA->typedArrayType() == TypedArrayType::Uint8Clamped) || (TA->typedArrayType() == TypedArrayType::Float32) || (TA->typedArrayType() == TypedArrayType::Float64)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    Object* buffer = TA->buffer();
    if (!buffer->isSharedArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    return buffer->asSharedArrayBufferObject();
}

static size_t validateAtomicAccess(ExecutionState& state, TypedArrayObject* typedArray, Value index)
{
    uint64_t accessIndex = index.toIndex(state);
    size_t length = typedArray->arrayLength();
    if (accessIndex >= (uint64_t)length || accessIndex > (uint64_t)std::numeric_limits<size_t>::max()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_RangeError);
    }
    return accessIndex;
}

static Value getModifySetValueInBuffer(ExecutionState& state, SharedArrayBufferObject* buffer, size_t indexedPosition, TypedArrayType type, Value v2, AtomicOps op)
{
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(indexedPosition + elemSize <= buffer->byteLength());

    uint8_t* rawStart = buffer->data() + indexedPosition;
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

static Value atomicReadModifyWrite(ExecutionState& state, Value typedArray, Value index, Value value, AtomicOps op)
{
    SharedArrayBufferObject* buffer = validateSharedIntegerTypedArray(state, typedArray);
    TypedArrayObject* TA = typedArray.asObject()->asTypedArrayObject();
    size_t i = validateAtomicAccess(state, TA, index);
    TypedArrayType type = TA->typedArrayType();

    Value v;
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        v = value.toBigInt(state);
    } else {
        v = Value(value.toInteger(state));
    }

    size_t elementSize = TA->elementSize();
    size_t offset = TA->byteOffset();
    size_t indexedPosition = (i * elementSize) + offset;

    return getModifySetValueInBuffer(state, buffer, indexedPosition, type, v, op);
}

static Value builtinAtomicsAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicOps::ADD);
}

static Value builtinAtomicsAnd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicOps::AND);
}

static Value builtinAtomicsOr(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicOps::OR);
}

static Value builtinAtomicsSub(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicOps::SUB);
}

static Value builtinAtomicsXor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicOps::XOR);
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

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringOr),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringOr, builtinAtomicsOr, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sub),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sub, builtinAtomicsSub, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringXor),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringXor, builtinAtomicsXor, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Atomics),
                      ObjectPropertyDescriptor(m_atomics, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot

#endif
