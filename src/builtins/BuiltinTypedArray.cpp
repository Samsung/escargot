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
#include "runtime/TypedArrayInlines.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "util/Base64.h"


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
    case TypedArrayType::Float16:
        return glob->float16Array();
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

static TypedArrayObject* typedArraySpeciesCreate(ExecutionState& state, TypedArrayObject* exemplar, size_t argc, Value* argumentList)
{
    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, exemplar->typedArrayType());
    // Let C be SpeciesConstructor(O, defaultConstructor).
    Value C = exemplar->speciesConstructor(state, defaultConstructor);
    Value A = Object::construct(state, C, argc, argumentList);
    TypedArrayObject::validateTypedArray(state, A);
    if (argc == 1 && argumentList[0].isNumber() && A.asObject()->asTypedArrayObject()->arrayLength() < argumentList->toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    return A.asObject()->asTypedArrayObject();
}

static Value typedArrayCreateSameType(ExecutionState& state, TypedArrayObject* exemplar, size_t argc, Value* argumentList)
{
    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, exemplar->typedArrayType());
    Value A = Object::construct(state, defaultConstructor, argc, argumentList);
    TypedArrayObject::validateTypedArray(state, A);
    if (argc == 1 && argumentList[0].isNumber() && A.asObject()->asTypedArrayObject()->arrayLength() < argumentList->toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_ThisNotConstructor);
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
        Value kValue = arrayLike->getIndexedPropertyValue(state, Value(k), arrayLike);
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_ThisNotConstructor);
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
    TypedArrayObject::validateTypedArray(state, srcArray);

    size_t elementLength = srcArray->arrayLength();
    size_t srcElementSize = srcArray->elementSize();
    size_t srcByteOffset = srcArray->byteOffset();
    size_t elementSize = obj->elementSize();
    uint64_t byteLength = static_cast<uint64_t>(elementSize) * elementLength;

    Value bufferConstructor = state.context()->globalObject()->arrayBuffer();

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
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
    }

    uint64_t newLength = 0;
    if (!length.isUndefined()) {
        newLength = length.toIndex(state);
        if (newLength == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
    }

    buffer->throwTypeErrorIfDetached(state);
    uint64_t bufferByteLength = buffer->byteLength();

    uint64_t newByteLength = 0;
    if (length.isUndefined()) {
        if ((bufferByteLength % elementSize != 0 && !buffer->isResizableArrayBuffer()) || (bufferByteLength < offset)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
        newByteLength = bufferByteLength - offset;
    } else {
        newByteLength = newLength * elementSize;

        if ((bufferByteLength < newByteLength) || (offset > bufferByteLength - newByteLength)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
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
        obj->setIndexedPropertyThrowsException(state, Value(k), arrayLike->getIndexedPropertyValue(state, Value(k), arrayLike));
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
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
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
        // Set taRecord to MakeTypedArrayWithBufferWitnessRecord(O, seq-cst).
        // If IsTypedArrayOutOfBounds(taRecord) is true, throw a TypeError exception.
        TypedArrayObject::validateTypedArray(state, thisValue);
        // Set len to TypedArrayLength(taRecord).
        len = O->arrayLength();
        // Let typedArrayName be the String value of O.[[TypedArrayName]].
        // Let elementSize be the Number value of the Element Size value specified in Table 59 for typedArrayName.
        size_t elementSize = O->elementSize();
        // Let byteOffset be O.[[ByteOffset]].
        size_t byteOffset = O->byteOffset();
        // Let bufferByteLimit be (len × elementSize) + byteOffset.
        size_t bufferByteLimit = (len * elementSize) + byteOffset;
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
            // If fromByteIndex < bufferByteLimit and toByteIndex < bufferByteLimit, then
            if (fromByteIndex < bufferByteLimit && toByteIndex < bufferByteLimit) {
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
            } else {
                countBytes = 0;
            }
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
    int64_t byteK = static_cast<int64_t>(k) * elementSize;
    int64_t byteLength = static_cast<int64_t>(len) * elementSize;

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
        byteK = byteLength - elementSize;
    }

    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        uint64_t argv0ToUint64 = type == TypedArrayType::BigInt64 ? bitwise_cast<uint64_t>(value.asBigInt()->toInt64()) : value.asBigInt()->toUint64();
        while (compFn(byteK, byteLength)) {
            if (*reinterpret_cast<uint64_t*>(&buffer[byteK]) == argv0ToUint64) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, 8);
        }
    } else if (type == TypedArrayType::Float16) {
        double num = value.asNumber();
        if (std::isnan(num)) {
            return Value(-1);
        }
        while (compFn(byteK, byteLength)) {
            double c = convertFloat16ToFloat64(*reinterpret_cast<uint16_t*>(&buffer[byteK]));
            if (c == num) {
                return Value(byteK / elementSize);
            }
            updateFn(byteK, 2);
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
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    size_t len = O->arrayLength();

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
    len = std::min((size_t)O->arrayLength(), len);
    return fastTypedArrayIndexSearch<true>(O, k, len, argv[0]);
}

static Value builtinTypedArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    size_t len = O->arrayLength();

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

    // If n = -∞, return -1𝔽.
    if (n == -std::numeric_limits<double>::infinity()) {
        return Value(-1);
    }

    // If n ≥ 0, then let k be min(n, len – 1).
    double k;
    if (n >= 0) {
        k = (n == -0) ? 0 : std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len + n.
        k = len + n;
    }

    if (k < 0) {
        return Value(-1);
    }
    // Reloading `len` because second argument can affect arrayLength
    len = O->arrayLength();
    return fastTypedArrayIndexSearch<false>(O, static_cast<int64_t>(k), len, argv[0]);
}

static Value builtinTypedArrayIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    size_t len = O->arrayLength();

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
        Value elementK = O->getIndexedPropertyValue(state, Value(k), O);
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

// https://tc39.es/ecma262/#sec-settypedarrayfromtypedarray
static void setTypedArrayFromTypedArray(ExecutionState& state, TypedArrayObject* target, double targetOffset, TypedArrayObject* source)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    // Let targetBuffer be target.[[ViewedArrayBuffer]].
    auto targetBuffer = target->buffer();
    // Let targetRecord be MakeTypedArrayWithBufferWitnessRecord(target, seq-cst).
    // If IsTypedArrayOutOfBounds(targetRecord) is true, throw a TypeError exception.
    TypedArrayObject::validateTypedArray(state, target);
    // Let targetLength be TypedArrayLength(targetRecord).
    double targetLength = target->arrayLength();
    // Let srcBuffer be source.[[ViewedArrayBuffer]].
    auto srcBuffer = source->buffer();
    // Let srcRecord be MakeTypedArrayWithBufferWitnessRecord(source, seq-cst).
    // If IsTypedArrayOutOfBounds(srcRecord) is true, throw a TypeError exception.
    TypedArrayObject::validateTypedArray(state, source);
    // Let srcLength be TypedArrayLength(srcRecord).
    double srcLength = source->arrayLength();
    // Let targetType be TypedArrayElementType(target).
    auto targetType = target->typedArrayType();
    // Let targetElementSize be TypedArrayElementSize(target).
    auto targetElementSize = target->elementSize();
    // Let targetByteOffset be target.[[ByteOffset]].
    auto targetByteOffset = target->byteOffset();
    // Let srcType be TypedArrayElementType(source).
    auto srcType = source->typedArrayType();
    // Let srcElementSize be TypedArrayElementSize(source).
    auto srcElementSize = source->elementSize();
    // Let srcByteOffset be source.[[ByteOffset]].
    auto srcByteOffset = source->byteOffset();
    // If targetOffset = +∞, throw a RangeError exception.
    if (targetOffset == std::numeric_limits<double>::infinity()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    // If srcLength + targetOffset > targetLength, throw a RangeError exception.
    if (srcLength + targetOffset > targetLength) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    // If target.[[ContentType]] is not source.[[ContentType]], throw a TypeError exception.
    bool isSrcBigIntArray = srcType == TypedArrayType::BigInt64 || srcType == TypedArrayType::BigUint64;
    bool isTargetBigIntArray = targetType == TypedArrayType::BigInt64 || targetType == TypedArrayType::BigUint64;
    if (isTargetBigIntArray != isSrcBigIntArray) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->TypedArray.string(), true, strings->set.string(), "Cannot mix BigIntArray with other Array");
    }
    // If IsSharedArrayBuffer(srcBuffer) is true, IsSharedArrayBuffer(targetBuffer) is true,
    // and srcBuffer.[[ArrayBufferData]] is targetBuffer.[[ArrayBufferData]],
    // let sameSharedArrayBuffer be true; otherwise, let sameSharedArrayBuffer be false.
    bool sameSharedArrayBuffer;
    if (srcBuffer->isSharedArrayBufferObject() && srcBuffer == targetBuffer) {
        sameSharedArrayBuffer = true;
    } else {
        sameSharedArrayBuffer = false;
    }

    size_t srcByteIndex;
    // If SameValue(srcBuffer, targetBuffer) is true or sameSharedArrayBuffer is true, then
    if (srcBuffer == targetBuffer || sameSharedArrayBuffer) {
        // Let srcByteLength be TypedArrayByteLength(srcRecord).
        size_t srcByteLength = source->byteLength();
        // Set srcBuffer to ? CloneArrayBuffer(srcBuffer, srcByteOffset, srcByteLength).
#if defined(ENABLE_THREADING)
        srcBuffer = ArrayBufferObject::cloneArrayBuffer(state, srcBuffer, srcByteOffset, srcByteLength,
                                                        srcBuffer->isSharedArrayBufferObject() ? state.context()->globalObject()->sharedArrayBuffer() : state.context()->globalObject()->arrayBuffer());
#else
        srcBuffer = ArrayBufferObject::cloneArrayBuffer(state, srcBuffer, srcByteOffset, srcByteLength, state.context()->globalObject()->arrayBuffer());
#endif
        // Let srcByteIndex be 0.
        srcByteIndex = 0;
    } else {
        // Else,
        // Let srcByteIndex be srcByteOffset.
        srcByteIndex = srcByteOffset;
    }

    // Let targetByteIndex be (targetOffset × targetElementSize) + targetByteOffset.
    double targetByteIndex = (targetOffset * targetElementSize) + targetByteOffset;
    // Let limit be targetByteIndex + (targetElementSize × srcLength).
    double limit = targetByteIndex + (targetElementSize * srcLength);

    // If srcType is targetType, then
    if (srcType == targetType) {
        // NOTE: The transfer must be performed in a manner that preserves the bit-level encoding of the source data.
        // Repeat, while targetByteIndex < limit,
        //   Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, uint8, true, unordered).
        //   Perform SetValueInBuffer(targetBuffer, targetByteIndex, uint8, value, true, unordered).
        //   Set srcByteIndex to srcByteIndex + 1.
        //   Set targetByteIndex to targetByteIndex + 1.
        memmove(targetBuffer->data() + static_cast<size_t>(targetByteIndex),
                srcBuffer->data() + srcByteIndex, limit - targetByteIndex);

    } else {
        // Else,
        // Repeat, while targetByteIndex < limit,
        while (targetByteIndex < limit) {
            // Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, srcType, true, unordered).
            Value value = srcBuffer->getValueFromBuffer(state, srcByteIndex, srcType);
            // Perform SetValueInBuffer(targetBuffer, targetByteIndex, targetType, value, true, unordered).
            targetBuffer->setValueInBuffer(state, targetByteIndex, targetType, value);
            // Set srcByteIndex to srcByteIndex + srcElementSize.
            srcByteIndex += srcElementSize;
            // Set targetByteIndex to targetByteIndex + targetElementSize.
            targetByteIndex += targetElementSize;
        }
    }
    // Return unused.
}

