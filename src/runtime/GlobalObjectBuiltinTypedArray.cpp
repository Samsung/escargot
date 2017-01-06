#if ESCARGOT_ENABLE_TYPEDARRAY

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "TypedArrayObject.h"
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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, errorMessage_GlobalObject_FirstArgumentInvalidLength);
        obj->allocateBuffer(byteLength);
    } else {
        obj->allocateBuffer(0);
    }
    return obj;
}

static Value builtinArrayBufferByteLenthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinTypedArrayByteLenthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->bytelength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinTypedArrayLenthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isTypedArrayObject())) {
        return Value(thisValue.asObject()->asArrayBufferView()->arraylength());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get TypedArray.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

template <typename T, int typedArrayElementSize>
Value builtinTypedArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // if NewTarget is undefined, throw a TypeError
    if (!isNewExpression)
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), false, String::emptyString, errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor);
    TypedArrayObject<T, typedArrayElementSize>* obj = thisValue.asObject()->asTypedArrayObject<T, typedArrayElementSize>();
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
            obj->setBuffer(buffer);
            obj->setBytelength(newByteLength);
            obj->setByteoffset(offset);
            obj->setArraylength(newByteLength / elementSize);
        } else if (val.isObject()) {
            // TODO implement 22.2.1.4
            Object* inputObj = val.asObject();
            uint64_t length = inputObj->length(state);
            ASSERT(length >= 0);
            unsigned elementSize = obj->elementSize();
            ArrayBufferObject* buffer = new ArrayBufferObject(state);
            buffer->allocateBuffer(length * elementSize);
            obj->setBuffer(buffer);
            obj->setBytelength(length * elementSize);
            obj->setByteoffset(0);
            obj->setArraylength(length);
            for (uint64_t i = 0; i < length; i++) {
                ObjectPropertyName pK(state, Value(i));
                obj->setThrowsException(state, pK, inputObj->get(state, pK).value(state, inputObj), obj);
            }
        } else {
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
        }
        // TODO
        RELEASE_ASSERT(obj->arraylength() < 210000000);
    }
    return obj;
}


