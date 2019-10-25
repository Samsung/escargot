/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "VMInstance.h"
#include "TypedArrayObject.h"
#include "IteratorObject.h"
#include "IteratorOperations.h"
#include "NativeFunctionObject.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

static Value builtinArrayBufferConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_NotExistNewInArrayBufferConstructor);
    }

    ArrayBufferObject* obj = new ArrayBufferObject(state);
    if (argc >= 1) {
        Value& val = argv[0];
        double numberLength = val.toNumber(state);
        double byteLength = Value(numberLength).toLength(state);
        if (numberLength != byteLength) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_FirstArgumentInvalidLength);
        }
        obj->allocateBuffer(state, byteLength);
    } else {
        obj->allocateBuffer(state, 0);
    }
    return obj;
}

static Value builtinArrayBufferByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isArrayBufferObject())) {
        return Value(thisValue.asObject()->asArrayBufferObject()->byteLength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get ArrayBuffer.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-arraybuffer.prototype.slice
static Value builtinArrayBufferByteSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Object* thisObject = thisValue.toObject(state);
    Value end = argv[1];
    if (!thisObject->isArrayBufferObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), errorMessage_GlobalObject_ThisNotArrayBufferObject);
    ArrayBufferObject* obj = thisObject->asArrayBufferObject();
    if (obj->isDetachedBuffer())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), errorMessage_GlobalObject_DetachedBuffer);
    double len = obj->byteLength();
    double relativeStart = argv[0].toInteger(state);
    unsigned first = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);
    double relativeEnd = end.isUndefined() ? len : end.toInteger(state);
    unsigned final_ = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);
    unsigned newLen = std::max((int)final_ - (int)first, 0);
    Value constructor = thisObject->speciesConstructor(state, state.context()->globalObject()->arrayBuffer());
    Value arguments[] = { Value(newLen) };
    Value newValue = Object::construct(state, constructor, 1, arguments);
    if (!newValue.isObject() || !newValue.asObject()->isArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    }

    ArrayBufferObject* newObject = newValue.asObject()->asArrayBufferObject();
    if (newObject->isDetachedBuffer()) // 18
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (newObject == obj) // 19
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (newObject->byteLength() < newLen) // 20
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (obj->isDetachedBuffer()) // 22
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");

    newObject->fillData(obj->data() + first, newLen);
    return newObject;
}

static Value builtinArrayBufferIsView(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return Value(false);
    }
    Object* thisObject = argv[0].toObject(state);
    if (thisObject->isTypedArrayObject() || thisObject->isDataViewObject()) {
        return Value(true);
    }
    return Value(false);
}

static Value TypedArrayFrom(ExecutionState& state, const Value& constructor, const Value& items, const Value& mapfn, const Value& thisArg)
{
    // Let C be constructor.
    Value C = constructor;
    // Assert: IsConstructor(C) is true.
    ASSERT(C.isConstructor());
    // Assert: mapfn is either a callable Object or undefined.
    ASSERT(mapfn.isUndefined() || mapfn.isCallable());

    // If mapfn is undefined, let mapping be false.
    bool mapping = false;
    // Else
    // Let T be thisArg.
    // Let mapping be true
    Value T;
    if (!mapfn.isUndefined()) {
        T = thisArg;
        mapping = true;
    }
    // Let usingIterator be GetMethod(items, @@iterator).
    Value usingIterator = Object::getMethod(state, items, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator));

    // If usingIterator is not undefined, then
    if (!usingIterator.isUndefined()) {
        // Let iterator be GetIterator(items, usingIterator).
        Value iterator = getIterator(state, items, usingIterator);
        // Let values be a new empty List.
        ValueVector values;
        // Let next be true.
        Value next(Value::True);
        // Repeat, while next is not false
        while (!next.isFalse()) {
            // Let next be IteratorStep(iterator).
            next = iteratorStep(state, iterator);
            // If next is not false, then
            if (!next.isFalse()) {
                // Let nextValue be IteratorValue(next).
                Value nextValue = iteratorValue(state, next);
                // Append nextValue to the end of the List values.
                values.push_back(nextValue);
            }
        }
        // Let len be the number of elements in values.
        size_t len = values.size();

        // FIXME Let targetObj be AllocateTypedArray(C, len).
        Value arg[1] = { Value(len) };
        Value targetObj = Object::construct(state, C, 1, arg);

        // Let k be 0.
        size_t k = 0;
        // Repeat, while k < len
        while (k < len) {
            // Let Pk be ToString(k).
            // Let kValue be the first element of values and remove that element from values.
            Value kValue = values[k];
            Value mappedValue = kValue;
            // If mapping is true, then
            if (mapping) {
                // Let mappedValue be Call(mapfn, T, «kValue, k»).
                Value args[2] = { kValue, Value(k) };
                mappedValue = Object::call(state, mapfn, T, 2, args);
            }
            // Let setStatus be Set(targetObj, Pk, mappedValue, true).
            targetObj.asObject()->setIndexedPropertyThrowsException(state, Value(k), mappedValue);
            // Increase k by 1.
            k++;
        }
        // Return targetObj.
        return targetObj;
    }

    // Let arrayLike be ToObject(items).
    Object* arrayLike = items.toObject(state);
    // Let len be ToLength(Get(arrayLike, "length")).
    size_t len = arrayLike->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, arrayLike).toLength(state);

    // FIXME Let targetObj be AllocateTypedArray(C, len).
    Value arg[1] = { Value(len) };
    Value targetObj = Object::construct(state, C, 1, arg);

    // Let k be 0.
    size_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        // Let kValue be Get(arrayLike, Pk).
        Value kValue = arrayLike->get(state, ObjectPropertyName(state, Value(k))).value(state, arrayLike);
        Value mappedValue = kValue;
        // If mapping is true, then
        if (mapping) {
            // Let mappedValue be Call(mapfn, T, «kValue, k»).
            Value args[2] = { kValue, Value(k) };
            mappedValue = Object::call(state, mapfn, T, 2, args);
        }
        // Let setStatus be Set(targetObj, Pk, mappedValue, true).
        targetObj.asObject()->setIndexedPropertyThrowsException(state, Value(k), mappedValue);
        // Increase k by 1.
        k++;
    }
    // Return targetObj.
    return targetObj;
}