// https://tc39.es/ecma262/#sec-settypedarrayfromarraylike
static void setTypedArrayFromArrayLike(ExecutionState& state, TypedArrayObject* target, double targetOffset, const Value& source)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    // Let targetRecord be MakeTypedArrayWithBufferWitnessRecord(target, seq-cst).
    // If IsTypedArrayOutOfBounds(targetRecord) is true, throw a TypeError exception.
    TypedArrayObject::validateTypedArray(state, target);
    // Let targetLength be TypedArrayLength(targetRecord).
    auto targetLength = target->arrayLength();
    // Let src be ? ToObject(source).
    Object* src = source.toObject(state);
    // Let srcLength be ? LengthOfArrayLike(src).
    auto srcLength = src->length(state);
    // If targetOffset = +∞, throw a RangeError exception.
    if (targetOffset == std::numeric_limits<double>::infinity()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    // If srcLength + targetOffset > targetLength, throw a RangeError exception.
    if (srcLength + targetOffset > targetLength) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }
    // Let k be 0.
    double k = 0;
    // Repeat, while k < srcLength,
    while (k < srcLength) {
        // Let Pk be ! ToString(𝔽(k)).
        // Let value be ? Get(src, Pk).
        Value value = src->getIndexedProperty(state, Value(Value::DoubleToIntConvertibleTestNeeds, k)).value(state, src);
        // Let targetIndex be 𝔽(targetOffset + k).
        // Perform ? TypedArraySetElement(target, targetIndex, value).
        target->setIndexedProperty(state, Value(Value::DoubleToIntConvertibleTestNeeds, k + targetOffset), value);
        // Set k to k + 1.
        k++;
    }
    // Return unused.
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.set-overloaded-offset
static Value builtinTypedArraySet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    // Let target be the this value.
    // Perform ? RequireInternalSlot(target, [[TypedArrayName]]).
    // Assert: target has a [[ViewedArrayBuffer]] internal slot.
    TypedArrayObject::validateTypedArray(state, thisValue, false);
    TypedArrayObject* target = thisValue.asObject()->asTypedArrayObject();
    // Let targetOffset be ? ToIntegerOrInfinity(offset).
    double targetOffset = argc > 1 ? argv[1].toInteger(state) : Value().toInteger(state);
    // If targetOffset < 0, throw a RangeError exception.
    if (targetOffset < 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->TypedArray.string(), true, strings->set.string(), "Start offset is negative");
    }
    // If source is an Object that has a [[TypedArrayName]] internal slot, then
    if (argv[0].isObject() && argv[0].asObject()->isTypedArrayObject()) {
        // Perform ? SetTypedArrayFromTypedArray(target, targetOffset, source).
        auto source = argv[0].asObject()->asTypedArrayObject();
        setTypedArrayFromTypedArray(state, target, targetOffset, source);
    } else {
        // Else,
        // Perform ? SetTypedArrayFromArrayLike(target, targetOffset, source).
        setTypedArrayFromArrayLike(state, target, targetOffset, argv[0]);
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.some as defined in 22.1.3.23 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->arrayLength();

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
        // Let kValue be the result of calling the [[Get]] internal method of O with argument ToString(k).
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }

    // Let buffer be TypedArrayObject::validateTypedArray(obj).
    ArrayBuffer* buffer = TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    uint64_t len = O->arrayLength();
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();
    // [defaultSort, &cmpfn, &state, &buffer]
    O->sort(state, len, [&](const Value& x, const Value& y) -> bool {
        ASSERT((x.isNumber() || x.isBigInt()) && (y.isNumber() || y.isBigInt()));
        if (!defaultSort) {
            Value args[] = { x, y };
            double v = Object::call(state, cmpfn, Value(), 2, args).toNumber(state);
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

static Value builtinTypedArrayToSorted(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toSorted.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }

    // Let buffer be TypedArrayObject::validateTypedArray(obj).
    ArrayBuffer* buffer = TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    uint64_t len = O->arrayLength();
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    Value arg[1] = { Value(len) };
    TypedArrayObject* A = typedArrayCreateSameType(state, O, 1, arg).asObject()->asTypedArrayObject();

    // [defaultSort, &cmpfn, &state, &buffer]
    O->toSorted(state, A, len, [&](const Value& x, const Value& y) -> bool {
        ASSERT((x.isNumber() || x.isBigInt()) && (y.isNumber() || y.isBigInt()));
        if (!defaultSort) {
            Value args[] = { x, y };
            double v = Object::call(state, cmpfn, Value(), 2, args).toNumber(state);
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

    return A;
}

static Value builtinTypedArrayToReversed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    size_t length = O->arrayLength();

    Value arg[1] = { Value(length) };
    TypedArrayObject* A = typedArrayCreateSameType(state, O, 1, arg).asObject()->asTypedArrayObject();

    size_t k = 0;
    while (k < length) {
        Value fromValue = O->getIndexedPropertyValue(state, Value(length - k - 1), O);
        A->setIndexedProperty(state, Value(k), fromValue);
        k++;
    }

    return A;
}

// https://tc39.es/ecma262/#sec-%typedarray%.prototype.subarray
static Value builtinTypedArraySubArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    // Perform ? RequireInternalSlot(O, [[TypedArrayName]]).
    // Assert: O has a [[ViewedArrayBuffer]] internal slot.
    RESOLVE_THIS_BINDING_TO_TYPEDARRAY(O, TypedArray, subarray);
    // Let buffer be O.[[ViewedArrayBuffer]].
    auto buffer = O->buffer();
    // Let srcRecord be MakeTypedArrayWithBufferWitnessRecord(O, seq-cst).
    // If IsTypedArrayOutOfBounds(srcRecord) is true, then
    size_t srcLength;
    if (O->wasResetByInvalidByteLength()) {
        // Let srcLength be 0.
        srcLength = 0;
    } else {
        // Else,
        // Let srcLength be TypedArrayLength(srcRecord).
        srcLength = O->arrayLength();
    }

    // Let relativeStart be ? ToIntegerOrInfinity(start).
    double relativeStart = argv[0].toInteger(state);
    // If relativeStart = -∞, let startIndex be 0.
    double startIndex;
    if (relativeStart == -std::numeric_limits<double>::infinity()) {
        startIndex = 0;
    } else if (relativeStart < 0) {
        // Else if relativeStart < 0, let startIndex be max(srcLength + relativeStart, 0).
        startIndex = std::max(srcLength + relativeStart, 0.0);
    } else {
        // Else, let startIndex be min(relativeStart, srcLength).
        startIndex = std::min(relativeStart, static_cast<double>(srcLength));
    }
    // Let elementSize be TypedArrayElementSize(O).
    auto elementSize = O->elementSize();
    // Let srcByteOffset be O.[[ByteOffset]].
    // NOTE we need original byteOffset here
    size_t srcByteOffset = O->originalByteOffset();
    // Let beginByteOffset be srcByteOffset + (startIndex × elementSize).
    double beginByteOffset = srcByteOffset + (startIndex * elementSize);

    // If O.[[ArrayLength]] is auto and end is undefined, then
    Value argumentsList[3];
    size_t argumentsListSize = 3;
    if (O->isAuto() && argv[1].isUndefined()) {
        // Let argumentsList be « buffer, 𝔽(beginByteOffset) ».
        argumentsListSize = 2;
        argumentsList[0] = buffer;
        argumentsList[1] = Value(Value::DoubleToIntConvertibleTestNeeds, beginByteOffset);
    } else {
        // Else,
        // If end is undefined, let relativeEnd be srcLength; else let relativeEnd be ? ToIntegerOrInfinity(end).
        double relativeEnd;
        if (argv[1].isUndefined()) {
            relativeEnd = srcLength;
        } else {
            relativeEnd = argv[1].toInteger(state);
        }
        double endIndex = 0;
        // If relativeEnd = -∞, let endIndex be 0.
        if (relativeEnd == -std::numeric_limits<double>::infinity()) {
            endIndex = 0;
        } else if (relativeEnd < 0) {
            // Else if relativeEnd < 0, let endIndex be max(srcLength + relativeEnd, 0).
            endIndex = std::max(srcLength + relativeEnd, 0.0);
        } else {
            // Else, let endIndex be min(relativeEnd, srcLength).
            endIndex = std::min(relativeEnd, static_cast<double>(srcLength));
        }
        // Let newLength be max(endIndex - startIndex, 0).
        double newLength = std::max(endIndex - startIndex, 0.0);
        // Let argumentsList be « buffer, 𝔽(beginByteOffset), 𝔽(newLength) ».
        argumentsList[0] = buffer;
        argumentsList[1] = Value(Value::DoubleToIntConvertibleTestNeeds, beginByteOffset);
        argumentsList[2] = Value(Value::DoubleToIntConvertibleTestNeeds, newLength);
    }

    // Return ? TypedArraySpeciesCreate(O, argumentsList).
    return typedArraySpeciesCreate(state, O, argumentsListSize, argumentsList);
}

static Value builtinTypedArrayEvery(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.every as defined in 22.1.3.5 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    unsigned len = O->arrayLength();

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
        // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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

// https://www.ecma-international.org/ecma-262/10.0/#sec-%typedarray%.prototype.fill
static Value builtinTypedArrayFill(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Perform ? TypedArrayObject::validateTypedArray(O).
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    size_t len = O->arrayLength();

    Value value(Value::ForceUninitialized);
    // If O.[[TypedArrayName]] is "BigUint64Array" or "BigInt64Array", let value be ? ToBigInt(value).
    auto typedArrayType = O->typedArrayType();
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
    // If relativeStart = -∞, let startIndex be 0.
    size_t startIndex;
    if (relativeStart == -std::numeric_limits<double>::infinity()) {
        startIndex = 0;
    } else if (relativeStart < 0) {
        // Else if relativeStart < 0, let startIndex be max(len + relativeStart, 0).
        startIndex = std::max(len + relativeStart, 0.0);
    } else {
        // Else, let startIndex be min(relativeStart, len).
        startIndex = std::min(relativeStart, static_cast<double>(len));
    }
    Value end = argc > 2 ? argv[2] : Value();
    //  If end is undefined, let relativeEnd be len; else let relativeEnd be ? ToIntegerOrInfinity(end).
    double relativeEnd;
    if (end.isUndefined()) {
        relativeEnd = len;
    } else {
        relativeEnd = end.toInteger(state);
    }
    size_t endIndex;
    //  If relativeEnd = -∞, let endIndex be 0.
    if (relativeEnd == -std::numeric_limits<double>::infinity()) {
        endIndex = 0;
    } else if (relativeEnd < 0) {
        //  Else if relativeEnd < 0, let endIndex be max(len + relativeEnd, 0).
        endIndex = std::max(len + relativeEnd, 0.0);
    } else {
        //  Else, let endIndex be min(relativeEnd, len).
        endIndex = std::min(relativeEnd, static_cast<double>(len));
    }
    //  Set taRecord to MakeTypedArrayWithBufferWitnessRecord(O, seq-cst).
    //  If IsTypedArrayOutOfBounds(taRecord) is true, throw a TypeError exception.
    TypedArrayObject::validateTypedArray(state, thisValue);
    //  Set len to TypedArrayLength(taRecord).
    len = O->arrayLength();

    // Set endIndex to min(endIndex, len).
    double index = std::min(endIndex, len);
    // Let k be startIndex.
    auto k = startIndex;
    // Repeat, while k < endIndex,
    while (k < endIndex) {
        // Let Pk be ! ToString(𝔽(k)).
        // b. Perform ! Set(O, Pk, value, true).
        // c. Set k to k + 1.
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
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    Value A = typedArraySpeciesCreate(state, O, 1, arg);

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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.find as defined in 22.1.3.8 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().find.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be Get(O, Pk).
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.findIndex as defined in 22.1.3.9 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().findIndex.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    int64_t len = static_cast<int64_t>(O->arrayLength());

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
        kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Let len be O.[[ArrayLength]].
    int64_t len = static_cast<int64_t>(O->arrayLength());

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
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.forEach as defined in 22.1.3.10 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->arrayLength();

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
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
        Value args[] = { kValue, Value(k), O };
        Object::call(state, callbackfn, T, 3, args);
        k++;
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.join as defined in 22.1.3.12 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->arrayLength();

    Value separator = argv[0];
    size_t lenMax = STRING_MAXIMUM_LENGTH;
    String* sep;

    if (separator.isUndefined()) {
        sep = state.context()->staticStrings().asciiTable[(size_t)','].string();
    } else {
        sep = separator.toString(state);
    }

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(O) || len == 0) {
        return String::emptyString();
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, O);

    StringBuilder builder;
    Value elem = O->getIndexedPropertyValue(state, Value(0), O);
    if (elem.isUndefinedOrNull()) {
        elem = String::emptyString();
    }
    builder.appendString(elem.toString(state), &state);

    size_t curIndex = 1;
    while (curIndex < len) {
        if (sep->length() > 0) {
            builder.appendString(sep, &state);
        }
        elem = O->getIndexedPropertyValue(state, Value(curIndex), O);
        if (elem.isUndefinedOrNull()) {
            elem = String::emptyString();
        }
        builder.appendString(elem.toString(state), &state);
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
    Value A = typedArraySpeciesCreate(state, O, 1, arg);

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
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
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.reduce as defined in 22.1.3.18 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->arrayLength();

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
        accumulator = O->getIndexedPropertyValue(state, Value(k), O);
        k++;
    }
    while (k < len) { // 9
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k++;
    }
    return accumulator;
}

static Value builtinTypedArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.reduceRight as defined in 22.1.3.19 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->arrayLength();

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
        accumulator = O->getIndexedPropertyValue(state, Value(k), O);
        k--;
    }

    // Repeat, while k ≥ 0
    while (k >= 0) {
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k--;
    }
    // Return accumulator.
    return accumulator;
}

static Value builtinTypedArrayReverse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.reverse as defined in 22.1.3.20 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = O->arrayLength();
    size_t middle = std::floor(len / 2);
    size_t lower = 0;
    while (middle > lower) {
        size_t upper = len - lower - 1;

        Value upperValue = O->getIndexedPropertyValue(state, Value(upper), O);
        Value lowerValue = O->getIndexedPropertyValue(state, Value(lower), O);

        if (!O->setIndexedProperty(state, Value(lower), upperValue)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::DefineProperty_NotWritable);
        }
        if (!O->setIndexedProperty(state, Value(upper), lowerValue)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString(), ErrorObject::Messages::DefineProperty_NotWritable);
        }

        lower++;
    }
    return O;
}

