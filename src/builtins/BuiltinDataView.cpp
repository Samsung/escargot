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
#include "runtime/DataViewObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

#define FOR_EACH_DATAVIEW_TYPES(F) \
    F(Float32)                     \
    F(Float64)                     \
    F(Int8)                        \
    F(Int16)                       \
    F(Int32)                       \
    F(Uint8)                       \
    F(Uint16)                      \
    F(Uint32)                      \
    F(BigInt64)                    \
    F(BigUint64)

static Value builtinDataViewConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
        return Value();
    }
    if (!(argv[0].isObject() && argv[0].asPointerValue()->isArrayBuffer())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_ThisNotArrayBufferObject);
    }

    ArrayBuffer* buffer = argv[0].asObject()->asArrayBuffer();
    double byteOffset = 0;
    if (argc >= 2) {
        Value& val = argv[1];
        byteOffset = val.toIndex(state);
        if (byteOffset == Value::InvalidIndexValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
        }
    }
    if (buffer->isDetachedBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, "%s: ArrayBuffer is detached buffer");
    }
    double bufferByteLength = buffer->byteLength();

    if (byteOffset > bufferByteLength) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
    }
    double byteLength = bufferByteLength - byteOffset;

    if (argc >= 3) {
        Value& val = argv[2];
        if (!val.isUndefined()) {
            byteLength = val.toIndex(state);
            if (byteOffset + byteLength > bufferByteLength || byteLength == Value::InvalidIndexValue) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
            }
        }
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->dataViewPrototype();
    });
    ArrayBufferView* obj = new DataViewObject(state, proto);
    obj->setBuffer(buffer, byteOffset, byteLength);

    if (obj->buffer()->isDetachedBuffer()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().DataView.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_DetachedBuffer);
    }

    return obj;
}

#define DECLARE_DATAVIEW_GETTER(Name)                                                                                                    \
    static Value builtinDataViewGet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                    \
        Object* thisObject = thisValue.toObject(state);                                                                                  \
        if (!(thisObject->isDataViewObject())) {                                                                                         \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().DataView.string(),              \
                                           true, state.context()->staticStrings().get##Name.string(),                                    \
                                           ErrorObject::Messages::GlobalObject_ThisNotDataViewObject);                                   \
        }                                                                                                                                \
        if (argc < 2) {                                                                                                                  \
            return thisObject->asDataViewObject()->getViewValue(state, argv[0], Value(false), TypedArrayType::Name);                     \
        } else {                                                                                                                         \
            return thisObject->asDataViewObject()->getViewValue(state, argv[0], argv[1], TypedArrayType::Name);                          \
        }                                                                                                                                \
    }

#define DECLARE_DATAVIEW_SETTER(Name)                                                                                                    \
    static Value builtinDataViewSet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                    \
        Object* thisObject = thisValue.toObject(state);                                                                                  \
        if (!(thisObject->isDataViewObject())) {                                                                                         \
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().DataView.string(),              \
                                           true, state.context()->staticStrings().set##Name.string(),                                    \
                                           ErrorObject::Messages::GlobalObject_ThisNotDataViewObject);                                   \
        }                                                                                                                                \
        if (argc < 3) {                                                                                                                  \
            thisObject->asDataViewObject()->setViewValue(state, argv[0], Value(false), TypedArrayType::Name, argv[1]);                   \
        } else {                                                                                                                         \
            thisObject->asDataViewObject()->setViewValue(state, argv[0], argv[2], TypedArrayType::Name, argv[1]);                        \
        }                                                                                                                                \
        return Value();                                                                                                                  \
    }

FOR_EACH_DATAVIEW_TYPES(DECLARE_DATAVIEW_GETTER);
FOR_EACH_DATAVIEW_TYPES(DECLARE_DATAVIEW_SETTER);

static Value builtinDataViewBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isDataViewObject())) {
        ArrayBuffer* buffer = thisValue.asObject()->asArrayBufferView()->buffer();
        if (buffer) {
            return Value(buffer);
        }
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "get DataView.prototype.buffer called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinDataViewByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isDataViewObject() && thisValue.asObject()->asDataViewObject()->buffer() && !thisValue.asObject()->asDataViewObject()->buffer()->isDetachedBuffer())) {
        return Value(thisValue.asObject()->asArrayBufferView()->byteLength());
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "get DataView.prototype.byteLength called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinDataViewByteOffsetGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isDataViewObject() && thisValue.asObject()->asDataViewObject()->buffer() && !thisValue.asObject()->asDataViewObject()->buffer()->isDetachedBuffer())) {
        return Value(thisValue.asObject()->asArrayBufferView()->byteOffset());
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "get DataView.prototype.byteOffset called on incompatible receiver");
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::initializeDataView(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->dataView();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().DataView), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installDataView(ExecutionState& state)
{
    m_dataView = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().DataView, builtinDataViewConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_dataView->setGlobalIntrinsicObject(state);

    m_dataViewPrototype = new DataViewObject(state, m_objectPrototype);
    m_dataViewPrototype->setGlobalIntrinsicObject(state, true);
    m_dataView->setFunctionPrototype(state, m_dataViewPrototype);

    m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_dataView, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                 ObjectPropertyDescriptor(Value(state.context()->staticStrings().DataView.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
#define DATAVIEW_DEFINE_GETTER(Name)                                                                                                                                                                                                \
    m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get##Name),                                                                                                             \
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get##Name, builtinDataViewGet##Name, 1, NativeFunctionInfo::Strict)), \
                                                                          (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

#define DATAVIEW_DEFINE_SETTER(Name)                                                                                                                                                                                                \
    m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set##Name),                                                                                                             \
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set##Name, builtinDataViewSet##Name, 2, NativeFunctionInfo::Strict)), \
                                                                          (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FOR_EACH_DATAVIEW_TYPES(DATAVIEW_DEFINE_GETTER);
    FOR_EACH_DATAVIEW_TYPES(DATAVIEW_DEFINE_SETTER);

    const StaticStrings* strings = &state.context()->staticStrings();

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getBuffer, builtinDataViewBufferGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor bufferDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->buffer), bufferDesc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinDataViewByteLengthGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteLengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteOffset, builtinDataViewByteOffsetGetter, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor byteOffsetDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_dataViewPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->byteOffset), byteOffsetDesc);
    }

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().DataView),
                        ObjectPropertyDescriptor(m_dataView, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