static ArrayBufferObject* validateTypedArray(ExecutionState& state, Object* thisObject, String* func)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!thisObject->isTypedArrayObject() || !thisObject->isArrayBufferView()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, func, errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }

    auto wrapper = thisObject->asArrayBufferView();
    ArrayBufferObject* buffer = wrapper->buffer();
    if (!buffer && buffer->isDetachedBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, func, errorMessage_GlobalObject_DetachedBuffer);
    }
    return buffer;
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
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return Value();
}

Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%.from
static Value builtinTypedArrayFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value C = thisValue;
    Value source = argv[0];

    if (!C.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_ThisNotConstructor);
    }

    Value f;
    if (argc > 1) {
        f = argv[1];
    }

    if (!f.isUndefined()) {
        if (!f.isObject() || !f.asObject()->isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "mapfn is not callable");
        }
    }

    Value t;
    if (argc > 2) {
        t = argv[2];
    }
    return TypedArrayFrom(state, C, source, f, t);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%.of
static Value builtinTypedArrayOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    size_t len = argc;
    Value C = thisValue;
    if (!C.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_ThisNotConstructor);
    }

    // FIXME AllocateTypedArray(C, len)
    Value arg[1] = { Value(len) };
    Value newObj = Object::construct(state, C, 1, arg);

    size_t k = 0;
    while (k < len) {
        Value kValue = argv[k];
        newObj.asObject()->setIndexedPropertyThrowsException(state, Value(k), kValue);
        k++;
    }
    return newObj;
}

static Value builtinTypedArrayByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->byteLength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayByteOffsetGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->byteOffset());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteOffset called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->arrayLength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.length called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        if (thisValue.asObject()->asArrayBufferView()->buffer()) {
            return Value(thisValue.asObject()->asArrayBufferView()->buffer());
        }
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.buffer called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

template <typename TA, int typedArrayElementSize, typename TypeAdaptor>
Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // if NewTarget is undefined, throw a TypeError
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor);
    }

    TA* obj = new TA(state);
    if (argc == 0) {
        // $22.2.1.1 %TypedArray% ()
        obj->allocateTypedArray(state, 0);
    } else if (argc >= 1) {
        const Value& val = argv[0];
        if (!val.isObject()) {
            // $22.2.1.2 %TypedArray%(length)
            int numlen = val.toNumber(state);
            int elemlen = val.toLength(state);
            if (numlen != elemlen)
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_FirstArgumentInvalidLength);
            obj->allocateTypedArray(state, elemlen);
        } else if (val.isPointerValue() && val.asPointerValue()->isArrayBufferObject()) {
            // $22.2.1.5 %TypedArray%(buffer [, byteOffset [, length] ] )
            unsigned elementSize = obj->elementSize();
            int64_t offset = 0;
            Value lenVal;
            if (argc >= 2) {
                offset = argv[1].toInt32(state);
            }
            if (offset < 0) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            if (offset % elementSize != 0) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            ArrayBufferObject* buffer = val.asObject()->asArrayBufferObject();
            unsigned bufferByteLength = buffer->byteLength();
            if (argc >= 3) {
                lenVal = argv[2];
            }
            unsigned newByteLength;
            if (lenVal.isUndefined()) {
                if (bufferByteLength % elementSize != 0)
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
                if (bufferByteLength < offset)
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
                newByteLength = bufferByteLength - offset;
            } else {
                int length = lenVal.toLength(state);
                newByteLength = length * elementSize;
                if (offset + newByteLength > bufferByteLength)
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            obj->setBuffer(buffer, offset, newByteLength, newByteLength / elementSize);
        } else if (val.isObject() && val.asObject()->isTypedArrayObject()) {
            // $22.2.1.3 %TypedArray% ( typedArray )
            // FIXME Let O be AllocateTypedArray(NewTarget).
            obj->allocateTypedArray(state, 0);
            // Let srcArray be typedArray.
            ArrayBufferView* srcArray = val.asObject()->asArrayBufferView();
            // Let srcData be the value of srcArray’s [[ViewedArrayBuffer]] internal slot.
            ArrayBufferObject* srcData = srcArray->buffer();
            // If IsDetachedBuffer(srcData) is true, throw a TypeError exception.
            if (srcData->isDetachedBuffer()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().constructor.string(), errorMessage_GlobalObject_DetachedBuffer);
            }
            // Let constructorName be the String value of O’s [[TypedArrayName]] internal slot.
            // Let elementType be the String value of the Element Type value in Table 49 for constructorName.
            // Let elementLength be the value of srcArray’s [[ArrayLength]] internal slot.
            double elementLength = srcArray->arrayLength();
            // Let srcName be the String value of srcArray’s [[TypedArrayName]] internal slot.
            // Let srcType be the String value of the Element Type value in Table 49 for srcName.
            // Let srcElementSize be the Element Size value in Table 49 for srcName.
            int srcElementSize = ArrayBufferView::getElementSize(srcArray->typedArrayType());
            // Let srcByteOffset be the value of srcArray’s [[ByteOffset]] internal slot.
            unsigned srcByteOffset = srcArray->byteOffset();
            // Let elementSize be the Element Size value in Table 49 for constructorName.
            int elementSize = ArrayBufferView::getElementSize(obj->typedArrayType());
            // Let byteLength be elementSize × elementLength.
            unsigned byteLength = elementSize * elementLength;

            ArrayBufferObject* data;
            // If SameValue(elementType,srcType) is true, then
            if (obj->typedArrayType() == srcArray->typedArrayType()) {
                // Let data be CloneArrayBuffer(srcData, srcByteOffset).
                data = new ArrayBufferObject(state);
                data->cloneBuffer(state, srcData, srcByteOffset, byteLength);
            } else {
                // Let bufferConstructor be SpeciesConstructor(srcData, %ArrayBuffer%).
                Value bufferConstructor = srcData->speciesConstructor(state, state.context()->globalObject()->arrayBuffer());
                // FIXME Let data be AllocateArrayBuffer(bufferConstructor, byteLength).
                Value arg[1] = { Value(byteLength) };
                data = Object::construct(state, bufferConstructor, 1, arg)->asArrayBufferObject();

                // Let srcByteIndex be srcByteOffset.
                unsigned srcByteIndex = srcByteOffset;
                // Let targetByteIndex be 0.
                unsigned targetByteIndex = 0;
                // Let count be elementLength.
                unsigned count = elementLength;
                // Repeat, while count > 0
                while (count > 0) {
                    // Let value be GetValueFromBuffer(srcData, srcByteIndex, srcType).
                    Value value = srcData->getTypedValueFromBuffer(state, srcByteIndex, val.asObject()->asArrayBufferView()->typedArrayType());
                    // Perform SetValueInBuffer(data, targetByteIndex, elementType, value).
                    data->setValueInBuffer<TypeAdaptor>(state, targetByteIndex, value);
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
        } else if (val.isObject()) {
            // TODO implement 22.2.1.4
            Object* inputObj = val.asObject();
            uint64_t length = inputObj->lengthES6(state);
            ASSERT(length >= 0);
            unsigned elementSize = obj->elementSize();
            uint64_t bufferSize = length * elementSize;
            if (bufferSize > ArrayBufferObject::maxArrayBufferSize) {
                // 6.2.6.1.2
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferSize);
            }
            ArrayBufferObject* buffer = new ArrayBufferObject(state);
            buffer->allocateBuffer(state, bufferSize);
            obj->setBuffer(buffer, 0, length * elementSize, length);
            for (uint64_t i = 0; i < length; i++) {
                ObjectPropertyName pK(state, Value(i));
                obj->setThrowsException(state, pK, inputObj->get(state, pK).value(state, inputObj), obj);
            }
        } else {
            state.throwException(new ASCIIString(errorMessage_NotImplemented));
            RELEASE_ASSERT_NOT_REACHED();
        }
        // TODO
        if (obj->arrayLength() >= ArrayBufferObject::maxArrayBufferSize) {
            state.throwException(new ASCIIString(errorMessage_NotImplemented));
        }
        RELEASE_ASSERT(obj->arrayLength() < ArrayBufferObject::maxArrayBufferSize);
    }
    return obj;
}

