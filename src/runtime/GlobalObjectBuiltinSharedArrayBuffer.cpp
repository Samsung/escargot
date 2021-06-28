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

#if defined(ENABLE_THREADING)

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "NativeFunctionObject.h"
#include "ArrayBufferObject.h"
#include "SharedArrayBufferObject.h"

namespace Escargot {

// https://262.ecma-international.org/#sec-sharedarraybuffer-constructor
static Value builtinSharedArrayBufferConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    uint64_t byteLength = argv[0].toIndex(state);
    if (byteLength == Value::InvalidIndexValue) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_FirstArgumentInvalidLength);
    }

    return SharedArrayBufferObject::allocateSharedArrayBuffer(state, newTarget.value(), byteLength);
}

// https://262.ecma-international.org/#sec-get-sharedarraybuffer.prototype.bytelength
static Value builtinSharedArrayBufferByteLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isSharedArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().SharedArrayBuffer.string(), true, state.context()->staticStrings().getbyteLength.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }
    SharedArrayBufferObject* obj = thisValue.asObject()->asSharedArrayBufferObject();
    return Value(obj->byteLength());
}

// TODO
// https://262.ecma-international.org/#sec-sharedarraybuffer.prototype.slice

void GlobalObject::installSharedArrayBuffer(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_sharedArrayBuffer = new NativeFunctionObject(state, NativeFunctionInfo(strings->SharedArrayBuffer, builtinSharedArrayBufferConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_sharedArrayBuffer->setGlobalIntrinsicObject(state);

    m_sharedArrayBufferPrototype = new Object(state, m_objectPrototype);
    m_sharedArrayBufferPrototype->setGlobalIntrinsicObject(state, true);

    m_sharedArrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_sharedArrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_sharedArrayBufferPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                                   ObjectPropertyDescriptor(Value(strings->SharedArrayBuffer.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_sharedArrayBuffer->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->getbyteLength, builtinSharedArrayBufferByteLengthGetter, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor byteLengthDesc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_sharedArrayBufferPrototype->defineOwnProperty(state, ObjectPropertyName(strings->byteLength), byteLengthDesc);

    m_sharedArrayBuffer->setFunctionPrototype(state, m_sharedArrayBufferPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->SharedArrayBuffer),
                      ObjectPropertyDescriptor(m_sharedArrayBuffer, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot

#endif
