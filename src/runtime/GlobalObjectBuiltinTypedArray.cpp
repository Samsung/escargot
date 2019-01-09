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

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "TypedArrayObject.h"
#include "IteratorObject.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

Value builtinArrayBufferConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!isNewExpression)
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_NotExistNewInArrayBufferConstructor);
    ArrayBufferObject* obj = thisValue.asObject()->asArrayBufferObject();
    if (argc >= 1) {
        Value& val = argv[0];
        double numberLength = val.toNumber(state);
        double byteLength = Value(numberLength).toLength(state);
        if (numberLength != byteLength)
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_FirstArgumentInvalidLength);
        obj->allocateBuffer(byteLength);
    } else {
        obj->allocateBuffer(0);
    }
    return obj;
}

static Value builtinArrayBufferByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isArrayBufferObject())) {
        return Value(thisValue.asObject()->asArrayBufferObject()->bytelength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get ArrayBuffer.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinArrayBufferByteSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Object* thisObject = thisValue.toObject(state);
    Value end = argv[1];
    if (!thisObject->isArrayBufferObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), errorMessage_GlobalObject_ThisNotArrayBufferObject);
    ArrayBufferObject* obj = thisObject->asArrayBufferObject();
    if (obj->isDetachedBuffer())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), errorMessage_GlobalObject_DetachedBuffer);
    double len = obj->bytelength();
    double relativeStart = argv[0].toInteger(state);
    unsigned first = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);
    double relativeEnd = end.isUndefined() ? len : end.toInteger(state);
    unsigned final_ = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);
    unsigned newLen = std::max((int)final_ - (int)first, 0);
    ArrayBufferObject* newObject;
    Value constructor = thisObject->get(state, state.context()->staticStrings().constructor).value(state, thisObject);
    if (constructor.isUndefined()) {
        newObject = new ArrayBufferObject(state);
        newObject->allocateBuffer(newLen);
    } else {
        if (!constructor.isFunction())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: constructor of ArrayBuffer is not a function");
        Value arguments[] = { Value(newLen) };
        Value newValue = ByteCodeInterpreter::newOperation(state, constructor, 1, arguments);

        if (!newValue.isObject() || !newValue.asObject()->isArrayBufferObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
        }
        newObject = newValue.asObject()->asArrayBufferObject();
    }

    if (newObject->isDetachedBuffer()) // 18
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (newObject == obj) // 19
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (newObject->bytelength() < newLen) // 20
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    if (newObject->isDetachedBuffer()) // 22
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

Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Value builtinTypedArrayByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->bytelength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayByteOffsetGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->byteoffset());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteOffset called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->arraylength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.length called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->buffer());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.buffer called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

template <typename TA, int typedArrayElementSize>
Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // if NewTarget is undefined, throw a TypeError
    if (!isNewExpression)
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor);
    TA* obj = thisValue.asObject()->asTypedArrayObject<TA>();
    if (argc == 0) {
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
            if (argc >= 2)
                offset = argv[1].toInt32(state);
            if (offset < 0) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            if (offset % elementSize != 0) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            ArrayBufferObject* buffer = val.asObject()->asArrayBufferObject();
            unsigned bufferByteLength = buffer->bytelength();
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
        } else if (val.isObject()) {
            // TODO implement 22.2.1.4
            Object* inputObj = val.asObject();
            uint64_t length = inputObj->length(state);
            ASSERT(length >= 0);
            unsigned elementSize = obj->elementSize();
            uint64_t bufferSize = length * elementSize;
            if (bufferSize > ArrayBufferObject::maxArrayBufferSize) {
                // 6.2.6.1.2
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferSize);
            }
            ArrayBufferObject* buffer = new ArrayBufferObject(state);
            buffer->allocateBuffer(bufferSize);
            obj->setBuffer(buffer, 0, length * elementSize, length);
            for (uint64_t i = 0; i < length; i++) {
                ObjectPropertyName pK(state, Value(i));
                obj->setThrowsException(state, pK, inputObj->get(state, pK).value(state, inputObj), obj);
            }
        } else {
            // TODO
            state.throwException(new ASCIIString(errorMessage_NotImplemented));
            RELEASE_ASSERT_NOT_REACHED();
        }
        // TODO
        if (obj->arraylength() >= ArrayBufferObject::maxArrayBufferSize) {
            state.throwException(new ASCIIString(errorMessage_NotImplemented));
        }
        RELEASE_ASSERT(obj->arraylength() < ArrayBufferObject::maxArrayBufferSize);
    }
    return obj;
}

