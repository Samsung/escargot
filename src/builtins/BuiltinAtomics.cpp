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

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/TypedArrayInlines.h"
#include "runtime/Global.h"
#include "runtime/BigInt.h"

#if !defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS) && !defined(ENABLE_ATOMICS_GLOBAL_LOCK)
#error "without builtin atomic functions, we need to atomics global lock for implementing atomics builtin"
#endif

namespace Escargot {

#if defined(ENABLE_THREADING)

enum class AtomicBinaryOps : uint8_t {
    ADD,
    AND,
    EXCH,
    OR,
    SUB,
    XOR,
};

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

template <typename T, typename ArgType = int64_t>
static T atomicOperation(volatile uint8_t* rawStart, ArgType value, AtomicBinaryOps op)
{
    T returnValue;
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    volatile T* ptr = reinterpret_cast<volatile T*>(rawStart);
    switch (op) {
    case AtomicBinaryOps::ADD:
        returnValue = __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
        break;
    case AtomicBinaryOps::AND:
        returnValue = __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
        break;
    case AtomicBinaryOps::EXCH: {
        T v = value;
        __atomic_exchange(ptr, &v, &returnValue, __ATOMIC_SEQ_CST);
    } break;
    case AtomicBinaryOps::OR:
        returnValue = __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
        break;
    case AtomicBinaryOps::SUB:
        returnValue = __atomic_fetch_sub(ptr, value, __ATOMIC_SEQ_CST);
        break;
    default:
        ASSERT(op == AtomicBinaryOps::XOR);
        returnValue = __atomic_fetch_xor(ptr, value, __ATOMIC_SEQ_CST);
        break;
    }
#else
    {
        volatile T* ptr = reinterpret_cast<volatile T*>(rawStart);
        std::lock_guard<SpinLock> guard(Global::atomicsLock());
        returnValue = *ptr;
        switch (op) {
        case AtomicBinaryOps::ADD:
            *ptr = *ptr + value;
            break;
        case AtomicBinaryOps::AND:
            *ptr = *ptr & value;
            break;
        case AtomicBinaryOps::EXCH:
            *ptr = value;
            break;
        case AtomicBinaryOps::OR:
            *ptr = *ptr | value;
            break;
        case AtomicBinaryOps::SUB:
            *ptr = *ptr - value;
            break;
        default:
            ASSERT(op == AtomicBinaryOps::XOR);
            *ptr = *ptr ^ value;
            break;
        }
    }
#endif
    return returnValue;
}

static Value getModifySetValueInBuffer(ExecutionState& state, ArrayBuffer* buffer, size_t indexedPosition, TypedArrayType type, Value v2, AtomicBinaryOps op)
{
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(indexedPosition + elemSize <= buffer->byteLength());
    uint8_t* rawStart = const_cast<uint8_t*>(buffer->data()) + indexedPosition;

    if (v2.isInt32()) {
        switch (type) {
        case TypedArrayType::Int8:
            return Value(atomicOperation<int8_t>(rawStart, v2.asInt32(), op));
        case TypedArrayType::Int16:
            return Value(atomicOperation<int16_t>(rawStart, v2.asInt32(), op));
        case TypedArrayType::Int32:
            return Value(atomicOperation<int32_t>(rawStart, v2.asInt32(), op));
        case TypedArrayType::Uint8:
            return Value(atomicOperation<uint8_t>(rawStart, v2.asInt32(), op));
        case TypedArrayType::Uint16:
            return Value(atomicOperation<uint16_t>(rawStart, v2.asInt32(), op));
        case TypedArrayType::Uint32:
            return Value(atomicOperation<uint32_t>(rawStart, v2.asInt32(), op));
        default:
            ASSERT(TypedArrayType::Uint8Clamped == type);
            return Value(atomicOperation<uint8_t>(rawStart, v2.asInt32(), op));
        }
    }

    switch (type) {
    case TypedArrayType::Int8:
        return Value(atomicOperation<int8_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Int16:
        return Value(atomicOperation<int16_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Int32:
        return Value(atomicOperation<int32_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint8:
        return Value(atomicOperation<uint8_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint16:
        return Value(atomicOperation<uint16_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint32:
        return Value(atomicOperation<uint32_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint8Clamped:
        return Value(atomicOperation<uint8_t, int64_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::BigInt64:
        return new BigInt(atomicOperation<int64_t, int64_t>(rawStart, v2.asBigInt()->toInt64(), op));
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        return new BigInt(atomicOperation<uint64_t, uint64_t>(rawStart, v2.asBigInt()->toUint64(), op));
    }
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

#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
template <typename T>
void atomicCompareExchange(uint8_t* rawStart, uint8_t* expectedBytes, uint8_t* replacementBytes)
{
    __atomic_compare_exchange(reinterpret_cast<T*>(rawStart), reinterpret_cast<T*>(expectedBytes),
                              reinterpret_cast<T*>(replacementBytes), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#endif

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
    uint8_t* rawStart = const_cast<uint8_t*>(buffer->data()) + indexedPosition;
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* replacementBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, replacement, replacementBytes);
    bool ret;
    switch (type) {
    case TypedArrayType::Int8:
        atomicCompareExchange<int8_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Int16:
        atomicCompareExchange<int16_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Int32:
        atomicCompareExchange<int32_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Uint8:
        atomicCompareExchange<uint8_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Uint16:
        atomicCompareExchange<uint16_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Uint32:
        atomicCompareExchange<uint32_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::Uint8Clamped:
        atomicCompareExchange<uint8_t>(rawStart, expectedBytes, replacementBytes);
        break;
    case TypedArrayType::BigInt64:
        atomicCompareExchange<int64_t>(rawStart, expectedBytes, replacementBytes);
        break;
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        atomicCompareExchange<uint64_t>(rawStart, expectedBytes, replacementBytes);
        break;
    }
    return TypedArrayHelper::rawBytesToNumber(state, type, expectedBytes);
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
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
#endif
}

static Value builtinAtomicsExchange(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::EXCH);
}

#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
template <typename T>
void atomicLoad(uint8_t* rawStart, uint8_t* rawBytes)
{
    __atomic_load(reinterpret_cast<T*>(rawStart), reinterpret_cast<T*>(rawBytes), __ATOMIC_SEQ_CST);
}
#endif

static Value builtinAtomicsLoad(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ArrayBuffer* buffer = validateIntegerTypedArray(state, argv[0]);
    TypedArrayObject* TA = argv[0].asObject()->asTypedArrayObject();
    size_t indexedPosition = validateAtomicAccess(state, TA, argv[1]);
    TypedArrayType type = TA->typedArrayType();

#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* rawStart = TA->buffer()->data() + indexedPosition;
    uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
    switch (type) {
    case TypedArrayType::Int8:
        atomicLoad<int8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Int16:
        atomicLoad<int16_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Int32:
        atomicLoad<int32_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint8:
        atomicLoad<uint8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint16:
        atomicLoad<uint16_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint32:
        atomicLoad<uint32_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint8Clamped:
        atomicLoad<uint8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::BigInt64:
        atomicLoad<int64_t>(rawStart, rawBytes);
        break;
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        atomicLoad<uint64_t>(rawStart, rawBytes);
        break;
    }
    return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
    return buffer->getValueFromBuffer(state, indexedPosition, type);
#endif
}

static Value builtinAtomicsOr(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::OR);
}

#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
template <typename T>
void atomicStore(uint8_t* rawStart, uint8_t* rawBytes)
{
    __atomic_store(reinterpret_cast<T*>(rawStart), reinterpret_cast<T*>(rawBytes), __ATOMIC_SEQ_CST);
}
#endif

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

#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* rawStart = TA->buffer()->data() + indexedPosition;
    uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, v, rawBytes);

    switch (type) {
    case TypedArrayType::Int8:
        atomicStore<int8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Int16:
        atomicStore<int16_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Int32:
        atomicStore<int32_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint8:
        atomicStore<uint8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint16:
        atomicStore<uint16_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint32:
        atomicStore<uint32_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::Uint8Clamped:
        atomicStore<uint8_t>(rawStart, rawBytes);
        break;
    case TypedArrayType::BigInt64:
        atomicStore<int64_t>(rawStart, rawBytes);
        break;
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        atomicStore<uint64_t>(rawStart, rawBytes);
        break;
    }
    return v;
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
    buffer->setValueInBuffer(state, indexedPosition, type, v);
    return v;
#endif
}

static Value builtinAtomicsSub(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::SUB);
}

static Value builtinAtomicsXor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return atomicReadModifyWrite(state, argv[0], argv[1], argv[2], AtomicBinaryOps::XOR);
}

void GlobalObject::initializeAtomics(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->atomics();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Atomics), nativeData, Value(Value::EmptyValue));
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

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Atomics),
                        ObjectPropertyDescriptor(m_atomics, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
#else

void GlobalObject::initializeAtomics(ExecutionState& state)
{
    // dummy initialize function
}

#endif
} // namespace Escargot