static Value builtinTypedArrayCopyWithin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, copyWithin);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().copyWithin.string());

    // Array.prototype.copyWithin as defined in 22.1.3.3 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

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
    int8_t direction;
    // If from<to and to<from+count
    if (from < to && to < from + count) {
        // Let direction be -1.
        direction = -1;
        // Let from be from + count -1.
        from = from + count - 1;
        // Let to be to + count -1.
        to = to + count - 1;
    } else {
        // Let direction = 1.
        direction = 1;
    }

    // Repeat, while count > 0
    while (count > 0) {
        // Let fromPresent be HasProperty(O, fromKey).
        ObjectGetResult fromValue = O->getIndexedProperty(state, Value(from));
        // If fromPresent is true, then
        if (fromValue.hasValue()) {
            // Let setStatus be Set(O, toKey, fromVal, true).
            O->setIndexedPropertyThrowsException(state, Value(to), fromValue.value(state, O));
        } else {
            // Let deleteStatus be DeletePropertyOrThrow(O, toKey).
            O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(to)));
        }
        // Let from be from + direction.
        from = from + direction;
        // Let to be to + direction.
        to = to + direction;
        // Let count be count − 1.
        count = count - 1;
    }
    // return O.
    return O;
}

static Value builtinTypedArrayIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // NOTE: Same algorithm as Array.prototype.indexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, indexOf);
    validateTypedArray(state, O, state.context()->staticStrings().indexOf.string());

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    int64_t len = O->asArrayBufferView()->arrayLength();

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

    double k;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        k = (n == -0) ? 0 : n;
    } else {
        // Else, n<0
        // Let k be len - abs(n).
        k = len - std::abs(n);

        // If k is less than 0, then let k be 0.
        if (k < 0) {
            k = 0;
        }
    }

    // Repeat, while k<len
    while (k < len) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        ObjectGetResult kPresent = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(kPresent.hasValue());
        // If kPresent is true, then
        // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
        Value elementK = kPresent.value(state, O);

        // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
        if (elementK.equalsTo(state, argv[0])) {
            // If same is true, return k.
            return Value(k);
        }
        // Increase k by 1.
        k++;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinTypedArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // NOTE: Same algorithm as Array.prototype.lastIndexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, lastIndexOf);
    validateTypedArray(state, O, state.context()->staticStrings().lastIndexOf.string());

    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    int64_t len = O->asArrayBufferView()->arrayLength();

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
    double k;
    if (n >= 0) {
        k = (n == -0) ? 0 : std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len - abs(n).
        k = len - std::abs(n);
    }

    // Repeat, while k≥ 0
    while (k >= 0) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        ObjectGetResult kPresent = O->getIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
            Value elementK = kPresent.value(state, O);

            // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
            if (elementK.equalsTo(state, argv[0])) {
                // If same is true, return k.
                return Value(k);
            }
        } else {
            int64_t result;
            Object::nextIndexBackward(state, O, k, -1, result);
            k = result;
            continue;
        }
        // Decrease k by 1.
        k--;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinTypedArraySet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, TypedArray, set);
    if (!thisBinded->isTypedArrayObject() || argc < 1) {
        const StaticStrings* strings = &state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, strings->set.string(), errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }
    auto wrapper = thisBinded->asArrayBufferView();
    double offset = 0;
    if (argc >= 2) {
        offset = argv[1].toInteger(state);
    }
    if (offset < 0) {
        const StaticStrings* strings = &state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), "Start offset is negative");
    }
    if (!(argv[0].isObject())) {
        const StaticStrings* strings = &state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->TypedArray.string(), true, strings->set.string(), "Invalid argument");
    }
    auto arg0 = argv[0].toObject(state);
    ArrayBufferObject* targetBuffer = wrapper->buffer();
    unsigned targetLength = wrapper->arrayLength();
    int targetByteOffset = wrapper->byteOffset();
    int targetElementSize = ArrayBufferView::getElementSize(wrapper->typedArrayType());
    if (!arg0->isTypedArrayObject()) {
        Object* src = arg0;
        uint64_t srcLength = src->lengthES6(state);
        if (srcLength + (uint64_t)offset > targetLength) {
            const StaticStrings* strings = &state.context()->staticStrings();
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), errorMessage_GlobalObject_InvalidArrayLength);
        }
        int targetByteIndex = offset * targetElementSize + targetByteOffset;
        int k = 0;
        int limit = targetByteIndex + targetElementSize * srcLength;
        while (targetByteIndex < limit) {
            double kNumber = src->get(state, ObjectPropertyName(state, Value(k))).value(state, src).toNumber(state);
            wrapper->setThrowsException(state, ObjectPropertyName(state, Value(targetByteIndex / targetElementSize)), Value(kNumber), wrapper);
            k++;
            targetByteIndex += targetElementSize;
        }
        return Value();
    } else {
        auto arg0Wrapper = arg0->asArrayBufferView();
        ArrayBufferObject* srcBuffer = arg0Wrapper->buffer();
        unsigned srcLength = arg0Wrapper->arrayLength();
        int srcByteOffset = arg0Wrapper->byteOffset();
        if (((double)srcLength + (double)offset) > (double)targetLength) {
            const StaticStrings* strings = &state.context()->staticStrings();
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), errorMessage_GlobalObject_InvalidArrayLength);
        }
        int srcByteIndex = 0;
        ArrayBufferObject* oldSrcBuffer = srcBuffer;
        unsigned oldSrcByteoffset = arg0Wrapper->byteOffset();
        unsigned oldSrcBytelength = arg0Wrapper->byteLength();
        unsigned oldSrcArraylength = arg0Wrapper->arrayLength();
        if (srcBuffer == targetBuffer) {
            // NOTE: Step 24
            srcBuffer = new ArrayBufferObject(state);
            bool succeed = srcBuffer->cloneBuffer(state, targetBuffer, srcByteOffset, oldSrcBytelength);
            if (!succeed) {
                const StaticStrings* strings = &state.context()->staticStrings();
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->TypedArray.string(), true, strings->set.string(), "");
            }
            arg0Wrapper->setBuffer(srcBuffer, 0, srcBuffer->byteLength(), oldSrcArraylength);
        } else {
            srcByteIndex = srcByteOffset;
        }
        unsigned targetIndex = (unsigned)offset, srcIndex = 0;
        unsigned targetByteIndex = offset * targetElementSize + targetByteOffset;
        unsigned limit = targetByteIndex + targetElementSize * srcLength;
        while (targetIndex < offset + srcLength) {
            Value value = arg0Wrapper->get(state, ObjectPropertyName(state, Value(srcIndex))).value(state, arg0Wrapper);
            wrapper->setThrowsException(state, ObjectPropertyName(state, Value(targetIndex)), value, wrapper);
            srcIndex++;
            targetIndex++;
        }
        if (oldSrcBuffer != srcBuffer)
            arg0Wrapper->setBuffer(oldSrcBuffer, oldSrcByteoffset, oldSrcBytelength, oldSrcArraylength);
        return Value();
    }
}