// https://tc39.es/ecma262/#sec-%typedarray%.prototype.slice
static Value builtinTypedArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    TypedArrayObject::validateTypedArray(state, thisValue);
    // Let taRecord be ? ValidateTypedArray(O, seq-cst).
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();
    // Let srcArrayLength be TypedArrayLength(taRecord).
    auto srcArrayLength = O->arrayLength();
    // Let relativeStart be ? ToIntegerOrInfinity(start).
    double relativeStart = argv[0].toInteger(state);
    // If relativeStart = -∞, let startIndex be 0.
    double startIndex;
    if (relativeStart == -std::numeric_limits<double>::infinity()) {
        startIndex = 0;
    } else if (relativeStart < 0) {
        // Else if relativeStart < 0, let startIndex be max(srcArrayLength + relativeStart, 0).
        startIndex = std::max(srcArrayLength + relativeStart, 0.0);
    } else {
        // Else, let startIndex be min(relativeStart, srcArrayLength).
        startIndex = std::min(relativeStart, static_cast<double>(srcArrayLength));
    }

    // If end is undefined, let relativeEnd be srcArrayLength; else let relativeEnd be ? ToIntegerOrInfinity(end).
    double relativeEnd;
    if (argv[1].isUndefined()) {
        relativeEnd = srcArrayLength;
    } else {
        relativeEnd = argv[1].toInteger(state);
    }
    double endIndex;
    // If relativeEnd = -∞, let endIndex be 0.
    if (relativeEnd == -std::numeric_limits<double>::infinity()) {
        endIndex = 0;
    } else if (relativeEnd < 0) {
        // Else if relativeEnd < 0, let endIndex be max(srcArrayLength + relativeEnd, 0).
        endIndex = std::max(srcArrayLength + relativeEnd, 0.0);
    } else {
        // Else, let endIndex be min(relativeEnd, srcArrayLength).
        endIndex = std::min(relativeEnd, static_cast<double>(srcArrayLength));
    }
    // Let countBytes be max(endIndex - startIndex, 0).
    double countBytes = std::max(endIndex - startIndex, 0.0);
    // Let A be ? TypedArraySpeciesCreate(O, « 𝔽(countBytes) »).
    // static Value TypedArraySpeciesCreate(ExecutionState& state, TypedArrayObject* exemplar, size_t argc, Value* argumentList)
    Value argumentList(Value::DoubleToIntConvertibleTestNeeds, countBytes);
    auto A = typedArraySpeciesCreate(state, O, 1, &argumentList);

    // If countBytes > 0, then
    if (countBytes > 0) {
        // Set taRecord to MakeTypedArrayWithBufferWitnessRecord(O, seq-cst).
        // If IsTypedArrayOutOfBounds(taRecord) is true, throw a TypeError exception.
        TypedArrayObject::validateTypedArray(state, O);
        // Set endIndex to min(endIndex, TypedArrayLength(taRecord)).
        endIndex = std::min(endIndex, static_cast<double>(O->arrayLength()));
        // Set countBytes to max(endIndex - startIndex, 0).
        countBytes = endIndex - startIndex;
        if (countBytes < 0) {
            countBytes = 0;
        }
        // Let srcType be TypedArrayElementType(O).
        auto srcType = O->typedArrayType();
        // Let targetType be TypedArrayElementType(A).
        auto targetType = A->typedArrayType();
        // If srcType is targetType, then
        if (srcType == targetType) {
            // NOTE: The transfer must be performed in a manner that preserves the bit-level encoding of the source data.
            // Let srcBuffer be O.[[ViewedArrayBuffer]].
            auto srcBuffer = O->buffer();
            // Let targetBuffer be A.[[ViewedArrayBuffer]].
            auto targetBuffer = A->buffer();
            // Let elementSize be TypedArrayElementSize(O).
            auto elementSize = O->elementSize();
            // Let srcByteOffset be O.[[ByteOffset]].
            auto srcByteOffset = O->byteOffset();
            // Let srcByteIndex be (startIndex × elementSize) + srcByteOffset.
            auto srcByteIndex = (startIndex * elementSize) + srcByteOffset;
            // Let targetByteIndex be A.[[ByteOffset]].
            auto targetByteIndex = A->byteOffset();
            // Let endByteIndex be targetByteIndex + (countBytes × elementSize).
            auto endByteIndex = targetByteIndex + (countBytes * elementSize);
            // Repeat, while targetByteIndex < endByteIndex,
            while (targetByteIndex < endByteIndex) {
                // Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, uint8, true, unordered).
                Value value = srcBuffer->getValueFromBuffer(state, srcByteIndex, TypedArrayType::Uint8);
                // Perform SetValueInBuffer(targetBuffer, targetByteIndex, uint8, value, true, unordered).
                targetBuffer->setValueInBuffer(state, targetByteIndex, TypedArrayType::Uint8, value);
                // Set srcByteIndex to srcByteIndex + 1.
                srcByteIndex++;
                // Set targetByteIndex to targetByteIndex + 1.
                targetByteIndex++;
            }
        } else {
            // Else,
            // Let n be 0.
            size_t n = 0;
            // Let k be startIndex.
            auto k = startIndex;
            // Repeat, while k < endIndex,
            while (k < endIndex) {
                // Let Pk be ! ToString(𝔽(k)).
                Value Pk(Value::DoubleToIntConvertibleTestNeeds, k);
                // Let kValue be ! Get(O, Pk).
                Value kValue = O->getIndexedPropertyValue(state, Pk, O);
                // Perform ! Set(A, ! ToString(𝔽(n)), kValue, true).
                A->setIndexedPropertyThrowsException(state, Value(n), kValue);
                // Set k to k + 1.
                k++;
                // Set n to n + 1.
                n++;
            }
        }
    }
    // Return A.
    return A;
}

