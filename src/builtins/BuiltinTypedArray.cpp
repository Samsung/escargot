/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/Object.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

#define RESOLVE_THIS_BINDING_TO_TYPEDARRAY(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                 \
    if (UNLIKELY(!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject())) {                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    TypedArrayObject* NAME = thisValue.asPointerValue()->asTypedArrayObject();


// https://www.ecma-international.org/ecma-262/10.0/#typedarray-create
static Object* createTypedArray(ExecutionState& state, const Value& constructor, size_t argc, Value* argv)
{
    Object* newTypedArray = Object::construct(state, constructor, argc, argv).toObject(state);
    TypedArrayObject::validateTypedArray(state, newTypedArray);

    if (argc == 1 && argv[0].isNumber()) {
        double arrayLength = newTypedArray->asTypedArrayObject()->arrayLength();
        if (arrayLength < argv[0].asNumber()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "invalid TypedArray length");
        }
    }

    return newTypedArray;
}

static Value getDefaultTypedArrayConstructor(ExecutionState& state, const TypedArrayType type)
{
    GlobalObject* glob = state.context()->globalObject();
    switch (type) {
    case TypedArrayType::Int8:
        return glob->int8Array();
    case TypedArrayType::Uint8:
        return glob->uint8Array();
    case TypedArrayType::Uint8Clamped:
        return glob->uint8ClampedArray();
    case TypedArrayType::Int16:
        return glob->int16Array();
    case TypedArrayType::Uint16:
        return glob->uint16Array();
    case TypedArrayType::Int32:
        return glob->int32Array();
    case TypedArrayType::Uint32:
        return glob->uint32Array();
    case TypedArrayType::Float32:
        return glob->float32Array();
    case TypedArrayType::Float64:
        return glob->float64Array();
    case TypedArrayType::BigInt64:
        return glob->bigInt64Array();
    case TypedArrayType::BigUint64:
        return glob->bigUint64Array();
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return Value();
}

static Value TypedArraySpeciesCreate(ExecutionState& state, TypedArrayObject* exemplar, size_t argc, Value* argumentList)
{
    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, exemplar->typedArrayType());
    // Let C be SpeciesConstructor(O, defaultConstructor).
    Value C = exemplar->speciesConstructor(state, defaultConstructor);
    Value A = Object::construct(state, C, argc, argumentList);
    TypedArrayObject::validateTypedArray(state, A);
    if (argc == 1 && argumentList[0].isNumber() && A.asObject()->asTypedArrayObject()->arrayLength() < argumentList->toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    return A;
}

static Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::Not_Constructor);
    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.from
static Value builtinTypedArrayFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value C = thisValue;
    Value source = argv[0];

    if (!C.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_ThisNotConstructor);
    }

    Value mapfn;
    if (argc > 1) {
        mapfn = argv[1];
    }
    Value T;
    if (argc > 2) {
        T = argv[2];
    }

    bool mapping = false;
    if (!mapfn.isUndefined()) {
        if (!mapfn.isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "mapfn is not callable");
        }
        mapping = true;
    }

    Value usingIterator = Object::getMethod(state, source, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
    if (!usingIterator.isUndefined()) {
        ValueVectorWithInlineStorage values = IteratorObject::iterableToList(state, source, usingIterator);
        size_t len = values.size();
        Value arg[1] = { Value(len) };
        Object* targetObj = createTypedArray(state, C, 1, arg);

        size_t k = 0;
        while (k < len) {
            Value mappedValue = values[k];
            if (mapping) {
                Value args[2] = { values[k], Value(k) };
                mappedValue = Object::call(state, mapfn, T, 2, args);
            }
            targetObj->setIndexedPropertyThrowsException(state, Value(k), mappedValue);
            k++;
        }

        return targetObj;
    }

    Object* arrayLike = source.toObject(state);
    size_t len = arrayLike->length(state);

    Value arg[1] = { Value(len) };
    Object* targetObj = createTypedArray(state, C, 1, arg);

    size_t k = 0;
    while (k < len) {
        Value kValue = arrayLike->getIndexedProperty(state, Value(k)).value(state, arrayLike);
        Value mappedValue = kValue;
        if (mapping) {
            // Let mappedValue be Call(mapfn, T, «kValue, k»).
            Value args[2] = { kValue, Value(k) };
            mappedValue = Object::call(state, mapfn, T, 2, args);
        }
        // Let setStatus be Set(targetObj, Pk, mappedValue, true).
        targetObj->setIndexedPropertyThrowsException(state, Value(k), mappedValue);
        k++;
    }

    return targetObj;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.of
static Value builtinTypedArrayOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    size_t len = argc;
    Value C = thisValue;
    if (!C.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_ThisNotConstructor);
    }

    Value arg[1] = { Value(len) };
    Object* newObj = createTypedArray(state, C, 1, arg);

    size_t k = 0;
    while (k < len) {
        Value kValue = argv[k];
        newObj->setIndexedPropertyThrowsException(state, Value(k), kValue);
        k++;
    }
    return newObj;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-get-%typedarray%.prototype.buffer
static Value builtinTypedArrayBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(buffer, TypedArray, getBuffer);
    return buffer->buffer();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-get-%typedarray%.prototype.bytelength
static Value builtinTypedArrayByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(buffer, TypedArray, getbyteLength);
    if (buffer->buffer()->isDetachedBuffer()) {
        return Value(0);
    }
    return Value(buffer->byteLength());
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-get-%typedarray%.prototype.byteoffset
static Value builtinTypedArrayByteOffsetGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(buffer, TypedArray, getbyteOffset);
    if (buffer->buffer()->isDetachedBuffer()) {
        return Value(0);
    }
    return Value(buffer->byteOffset());
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-get-%typedarray%.prototype.length
static Value builtinTypedArrayLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(buffer, TypedArray, getbyteOffset);
    if (buffer->buffer()->isDetachedBuffer()) {
        return Value(0);
    }
    return Value(buffer->arrayLength());
}