Value validateTypedArray(ExecutionState& state, Object* thisObject, String* func)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!thisObject->isTypedArrayObject() || !thisObject->isArrayBufferView()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, func, errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }

    auto wrapper = thisObject->asArrayBufferView();
    ArrayBufferObject* buffer = wrapper->buffer();
    if (buffer->isDetachedBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, func, errorMessage_GlobalObject_DetachedBuffer);
    }
    return buffer;
}

Value builtinTypedArrayCopyWithin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, copyWithin);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().copyWithin.string());

    // Array.prototype.copyWithin as defined in 22.1.3.3 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    double len = O->asArrayBufferView()->arraylength();

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

Value builtinTypedArrayIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // NOTE: Same algorithm as Array.prototype.indexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, indexOf);
    if (!O->isTypedArrayObject()) {
        const StaticStrings* strings = &state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, strings->indexOf.string(), errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }
    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    int64_t len = O->asArrayBufferView()->arraylength();

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
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
            continue;
        }
        // Increase k by 1.
        k++;
    }

    // Return -1.
    return Value(-1);
}

Value builtinTypedArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // NOTE: Same algorithm as Array.prototype.lastIndexOf
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, lastIndexOf);
    if (!O->isTypedArrayObject()) {
        const StaticStrings* strings = &state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, strings->lastIndexOf.string(), errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }
    // Let lenValue be this object's [[ArrayLength]] internal slot.
    // Let len be ToUint32(lenValue).
    int64_t len = O->asArrayBufferView()->arraylength();

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
            double result;
            Object::nextIndexBackward(state, O, k, -1, false, result);
            k = result;
            continue;
        }
        // Decrease k by 1.
        k--;
    }

    // Return -1.
    return Value(-1);
}

Value builtinTypedArraySet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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
    unsigned targetLength = wrapper->arraylength();
    int targetByteOffset = wrapper->byteoffset();
    int targetElementSize = ArrayBufferView::getElementSize(wrapper->typedArrayType());
    if (!arg0->isTypedArrayObject()) {
        Object* src = arg0;
        uint64_t srcLength = src->length(state);
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
        unsigned srcLength = arg0Wrapper->arraylength();
        int srcByteOffset = arg0Wrapper->byteoffset();
        if (((double)srcLength + (double)offset) > (double)targetLength) {
            const StaticStrings* strings = &state.context()->staticStrings();
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), errorMessage_GlobalObject_InvalidArrayLength);
        }
        int srcByteIndex = 0;
        ArrayBufferObject* oldSrcBuffer = srcBuffer;
        unsigned oldSrcByteoffset = arg0Wrapper->byteoffset();
        unsigned oldSrcBytelength = arg0Wrapper->bytelength();
        unsigned oldSrcArraylength = arg0Wrapper->arraylength();
        if (srcBuffer == targetBuffer) {
            // NOTE: Step 24
            srcBuffer = new ArrayBufferObject(state);
            bool succeed = srcBuffer->cloneBuffer(targetBuffer, srcByteOffset, oldSrcBytelength);
            if (!succeed) {
                const StaticStrings* strings = &state.context()->staticStrings();
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->TypedArray.string(), true, strings->set.string(), "");
            }
            arg0Wrapper->setBuffer(srcBuffer, 0, srcBuffer->bytelength(), oldSrcArraylength);
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