static Value builtinTypedArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // validateTypedArray is applied to the this value prior to evaluating the algorithm.
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* array = thisValue.asObject()->asTypedArrayObject();

    // Array.prototype.toLocaleString as defined in 22.1.3.26 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    size_t len = array->arrayLength();

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(array)) {
        return String::emptyString();
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, array);

    // Let separator be the String value for the list-separator String appropriate for the host environment’s current locale (this is derived in an implementation-defined way).
    String* separator = state.context()->staticStrings().asciiTable[(size_t)','].string();

    // Let R be the empty String.
    String* R = String::emptyString();

    // Let k be 0.
    size_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        // If k > 0, then
        if (k > 0) {
            // Set R to the string-concatenation of R and separator.
            StringBuilder builder;
            builder.appendString(R, &state);
            builder.appendString(separator, &state);
            R = builder.finalize(&state);
        }
        // Let nextElement be ? Get(array, ! ToString(k)).
        Value nextElement = array->getIndexedPropertyValue(state, Value(k), array);
        // If nextElement is not undefined or null, then
        if (!nextElement.isUndefinedOrNull()) {
            // Let S be ? ToString(? Invoke(nextElement, "toLocaleString")).
            Value func = nextElement.toObject(state)->get(state, state.context()->staticStrings().toLocaleString).value(state, nextElement);
            String* S = Object::call(state, func, nextElement, 0, nullptr).toString(state);
            // Set R to the string-concatenation of R and S.
            StringBuilder builder2;
            builder2.appendString(R, &state);
            builder2.appendString(S, &state);
            R = builder2.finalize(&state);
        }
        // Increase k by 1.
        k++;
    }

    // Return R.
    return R;
}