static void initializeTypedArrayFromTypedArray(ExecutionState& state, TypedArrayObject* obj, TypedArrayObject* srcArray)
{
    ArrayBuffer* srcData = srcArray->buffer();
    srcData->throwTypeErrorIfDetached(state);

    size_t elementLength = srcArray->arrayLength();
    size_t srcElementSize = srcArray->elementSize();
    size_t srcByteOffset = srcArray->byteOffset();
    size_t elementSize = obj->elementSize();
    uint64_t byteLength = static_cast<uint64_t>(elementSize) * elementLength;

    Value bufferConstructor;
#if defined(ENABLE_THREADING)
    if (srcData->isSharedArrayBufferObject())
        // Let bufferConstructor be %ArrayBuffer%.
        bufferConstructor = state.context()->globalObject()->arrayBuffer();
    else
#endif
        // Let bufferConstructor be ? SpeciesConstructor(srcData, %ArrayBuffer%).
        bufferConstructor = srcData->speciesConstructor(state, state.context()->globalObject()->arrayBuffer());

    ArrayBufferObject* data = nullptr;
    if (obj->typedArrayType() == srcArray->typedArrayType()) {
        // Let data be ? CloneArrayBuffer(srcData, srcByteOffset, byteLength, bufferConstructor).
        data = ArrayBufferObject::cloneArrayBuffer(state, srcData, srcByteOffset, byteLength, bufferConstructor.asObject());
    } else {
        // Let data be AllocateArrayBuffer(bufferConstructor, byteLength).
        data = ArrayBufferObject::allocateArrayBuffer(state, bufferConstructor.asObject(), byteLength);
        // If IsDetachedBuffer(srcData) is true, throw a TypeError exception.
        srcData->throwTypeErrorIfDetached(state);

        // Let srcByteIndex be srcByteOffset.
        size_t srcByteIndex = srcByteOffset;
        // Let targetByteIndex be 0.
        size_t targetByteIndex = 0;
        // Let count be elementLength.
        size_t count = elementLength;
        // Repeat, while count > 0
        while (count > 0) {
            // Let value be GetValueFromBuffer(srcData, srcByteIndex, srcType).
            Value value = srcData->getValueFromBuffer(state, srcByteIndex, srcArray->typedArrayType());
            // Perform SetValueInBuffer(data, targetByteIndex, elementType, value).
            data->setValueInBuffer(state, targetByteIndex, obj->typedArrayType(), value);
            // Set srcByteIndex to srcByteIndex + srcElementSize.
            srcByteIndex += srcElementSize;
            // Set targetByteIndex to targetByteIndex + elementSize.
            targetByteIndex += elementSize;
            // Decrement count by 1.
            count--;
        }
    }
    // Set O’s [[ViewedArrayBuffer]] internal slot to data.
    // Set O’s [[ByteLength]] internal slot to byteLength.
    // Set O’s [[ByteOffset]] internal slot to 0.
    // Set O’s [[ArrayLength]] internal slot to elementLength.
    obj->setBuffer(data, 0, byteLength, elementLength);
}

static void initializeTypedArrayFromArrayBuffer(ExecutionState& state, TypedArrayObject* obj, ArrayBuffer* buffer, const Value& byteOffset, const Value& length)
{
    size_t elementSize = obj->elementSize();
    uint64_t offset = byteOffset.toIndex(state);
    if (offset == Value::InvalidIndexValue || offset % elementSize != 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
    }

    uint64_t newLength = 0;
    if (!length.isUndefined()) {
        newLength = length.toIndex(state);
        if (newLength == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
    }

    buffer->throwTypeErrorIfDetached(state);
    size_t bufferByteLength = buffer->byteLength();

    size_t newByteLength = 0;
    if (length.isUndefined()) {
        if ((bufferByteLength % elementSize != 0) || (bufferByteLength < offset)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
        newByteLength = bufferByteLength - offset;
    } else {
        newByteLength = newLength * elementSize;
        if (offset + newByteLength > bufferByteLength) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
    }

    obj->setBuffer(buffer, offset, newByteLength, newByteLength / elementSize, length.isUndefined());
}

static void initializeTypedArrayFromList(ExecutionState& state, TypedArrayObject* obj, const ValueVectorWithInlineStorage& values)
{
    size_t len = values.size();

    // Perform ? AllocateTypedArrayBuffer(O, len).
    uint64_t elementSize = obj->elementSize();
    uint64_t byteLength = static_cast<uint64_t>(len) * elementSize;
    ArrayBufferObject* buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), byteLength);
    obj->setBuffer(buffer, 0, byteLength, len);

    size_t k = 0;
    while (k < len) {
        // Perform ? Set(O, Pk, kValue, true).
        obj->setIndexedPropertyThrowsException(state, Value(k), values[k]);
        k++;
    }
}

static void initializeTypedArrayFromArrayLike(ExecutionState& state, TypedArrayObject* obj, Object* arrayLike)
{
    size_t len = arrayLike->length(state);

    // Perform ? AllocateTypedArrayBuffer(O, len).
    size_t elementSize = obj->elementSize();
    uint64_t byteLength = static_cast<uint64_t>(len) * elementSize;
    ArrayBufferObject* buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), byteLength);
    obj->setBuffer(buffer, 0, byteLength, len);

    size_t k = 0;
    while (k < len) {
        // Perform ? Set(O, Pk, kValue, true).
        obj->setIndexedPropertyThrowsException(state, Value(k), arrayLike->getIndexedProperty(state, Value(k)).value(state, arrayLike));
        k++;
    }
}