static Value builtinTypedArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, some);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().some.string());

    // Array.prototype.some as defined in 22.1.3.23 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().some.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    int64_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        ObjectGetResult kResult = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(kResult.hasValue());
        // Let kValue be the result of calling the [[Get]] internal method of O with argument ToString(k).
        Value kValue = kResult.value(state, O);
        // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
        Value args[] = { kValue, Value(k), O };
        Value testResult = Object::call(state, callbackfn, T, 3, args);

        // If ToBoolean(testResult) is true, return true.
        if (testResult.toBoolean(state)) {
            return Value(true);
        }

        // Increase k by 1.
        k++;
    }
    // Return false.
    return Value(false);
}

static Value builtinTypedArraySort(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, sort);
    // Let buffer be ValidateTypedArray(obj).
    ArrayBufferObject* buffer = validateTypedArray(state, O, state.context()->staticStrings().sort.string());

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    int64_t len = O->asArrayBufferView()->arrayLength();

    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), errorMessage_GlobalObject_FirstArgumentNotCallable);
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    // [defaultSort, &cmpfn, &state, &buffer]
    O->sort(state, len, [&](const Value& x, const Value& y) -> bool {
        ASSERT(x.isNumber() && y.isNumber());
        if (!defaultSort) {
            Value args[] = { x, y };
            Value v = Object::call(state, cmpfn, Value(), 2, args);
            if (buffer->isDetachedBuffer()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().sort.string(), errorMessage_GlobalObject_DetachedBuffer);
            }
            return (v.toNumber(state) < 0);
        } else {
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
        } });
    return O;
}