template <typename T, int elementSize>
FunctionObject* GlobalObject::installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction)
{
    // $22.2.1.1~
    FunctionObject* taConstructor = new FunctionObject(state, NativeFunctionInfo(taName, builtinTypedArrayConstructor<T, elementSize>, 3, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                                           return new TypedArrayObject<T, elementSize>(state);
                                                       }),
                                                       FunctionObject::__ForBuiltin__);
    taConstructor->markThisObjectDontNeedStructureTransitionTable(state);
    taConstructor->setPrototype(state, m_functionPrototype);

    *proto = m_objectPrototype;
    Object* taPrototype = new TypedArrayObject<T, elementSize>(state);
    taPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    taPrototype->setPrototype(state, typedArrayFunction->getFunctionPrototype(state));
    /*
    // $22.2.3.2
    taPrototype->defineAccessorProperty(strings->byteLength, [](ESObject* self, ESObject* originalObj, ::escargot::ESString* propertyName) -> ESValue {
        // FIXME find right object from originalObj
        return ESValue(originalObj->asESTypedArrayObject<T>()->bytelength());
    }, nullptr, true, false, false);
    // $22.2.3.2
    taPrototype->defineAccessorProperty(strings->length, [](ESObject* self, ESObject* originalObj, ::escargot::ESString* propertyName) -> ESValue {
        // FIXME find right object from originalObj
        return ESValue(originalObj->asESTypedArrayObject<T>()->arraylength());
    }, nullptr, true, false, false);
    // TODO add reference
    taPrototype->defineAccessorProperty(strings->buffer, [](ESObject* self, ESObject* originalObj, ::escargot::ESString* propertyName) -> ESValue {
        // FIXME find right object from originalObj
        return ESValue(originalObj->asESTypedArrayObject<T>()->buffer());
    }, nullptr, true, false, false);
    // $22.2.3.22 %TypedArray%.prototype.set(overloaded[, offset])
    taPrototype->ESObject::defineDataProperty(strings->set, true, false, true, ESFunctionObject::create(NULL, [](ESVMInstance* instance)->ESValue {
        int arglen = instance->currentExecutionContext()->argumentCount();
        RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, TypedArray, set);
        if (!thisBinded->isESTypedArrayObject() || arglen < 1) {
            throwBuiltinError(instance, ErrorCode::TypeError, strings->TypedArray, true, strings->set, errorMessage_GlobalObject_ThisNotTypedArrayObject);
        }
        auto wrapper = thisBinded->asESTypedArrayObjectWrapper();
        int offset = 0;
        if (arglen >= 2)
            offset = instance->currentExecutionContext()->arguments()[1].toInt32();
        if (offset < 0)
            throwBuiltinError(instance, ErrorCode::TypeError, strings->TypedArray, true, strings->set, "");
        auto arg0 = instance->currentExecutionContext()->readArgument(0).asESPointer();
        escargot::ESArrayBufferObject* targetBuffer = wrapper->buffer();
        unsigned targetLength = wrapper->arraylength();
        int targetByteOffset = wrapper->byteoffset();
        int targetElementSize = wrapper->elementSize();
        if (!arg0->isESTypedArrayObject()) {
            ESObject* src = arg0->asESObject();
            uint32_t srcLength = (uint32_t)src->get(strings->length.string()).asInt32();
            if (srcLength + (uint32_t)offset > targetLength)
                throwBuiltinError(instance, ErrorCode::RangeError, strings->TypedArray, true, strings->set, "");
            int targetByteIndex = offset * targetElementSize + targetByteOffset;
            int k = 0;
            int limit = targetByteIndex + targetElementSize * srcLength;
            while (targetByteIndex < limit) {
                escargot::ESString* Pk = ESString::create(k);
                double kNumber = src->get(Pk).toNumber();
                wrapper->set(targetByteIndex / targetElementSize, ESValue(kNumber));
                k++;
                targetByteIndex += targetElementSize;
            }
            return ESValue();
        } else {
            auto arg0Wrapper = arg0->asESTypedArrayObjectWrapper();
            escargot::ESArrayBufferObject* srcBuffer = arg0Wrapper->buffer();
            unsigned srcLength = arg0Wrapper->arraylength();
            int srcByteOffset = arg0Wrapper->byteoffset();
            if (srcLength + (unsigned)offset > targetLength)
                throwBuiltinError(instance, ErrorCode::RangeError, strings->TypedArray, true, strings->set, "");
            int srcByteIndex = 0;
            if (srcBuffer == targetBuffer) {
                // TODO: 24) should clone targetBuffer
                RELEASE_ASSERT_NOT_REACHED();
            } else {
                srcByteIndex = srcByteOffset;
            }
            unsigned targetIndex = (unsigned)offset, srcIndex = 0;
            unsigned targetByteIndex = offset * targetElementSize + targetByteOffset;
            unsigned limit = targetByteIndex + targetElementSize * srcLength;
            if (wrapper->arraytype() != arg0Wrapper->arraytype()) {
                while (targetIndex < offset + srcLength) {
                    ESValue value = arg0Wrapper->get(srcIndex);
                    wrapper->set(targetIndex, value);
                    srcIndex++;
                    targetIndex++;
                }
            } else {
                while (targetByteIndex < limit) {
                    ESValue value = srcBuffer->getValueFromBuffer<uint8_t>(srcByteIndex, escargot::Uint8Array);
                    targetBuffer->setValueInBuffer<Uint8Adaptor>(targetByteIndex, escargot::Uint8Array, value);
                    srcByteIndex++;
                    targetByteIndex++;
                }
            }
            return ESValue();
        }
    }, strings->set));
    // $22.2.3.26 %TypedArray%.prototype.subarray([begin [, end]])
    taPrototype->ESObject::defineDataProperty(strings->subarray, true, false, true, ESFunctionObject::create(NULL, [](ESVMInstance* instance)->ESValue {
        size_t arglen = instance->currentExecutionContext()->argumentCount();
        RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, TypedArray, subarray);
        if (!thisBinded->isESTypedArrayObject())
            throwBuiltinError(instance, ErrorCode::TypeError, strings->TypedArray, true, strings->subarray, errorMessage_GlobalObject_ThisNotTypedArrayObject);
        auto wrapper = thisBinded->asESTypedArrayObjectWrapper();
        escargot::ESArrayBufferObject* buffer = wrapper->buffer();
        unsigned srcLength = wrapper->arraylength();
        int64_t relativeBegin = 0;
        unsigned beginIndex;
        if (arglen >= 1)
            relativeBegin = instance->currentExecutionContext()->arguments()[0].toInt32();
        if (relativeBegin < 0)
            beginIndex = (srcLength + relativeBegin) > 0 ? (srcLength + relativeBegin) : 0;
        else
            beginIndex = (unsigned) relativeBegin < srcLength ? relativeBegin : srcLength;
        int64_t relativeEnd = srcLength;
        unsigned endIndex;
        if (arglen >= 2)
            relativeEnd = instance->currentExecutionContext()->arguments()[1].toInt32();
        if (relativeEnd < 0)
            endIndex = (srcLength + relativeEnd) > 0 ? (srcLength + relativeEnd) : 0;
        else
            endIndex = relativeEnd < srcLength ? relativeEnd : srcLength;
        unsigned newLength = 0;
        if (endIndex - beginIndex > 0)
            newLength = endIndex - beginIndex;
        int srcByteOffset = wrapper->byteoffset();
        ESValue arg[3] = {buffer, ESValue(srcByteOffset + beginIndex * wrapper->elementSize()), ESValue(newLength)};
        escargot::ESTypedArrayObject<T>* newobj = escargot::ESTypedArrayObject<T>::create();
        ESValue ret = ESFunctionObject::call(instance, thisBinded->get(strings->constructor.string()), newobj, arg, 3, instance);
        return ret;
    }, strings->subarray));
    */

    taConstructor->setPrototype(state, m_functionPrototype); // empty Function
    taConstructor->setFunctionPrototype(state, taPrototype);

    taPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(taConstructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
        new FunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinArrayBufferByteLenthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor byteLengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                             ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->slice, builtinArrayBufferByteSlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBuffer->setFunctionPrototype(state, m_arrayBufferPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().ArrayBuffer),
                      ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* typedArrayFunction = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().TypedArray, builtinTypedArrayConstructor, 0, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                                                return new Object(state);
                                                            },
                                                                                      (NativeFunctionInfo::Flags)(NativeFunctionInfo::Strict | NativeFunctionInfo::Consturctor)),
                                                            FunctionObject::__ForBuiltin__);
    {
        JSGetterSetter gs2(
            new FunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinTypedArrayByteLenthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc2(gs2, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc2);
    }
    {
        JSGetterSetter gs2(
            new FunctionObject(state, NativeFunctionInfo(strings->getLength, builtinTypedArrayLenthGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor lengthDesc(gs2, ObjectPropertyDescriptor::ConfigurablePresent);
        typedArrayFunction->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(strings->length), lengthDesc);
    }

    m_int8Array = installTypedArray<Int8Adaptor, 1>(state, strings->Int8Array, &m_int8ArrayPrototype, typedArrayFunction);
    m_int16Array = installTypedArray<Int16Adaptor, 2>(state, strings->Int16Array, &m_int16ArrayPrototype, typedArrayFunction);
    m_int32Array = installTypedArray<Int32Adaptor, 4>(state, strings->Int32Array, &m_int32ArrayPrototype, typedArrayFunction);
    m_uint8Array = installTypedArray<Uint8Adaptor, 1>(state, strings->Uint8Array, &m_uint8ArrayPrototype, typedArrayFunction);
    m_uint16Array = installTypedArray<Uint16Adaptor, 2>(state, strings->Uint16Array, &m_uint16ArrayPrototype, typedArrayFunction);
    m_uint32Array = installTypedArray<Uint32Adaptor, 4>(state, strings->Uint32Array, &m_uint32ArrayPrototype, typedArrayFunction);
    m_uint8ClampedArray = installTypedArray<Uint8Adaptor, 1>(state, strings->Uint8ClampedArray, &m_uint8ClampedArrayPrototype, typedArrayFunction);
    m_float32Array = installTypedArray<Float32Adaptor, 4>(state, strings->Float32Array, &m_float32ArrayPrototype, typedArrayFunction);
    m_float64Array = installTypedArray<Float64Adaptor, 8>(state, strings->Float64Array, &m_float64ArrayPrototype, typedArrayFunction);
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