template <typename TA>
static Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    if (argc == 0) {
        // $22.2.4.1 TypedArray ()
        return TA::allocateTypedArray(state, newTarget.value(), 0);
    }

    const Value& firstArg = argv[0];
    if (!firstArg.isObject()) {
        uint64_t elemlen = firstArg.toIndex(state);
        if (elemlen == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
            return Value();
        }
        return TA::allocateTypedArray(state, newTarget.value(), elemlen);
    }

    ASSERT(firstArg.isObject());
    Object* argObj = firstArg.asObject();
    TypedArrayObject* obj = TA::allocateTypedArray(state, newTarget.value());

    if (argObj->isTypedArrayObject()) {
        initializeTypedArrayFromTypedArray(state, obj, argObj->asTypedArrayObject());
    } else if (argObj->isArrayBuffer()) {
        Value byteOffset = (argc > 1) ? argv[1] : Value();
        Value length = (argc > 2) ? argv[2] : Value();
        initializeTypedArrayFromArrayBuffer(state, obj, argObj->asArrayBuffer(), byteOffset, length);
    } else {
        Value usingIterator = Object::getMethod(state, argObj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
        if (!usingIterator.isUndefined()) {
            ValueVectorWithInlineStorage values = IteratorObject::iterableToList(state, argObj, usingIterator);
            initializeTypedArrayFromList(state, obj, values);
        } else {
            initializeTypedArrayFromArrayLike(state, obj, argObj);
        }
    }

    return obj;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.copywithin
static Value builtinTypedArrayCopyWithin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    double len = O->arrayLength();

    // Let relativeTarget be ToInteger(target).
    double relativeTarget = argv[0].toInteger(state);
    // If relativeTarget < 0, let to be max((len + relativeTarget),0); else let to be min(relativeTarget, len).
    double to = (relativeTarget < 0.0) ? std::max((len + relativeTarget), 0.0) : std::min(relativeTarget, len);

    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[1].toInteger(state);
    // If relativeStart < 0, let from be max((len + relativeStart),0); else let from be min(relativeStart, len).
    double from = (relativeStart < 0.0) ? std::max((len + relativeStart), 0.0) : std::min(relativeStart, len);

    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = (argc <= 2 || argv[2].isUndefined()) ? len : argv[2].toInteger(state);
    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    double finalEnd = (relativeEnd < 0.0) ? std::max((len + relativeEnd), 0.0) : std::min(relativeEnd, len);

    // Let count be min(final-from, len-to).
    double count = std::min(finalEnd - from, len - to);
    // If count > 0, then
    if (count > 0) {
        // Let buffer be O.[[ViewedArrayBuffer]].
        ArrayBuffer* buffer = O->buffer();
        // If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
        buffer->throwTypeErrorIfDetached(state);
        // Let typedArrayName be the String value of O.[[TypedArrayName]].
        // Let elementSize be the Number value of the Element Size value specified in Table 59 for typedArrayName.
        size_t elementSize = O->elementSize();
        // Let byteOffset be O.[[ByteOffset]].
        size_t byteOffset = O->byteOffset();
        // Let toByteIndex be to × elementSize + byteOffset.
        size_t toByteIndex = to * elementSize + byteOffset;
        // Let fromByteIndex be from × elementSize + byteOffset.
        size_t fromByteIndex = from * elementSize + byteOffset;
        // Let countBytes be count × elementSize.
        size_t countBytes = count * elementSize;

        int8_t direction = 0;
        // If fromByteIndex < toByteIndex and toByteIndex < fromByteIndex + countBytes, then
        if (fromByteIndex < toByteIndex && toByteIndex < fromByteIndex + countBytes) {
            // Let direction be -1.
            direction = -1;
            // Set fromByteIndex to fromByteIndex + countBytes - 1.
            fromByteIndex = fromByteIndex + countBytes - 1;
            // Set toByteIndex to toByteIndex + countBytes - 1.
            toByteIndex = toByteIndex + countBytes - 1;
        } else {
            // Let direction be 1.
            direction = 1;
        }

        // Repeat, while countBytes > 0
        while (countBytes > 0) {
            // Let value be GetValueFromBuffer(buffer, fromByteIndex, "Uint8", true, "Unordered").
            Value value = buffer->getValueFromBuffer(state, fromByteIndex, TypedArrayType::Uint8);
            // Perform SetValueInBuffer(buffer, toByteIndex, "Uint8", value, true, "Unordered").
            buffer->setValueInBuffer(state, toByteIndex, TypedArrayType::Uint8, value);
            // Set fromByteIndex to fromByteIndex + direction.
            fromByteIndex += direction;
            // Set toByteIndex to toByteIndex + direction.
            toByteIndex += direction;
            // Decrease countBytes by 1.
            countBytes--;
        }
    }

    // return O.
    return O;
}

template <const bool isForward>
static Value fastTypedArrayIndexSearch(TypedArrayObject* arr, size_t k, size_t len, const Value& value)
{
    auto type = arr->typedArrayType();
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        if (!value.isBigInt()) {
            return Value(-1);
        }
    } else if (UNLIKELY(!value.isNumber())) {
        return Value(-1);
    } else if (UNLIKELY(arr->rawBuffer() == nullptr)) {
        return Value(-1);
    }

    uint8_t* buffer = arr->rawBuffer();
    auto elementSize = arr->elementSize();
    int64_t byteK = static_cast<int64_t>(k * elementSize);
    int64_t byteLength = static_cast<int64_t>(len * elementSize);

    auto compFn = [](int64_t byteK, int64_t byteLength) -> bool {
        if (isForward) {
            return byteK < byteLength;
        } else {
            return byteK >= 0;
        }
    };

    auto updateFn = [](int64_t& byteK, const int diff) {
        if (isForward) {
            byteK += diff;
        } else {
            byteK -= diff;
        }
    };

    if (!isForward && byteK >= byteLength) {
        return Value(-1);
    }

    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        uint64_t argv0ToUint64 = type == TypedArrayType::BigInt64 ? bitwise_cast<uint64_t>(value.asBigInt()->toInt64()) : value.asBigInt()->toUint64();
        while (compFn(byteK, byteLength)) {
            if (*reinterpret_cast<uint64_t*>(&buffer[byteK]) == argv0ToUint64) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, 8);
        }
    } else if (type == TypedArrayType::Float32) {
        double num = value.asNumber();
        if (std::isnan(num)) {
            return Value(-1);
        }
        while (compFn(byteK, byteLength)) {
            double c = *reinterpret_cast<float*>(&buffer[byteK]);
            if (c == num) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, 4);
        }
    } else if (type == TypedArrayType::Float64) {
        double num = value.asNumber();
        if (std::isnan(num)) {
            return Value(-1);
        }
        while (compFn(byteK, byteLength)) {
            const double& c = *reinterpret_cast<double*>(&buffer[byteK]);
            if (c == num) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, 8);
        }
    } else {
        int32_t argv0ToUint32;
        if (value.isDouble()) {
            if (value.asDouble() == -0.0) {
                argv0ToUint32 = 0;
            } else {
                return Value(-1);
            }
        } else {
            argv0ToUint32 = value.asInt32();
        }

        while (compFn(byteK, byteLength)) {
            if (memcmp(&buffer[byteK], &argv0ToUint32, elementSize) == 0) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, elementSize);
        }
    }

    return Value(-1);
}