Value builtinTypedArraySubArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, TypedArray, subarray);
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!thisBinded->isTypedArrayObject() || argc < 1) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, strings->subarray.string(), errorMessage_GlobalObject_ThisNotTypedArrayObject);
    }
    auto wrapper = thisBinded->asArrayBufferView();
    ArrayBufferObject* buffer = wrapper->buffer();
    unsigned srcLength = wrapper->arraylength();
    int64_t relativeBegin = argv[0].toInt32(state);
    unsigned beginIndex;
    if (relativeBegin < 0)
        beginIndex = (srcLength + relativeBegin) > 0 ? (srcLength + relativeBegin) : 0;
    else
        beginIndex = (unsigned)relativeBegin < srcLength ? relativeBegin : srcLength;
    int64_t relativeEnd = srcLength;
    unsigned endIndex;
    if (argc >= 2 && !argv[1].isUndefined())
        relativeEnd = argv[1].toInt32(state);
    if (relativeEnd < 0)
        endIndex = (srcLength + relativeEnd) > 0 ? (srcLength + relativeEnd) : 0;
    else
        endIndex = relativeEnd < srcLength ? relativeEnd : srcLength;
    unsigned newLength = 0;
    if (endIndex > beginIndex)
        newLength = endIndex - beginIndex;
    int srcByteOffset = wrapper->byteoffset();
    Value arg[3] = { buffer, Value(srcByteOffset + beginIndex * ((wrapper->arraylength() == 0) ? 0 : (wrapper->bytelength() / wrapper->arraylength()))), Value(newLength) };
    return ByteCodeInterpreter::newOperation(state, thisBinded->get(state, strings->constructor).value(state, thisBinded), 3, arg);
}

Value builtinTypedArrayEvery(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, TypedArray, every);
    // ValidateTypedArray is applied to the this value prior to evaluating the algorithm.
    validateTypedArray(state, O, state.context()->staticStrings().every.string());

    // Array.prototype.every as defined in 22.1.3.5 except
    // that the this object’s [[ArrayLength]] internal slot is accessed
    // in place of performing a [[Get]] of "length"
    unsigned len = O->asArrayBufferView()->arraylength();

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
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
        // Let Pk be ToString(k).
        Value pk(k);

        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        bool kPresent = O->hasProperty(state, ObjectPropertyName(state, pk));

        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = O->get(state, ObjectPropertyName(state, pk)).value(state, O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value args[] = { kValue, Value(k), O };
            Value testResult = callbackfn.asFunction()->call(state, T, 3, args);

            if (!testResult.toBoolean(state)) {
                return Value(false);
            }

            // Increae k by 1.
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
        }
    }
    return Value(true);
}

template <typename TA, int elementSize>
FunctionObject* GlobalObject::installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    FunctionObject* taConstructor = new FunctionObject(state, NativeFunctionInfo(taName, builtinTypedArrayConstructor<TA, elementSize>, 3, [](ExecutionState& state, CodeBlock* cb, size_t argc, Value* argv) -> Object* {
                                                           return new TA(state);
                                                       }),
                                                       FunctionObject::__ForBuiltin__);
    taConstructor->markThisObjectDontNeedStructureTransitionTable(state);

    *proto = m_objectPrototype;
    Object* taPrototype = new Object(state);
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
    return M->keys(state);
}

static Value builtinTypedArrayValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, TypedArray, values);
    return M->values(state);
}

static Value builtinTypedArrayEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, TypedArray, entries);
    return M->entries(state);
}