static Value builtinTypedArrayWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TypedArrayObject::validateTypedArray(state, thisValue);
    TypedArrayObject* O = thisValue.asObject()->asTypedArrayObject();

    size_t len = O->arrayLength();
    double relativeIndex = argv[0].toInteger(state);
    double actualIndex = relativeIndex;
    auto type = O->typedArrayType();

    if (relativeIndex < 0) {
        actualIndex = len + relativeIndex;
    }

    Value numericValue;
    if (type == TypedArrayType::BigInt64 || type == TypedArrayType::BigUint64) {
        numericValue = argv[1].toBigInt(state);
    } else {
        numericValue = Value(Value::DoubleToIntConvertibleTestNeeds, argv[1].toNumber(state));
    }

    if (UNLIKELY(std::isinf(actualIndex) || actualIndex < 0 || actualIndex >= O->arrayLength())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
    }

    Value arg[1] = { Value(len) };
    TypedArrayObject* A = typedArrayCreateSameType(state, O, 1, arg).asObject()->asTypedArrayObject();

    size_t k = 0;
    Value fromValue;
    while (k < len) {
        if (k == actualIndex) {
            fromValue = numericValue;
        } else {
            fromValue = O->getIndexedPropertyValue(state, Value(k), O);
        }

        A->setIndexedProperty(state, Value(k), fromValue);
        k++;
    }

    return A;
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
    return obj->getIndexedPropertyValue(state, Value(Value::DoubleToIntConvertibleTestNeeds, relativeStart), thisValue);
}