static Value builtinTypedArrayIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // NOTE: Same algorithm as Array.prototype.indexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, indexOf);
    TypedArrayObject::validateTypedArray(state, O);

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    size_t len = O->asTypedArrayObject()->arrayLength();

    // If len is 0, return -1.
    if (len == 0) {
        return Value(-1);
    }

    // If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be 0.
    double n = 0;
    if (argc > 1) {
        n = argv[1].toInteger(state);
    }

    // If n ≥ len, return -1.
    if (n >= len) {
        return Value(-1);
    }

    double doubleK;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        doubleK = (n == -0) ? 0 : n;
    } else {
        // Else, n<0
        // Let k be len + n
        doubleK = len + n;
        // If k is less than 0, then let k be 0.
        if (doubleK < 0) {
            doubleK = 0;
        }
    }
    size_t k = (size_t)doubleK;
    // Reloading `len` because second argument can affect arrayLength
    len = std::min((size_t)O->asTypedArrayObject()->arrayLength(), len);
    return fastTypedArrayIndexSearch<true>(O->asTypedArrayObject(), k, len, argv[0]);
}

static Value builtinTypedArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // NOTE: Same algorithm as Array.prototype.lastIndexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, lastIndexOf);
    TypedArrayObject::validateTypedArray(state, O);

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    size_t len = O->asTypedArrayObject()->arrayLength();

    // If len is 0, return -1.
    if (len == 0) {
        return Value(-1);
    }

    // If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be len-1.
    double n;
    if (argc > 1) {
        n = argv[1].toInteger(state);
    } else {
        n = len - 1;
    }

    // If n ≥ 0, then let k be min(n, len – 1).
    double doubleK;
    if (n >= 0) {
        doubleK = (n == -0) ? 0 : std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len + n.
        doubleK = len + n;
    }

    if (doubleK < 0) {
        return Value(-1);
    }
    int64_t k = (int64_t)doubleK;
    // Reloading `len` because second argument can affect arrayLength
    len = std::min((size_t)O->asTypedArrayObject()->arrayLength(), len);
    return fastTypedArrayIndexSearch<false>(O->asTypedArrayObject(), k, len, argv[0]);
}

static Value builtinTypedArrayIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, includes);
    TypedArrayObject::validateTypedArray(state, O);

    size_t len = O->asTypedArrayObject()->arrayLength();

    // If len is 0, return false.
    if (len == 0) {
        return Value(false);
    }

    Value searchElement = argv[0];
    // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step produces the value 0.)
    double n = argc >= 2 ? argv[1].toInteger(state) : 0;
    if (n >= len) {
        return Value(false);
    }

    double doubleK;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        doubleK = n;
    } else {
        // Else n < 0,
        // Let k be len + n.
        doubleK = len + n;
        // If k < 0, let k be 0.
        if (doubleK < 0) {
            doubleK = 0;
        }
    }
    size_t k = (size_t)doubleK;

    // Repeat, while k < len
    while (k < len) {
        // Let elementK be the result of ? Get(O, ! ToString(k)).
        Value elementK = O->getIndexedProperty(state, Value(k)).value(state, O);
        // If SameValueZero(searchElement, elementK) is true, return true.
        if (elementK.equalsToByTheSameValueZeroAlgorithm(state, searchElement)) {
            return Value(true);
        }
        // Increase k by 1.
        k++;
    }

    // Return false.
    return Value(false);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.set-overloaded-offset
static Value builtinTypedArraySet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_ThisNotTypedArrayObject);
    }

    TypedArrayObject* target = thisValue.asObject()->asTypedArrayObject();

    double targetOffset = 0;
    if (argc > 1) {
        targetOffset = argv[1].toInteger(state);
    }
    if (targetOffset < 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), "Start offset is negative");
    }

    auto typedArrayType = target->typedArrayType();
    bool isBigIntArray = typedArrayType == TypedArrayType::BigInt64 || typedArrayType == TypedArrayType::BigUint64;

    ArrayBuffer* targetBuffer = target->buffer();
    targetBuffer->throwTypeErrorIfDetached(state);
    size_t targetLength = target->arrayLength();
    size_t targetElementSize = target->elementSize();
    size_t targetByteOffset = target->byteOffset();

    Object* src = argv[0].toObject(state);
    if (!src->isTypedArrayObject()) {
        // 22.2.3.23.1%TypedArray%.prototype.set ( array [ , offset ] )
        size_t srcLength = src->length(state);
        if (srcLength + targetOffset > targetLength) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
        }

        size_t targetByteIndex = targetOffset * targetElementSize + targetByteOffset;
        size_t k = 0;
        size_t limit = targetByteIndex + targetElementSize * srcLength;
        while (targetByteIndex < limit) {
            Value value = src->get(state, ObjectPropertyName(state, Value(k))).value(state, src);
            if (UNLIKELY(isBigIntArray)) {
                value = value.toBigInt(state);
            } else {
                value = Value(Value::DoubleToIntConvertibleTestNeeds, value.toNumber(state));
            }
            targetBuffer->throwTypeErrorIfDetached(state);

            // Perform SetValueInBuffer(targetBuffer, targetByteIndex, targetType, value, true, "Unordered").
            targetBuffer->setValueInBuffer(state, targetByteIndex, target->typedArrayType(), value);

            k++;
            targetByteIndex += targetElementSize;
        }

        return Value();
    }

    // 22.2.3.23.2%TypedArray%.prototype.set ( typedArray [ , offset ] )
    ASSERT(src->isTypedArrayObject());
    TypedArrayObject* srcTypedArray = src->asTypedArrayObject();
    ArrayBuffer* srcBuffer = srcTypedArray->buffer();
    srcBuffer->throwTypeErrorIfDetached(state);

    size_t srcElementSize = srcTypedArray->elementSize();
    size_t srcLength = srcTypedArray->arrayLength();
    size_t srcByteOffset = srcTypedArray->byteOffset();

    if (srcLength + targetOffset > targetLength) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }

    // If one of srcType and targetType contains the substring "Big" and the other does not, throw a TypeError exception.
    auto srcTypedArrayType = srcTypedArray->typedArrayType();
    bool isSrcBigIntArray = srcTypedArrayType == TypedArrayType::BigInt64 || srcTypedArrayType == TypedArrayType::BigUint64;
    if (isBigIntArray != isSrcBigIntArray) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->TypedArray.string(), true, strings->set.string(), "Cannot mix BigIntArray with other Array");
    }

    size_t srcByteIndex = srcByteOffset;
    if (srcBuffer == targetBuffer) {
        size_t srcByteLength = srcTypedArray->byteLength();
        srcBuffer = ArrayBufferObject::cloneArrayBuffer(state, targetBuffer, srcByteOffset, srcByteLength, state.context()->globalObject()->arrayBuffer());
        srcByteIndex = 0;
    }

    size_t targetByteIndex = targetOffset * targetElementSize + targetByteOffset;
    size_t limit = targetByteIndex + targetElementSize * srcLength;

    if (srcTypedArray->typedArrayType() == target->typedArrayType()) {
        memmove(targetBuffer->data() + targetByteIndex,
                srcBuffer->data() + srcByteIndex, limit - targetByteIndex);
    } else {
        while (targetByteIndex < limit) {
            // Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, srcType, true, "Unordered").
            Value value = srcBuffer->getValueFromBuffer(state, srcByteIndex, srcTypedArray->typedArrayType());
            // Perform SetValueInBuffer(targetBuffer, targetByteIndex, targetType, value, true, "Unordered").
            targetBuffer->setValueInBuffer(state, targetByteIndex, target->typedArrayType(), value);
            srcByteIndex += srcElementSize;
            targetByteIndex += targetElementSize;
        }
    }

    return Value();
}