static Value builtinTypedArrayGetToStringTag(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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
    m_arrayBuffer = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ArrayBuffer, builtinArrayBufferConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                           return new ArrayBufferObject(state);
                                       }),
                                       FunctionObject::__ForBuiltin__);
    m_arrayBuffer->markThisObjectDontNeedStructureTransitionTable(state);
    m_arrayBuffer->setPrototype(state, m_functionPrototype);
    m_arrayBuffer->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isView),
                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isView, builtinArrayBufferIsView, 1, nullptr, NativeFunctionInfo::Strict)),
                                                              (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBufferPrototype = m_objectPrototype;
    m_arrayBufferPrototype = new ArrayBufferObject(state);
    m_arrayBufferPrototype->setPrototype(state, m_objectPrototype);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                             ObjectPropertyDescriptor(Value(state.context()->staticStrings().ArrayBuffer.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    const StaticStrings* strings = &state.context()->staticStrings();

    JSGetterSetter gs(
        new FunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinArrayBufferByteLengthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor byteLengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                             ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->slice, builtinArrayBufferByteSlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBuffer->setFunctionPrototype(state, m_arrayBufferPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().ArrayBuffer),
                      ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // %TypedArray%
    FunctionObject* typedArrayFunction = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().TypedArray, builtinTypedArrayConstructor, 0, [](ExecutionState& state, CodeBlock* cb, size_t argc, Value* argv) -> Object* {
                                                                return new Object(state);
                                                            },
                                                                                      (NativeFunctionInfo::Flags)(NativeFunctionInfo::Strict | NativeFunctionInfo::Constructor)),
                                                            FunctionObject::__ForBuiltin__);
    // %TypedArray%.prototype
    Object* typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->subarray),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->subarray, builtinTypedArraySubArray, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->set),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->set, builtinTypedArraySet, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->copyWithin),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->copyWithin, builtinTypedArrayCopyWithin, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->every),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->every, builtinTypedArrayEvery, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->indexOf),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinTypedArrayIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->lastIndexOf),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinTypedArrayLastIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().keys),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().keys, builtinTypedArrayKeys, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinTypedArrayEntries, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto valuesFn = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinTypedArrayValues, 0, nullptr, NativeFunctionInfo::Strict));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().values),
                                                          ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                          ObjectPropertyDescriptor(valuesFn,
                                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("get [Symbol.toStringTag]")), builtinTypedArrayGetToStringTag, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayPrototype->defineOwnProperty(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().toStringTag), desc);
    }
    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinTypedArrayByteLengthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc2);
    }
    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings->getbyteOffset, builtinTypedArrayByteOffsetGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteOffsetDesc2(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->byteOffset), byteOffsetDesc2);
    }
    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings->getLength, builtinTypedArrayLengthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor lengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->length), lengthDesc);
    }
    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings->getBuffer, builtinTypedArrayBufferGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor bufferDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->buffer), bufferDesc);
    }

    m_int8Array = installTypedArray<Int8ArrayObject, 1>(state, strings->Int8Array, &m_int8ArrayPrototype, typedArrayFunction);
    m_int16Array = installTypedArray<Int16ArrayObject, 2>(state, strings->Int16Array, &m_int16ArrayPrototype, typedArrayFunction);
    m_int32Array = installTypedArray<Int32ArrayObject, 4>(state, strings->Int32Array, &m_int32ArrayPrototype, typedArrayFunction);
    m_uint8Array = installTypedArray<Uint8ArrayObject, 1>(state, strings->Uint8Array, &m_uint8ArrayPrototype, typedArrayFunction);
    m_uint16Array = installTypedArray<Uint16ArrayObject, 2>(state, strings->Uint16Array, &m_uint16ArrayPrototype, typedArrayFunction);
    m_uint32Array = installTypedArray<Uint32ArrayObject, 4>(state, strings->Uint32Array, &m_uint32ArrayPrototype, typedArrayFunction);
    m_uint8ClampedArray = installTypedArray<Uint8ClampedArrayObject, 1>(state, strings->Uint8ClampedArray, &m_uint8ClampedArrayPrototype, typedArrayFunction);
    m_float32Array = installTypedArray<Float32ArrayObject, 4>(state, strings->Float32Array, &m_float32ArrayPrototype, typedArrayFunction);
    m_float64Array = installTypedArray<Float64ArrayObject, 8>(state, strings->Float64Array, &m_float64ArrayPrototype, typedArrayFunction);
    m_int8ArrayPrototype = m_int8Array->getFunctionPrototype(state).asObject();
    m_int16ArrayPrototype = m_int16Array->getFunctionPrototype(state).asObject();
    m_int32ArrayPrototype = m_int32Array->getFunctionPrototype(state).asObject();
    m_uint8ArrayPrototype = m_uint8Array->getFunctionPrototype(state).asObject();
    m_uint16ArrayPrototype = m_uint16Array->getFunctionPrototype(state).asObject();
    m_uint32ArrayPrototype = m_uint32Array->getFunctionPrototype(state).asObject();
    m_float32ArrayPrototype = m_float32Array->getFunctionPrototype(state).asObject();
    m_float64ArrayPrototype = m_float64Array->getFunctionPrototype(state).asObject();
}
}

#endif // ESCARGOT_ENABLE_TYPEDARRAY
