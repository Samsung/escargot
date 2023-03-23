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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/SharedArrayBufferObject.h"

namespace Escargot {

#if defined(ENABLE_THREADING)

// https://262.ecma-international.org/#sec-sharedarraybuffer-constructor
static Value builtinSharedArrayBufferConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    uint64_t byteLength = argv[0].toIndex(state);
    if (byteLength == Value::InvalidIndexValue) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
    }

    Optional<uint64_t> maxByteLength;
    // Let requestedMaxByteLength be ? GetArrayBufferMaxByteLengthOption(options).
    if (UNLIKELY((argc > 1) && argv[1].isObject())) {
        Object* options = argv[1].asObject();
        Value maxLengthValue = options->get(state, ObjectPropertyName(state.context()->staticStrings().maxByteLength)).value(state, options);
        if (!maxLengthValue.isUndefined()) {
            maxByteLength = maxLengthValue.toIndex(state);
            if (UNLIKELY((maxByteLength.value() == Value::InvalidIndexValue) || (byteLength > maxByteLength.value()))) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
            }
        }
    }

    return SharedArrayBufferObject::allocateSharedArrayBuffer(state, newTarget.value(), byteLength, maxByteLength);
}

#define RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                          \
    if (UNLIKELY(!thisValue.isObject() || !thisValue.asObject()->isSharedArrayBufferObject())) {                                                                                                                                                       \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                       \
    SharedArrayBufferObject* NAME = thisValue.asObject()->asSharedArrayBufferObject();

// https://262.ecma-international.org/#sec-get-sharedarraybuffer.prototype.bytelength
static Value builtinSharedArrayBufferByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(obj, SharedArrayBuffer, getbyteLength);
    return Value(obj->byteLength());
}

static Value builtinSharedArrayBufferMaxByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(obj, SharedArrayBuffer, getmaxByteLength);

    if (obj->isResizableArrayBuffer()) {
        return Value(obj->maxByteLength());
    }

    return Value(obj->byteLength());
}

static Value builtinSharedArrayBufferGrowableGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(obj, SharedArrayBuffer, getgrowable);
    return Value(obj->isResizableArrayBuffer());
}

static Value builtinSharedArrayBufferGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(O, SharedArrayBuffer, grow);

    if (!O->isResizableArrayBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().SharedArrayBuffer.string(), true, state.context()->staticStrings().grow.string(), "SharedArrayBuffer is not a growable buffer");
    }

    // Let newByteLength to ? ToIntegerOrInfinity(newLength).
    auto newByteLength = argv[0].toInteger(state);

    if ((newByteLength < O->byteLength()) || (newByteLength > O->maxByteLength())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), true, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
    }

    O->backingStore()->resize(static_cast<size_t>(newByteLength));
    return Value();
}

// https://262.ecma-international.org/#sec-sharedarraybuffer.prototype.slice
static Value builtinSharedArrayBufferSlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SHAREDARRAYBUFFER(O, SharedArrayBuffer, slice);

    double len = O->byteLength();
    double relativeStart = argv[0].toInteger(state);
    size_t first = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);
    double relativeEnd = argv[1].isUndefined() ? len : argv[1].toInteger(state);
    double final_ = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);
    size_t newLen = std::max((int)final_ - (int)first, 0);

    Value constructor = O->speciesConstructor(state, state.context()->globalObject()->sharedArrayBuffer());
    Value arguments[] = { Value(newLen) };
    Object* newValue = Object::construct(state, constructor, 1, arguments).toObject(state);
    if (!newValue->isSharedArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().SharedArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor SharedArrayBuffer is not valid SharedArrayBuffer");
    }

    SharedArrayBufferObject* newBuffer = newValue->asSharedArrayBufferObject();
    if ((newBuffer->data() == O->data()) || (newBuffer->byteLength() < newLen)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().SharedArrayBuffer.string(), true, state.context()->staticStrings().slice.string(), "%s: return value of constructor SharedArrayBuffer is not valid SharedArrayBuffer");
    }

    newBuffer->fillData(O->data() + first, newLen);
    return newBuffer;
}

void GlobalObject::initializeSharedArrayBuffer(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->sharedArrayBuffer();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().SharedArrayBuffer), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installSharedArrayBuffer(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_sharedArrayBuffer = new NativeFunctionObject(state, NativeFunctionInfo(strings->SharedArrayBuffer, builtinSharedArrayBufferConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_sharedArrayBuffer->setGlobalIntrinsicObject(state);

    m_sharedArrayBufferPrototype = new PrototypeObject(state, m_objectPrototype);
    m_sharedArrayBufferPrototype->setGlobalIntrinsicObject(state, true);

    m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_sharedArrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                          ObjectPropertyDescriptor(Value(strings->SharedArrayBuffer.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        JSGetterSetter speciesGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor speciesDesc(speciesGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_sharedArrayBuffer->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), speciesDesc);

        JSGetterSetter byteLengthGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinSharedArrayBufferByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc(byteLengthGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);

        JSGetterSetter maxByteLengthGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getmaxByteLength, builtinSharedArrayBufferMaxByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor maxByteLengthDesc(maxByteLengthGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->maxByteLength), maxByteLengthDesc);

        JSGetterSetter growableGS(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getgrowable, builtinSharedArrayBufferGrowableGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor growableDesc(growableGS, ObjectPropertyDescriptor::ConfigurablePresent);
        m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->growable), growableDesc);
    }

    m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->grow),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->grow, builtinSharedArrayBufferGrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_sharedArrayBufferPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->slice),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->slice, builtinSharedArrayBufferSlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_sharedArrayBuffer->setFunctionPrototype(state, m_sharedArrayBufferPrototype);

    redefineOwnProperty(state, ObjectPropertyName(strings->SharedArrayBuffer),
                        ObjectPropertyDescriptor(m_sharedArrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

#else

void GlobalObject::initializeSharedArrayBuffer(ExecutionState& state)
{
    // dummy initialize function
}

#endif
} // namespace Escargot