static Value builtinTypedArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, some);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.some as defined in 22.1.3.23 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asTypedArrayObject()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().some.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    size_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        ObjectGetResult kResult = O->getIndexedProperty(state, Value(k));
        // Let kValue be the result of calling the [[Get]] internal method of O with argument ToString(k).
        Value kValue = kResult.value(state, O);
        // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
        Value args[] = { kValue, Value(k), O };
        Value testResult = Object::call(state, callbackfn, T, 3, args);

        // If ToBoolean(testResult) is true, return true.
        if (testResult.toBoolean()) {
            return Value(true);
        }

        // Increase k by 1.
        k++;
    }
    // Return false.
    return Value(false);
}

static Value builtinTypedArraySort(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, sort);
    // Let buffer be TypedArrayObject::validateTypedArray(obj).
    ArrayBuffer* buffer = TypedArrayObject::validateTypedArray(state, O);

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    int64_t len = O->asTypedArrayObject()->arrayLength();

    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    // [defaultSort, &cmpfn, &state, &buffer]
    O->sort(state, len, [&](const Value& x, const Value& y) -> bool {
        ASSERT((x.isNumber() || x.isBigInt()) && (y.isNumber() || y.isBigInt()));
        if (!defaultSort) {
            Value args[] = { x, y };
            double v = Object::call(state, cmpfn, Value(), 2, args).toNumber(state);
            buffer->throwTypeErrorIfDetached(state);
            if (std::isnan(v)) {
                return false;
            }
            return (v < 0);
        } else {
            if (LIKELY(x.isNumber())) {
                double xNum = x.asNumber();
                double yNum = y.asNumber();

                // 22.2.3.25.3-10
                if (std::isnan(xNum)) {
                    return false;
                }

                if (std::isnan(yNum)) {
                    return true;
                }

                if (xNum == 0.0 && xNum == yNum) {
                    return std::signbit(xNum);
                }

                return xNum <= yNum;
            } else {
                return x.asBigInt()->lessThanEqual(y.asBigInt());
            }
        } });
    return O;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.subarray
static Value builtinTypedArraySubArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(O, TypedArray, subarray);

    // Let buffer be O.[[ViewedArrayBuffer]].
    ArrayBuffer* buffer = O->buffer();
    // Let srcLength be O.[[ArrayLength]].
    double srcLength = O->arrayLength();
    // Let relativeBegin be ToInteger(begin).
    double relativeBegin = argv[0].toInteger(state);
    // If relativeBegin < 0, let beginIndex be max((srcLength + relativeBegin), 0); else let beginIndex be min(relativeBegin, srcLength).
    double beginIndex = (relativeBegin < 0) ? std::max((srcLength + relativeBegin), 0.0) : std::min(relativeBegin, srcLength);
    // If end is undefined, let relativeEnd be srcLength; else, let relativeEnd be ToInteger(end).
    double relativeEnd = srcLength;
    if (!argv[1].isUndefined()) {
        relativeEnd = argv[1].toInteger(state);
    }
    // If relativeEnd < 0, let endIndex be max((srcLength + relativeEnd), 0); else let endIndex be min(relativeEnd, srcLength).
    double endIndex = (relativeEnd < 0) ? std::max((srcLength + relativeEnd), 0.0) : std::min(relativeEnd, srcLength);

    // Let newLength be max(endIndex – beginIndex, 0).
    double newLength = std::max((int)(endIndex - beginIndex), 0);

    // Let constructorName be the String value of O’s [[TypedArrayName]] internal slot.
    // Let elementSize be the Number value of the Element Size value specified in Table 49 for constructorName.
    size_t elementSize = O->elementSize();
    // Let srcByteOffset be the value of O’s [[ByteOffset]] internal slot.
    size_t srcByteOffset = O->byteOffset();
    // Let beginByteOffset be srcByteOffset + beginIndex × elementSize.
    size_t beginByteOffset = srcByteOffset + beginIndex * elementSize;

    // Let argumentsList be «buffer, beginByteOffset, newLength».
    Value args[3] = { buffer, Value(Value::DoubleToIntConvertibleTestNeeds, beginByteOffset), Value(Value::DoubleToIntConvertibleTestNeeds, newLength) };
    // Return ? TypedArraySpeciesCreate(O, argumentsList).
    return TypedArraySpeciesCreate(state, O, 3, args);
}

static Value builtinTypedArrayEvery(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, every);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.every as defined in 22.1.3.5 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    unsigned len = O->asTypedArrayObject()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().every.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let k be 0.
    size_t k = 0;

    while (k < len) {
        ObjectGetResult value = O->getIndexedProperty(state, Value(k));
        // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
        Value kValue = value.value(state, O);
        // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
        Value args[] = { kValue, Value(k), O };
        Value testResult = Object::call(state, callbackfn, T, 3, args);

        if (!testResult.toBoolean()) {
            return Value(false);
        }

        // Increae k by 1.
        k++;
    }
    return Value(true);
}