static Value builtinTypedArraySubArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the this value.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, subarray);
    const StaticStrings* strings = &state.context()->staticStrings();
    // If O does not have a [[TypedArrayName]] internal slot, throw a TypeError exception.
    if (!O->isTypedArrayObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, strings->subarray.string(), errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }
    // Let buffer be the value of O’s [[ViewedArrayBuffer]] internal slot.
    auto wrapper = O->asArrayBufferView();
    ArrayBufferObject* buffer = wrapper->buffer();
    // Let srcLength be the value of O’s [[ArrayLength]] internal slot.
    double srcLength = wrapper->arrayLength();
    // Let relativeBegin be ToInteger(begin).
    double relativeBegin = argv[0].toInteger(state);
    // If relativeBegin < 0, let beginIndex be max((srcLength + relativeBegin), 0); else let beginIndex be min(relativeBegin, srcLength).
    unsigned beginIndex = (relativeBegin < 0) ? std::max((srcLength + relativeBegin), 0.0) : std::min(relativeBegin, srcLength);
    // If end is undefined, let relativeEnd be srcLength; else, let relativeEnd be ToInteger(end).
    double relativeEnd = srcLength;
    if (argc > 1 && !argv[1].isUndefined()) {
        relativeEnd = argv[1].toInteger(state);
    }
    // If relativeEnd < 0, let endIndex be max((srcLength + relativeEnd), 0); else let endIndex be min(relativeEnd, srcLength).
    unsigned endIndex = (relativeEnd < 0) ? std::max((srcLength + relativeEnd), 0.0) : std::min(relativeEnd, srcLength);
    // Let newLength be max(endIndex – beginIndex, 0).
    unsigned newLength = std::max((int)(endIndex - beginIndex), 0);

    // Let constructorName be the String value of O’s [[TypedArrayName]] internal slot.
    // Let elementSize be the Number value of the Element Size value specified in Table 49 for constructorName.
    unsigned elementSize = ArrayBufferView::getElementSize(wrapper->typedArrayType());
    // Let srcByteOffset be the value of O’s [[ByteOffset]] internal slot.
    unsigned srcByteOffset = wrapper->byteOffset();
    // Let beginByteOffset be srcByteOffset + beginIndex × elementSize.
    unsigned beginByteOffset = srcByteOffset + beginIndex * elementSize;

    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for constructorName.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, O->asArrayBufferView()->typedArrayType());
    // Let constructor be SpeciesConstructor(O, defaultConstructor).
    Value constructor = O->speciesConstructor(state, defaultConstructor);
    // Let argumentsList be «buffer, beginByteOffset, newLength».
    Value args[3] = { buffer, Value(beginByteOffset), Value(newLength) };
    // Return Construct(constructor, argumentsList).
    return Object::construct(state, constructor, 3, args);
}

static Value builtinTypedArrayEvery(ExecutionState& state, Value thisValue, size_t argc, NULLABLE Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, every);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().every.string());

    // Array.prototype.every as defined in 22.1.3.5 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    unsigned len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().every.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let k be 0.
    unsigned k = 0;

    while (k < len) {
        ObjectGetResult value = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(value.hasValue());
        // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
        Value kValue = value.value(state, O);
        // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
        Value args[] = { kValue, Value(k), O };
        Value testResult = Object::call(state, callbackfn, T, 3, args);

        if (!testResult.toBoolean(state)) {
            return Value(false);
        }

        // Increae k by 1.
        k++;
    }
    return Value(true);
}

static Value builtinTypedArrayFill(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, fill);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().fill.string());

    // Array.prototype.fill as defined in 22.1.3.5 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    // Let relativeStart be ToInteger(start).
    double relativeStart = 0;
    if (argc > 1) {
        relativeStart = argv[1].toInteger(state);
    }

    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    unsigned k = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);

    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = len;
    if (argc > 2 && !argv[2].isUndefined()) {
        relativeEnd = argv[2].toInteger(state);
    }

    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    unsigned fin = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);

    Value value = argv[0];
    while (k < fin) {
        O->setIndexedPropertyThrowsException(state, Value(k), value);
        k++;
    }
    // return O.
    return O;
}

static Value builtinTypedArrayFilter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, filter);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().filter.string());

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    double len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().filter.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, O->asArrayBufferView()->typedArrayType());
    // Let C be SpeciesConstructor(O, defaultConstructor).
    Value C = O->speciesConstructor(state, defaultConstructor);

    // Let kept be a new empty List.
    ValueVector kept;
    // Let k be 0.
    size_t k = 0;
    // Let captured be 0.
    size_t captured = 0;
    // Repeat, while k < len
    while (k < len) {
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        Value args[] = { kValue, Value(k), O };
        bool selected = Object::call(state, callbackfn, T, 3, args).toBoolean(state);
        if (selected) {
            kept.push_back(kValue);
            captured++;
        }
        k++;
    }

    // FIXME Let A be AllocateTypedArray(C, captured).
    Value arg[1] = { Value(captured) };
    Value A = Object::construct(state, C, 1, arg);

    // Let n be 0.
    size_t n = 0;
    // For each element e of kept
    for (size_t i = 0; i < kept.size(); i++) {
        // Let status be Set(A, ToString(n), e, true ).
        A.asObject()->setIndexedPropertyThrowsException(state, Value(n), kept[i]);
        // Increment n by 1.
        n++;
    }
    // Return A.
    return A;
}

static Value builtinTypedArrayFind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, find);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().find.string());

    // Array.prototype.find as defined in 22.1.3.8 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    Value predicate = argv[0];
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().find.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    double k = 0;
    Value kValue;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be Get(O, Pk).
        kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(Call(predicate, T, «kValue, k, O»)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, T, 3, args).toBoolean(state);
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

static Value builtinTypedArrayFindIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, findIndex);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().findIndex.string());

    // Array.prototype.findIndex as defined in 22.1.3.9 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(predicate) is false, throw a TypeError exception.
    Value predicate = argv[0];
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().findIndex.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    double k = 0;
    Value kValue;
    // Repeat, while k < len
    while (k < len) {
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, T, « kValue, k, O »)).
        Value args[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, predicate, T, 3, args).toBoolean(state);
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