template <typename T>
std::tuple<bool, size_t, size_t> decodeHex(T* cursor, size_t srcLength, uint8_t* result, size_t resultLength)
{
    uint8_t* resultBegin = result;
    uint8_t* resultEnd = result + resultLength;
    T* begin = cursor;
    T* end = cursor + srcLength;
    for (; cursor < end; cursor += 2, result += 1) {
        if (UNLIKELY(resultEnd == result)) {
            return std::make_tuple(true, cursor - begin, result - resultBegin);
        }
        int r1 = parseDigit(*cursor, 16);
        if (UNLIKELY(r1 == -1)) {
            return std::make_tuple(false, cursor - begin, result - resultBegin);
        }
        int r2 = parseDigit(*(cursor + 1), 16);
        if (UNLIKELY(r2 == -1)) {
            return std::make_tuple(false, cursor - begin, result - resultBegin);
        }
        *result = (r1 * 16) + r2;
    }
    return std::make_tuple(true, cursor - begin, result - resultBegin);
}

static Value builtinUint8ArrayFromHex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.fromHex requires that input be an string");
    }

    String* string = argv[0].asString();
    StringBufferAccessData bad = string->bufferAccessData();

    if (bad.length % 2 == 1) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Invalid hex input");
    }

    size_t resultLength = bad.length / 2;
    Uint8ArrayObject* obj = new Uint8ArrayObject(state);
    ArrayBufferObject* abo = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), resultLength);
    obj->setBuffer(abo, 0, resultLength, resultLength);

    std::tuple<bool, size_t, size_t> decodeResult;
    if (bad.has8BitContent) {
        decodeResult = decodeHex(bad.bufferAs8Bit, bad.length, obj->rawBuffer(), obj->arrayLength());
    } else {
        decodeResult = decodeHex(bad.bufferAs16Bit, bad.length, obj->rawBuffer(), obj->arrayLength());
    }

    if (!std::get<0>(decodeResult)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Invalid hex input");
    }

    return obj;
}

