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
#include "runtime/Platform.h"
#include "runtime/PromiseObject.h"

namespace Escargot {

#if defined(ENABLE_THREADING)

#if !defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS) && !defined(ENABLE_ATOMICS_GLOBAL_LOCK)
#error "without builtin atomic functions, we need to atomics global lock for implementing atomics builtin"
#endif

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
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    } else if ((TA->typedArrayType() == TypedArrayType::Uint8Clamped) || (TA->typedArrayType() == TypedArrayType::Float32) || (TA->typedArrayType() == TypedArrayType::Float64)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    return buffer;
}

static size_t validateAtomicAccess(ExecutionState& state, TypedArrayObject* typedArray, Value index)
{
    uint64_t accessIndex = index.toIndex(state);
    size_t length = typedArray->arrayLength();
    if (UNLIKELY(accessIndex == Value::InvalidIndexValue || accessIndex >= (uint64_t)length)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::GlobalObject_RangeError);
    }

    ASSERT(accessIndex < std::numeric_limits<size_t>::max());
    size_t elementSize = typedArray->elementSize();
    size_t offset = typedArray->byteOffset();
    return (static_cast<size_t>(accessIndex) * elementSize) + offset;
}

template <typename T>
static T atomicOperation(volatile uint8_t* rawStart, int64_t v, AtomicBinaryOps op)
{
    T returnValue = 0;
    T value = static_cast<T>(v);
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
        __atomic_exchange(ptr, &value, &returnValue, __ATOMIC_SEQ_CST);
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

    switch (type) {
    case TypedArrayType::Int8:
        return Value(atomicOperation<int8_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Int16:
        return Value(atomicOperation<int16_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Int32:
        return Value(atomicOperation<int32_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint8:
        return Value(atomicOperation<uint8_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint16:
        return Value(atomicOperation<uint16_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint32:
        return Value(atomicOperation<uint32_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::Uint8Clamped:
        return Value(atomicOperation<uint8_t>(rawStart, v2.asNumber(), op));
    case TypedArrayType::BigInt64:
        return new BigInt(atomicOperation<int64_t>(rawStart, v2.asBigInt()->toInt64(), op));
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        return new BigInt(atomicOperation<uint64_t>(rawStart, v2.asBigInt()->toUint64(), op));
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
        v = Value(Value::DoubleToIntConvertibleTestNeeds, value.toInteger(state));
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
        expected = Value(Value::DoubleToIntConvertibleTestNeeds, argv[2].toInteger(state));
        replacement = Value(Value::DoubleToIntConvertibleTestNeeds, argv[3].toInteger(state));
    }

    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(indexedPosition + elemSize <= buffer->byteLength());
    uint8_t* expectedBytes = ALLOCA(8, uint8_t);
    TypedArrayHelper::numberToRawBytes(state, type, expected, expectedBytes);
    uint8_t* rawStart = const_cast<uint8_t*>(buffer->data()) + indexedPosition;
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* replacementBytes = ALLOCA(8, uint8_t);
    TypedArrayHelper::numberToRawBytes(state, type, replacement, replacementBytes);
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
        uint8_t* replacementBytes = ALLOCA(8, uint8_t);
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
        v = Value(Value::DoubleToIntConvertibleTestNeeds, value.toInteger(state));
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

static Value doWait(ExecutionState& state, bool isAsync, const Value& typedArrayValue, const Value& index, const Value& value, const Value& timeout)
{
    // https://tc39.es/proposal-atomics-wait-async/#sec-dowait
    // Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ArrayBuffer* buffer = validateIntegerTypedArray(state, typedArrayValue, true);
    TypedArrayObject* typedArray = typedArrayValue.asObject()->asTypedArrayObject();
    if (!buffer->isSharedArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "This function expects SharedArrayBuffer");
    }

    // Let i be ? ValidateAtomicAccess(typedArray, index).
    size_t i = validateAtomicAccess(state, typedArray, index);
    // Let arrayTypeName be typedArray.[[TypedArrayName]].
    auto arrayTypeName = typedArray->typedArrayType();
    // If arrayTypeName is "BigInt64Array", let v be ? ToBigInt64(value).
    Value v;
    if (arrayTypeName == TypedArrayType::BigInt64) {
        v = value.toBigInt(state);
    } else {
        // Otherwise, let v be ? ToInt32(value).
        v = Value(value.toInt32(state));
    }

    //  Let q be ? ToNumber(timeout).
    double q = timeout.toNumber(state);
    // If q is NaN, let t be +∞, else let t be max(q, 0).
    double t;
    if (std::isnan(q) || q == std::numeric_limits<double>::infinity()) {
        t = std::numeric_limits<double>::infinity();
    } else if (q == -std::numeric_limits<double>::infinity()) {
        t = 0;
    } else {
        t = std::max(q, 0.0);
    }
    // If mode is sync, then
    if (!isAsync) {
        // Let B be AgentCanSuspend().
        // If B is false, throw a TypeError exception.
        if (!Global::platform()->canBlockExecution(state.context())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Cannot suspend this thread");
        }
    }
    // Let block be buffer.[[ArrayBufferData]].
    // Let offset be typedArray.[[ByteOffset]].
    // Let indexedPosition be (i × 4) + offset.
    void* indexedPosition = reinterpret_cast<int32_t*>(buffer->data()) + i;
    // Let WL be GetWaiterList(block, indexedPosition).
    Global::Waiter* WL = Global::waiter(indexedPosition);

    // Let promiseCapability be undefined.
    PromiseReaction::Capability promiseCapability;
    // Let resultObject be undefined.
    Optional<Object*> resultObject;
    // If mode is async, then
    if (isAsync) {
        // Set promiseCapability to ! NewPromiseCapability(%Promise%).
        promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
        // Set resultObject to ! OrdinaryObjectCreate(%Object.prototype%).
        resultObject = new Object(state);
    }

    // Perform EnterCriticalSection(WL).
    WL->m_mutex.lock();
    // Let w be ! AtomicLoad(typedArray, i).
    Value w(Value::ForceUninitialized);
    ASSERT(arrayTypeName == TypedArrayType::Int32 || arrayTypeName == TypedArrayType::BigInt64);
    w = buffer->getValueFromBuffer(state, i, arrayTypeName);
    // If v is not equal to w, then
    if (!v.equalsTo(state, w)) {
        // Perform LeaveCriticalSection(WL).
        WL->m_mutex.unlock();
        // If mode is sync, then
        if (!isAsync) {
            //  Return the String "not-equal".
            return Value(state.context()->staticStrings().lazyNotEqual().string());
        }
        // Perform ! CreateDataPropertyOrThrow(resultObject, "async", false).
        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().async), ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        // Perform ! CreateDataPropertyOrThrow(resultObject, "value", "not-equal").
        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(state.context()->staticStrings().lazyNotEqual().string(), ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        // Return resultObject.
        return Value(resultObject.value());
    }
    // If t is 0 and mode is async, then
    if (t == 0 && isAsync) {
        // NOTE: There is no special handling of synchronous immediate timeouts. Asynchronous immediate timeouts have special handling in order to fail fast and avoid Promise machinery.
        // Perform LeaveCriticalSection(WL).
        WL->m_mutex.unlock();
        // Perform ! CreateDataPropertyOrThrow(resultObject, "async", false).
        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().async), ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        // Perform ! CreateDataPropertyOrThrow(resultObject, "value", "timed-out").
        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(state.context()->staticStrings().lazyTimedOut().string(), ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        // Return resultObject.
        return Value(resultObject.value());
    }

    // Let W be AgentSignifier().
    // Let waiterRecord be a new Waiter Record { [[AgentSignifier]]: W, [[PromiseCapability]]: promiseCapability, [[Timeout]]: t, [[Result]]: "ok" }.
    // Perform AddWaiter(WL, waiterRecord).
    // If mode is sync, then
    //     Perform Suspend(WL, W).
    // Perform LeaveCriticalSection(WL).
    // If mode is sync, then
    //     Return waiterRecord.[[Result]].
    // Perform ! CreateDataPropertyOrThrow(resultObject, "async", true).
    // Perform ! CreateDataPropertyOrThrow(resultObject, "value", promiseCapability.[[Promise]]).
    // Return resultObject.
    if (isAsync) {
        auto waiterItem = std::shared_ptr<Global::WaiterItem>(new Global::WaiterItem(state.context(), WL, promiseCapability.m_promise));
        WL->m_waiterList.push_back(waiterItem);
        {
            auto worker = [](double t, Global::Waiter* WL, std::shared_ptr<Global::WaiterItem> waiterItem, Context* context) {
                bool notified = true;
                {
                    std::unique_lock<std::mutex> ul(WL->m_mutex);
                    bool isExistOnWaiterList = std::find(WL->m_waiterList.begin(), WL->m_waiterList.end(), waiterItem) != WL->m_waiterList.end();
                    if (isExistOnWaiterList) {
                        if (t == std::numeric_limits<double>::infinity()) {
                            WL->m_waiter.wait(ul);
                        } else {
                            notified = WL->m_waiter.wait_for(ul, std::chrono::milliseconds((int64_t)t)) == std::cv_status::no_timeout;
                        }
                    }
                }

                {
                    std::unique_lock<std::mutex> ul(context->vmInstance()->asyncWaiterDataMutex());
                    auto& v = context->vmInstance()->asyncWaiterData();
                    for (auto& item : v) {
                        if (std::get<2>(item) == waiterItem.get()) {
                            std::get<2>(item) = nullptr;
                            std::get<3>(item) = notified;
                            context->vmInstance()->pendingAsyncWaiterCount()++;
                            context->vmInstance()->waitEventFromAnotherThreadConditionVariable().notify_all();
                            break;
                        }
                    }
                }
                {
                    std::unique_lock<std::mutex> ul(WL->m_mutex);
                    WL->m_waiterList.erase(std::remove(WL->m_waiterList.begin(), WL->m_waiterList.end(), waiterItem), WL->m_waiterList.end());
                }
                Global::platform()->markJSJobFromAnotherThreadExists(context);
            };
            std::unique_lock<std::mutex> ul(state.context()->vmInstance()->asyncWaiterDataMutex());
            state.context()->vmInstance()->asyncWaiterData().pushBack(std::make_tuple(state.context(), promiseCapability.m_promise, waiterItem.get(), false,
                                                                                      std::shared_ptr<std::thread>(new std::thread(worker, t, WL, waiterItem, state.context()))));
        }
        WL->m_mutex.unlock();

        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().async), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        resultObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(promiseCapability.m_promise, ObjectPropertyDescriptor::PresentAttribute::AllPresent));
        return Value(resultObject.value());
    } else {
        bool notified = true;
        WL->m_mutex.unlock();
        std::unique_lock<std::mutex> ul(WL->m_mutex);
        auto waiterItem = std::shared_ptr<Global::WaiterItem>(new Global::WaiterItem(state.context(), WL));
        WL->m_waiterList.push_back(waiterItem);
        if (t == std::numeric_limits<double>::infinity()) {
            WL->m_waiter.wait(ul);
        } else {
            notified = WL->m_waiter.wait_for(ul, std::chrono::milliseconds((int64_t)t)) == std::cv_status::no_timeout;
        }
        WL->m_waiterList.erase(std::remove(WL->m_waiterList.begin(), WL->m_waiterList.end(), waiterItem), WL->m_waiterList.end());
        if (notified) {
            return Value(state.context()->staticStrings().lazyOk().string());
        }
        return Value(state.context()->staticStrings().lazyTimedOut().string());
    }
}

static Value builtinAtomicsWait(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return doWait(state, false, argv[0], argv[1], argv[2], argv[3]);
}

static Value builtinAtomicsWaitAsync(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return doWait(state, true, argv[0], argv[1], argv[2], argv[3]);
}

static Value builtinAtomicsNotify(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/ecma262/#sec-atomics.notify
    // Atomics.notify ( typedArray, index, count )
    // 1. Let buffer be ? ValidateIntegerTypedArray(typedArray, true).
    ArrayBuffer* buffer = validateIntegerTypedArray(state, argv[0], true);
    TypedArrayObject* typedArray = argv[0].asObject()->asTypedArrayObject();
    // 2. Let indexedPosition be ? ValidateAtomicAccess(typedArray, index).
    size_t indexedPosition = validateAtomicAccess(state, typedArray, argv[1]);
    double c;
    // 3. If count is undefined, let c be +∞.
    if (argv[2].isUndefined()) {
        c = std::numeric_limits<double>::infinity();
    } else {
        // 4. Else,
        // a. Let intCount be ? ToIntegerOrInfinity(count).
        auto intCount = argv[2].toInteger(state);
        // b. Let c be max(intCount, 0).
        c = std::max(intCount, 0.0);
    }
    // 5. Let block be buffer.[[ArrayBufferData]].
    void* blockAddress = reinterpret_cast<int32_t*>(buffer->data()) + indexedPosition;
    // 6. Let arrayTypeName be typedArray.[[TypedArrayName]].
    // 7. If IsSharedArrayBuffer(buffer) is false, return +0𝔽.
    if (!buffer->isSharedArrayBufferObject()) {
        return Value(0);
    }
    // 8. Let WL be GetWaiterList(block, indexedPosition).
    Global::Waiter* WL = Global::waiter(blockAddress);
    // 9. Let n be 0.
    double n = 0;
    // 10. Perform EnterCriticalSection(WL).
    WL->m_mutex.lock();
    double count = std::min((double)WL->m_waiterList.size(), c);
    // 11. Let S be RemoveWaiters(WL, c).
    // 12. Repeat, while S is not an empty List,
    //     a. Let W be the first agent in S.
    //     b. Remove W from the front of S.
    //     c. Perform NotifyWaiter(WL, W).
    //     d. Set n to n + 1.
    for (n = 0; n < count; n++) {
        const auto& f = WL->m_waiterList.front();
        f->m_waiter->m_waiter.notify_one();
        WL->m_waiterList.erase(WL->m_waiterList.begin());
    }
    // 13. Perform LeaveCriticalSection(WL).
    WL->m_mutex.unlock();
    // 14. Return 𝔽(n).
    return Value(Value::DoubleToIntConvertibleTestNeeds, n);
}

static Value builtinAtomicsIsLockFree(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const auto size = argv[0].toInteger(state);
    // https://tc39.es/ecma262/multipage/structured-data.html#sec-atomics.islockfree
    // Atomics.isLockFree(4) always returns true as that can be supported on all known relevant hardware. Being able to assume this will generally simplify programs.
    if (size == 4) {
        return Value(true);
    }
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    if (size == 1 || size == 2 || size == 8) {
        return Value(true);
    }
    return Value(false);
#else
    // spec want to do toInteger operation
    UNUSED_VARIABLE(size);
    return Value(false);
#endif
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
    m_atomics = new PrototypeObject(state);
    m_atomics->setGlobalIntrinsicObject(state);

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                       ObjectPropertyDescriptor(Value(state.context()->staticStrings().Atomics.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().add),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinAtomicsAdd, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringAnd),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringAnd, builtinAtomicsAnd, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().compareExchange),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().compareExchange, builtinAtomicsCompareExchange, 4, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().exchange),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().exchange, builtinAtomicsExchange, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().load),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().load, builtinAtomicsLoad, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringOr),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringOr, builtinAtomicsOr, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().store),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().store, builtinAtomicsStore, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sub),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sub, builtinAtomicsSub, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringXor),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringXor, builtinAtomicsXor, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isLockFree),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isLockFree, builtinAtomicsIsLockFree, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().wait),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().wait, builtinAtomicsWait, 4, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().waitAsync),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().waitAsync, builtinAtomicsWaitAsync, 4, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_atomics->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().notify),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().notify, builtinAtomicsNotify, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