static Value builtinTypedArrayForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, forEach);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().forEach.string());

    // Array.prototype.forEach as defined in 22.1.3.10 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().forEach.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    double k = 0;
    while (k < len) {
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(res.hasValue());
        Value kValue = res.value(state, O);
        Value args[] = { kValue, Value(k), O };
        Object::call(state, callbackfn, T, 3, args);
        k++;
    }
    // Return undefined.
    return Value();
}

static Value builtinTypedArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, join);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().join.string());

    // Array.prototype.join as defined in 22.1.3.12 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

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
    RELEASE_ASSERT(!elem.isUndefinedOrNull());
    builder.appendString(elem.toString(state));

    double curIndex = 1;
    while (curIndex < len) {
        if (sep->length() > 0) {
            if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - sep->length())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
            }
            builder.appendString(sep);
        }
        elem = O->getIndexedProperty(state, Value(curIndex)).value(state, O);
        RELEASE_ASSERT(!elem.isUndefinedOrNull());
        builder.appendString(elem.toString(state));
        curIndex++;
    }

    return builder.finalize(&state);
}

static Value builtinTypedArrayMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, map);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().map.string());

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    size_t len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().map.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1) {
        T = argv[1];
    }

    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, O->asArrayBufferView()->typedArrayType());
    // Let C be SpeciesConstructor(O, defaultConstructor).
    Value C = O->speciesConstructor(state, defaultConstructor);

    // FIXME Let A be AllocateTypedArray(C, len).
    Value arg[1] = { Value(len) };
    Value A = Object::construct(state, C, 1, arg);

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

static Value builtinTypedArrayReduce(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reduce);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().reduce.string());

    // Array.prototype.reduce as defined in 22.1.3.18 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduce.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduce.string(), errorMessage_GlobalObject_ReduceError);
    }

    size_t k = 0; // 6
    Value accumulator;
    if (argc > 1) { // 7
        accumulator = argv[1];
    } else { // 8
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(res.hasValue());
        accumulator = res.value(state, O);
        k++;
    }
    while (k < len) { // 9
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(res.hasValue());
        Value kValue = res.value(state, O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k++;
    }
    return accumulator;
}

static Value builtinTypedArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reduceRight);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().reduceRight.string());

    // Array.prototype.reduceRight as defined in 22.1.3.19 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true,
                                       state.context()->staticStrings().reduceRight.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If len is 0 and initialValue is not present, throw a TypeError exception.
    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().reduceRight.string(), errorMessage_GlobalObject_ReduceError);
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
        RELEASE_ASSERT(res.hasValue());
        accumulator = res.value(state, O);
        k--;
    }

    // Repeat, while k ≥ 0
    while (k >= 0) {
        ObjectGetResult res = O->getIndexedProperty(state, Value(k));
        RELEASE_ASSERT(res.hasValue());
        Value kValue = res.value(state, O);
        Value args[] = { accumulator, kValue, Value(k), O };
        accumulator = Object::call(state, callbackfn, Value(), 4, args);
        k--;
    }
    // Return accumulator.
    return accumulator;
}

static Value builtinTypedArrayReverse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, reverse);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().reverse.string());

    // Array.prototype.reverse as defined in 22.1.3.20 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    unsigned len = O->asArrayBufferView()->arrayLength();
    unsigned middle = std::floor(len / 2);
    unsigned lower = 0;
    while (middle > lower) {
        unsigned upper = len - lower - 1;

        ObjectGetResult upperResult = O->getIndexedProperty(state, Value(upper));
        ObjectGetResult lowerResult = O->getIndexedProperty(state, Value(lower));
        RELEASE_ASSERT(upperResult.hasValue() && lowerResult.hasValue());

        Value upperValue = upperResult.value(state, O);
        Value lowerValue = lowerResult.value(state, O);

        if (!O->setIndexedProperty(state, Value(lower), upperValue)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
        }
        if (!O->setIndexedProperty(state, Value(upper), lowerValue)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
        }

        lower++;
    }
    return O;
}

static Value builtinTypedArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, slice);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().slice.string());

    // Let len be the value of O’s [[ArrayLength]] internal slot.
    int64_t len = O->asArrayBufferView()->arrayLength();
    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[0].toInteger(state);
    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    double k = (relativeStart < 0) ? std::max((double)len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    double finalEnd = (relativeEnd < 0) ? std::max((double)len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);
    // Let count be max(final – k, 0).
    double count = std::max((double)finalEnd - k, 0.0);

    // Let defaultConstructor be the intrinsic object listed in column one of Table 49 for the value of O’s [[TypedArrayName]] internal slot.
    Value defaultConstructor = getDefaultTypedArrayConstructor(state, O->asArrayBufferView()->typedArrayType());
    // Let C be SpeciesConstructor(O, defaultConstructor).
    Value C = O->speciesConstructor(state, defaultConstructor);
    // FIXME Let A be AllocateTypedArray(C, count).
    Value arg[1] = { Value(count) };
    Value A = Object::construct(state, C, 1, arg);

    // If SameValue(srcType, targetType) is false, then
    if (O->asArrayBufferView()->typedArrayType() != A.asObject()->asArrayBufferView()->typedArrayType()) {
        size_t n = 0;
        while (k < finalEnd) {
            Value kValue = O->getIndexedProperty(state, Value(k)).value(state, O);
            A.asObject()->setIndexedPropertyThrowsException(state, Value(n), kValue);
            k++;
            n++;
        }
        // Else if count > 0,
    } else if (count > 0) {
        auto srcWrapper = O->asArrayBufferView();
        auto targetWrapper = A.asObject()->asArrayBufferView();

        ArrayBufferObject* srcBuffer = srcWrapper->buffer();
        if (srcBuffer->isDetachedBuffer()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_DetachedBuffer);
        }
        ArrayBufferObject* targetBuffer = targetWrapper->buffer();
        unsigned elementSize = ArrayBufferView::getElementSize(srcWrapper->typedArrayType());
        unsigned srcByteOffset = srcWrapper->byteOffset();
        unsigned targetByteIndex = 0;
        unsigned srcByteIndex = k * elementSize + srcByteOffset;

        while (targetByteIndex < count * elementSize) {
            Value value = srcWrapper->getValueFromBuffer<uint8_t>(state, srcByteIndex);
            targetWrapper->setValueInBuffer<Uint8Adaptor>(state, targetByteIndex, value);
            srcByteIndex++;
            targetByteIndex++;
        }
    }
    // Return A.
    return A;
}

