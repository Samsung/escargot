/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "TypedArrayObject.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

static int getElementSize(TypedArrayType type)
{
    switch (type) {
    case TypedArrayType::Int8:
    case TypedArrayType::Uint8:
    case TypedArrayType::Uint8Clamped:
        return 1;
    case TypedArrayType::Int16:
    case TypedArrayType::Uint16:
        return 2;
    case TypedArrayType::Int32:
    case TypedArrayType::Uint32:
    case TypedArrayType::Float32:
        return 4;
    case TypedArrayType::Float64:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_FirstArgumentInvalidLength);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: ArrayBuffer is detached buffer");
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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            if (offset % elementSize != 0) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            ArrayBufferObject* buffer = val.asObject()->asArrayBufferObject();
            unsigned bufferByteLength = buffer->bytelength();
            if (argc >= 3) {
                lenVal = argv[2];
            }
            unsigned newByteLength;
            if (lenVal.isUndefined()) {
                if (bufferByteLength % elementSize != 0)
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
                if (bufferByteLength < offset)
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
                newByteLength = bufferByteLength - offset;
            } else {
                int length = lenVal.toLength(state);
                newByteLength = length * elementSize;
                if (offset + newByteLength > bufferByteLength)
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_InvalidArrayBufferOffset);
            }
            obj->setBuffer(buffer, offset, newByteLength, newByteLength / elementSize);
        } else if (val.isObject()) {
            // TODO implement 22.2.1.4
            Object* inputObj = val.asObject();
            uint64_t length = inputObj->length(state);
            ASSERT(length >= 0);
            unsigned elementSize = obj->elementSize();
            ArrayBufferObject* buffer = new ArrayBufferObject(state);
            buffer->allocateBuffer(length * elementSize);
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
        if (obj->arraylength() >= 210000000) {
            state.throwException(new ASCIIString(errorMessage_NotImplemented));
        }
        RELEASE_ASSERT(obj->arraylength() < 210000000);
    }
    return obj;
}

Value builtinTypedArrayCopyWithin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
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
    auto arg0 = argv[0].toObject(state);
    ArrayBufferObject* targetBuffer = wrapper->buffer();
    unsigned targetLength = wrapper->arraylength();
    int targetByteOffset = wrapper->byteoffset();
    int targetElementSize = getElementSize(wrapper->typedArrayType());
    if (!arg0->isTypedArrayObject()) {
        Object* src = arg0;
        uint64_t srcLength = src->length(state);
        if (srcLength + (uint64_t)offset > targetLength) {
            const StaticStrings* strings = &state.context()->staticStrings();
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), "");
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
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->TypedArray.string(), true, strings->set.string(), "");
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
    int64_t relativeBegin = 0;
    unsigned beginIndex;
    if (argc >= 1)
        relativeBegin = argv[0].toInt32(state);
    if (relativeBegin < 0)
        beginIndex = (srcLength + relativeBegin) > 0 ? (srcLength + relativeBegin) : 0;
    else
        beginIndex = (unsigned)relativeBegin < srcLength ? relativeBegin : srcLength;
    int64_t relativeEnd = srcLength;
    unsigned endIndex;
    if (argc >= 2)
        relativeEnd = argv[1].toInt32(state);
    if (relativeEnd < 0)
        endIndex = (srcLength + relativeEnd) > 0 ? (srcLength + relativeEnd) : 0;
    else
        endIndex = relativeEnd < srcLength ? relativeEnd : srcLength;
    unsigned newLength = 0;
    if (endIndex - beginIndex > 0)
        newLength = endIndex - beginIndex;
    int srcByteOffset = wrapper->byteoffset();
    Value arg[3] = { buffer, Value(srcByteOffset + beginIndex * (wrapper->bytelength() / wrapper->arraylength())), Value(newLength) };
    return ByteCodeInterpreter::newOperation(state, thisBinded->get(state, strings->constructor).value(state, thisBinded), 3, arg);
}

template <typename TA, int elementSize>
FunctionObject* GlobalObject::installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    FunctionObject* taConstructor = new FunctionObject(state, NativeFunctionInfo(taName, builtinTypedArrayConstructor<TA, elementSize>, 3, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
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

void GlobalObject::installTypedArray(ExecutionState& state)
{
    m_arrayBuffer = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ArrayBuffer, builtinArrayBufferConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                           return new ArrayBufferObject(state);
                                       }),
                                       FunctionObject::__ForBuiltin__);
    m_arrayBuffer->markThisObjectDontNeedStructureTransitionTable(state);
    m_arrayBuffer->setPrototype(state, m_functionPrototype);

    m_arrayBufferPrototype = m_objectPrototype;
    m_arrayBufferPrototype = new ArrayBufferObject(state);
    m_arrayBufferPrototype->setPrototype(state, m_objectPrototype);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
    FunctionObject* typedArrayFunction = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().TypedArray, builtinTypedArrayConstructor, 0, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                                                return new Object(state);
                                                            },
                                                                                      (NativeFunctionInfo::Flags)(NativeFunctionInfo::Strict | NativeFunctionInfo::Consturctor)),
                                                            FunctionObject::__ForBuiltin__);
    // %TypedArray%.prototype
    Object* typedArrayPrototype = typedArrayFunction->getFunctionPrototype(state).asObject();
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->subarray),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->subarray, builtinTypedArraySubArray, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->set),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->set, builtinTypedArraySet, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->copyWithin),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->copyWithin, builtinTypedArrayCopyWithin, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->indexOf),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->indexOf, builtinTypedArrayIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    typedArrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->lastIndexOf),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->lastIndexOf, builtinTypedArrayLastIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
