/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "runtime/NativeFunctionObject.h"
#include "runtime/ArrayBufferObject.h"

namespace Escargot {

// https://262.ecma-international.org/#sec-arraybuffer-constructor
static Value builtinArrayBufferConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    uint64_t byteLength = argv[0].toIndex(state);
    if (UNLIKELY(byteLength == Value::InvalidIndexValue)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
    }

    Optional<uint64_t> maxByteLength;
    // Let requestedMaxByteLength be ? GetArrayBufferMaxByteLengthOption(options).
    if (UNLIKELY((argc > 1) && argv[1].isObject())) {
        Object* options = argv[1].asObject();
        Value maxLengthValue = options->get(state, ObjectPropertyName(state.context()->staticStrings().maxByteLength)).value(state, options);
        if (!maxLengthValue.isUndefined()) {
            maxByteLength = maxLengthValue.toIndex(state);
            if (UNLIKELY((maxByteLength.value() == Value::InvalidIndexValue) || (byteLength > maxByteLength.value()))) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
            }
        }
    }

    return ArrayBufferObject::allocateArrayBuffer(state, newTarget.value(), byteLength, maxByteLength);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-arraybuffer.isview
static Value builtinArrayBufferIsView(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return Value(false);
    }

    Object* obj = argv[0].asObject();
    if (obj->isTypedArrayObject() || obj->isDataViewObject()) {
        return Value(true);
    }

    return Value(false);
}


#define RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isArrayBuffer()) {                                                                                                                                                                               \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                         \
    ArrayBuffer* NAME = thisValue.asObject()->asArrayBuffer();                                                                                                                                                                                           \
    if (obj->isSharedArrayBufferObject()) {                                                                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }


// https://262.ecma-international.org/#sec-get-arraybuffer.prototype.bytelength
static Value builtinArrayBufferByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, getbyteLength);

    if (obj->isDetachedBuffer()) {
        return Value(0);
    }

    return Value(obj->byteLength());
}

static Value builtinArrayBufferMaxByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, getmaxByteLength);

    if (obj->isDetachedBuffer()) {
        return Value(0);
    }

    if (obj->isResizableArrayBuffer()) {
        return Value(obj->maxByteLength());
    }

    return Value(obj->byteLength());
}

static Value builtinArrayBufferResizableGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, getresizable);

    return Value(obj->isResizableArrayBuffer());
}

static Value builtinArrayBufferResize(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, resize);
    obj->throwTypeErrorIfDetached(state);

    if (!obj->isResizableArrayBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().resize.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    double newByteLength = argv[0].toInteger(state);
    if (newByteLength < 0 || newByteLength > obj->maxByteLength()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().resize.string(), ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
    }

    obj->backingStore()->resize(static_cast<size_t>(newByteLength));

    return Value();
}

static Value builtinArrayBufferTransfer(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, transfer);
    obj->throwTypeErrorIfDetached(state);

    double newByteLength = obj->byteLength();
    if (argc > 0 && !argv[0].isUndefined()) {
        newByteLength = argv[0].toInteger(state);
    }

    Value arguments[] = { Value(newByteLength) };
    ArrayBuffer* newValue = Object::construct(state, state.context()->globalObject()->arrayBuffer(), 1, arguments).asObject()->asArrayBuffer();

    // Let copyLength be min(newByteLength, O.[[ArrayBufferByteLength]]).
    // Perform CopyDataBlockBytes(toBlock, 0, fromBlock, 0, copyLength).
    newValue->fillData(obj->data(), std::min(newByteLength, static_cast<double>(obj->byteLength())));

    obj->asArrayBufferObject()->detachArrayBuffer();

    return newValue;
}

// https://262.ecma-international.org/#sec-arraybuffer.prototype.slice
static Value builtinArrayBufferSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_ARRAYBUFFER(obj, ArrayBuffer, slice);

    obj->throwTypeErrorIfDetached(state);

    double len = obj->byteLength();
    double relativeStart = argv[0].toInteger(state);
    size_t first = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);
    double relativeEnd = argv[1].isUndefined() ? len : argv[1].toInteger(state);
    double final_ = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);
    size_t newLen = std::max((int)final_ - (int)first, 0);

    Value constructor = obj->speciesConstructor(state, state.context()->globalObject()->arrayBuffer());
    Value arguments[] = { Value(newLen) };
    Object* newValue = Object::construct(state, constructor, 1, arguments).toObject(state);
    if (!newValue->isArrayBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    }

    ArrayBuffer* newObject = newValue->asArrayBuffer();
    newObject->throwTypeErrorIfDetached(state);

    if (newObject == obj) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    }

    if (newObject->byteLength() < newLen) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor ArrayBuffer is not valid ArrayBuffer");
    }
    obj->throwTypeErrorIfDetached(state);

    newObject->fillData(obj->data() + first, newLen);
    return newObject;
}

void GlobalObject::initializeArrayBuffer(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->arrayBuffer();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().ArrayBuffer), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installArrayBuffer(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_arrayBuffer = new NativeFunctionObject(state, NativeFunctionInfo(strings->ArrayBuffer, builtinArrayBufferConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_arrayBuffer->setGlobalIntrinsicObject(state);

    m_arrayBuffer->defineOwnProperty(state, ObjectPropertyName(strings->isView),
                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->isView, builtinArrayBufferIsView, 1, NativeFunctionInfo::Strict)),
                                                              (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBufferPrototype = new Object(state, m_objectPrototype);
    m_arrayBufferPrototype->setGlobalIntrinsicObject(state, true);

    m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                             ObjectPropertyDescriptor(Value(strings->ArrayBuffer.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        JSGetterSetter speciesGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor speciesDesc(speciesGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_arrayBuffer->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), speciesDesc);

        JSGetterSetter byteLengthGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinArrayBufferByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc(byteLengthGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);

        JSGetterSetter maxByteLengthGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getmaxByteLength, builtinArrayBufferMaxByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor maxByteLengthDesc(maxByteLengthGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->maxByteLength), maxByteLengthDesc);

        JSGetterSetter resizableGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getresizable, builtinArrayBufferResizableGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor resizableDesc(resizableGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_arrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->resizable), resizableDesc);
    }

    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->resize),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resize, builtinArrayBufferResize, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->transfer),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->transfer, builtinArrayBufferTransfer, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->slice),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinArrayBufferSlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayBuffer->setFunctionPrototype(state, m_arrayBufferPrototype);

    redefineOwnProperty(state, ObjectPropertyName(strings->ArrayBuffer),
                        ObjectPropertyDescriptor(m_arrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