static Value builtinTypedArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, toLocaleString);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().toLocaleString.string());

    // Array.prototype.toLocaleString as defined in 22.1.3.26 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arrayLength();

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(O)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, O);

    // Let separator be the String value for the list-separator String appropriate for the host environment’s current locale (this is derived in an implementation-defined way).
    String* separator = state.context()->staticStrings().asciiTable[(size_t)','].string();

    // If len is zero, return the empty String.
    if (len == 0) {
        return String::emptyString;
    }

    // Let firstElement be the result of calling the [[Get]] internal method of array with argument "0".
    Value firstElement = O->getIndexedProperty(state, Value(0)).value(state, O);

    // If firstElement is undefined or null, then
    Value R;
    RELEASE_ASSERT(!firstElement.isUndefinedOrNull());
    // Let elementObj be ToObject(firstElement).
    Object* elementObj = firstElement.toObject(state);
    // Let func be the result of calling the [[Get]] internal method of elementObj with argument "toLocaleString".
    Value func = elementObj->get(state, state.context()->staticStrings().toLocaleString).value(state, elementObj);
    // If IsCallable(func) is false, throw a TypeError exception.
    if (!func.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);
    }
    // Let R be the result of calling the [[Call]] internal method of func providing elementObj as the this value and an empty arguments list.
    R = Object::call(state, func, elementObj, 0, nullptr);

    // Let k be 1.
    int64_t k = 1;

    // Repeat, while k < len
    while (k < len) {
        // Let S be a String value produced by concatenating R and separator.
        StringBuilder builder;
        builder.appendString(R.toString(state));
        builder.appendString(separator);
        String* S = builder.finalize(&state);

        // Let nextElement be the result of calling the [[Get]] internal method of array with argument ToString(k).
        Value nextElement = O->getIndexedProperty(state, Value(k)).value(state, O);

        // If nextElement is undefined or null, then
        RELEASE_ASSERT(!nextElement.isUndefinedOrNull());
        // Let elementObj be ToObject(nextElement).
        Object* elementObj = nextElement.toObject(state);
        // Let func be the result of calling the [[Get]] internal method of elementObj with argument "toLocaleString".
        Value func = elementObj->get(state, state.context()->staticStrings().toLocaleString).value(state, elementObj);
        // If IsCallable(func) is false, throw a TypeError exception.
        if (!func.isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);
        }
        // Let R be the result of calling the [[Call]] internal method of func providing elementObj as the this value and an empty arguments list.
        R = Object::call(state, func, elementObj, 0, nullptr);

        // Let R be a String value produced by concatenating S and R.
        StringBuilder builder2;
        builder2.appendString(S);
        builder2.appendString(R.toString(state));
        R = builder2.finalize(&state);
        // Increase k by 1.
        k++;
    }
    // Return R.
    return R;
}

static Value builtinTypedArrayToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, toString);
    validateTypedArray(state, O, state.context()->staticStrings().toString.string());

    Value toString = O->get(state, state.context()->staticStrings().join).value(state, O);
    if (!toString.isCallable()) {
        toString = state.context()->globalObject()->objectPrototypeToString();
    }
    return Object::call(state, toString, O, 0, nullptr);
}

template <typename TA, int elementSize, typename TypeAdaptor>
FunctionObject* GlobalObject::installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    NativeFunctionObject* taConstructor = new NativeFunctionObject(state, NativeFunctionInfo(taName, builtinTypedArrayConstructor<TA, elementSize, TypeAdaptor>, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    taConstructor->markThisObjectDontNeedStructureTransitionTable(state);

    *proto = m_objectPrototype;
    Object* taPrototype = new TypedArrayObjectPrototype(state);
    taPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    taPrototype->setPrototype(state, typedArrayFunction->getFunctionPrototype(state));

    taConstructor->setPrototype(state, typedArrayFunction); // %TypedArray%
    taConstructor->setFunctionPrototype(state, taPrototype);

    // 22.2.5.1 /TypedArray/.BYTES_PER_ELEMENT
    taConstructor->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().BYTES_PER_ELEMENT), ObjectPropertyDescriptor(Value(elementSize), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // 22.2.6.1 /TypedArray/.prototype.BYTES_PER_ELEMENT
    taPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().BYTES_PER_ELEMENT), ObjectPropertyDescriptor(Value(elementSize), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ValuePresent)));

    // 22.2.6.2 /TypedArray/.prototype.constructor
    taPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(taConstructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(taName),
                      ObjectPropertyDescriptor(taConstructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    return taConstructor;
}

static Value builtinTypedArrayKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, TypedArray, keys);
    validateTypedArray(state, M, state.context()->staticStrings().keys.string());
    return M->keys(state);
}

static Value builtinTypedArrayValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, TypedArray, values);
    validateTypedArray(state, M, state.context()->staticStrings().values.string());
    return M->values(state);
}

static Value builtinTypedArrayEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, TypedArray, entries);
    validateTypedArray(state, M, state.context()->staticStrings().entries.string());
    return M->entries(state);
}

static Value builtinTypedArrayToStringTagGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value O = thisValue;
    if (!O.isObject()) {
        return Value();
    }
    if (O.asObject() && O.asObject()->isTypedArrayObject()) {
        return Value(String::fromASCII(O.asObject()->internalClassProperty()));
    }
    return Value();
}