//https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.fill
static Value builtinTypedArrayFill(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    size_t len = O->arrayLength();

    Value value(Value::ForceUninitialized);
    // If O.[[TypedArrayName]] is "BigUint64Array" or "BigInt64Array", let value be ? ToBigInt(value).
    auto typedArrayType = O->asTypedArrayObject()->typedArrayType();
    if (UNLIKELY(typedArrayType == TypedArrayType::BigInt64 || typedArrayType == TypedArrayType::BigUint64)) {
        value = argv[0].toBigInt(state);
    } else {
        // Otherwise, let value be ? ToNumber(value).
        // Set value to ? ToNumber(value).
        value = Value(Value::DoubleToIntConvertibleTestNeeds, argv[0].toNumber(state));
    }
    // Let relativeStart be ? ToInteger(start).
    double relativeStart = 0;
    if (argc > 1) {
        relativeStart = argv[1].toInteger(state);
    }
    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    size_t k = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = len;
    if (argc > 2 && !argv[2].isUndefined()) {
        relativeEnd = argv[2].toInteger(state);
    }
    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    size_t fin = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);

    // If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, throw a TypeError exception.
    O->buffer()->throwTypeErrorIfDetached(state);

    // Repeat, while k < final
    while (k < fin) {
        O->setIndexedPropertyThrowsException(state, Value(k), value);
        k++;
    }
    // return O.
    return O;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.filter
static Value builtinTypedArrayFilter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    size_t len = O->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().filter.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let kept be a new empty List.
    ValueVectorWithInlineStorage kept;
    // Let k be 0.
    size_t k = 0;
    // Let captured be 0.
    size_t captured = 0;
    // Repeat, while k < len
    while (k < len) {
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        Value args[] = { kValue, Value(k), O };
        bool selected = Object::call(state, callbackfn, T, 3, args).toBoolean();
        if (selected) {
            kept.push_back(kValue);
            captured++;
        }
        k++;
    }

    // Let A be ? TypedArraySpeciesCreate(O, « captured »).
    Value arg[1] = { Value(captured) };
    Value A = TypedArraySpeciesCreate(state, O, 1, arg);

    // Let n be 0.
    // For each element e of kept
    for (size_t n = 0; n < kept.size(); n++) {
        // Let status be Set(A, ToString(n), e, true ).
        A.asObject()->setIndexedPropertyThrowsException(state, Value(n), kept[n]);
    }
    // Return A.
    return A;
}

static Value builtinTypedArrayFind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, find);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.find as defined in 22.1.3.8 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->asTypedArrayObject()->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().find.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be Get(O, Pk).
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(Call(predicate, thisArg, «kValue, k, O»)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, thisArg, 3, args).toBoolean();
        // If testResult is true, return kValue.
        if (testResult) {
            return kValue;
        }
        // Increase k by 1.
        k++;
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArrayFindIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, findIndex);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.findIndex as defined in 22.1.3.9 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->asTypedArrayObject()->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().findIndex.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, thisArg, « kValue, k, O »)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, thisArg, 3, args).toBoolean();
        // If testResult is true, return k.
        if (testResult) {
            return Value(k);
        }
        // Increase k by 1.
        k++;
    }
    // Return -1
    return Value(-1);
}

static Value builtinTypedArrayFindLast(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, find);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Let len be O.[[ArrayLength]].
    int64_t len = static_cast<int64_t>(O->asTypedArrayObject()->arrayLength());

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().findLast.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be len - 1.
    int64_t k = len - 1;
    Value kValue;
    // Repeat, while k >= 0
    while (k >= 0) {
        // Let kValue be Get(O, Pk).
        kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(Call(predicate, thisArg, «kValue, k, O»)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, thisArg, 3, args).toBoolean();
        // If testResult is true, return kValue.
        if (testResult) {
            return kValue;
        }
        // Set k to k - 1.
        k--;
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArrayFindLastIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, findIndex);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Let len be O.[[ArrayLength]].
    int64_t len = static_cast<int64_t>(O->asTypedArrayObject()->arrayLength());

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().findIndex.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be len - 1.
    int64_t k = len - 1;
    Value kValue;
    // Repeat, while k >= 0
    while (k >= 0) {
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, thisArg, « kValue, k, O »)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, thisArg, 3, args).toBoolean();
        // If testResult is true, return k.
        if (testResult) {
            return Value(k);
        }
        // Set k to k - 1.
        k--;
    }
    // Return -1
    return Value(-1);
}

static Value builtinTypedArrayForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, forEach);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.forEach as defined in 22.1.3.10 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asTypedArrayObject()->arrayLength();

    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().forEach.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    size_t k = 0;
    while (k < len) {
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        Value kValue = res.value(state, O);
        Value args[] = { kValue, Value(k), O };
        Object::call(state, callbackfn, T, 3, args);
        k++;
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, join);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.join as defined in 22.1.3.12 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asTypedArrayObject()->arrayLength();

    Value separator = argv[0];
    size_t lenMax = STRING_MAXIMUM_LENGTH;
    String* sep;

    if (separator.isUndefined()) {
        sep = state.context()->staticStrings().asciiTable[(size_t)','].string();
    } else {
        sep = separator.toString(state);
    }

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(O) || len == 0) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, O);

    StringBuilder builder;
    Value elem = O->getIndexedProperty(state, Value(0)).value(state, O);
    if (elem.isUndefinedOrNull()) {
        elem = String::emptyString;
    }
    builder.appendString(elem.toString(state));

    size_t curIndex = 1;
    while (curIndex < len) {
        if (sep->length() > 0) {
            if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - sep->length())) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
            }
            builder.appendString(sep);
        }
        elem = O->getIndexedProperty(state, Value(curIndex)).value(state, O);
        if (elem.isUndefinedOrNull()) {
            elem = String::emptyString;
        }
        builder.appendString(elem.toString(state));
        curIndex++;
    }

    return builder.finalize(&state);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.map
static Value builtinTypedArrayMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    size_t len = O->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().map.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let A be ? TypedArraySpeciesCreate(O, « len »).
    Value arg[1] = { Value(len) };
    Value A = TypedArraySpeciesCreate(state, O, 1, arg);

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        Value args[] = { kValue, Value(k), O };
        Value mappedValue = Object::call(state, callbackfn, T, 3, args);
        A.asObject()->setIndexedPropertyThrowsException(state, Value(k), mappedValue);
        k++;
    }
    // Return A.
    return A;
}