static Value builtinUint8ArrayFromBase64(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.fromBase64 requires that input be an string");
    }

    Alphabet alphabet = Alphabet::Base64;
    LastChunkHandling lastChunkHandling = LastChunkHandling::Loose;

    String* string = argv[0].asString();
    Value optionsValue = argc > 1 ? argv[1] : Value();
    if (!optionsValue.isUndefined()) {
        if (!optionsValue.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.fromBase64 requires that options be an object");
        }

        ObjectGetResult optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().alphabet));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (!optionValue.isString()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.fromBase64 options value requires string");
            }
            if (optionValue.asString()->equals("base64url")) {
                alphabet = Alphabet::Base64URL;
            } else if (optionValue.asString()->equals("base64")) {
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value for Uint8Array.fromBase64 options");
            }
        }

        optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lastChunkHandling));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (!optionValue.isString()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.fromBase64 options value requires string");
            }
            if (optionValue.asString()->equals("loose")) {
            } else if (optionValue.asString()->equals("strict")) {
                lastChunkHandling = LastChunkHandling::Strict;
            } else if (optionValue.asString()->equals("stop-before-partial")) {
                lastChunkHandling = LastChunkHandling::StopBeforePartial;
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value for Uint8Array.fromBase64 options");
            }
        }
    }

    auto result = fromBase64(string, nullptr, 0, alphabet, lastChunkHandling);
    if (UNLIKELY(std::get<0>(result) == FromBase64ShouldThrowError::Yes)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Failed to decode Base64 string");
    }

    Uint8ArrayObject* obj = new Uint8ArrayObject(state);
    auto& v = std::get<3>(result);
    ArrayBuffer* abo = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), v.size());
    obj->setBuffer(abo, 0, v.size(), v.size());
    memcpy(abo->data(), v.data(), v.size());
    return obj;
}

static Value builtinUint8ArraySetFromHex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject() || thisValue.asObject()->asTypedArrayObject()->typedArrayType() != TypedArrayType::Uint8) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Uint8Array.string(), true, state.context()->staticStrings().toBase64.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.prototype.setFromHex requires that input be an string");
    }

    String* string = argv[0].asString();
    StringBufferAccessData bad = string->bufferAccessData();
    if (bad.length % 2 == 1) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Invalid hex input");
    }

    TypedArrayObject::validateTypedArray(state, thisValue);
    auto o = thisValue.asObject()->asTypedArrayObject();

    if (!o->arrayLength()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Invalid Uint8Array length");
    }

    std::tuple<bool, size_t, size_t> decodeResult;
    if (bad.has8BitContent) {
        decodeResult = decodeHex(bad.bufferAs8Bit, bad.length, o->rawBuffer(), o->arrayLength());
    } else {
        decodeResult = decodeHex(bad.bufferAs16Bit, bad.length, o->rawBuffer(), o->arrayLength());
    }

    if (!std::get<0>(decodeResult)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Invalid hex input");
    }

    Object* obj = new Object(state);

    obj->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().read), ObjectPropertyDescriptor(Value(std::get<1>(decodeResult)), ObjectPropertyDescriptor::AllPresent));
    obj->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().written), ObjectPropertyDescriptor(Value(std::get<2>(decodeResult)), ObjectPropertyDescriptor::AllPresent));

    return obj;
}