void GlobalObject::installTypedArray(ExecutionState& state)
{
    m_arrayBuffer = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ArrayBuffer, builtinArrayBufferConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_arrayBuffer->markThisObjectDontNeedStructureTransitionTable(state);
    m_arrayBuffer->setPrototype(state, m_functionPrototype);
    m_arrayBuffer->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isView),
                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isView, builtinArrayBufferIsView, 1, NativeFunctionInfo::Strict)),
                                                              (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBufferPrototype = m_objectPrototype;
    m_arrayBufferPrototype = new ArrayBufferObject(state);
    m_arrayBufferPrototype->setPrototype(state, m_objectPrototype);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().toStringTag),
                                                             ObjectPropertyDescriptor(Value(state.context()->staticStrings().ArrayBuffer.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    const StaticStrings* strings = &state.context()->staticStrings();

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_arrayBuffer->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species), desc);
    }

    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinArrayBufferByteLengthGetter, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor byteLengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinArrayBufferByteSlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBuffer->setFunctionPrototype(state, m_arrayBufferPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->ArrayBuffer),
                      ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // %TypedArray%
    FunctionObject* typedArrayFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->TypedArray, builtinTypedArrayConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);

    typedArrayFunction->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->from),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTypedArrayFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayFunction->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->of),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->of, builtinTypedArrayOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species), desc);
    }


    // %TypedArray%.prototype
    Object* typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->subarray),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->subarray, builtinTypedArraySubArray, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->set),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinTypedArraySet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->some),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->some, builtinTypedArraySome, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->sort),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->sort, builtinTypedArraySort, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->copyWithin),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->copyWithin, builtinTypedArrayCopyWithin, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->every),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->every, builtinTypedArrayEvery, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->fill),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->fill, builtinTypedArrayFill, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->filter),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->filter, builtinTypedArrayFilter, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->find),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->find, builtinTypedArrayFind, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->findIndex),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->findIndex, builtinTypedArrayFindIndex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->forEach),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->forEach, builtinTypedArrayForEach, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->join),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->join, builtinTypedArrayJoin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->map),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->map, builtinTypedArrayMap, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->reduce),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reduce, builtinTypedArrayReduce, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->reduceRight),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reduceRight, builtinTypedArrayReduceRight, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->reverse),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reverse, builtinTypedArrayReverse, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinTypedArraySlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleString),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTypedArrayToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTypedArrayToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->indexOf),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinTypedArrayIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->lastIndexOf),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinTypedArrayLastIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->keys),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->keys, builtinTypedArrayKeys, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->entries),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->entries, builtinTypedArrayEntries, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto valuesFn = new NativeFunctionObject(state, NativeFunctionInfo(strings->values, builtinTypedArrayValues, 0, NativeFunctionInfo::Strict));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->values),
                                                          ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                          ObjectPropertyDescriptor(valuesFn,
                                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolToStringTag, builtinTypedArrayToStringTagGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().toStringTag), desc);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinTypedArrayByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc2);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteOffset, builtinTypedArrayByteOffsetGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteOffsetDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteOffset), byteOffsetDesc2);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getLength, builtinTypedArrayLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor lengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(strings->length), lengthDesc);
    }
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getBuffer, builtinTypedArrayBufferGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor bufferDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(strings->buffer), bufferDesc);
    }

    m_typedArray = typedArrayFunction;
    m_int8Array = installTypedArray<Int8ArrayObject, 1, Int8Adaptor>(state, strings->Int8Array, &m_int8ArrayPrototype, typedArrayFunction);
    m_int16Array = installTypedArray<Int16ArrayObject, 2, Int16Adaptor>(state, strings->Int16Array, &m_int16ArrayPrototype, typedArrayFunction);
    m_int32Array = installTypedArray<Int32ArrayObject, 4, Int32Adaptor>(state, strings->Int32Array, &m_int32ArrayPrototype, typedArrayFunction);
    m_uint8Array = installTypedArray<Uint8ArrayObject, 1, Uint8Adaptor>(state, strings->Uint8Array, &m_uint8ArrayPrototype, typedArrayFunction);
    m_uint16Array = installTypedArray<Uint16ArrayObject, 2, Uint16Adaptor>(state, strings->Uint16Array, &m_uint16ArrayPrototype, typedArrayFunction);
    m_uint32Array = installTypedArray<Uint32ArrayObject, 4, Uint32Adaptor>(state, strings->Uint32Array, &m_uint32ArrayPrototype, typedArrayFunction);
    m_uint8ClampedArray = installTypedArray<Uint8ClampedArrayObject, 1, Uint8ClampedAdaptor>(state, strings->Uint8ClampedArray, &m_uint8ClampedArrayPrototype, typedArrayFunction);
    m_float32Array = installTypedArray<Float32ArrayObject, 4, Float32Adaptor>(state, strings->Float32Array, &m_float32ArrayPrototype, typedArrayFunction);
    m_float64Array = installTypedArray<Float64ArrayObject, 8, Float64Adaptor>(state, strings->Float64Array, &m_float64ArrayPrototype, typedArrayFunction);
    m_typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
    m_int8ArrayPrototype = m_int8Array->getFunctionPrototype(state).asObject();
    m_int16ArrayPrototype = m_int16Array->getFunctionPrototype(state).asObject();
    m_int32ArrayPrototype = m_int32Array->getFunctionPrototype(state).asObject();
    m_uint8ArrayPrototype = m_uint8Array->getFunctionPrototype(state).asObject();
    m_uint8ClampedArrayPrototype = m_uint8ClampedArray->getFunctionPrototype(state).asObject();
    m_uint16ArrayPrototype = m_uint16Array->getFunctionPrototype(state).asObject();
    m_uint32ArrayPrototype = m_uint32Array->getFunctionPrototype(state).asObject();
    m_float32ArrayPrototype = m_float32Array->getFunctionPrototype(state).asObject();
    m_float64ArrayPrototype = m_float64Array->getFunctionPrototype(state).asObject();
}
}