static Value builtinTypedArrayReduce(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reduce);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.reduce as defined in 22.1.3.18 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asTypedArrayObject()->arrayLength();

    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduce.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduce.string(), ErrorObject::Messages::GlobalObject_ReduceError);
    }

    size_t k = 0; // 6
    Value accumulator;
    if (argc > 1) { // 7
        accumulator = argv[1];
    } else { // 8
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        accumulator = res.value(state, O);
        k++;
    }
    while (k < len) { // 9
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        Value kValue = res.value(state, O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k++;
    }
    return accumulator;
}

static Value builtinTypedArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reduceRight);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.reduceRight as defined in 22.1.3.19 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asTypedArrayObject()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().reduceRight.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If len is 0 and initialValue is not present, throw a TypeError exception.
    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduceRight.string(), ErrorObject::Messages::GlobalObject_ReduceError);
    }

    // Let k be len-1.
    int64_t k = len - 1;

    Value accumulator;
    // If initialValue is present, then
    if (argc > 1) {
        // Set accumulator to initialValue.
        accumulator = argv[1];
    } else {
        // Else, initialValue is not present
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        accumulator = res.value(state, O);
        k--;
    }

    // Repeat, while k ≥ 0
    while (k >= 0) {
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        Value kValue = res.value(state, O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k--;
    }
    // Return accumulator.
    return accumulator;
}

static Value builtinTypedArrayReverse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reverse);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, O);

    // Array.prototype.reverse as defined in 22.1.3.20 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->asTypedArrayObject()->arrayLength();
    size_t middle = std::floor(len / 2);
    size_t lower = 0;
    while (middle > lower) {
        size_t upper = len - lower - 1;

        ObjectGetResult upperResult = O->getIndexedProperty(state, Value(upper));
        ObjectGetResult lowerResult = O->getIndexedProperty(state, Value(lower));
        RELEASE_ASSERT(upperResult.hasValue() && lowerResult.hasValue());

        Value upperValue = upperResult.value(state, O);
        Value lowerValue = lowerResult.value(state, O);

        if (!O->setIndexedProperty(state, Value(lower), upperValue)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
        }
        if (!O->setIndexedProperty(state, Value(upper), lowerValue)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
        }

        lower++;
    }
    return O;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.slice
static Value builtinTypedArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    size_t len = O->arrayLength();
    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[0].toInteger(state);
    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    size_t k = (relativeStart < 0) ? std::max((double)len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    double finalEnd = (relativeEnd < 0) ? std::max((double)len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);
    // Let count be max(final – k, 0).
    size_t count = std::max((double)finalEnd - k, 0.0);

    Value arg[1] = { Value(count) };
    Value A = TypedArraySpeciesCreate(state, O, 1, arg);
    TypedArrayObject* target = A.asObject()->asTypedArrayObject();

    // If SameValue(srcType, targetType) is false, then
    if (O->typedArrayType() != target->typedArrayType()) {
        size_t n = 0;
        while (k < finalEnd) {
            O->buffer()->throwTypeErrorIfDetached(state);
            Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
            A.asObject()->setIndexedPropertyThrowsException(state, Value(n), kValue);
            k++;
            n++;
        }
    } else if (count > 0) {
        // Else if count > 0,
        ArrayBuffer* srcBuffer = O->buffer();
        srcBuffer->throwTypeErrorIfDetached(state);
        ArrayBuffer* targetBuffer = target->buffer();

        size_t elementSize = O->elementSize();
        size_t srcByteOffset = O->byteOffset();
        size_t targetByteIndex = target->byteOffset();
        size_t srcByteIndex = (size_t)k * elementSize + srcByteOffset;
        size_t limit = targetByteIndex + count * elementSize;

        while (targetByteIndex < limit) {
            // Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, "Uint8", true, "Unordered").
            Value value = srcBuffer->getValueFromBuffer(state, srcByteIndex, TypedArrayType::Uint8);
            // Perform SetValueInBuffer(targetBuffer, targetByteIndex, "Uint8", value, true, "Unordered").
            targetBuffer->setValueInBuffer(state, targetByteIndex, TypedArrayType::Uint8, value);
            srcByteIndex++;
            targetByteIndex++;
        }
    }

    return A;
}

static Value builtinTypedArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(array, TypedArray, toLocaleString);
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, array);

    // Array.prototype.toLocaleString as defined in 22.1.3.26 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = array->asTypedArrayObject()->arrayLength();

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(array)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, array);

    // Let separator be the String value for the list-separator String appropriate for the host environment’s current locale (this is derived in an implementation-defined way).
    String* separator = state.context()->staticStrings().asciiTable[(size_t)','].string();

    // Let R be the empty String.
    String* R = String::emptyString;

    // Let k be 0.
    size_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        // If k > 0, then
        if (k > 0) {
            // Set R to the string-concatenation of R and separator.
            StringBuilder builder;
            builder.appendString(R);
            builder.appendString(separator);
            R = builder.finalize(&state);
        }
        // Let nextElement be ? Get(array, ! ToString(k)).
        Value nextElement = array->getIndexedProperty(state, Value(k)).value(state, array);
        // If nextElement is not undefined or null, then
        ASSERT(!nextElement.isUndefinedOrNull());
        // Let S be ? ToString(? Invoke(nextElement, "toLocaleString")).
        Value func = nextElement.toObject(state)->get(state, state.context()->staticStrings().toLocaleString).value(state, nextElement);
        String* S = Object::call(state, func, nextElement, 0, nullptr).toString(state);
        // Set R to the string-concatenation of R and S.
        StringBuilder builder2;
        builder2.appendString(R);
        builder2.appendString(S);
        R = builder2.finalize(&state);
        // Increase k by 1.
        k++;
    }

    // Return R.
    return R;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.keys
static Value builtinTypedArrayKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    // Return CreateArrayIterator(O, "key").
    return thisValue.asObject()->keys(state);
}

static Value builtinTypedArrayValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    return thisValue.asObject()->values(state);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.entries
static Value builtinTypedArrayEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TypedArrayObject::validateTypedArray(state, thisValue);
    return thisValue.asObject()->entries(state);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-get-%typedarray%.prototype-@@tostringtag
static Value builtinTypedArrayToStringTagGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value O = thisValue;
    if (!O.isObject()) {
        return Value();
    }

    if (O.asObject()->isTypedArrayObject()) {
        return Value(O.asObject()->asTypedArrayObject()->typedArrayName(state));
    }
    return Value();
}