static Value builtinUint8ArrayToHex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject() || thisValue.asObject()->asTypedArrayObject()->typedArrayType() != TypedArrayType::Uint8) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Uint8Array.string(), true, state.context()->staticStrings().toBase64.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    TypedArrayObject::validateTypedArray(state, thisValue);
    auto o = thisValue.asObject()->asTypedArrayObject();

    constexpr char radixDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    uint8_t* end = o->rawBuffer() + o->arrayLength();
    LargeStringBuilder builder;
    for (uint8_t* cursor = o->rawBuffer(); cursor < end; cursor++) {
        auto character = *cursor;
        builder.appendChar(radixDigits[character / 16], &state);
        builder.appendChar(radixDigits[character % 16], &state);
    }

    return builder.finalize(&state);
}

static Value builtinUint8ArraySetFromBase64(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject() || thisValue.asObject()->asTypedArrayObject()->typedArrayType() != TypedArrayType::Uint8) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Uint8Array.string(), true, state.context()->staticStrings().toBase64.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.protype.setFromBase64 requires that input be an string");
    }

    Alphabet alphabet = Alphabet::Base64;
    LastChunkHandling lastChunkHandling = LastChunkHandling::Loose;

    String* string = argv[0].asString();
    Value optionsValue = argc > 1 ? argv[1] : Value();
    if (!optionsValue.isUndefined()) {
        if (!optionsValue.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.protype.setFromBase64 requires that options be an object");
        }

        ObjectGetResult optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().alphabet));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (!optionValue.isString()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.protype.setFromBase64 options value requires string");
            }
            if (optionValue.asString()->equals("base64url")) {
                alphabet = Alphabet::Base64URL;
            } else if (optionValue.asString()->equals("base64")) {
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value for Uint8Array.protype.setFromBase64 options");
            }
        }

        optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lastChunkHandling));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (!optionValue.isString()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.protype.setFromBase64 options value requires string");
            }
            if (optionValue.asString()->equals("loose")) {
            } else if (optionValue.asString()->equals("strict")) {
                lastChunkHandling = LastChunkHandling::Strict;
            } else if (optionValue.asString()->equals("stop-before-partial")) {
                lastChunkHandling = LastChunkHandling::StopBeforePartial;
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value for Uint8Array.protype.setFromBase64 options");
            }
        }
    }

    TypedArrayObject::validateTypedArray(state, thisValue);

    auto o = thisValue.asObject()->asTypedArrayObject();
    auto result = fromBase64(string, o->rawBuffer(), o->arrayLength(), alphabet, lastChunkHandling);
    if (UNLIKELY(std::get<0>(result) == FromBase64ShouldThrowError::Yes)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Failed to decode Base64 string");
    }

    Object* obj = new Object(state);

    obj->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().read), ObjectPropertyDescriptor(Value(std::get<1>(result)), ObjectPropertyDescriptor::AllPresent));
    obj->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().written), ObjectPropertyDescriptor(Value(std::get<2>(result)), ObjectPropertyDescriptor::AllPresent));

    return obj;
}

static Value builtinUint8ArrayToBase64(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isTypedArrayObject() || thisValue.asObject()->asTypedArrayObject()->typedArrayType() != TypedArrayType::Uint8) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Uint8Array.string(), true, state.context()->staticStrings().toBase64.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Base64EncodeOption option = (Base64EncodeOption)0;
    Value optionsValue = argc ? argv[0] : Value();
    if (!optionsValue.isUndefined()) {
        if (!optionsValue.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.prototype.toBase64 requires that options be an object");
        }

        ObjectGetResult optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().alphabet));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (!optionValue.isString()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Uint8Array.prototype.toBase64 options value requires string");
            }
            if (optionValue.asString()->equals("base64url")) {
                option = (Base64EncodeOption)((int)option | (int)Base64EncodeOption::URL);
            } else if (optionValue.asString()->equals("base64")) {
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value for Uint8Array.prototype.toBase64 options");
            }
        }

        optionValueGetResult = optionsValue.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().omitPadding));
        if (optionValueGetResult.hasValue()) {
            Value optionValue = optionValueGetResult.value(state, optionsValue);
            if (optionValue.toBoolean()) {
                option = (Base64EncodeOption)((int)option | (int)Base64EncodeOption::OmitPadding);
            }
        }
    }

    TypedArrayObject::validateTypedArray(state, thisValue);

    auto ta = thisValue.asObject()->asTypedArrayObject();
    return base64EncodeToString(ta->rawBuffer(), ta->arrayLength(), option, &state);
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

#define INITIALIZE_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                                                                                                                                          \
    {                                                                                                                                                                                                                                                               \
        ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value { \
                                                                                                        ASSERT(self->isGlobalObject());                                                                                             \
                                                                                                        return self->asGlobalObject()->type##Array(); }, nullptr); \
        defineNativeDataAccessorProperty(state, ObjectPropertyName(strings->TYPE##Array), nativeData, Value(Value::EmptyValue));                                                                                                                                    \
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
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toReversed),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toReversed, builtinTypedArrayToReversed, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toSorted),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toSorted, builtinTypedArrayToSorted, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
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
    typedArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTypedArrayWith, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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

    m_uint8Array->directDefineOwnProperty(state, ObjectPropertyName(strings->fromHex),
                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fromHex, builtinUint8ArrayFromHex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_uint8Array->directDefineOwnProperty(state, ObjectPropertyName(strings->fromBase64),
                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fromBase64, builtinUint8ArrayFromBase64, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_uint8ArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toHex),
                                                   ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toHex, builtinUint8ArrayToHex, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_uint8ArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->setFromHex),
                                                   ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->setFromHex, builtinUint8ArraySetFromHex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_uint8ArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toBase64),
                                                   ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toBase64, builtinUint8ArrayToBase64, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_uint8ArrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->setFromBase64),
                                                   ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->setFromBase64, builtinUint8ArraySetFromBase64, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