static Value builtinTypedArrayAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TypedArrayObject::validateTypedArray(state, thisValue);
    Object* obj = thisValue.asObject();
    size_t len = obj->length(state);
    double relativeStart = argv[0].toInteger(state);
    if (relativeStart < 0) {
        relativeStart = len + relativeStart;
    }
    if (relativeStart < 0 || relativeStart >= len) {
        return Value();
    }
    return obj->getIndexedProperty(state, Value(Value::DoubleToIntConvertibleTestNeeds, relativeStart)).value(state, thisValue);
}

template <typename TA, int elementSize>
FunctionObject* GlobalObject::installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    NativeFunctionObject* taConstructor = new NativeFunctionObject(state, NativeFunctionInfo(taName, builtinTypedArrayConstructor<TA>, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    taConstructor->setGlobalIntrinsicObject(state);

    *proto = m_objectPrototype;
    Object* taPrototype = new TypedArrayPrototypeObject(state);
    taPrototype->setGlobalIntrinsicObject(state, true);
    taPrototype->setPrototype(state, typedArrayFunction->getFunctionPrototype(state));

    taConstructor->setPrototype(state, typedArrayFunction); // %TypedArray%
    taConstructor->setFunctionPrototype(state, taPrototype);

    // 22.2.5.1 /TypedArray/.BYTES_PER_ELEMENT
    taConstructor->directDefineOwnProperty(state, ObjectPropertyName(strings->BYTES_PER_ELEMENT), ObjectPropertyDescriptor(Value(elementSize), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // 22.2.6.1 /TypedArray/.prototype.BYTES_PER_ELEMENT
    taPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->BYTES_PER_ELEMENT), ObjectPropertyDescriptor(Value(elementSize), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // 22.2.6.2 /TypedArray/.prototype.constructor
    taPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(taConstructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(taName),
                        ObjectPropertyDescriptor(taConstructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    return taConstructor;
}

void GlobalObject::initializeTypedArray(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

#define INITIALIZE_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                                                                                                          \
    {                                                                                                                                                                                                                               \
        ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,                                                                                                              \
                                                                                                    [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value { \
                                                                                                        ASSERT(self->isGlobalObject());                                                                                             \
                                                                                                        return self->asGlobalObject()->type##Array();                                                                               \
                                                                                                    },                                                                                                                              \
                                                                                                    nullptr);                                                                                                                       \
        defineNativeDataAccessorProperty(state, ObjectPropertyName(strings->TYPE##Array), nativeData, Value(Value::EmptyValue));                                                                                                    \
    }

    FOR_EACH_TYPEDARRAY_TYPES(INITIALIZE_TYPEDARRAY)
#undef INITIALIZE_TYPEDARRAY
}

void GlobalObject::installTypedArray(ExecutionState& state)
{
    ASSERT(!!m_arrayToString);

    const StaticStrings* strings = &state.context()->staticStrings();

    // %TypedArray%
    FunctionObject* typedArrayFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->TypedArray, builtinTypedArrayConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    typedArrayFunction->setGlobalIntrinsicObject(state);

    typedArrayFunction->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTypedArrayFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayFunction->directDefineOwnProperty(state, ObjectPropertyName(strings->of),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->of, builtinTypedArrayOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }


    // %TypedArray%.prototype
    Object* typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
    typedArrayPrototype->setGlobalIntrinsicObject(state, true);
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->subarray),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->subarray, builtinTypedArraySubArray, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->set),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinTypedArraySet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->some),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->some, builtinTypedArraySome, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->sort),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->sort, builtinTypedArraySort, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->copyWithin),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->copyWithin, builtinTypedArrayCopyWithin, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->every),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->every, builtinTypedArrayEvery, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->fill),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fill, builtinTypedArrayFill, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->filter),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->filter, builtinTypedArrayFilter, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->find),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->find, builtinTypedArrayFind, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->findIndex),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->findIndex, builtinTypedArrayFindIndex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->findLast),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->findLast, builtinTypedArrayFindLast, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->findLastIndex),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->findLastIndex, builtinTypedArrayFindLastIndex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->forEach),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->forEach, builtinTypedArrayForEach, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->join),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->join, builtinTypedArrayJoin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->map),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->map, builtinTypedArrayMap, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->reduce),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reduce, builtinTypedArrayReduce, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->reduceRight),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reduceRight, builtinTypedArrayReduceRight, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->reverse),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reverse, builtinTypedArrayReverse, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->slice),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinTypedArraySlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTypedArrayToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.tostring
    // The initial value of the %TypedArray%.prototype.toString data property is the same built-in function object as the Array.prototype.toString method
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString),
                                                 ObjectPropertyDescriptor(m_arrayToString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->indexOf),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinTypedArrayIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lastIndexOf),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinTypedArrayLastIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->includes),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->includes, builtinTypedArrayIncludes, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->keys),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->keys, builtinTypedArrayKeys, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->entries),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->entries, builtinTypedArrayEntries, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->at),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->at, builtinTypedArrayAt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    auto valuesFn = new NativeFunctionObject(state, NativeFunctionInfo(strings->values, builtinTypedArrayValues, 0, NativeFunctionInfo::Strict));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->values),
                                                 ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                 ObjectPropertyDescriptor(valuesFn,
                                                                          (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolToStringTag, builtinTypedArrayToStringTagGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag), desc);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinTypedArrayByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc2);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteOffset, builtinTypedArrayByteOffsetGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteOffsetDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->byteOffset), byteOffsetDesc2);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getLength, builtinTypedArrayLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor lengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->length), lengthDesc);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getBuffer, builtinTypedArrayBufferGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor bufferDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->buffer), bufferDesc);
    }

    m_typedArray = typedArrayFunction;
    m_typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
#define INSTALL_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                      \
    m_##type##Array = installTypedArray<TYPE##ArrayObject, siz>(state, strings->TYPE##Array, &m_##type##ArrayPrototype, typedArrayFunction); \
    m_##type##ArrayPrototype = m_##type##Array->getFunctionPrototype(state).asObject();

    FOR_EACH_TYPEDARRAY_TYPES(INSTALL_TYPEDARRAY)
#undef INSTALL_TYPEDARRAY
}
} // namespace Escargot
