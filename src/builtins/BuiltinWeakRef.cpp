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
#include "runtime/WeakRefObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

static Value builtinWeakRefConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }
    if (argc == 0 || !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "target is not object");
    }

    // Let weakRef be ? OrdinaryCreateFromConstructor(NewTarget, "%WeakRefPrototype%", « [[WeakRefTarget]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->weakRefPrototype();
    });
    WeakRefObject* weakRef = new WeakRefObject(state, proto, argv[0].asObject());

    return weakRef;
}


static Value builtinWeakRefDeRef(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakRefObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }
    // Let weakRef be the this value.
    WeakRefObject* weakRef = thisValue.asObject()->asWeakRefObject();
    return weakRef->targetAsValue();
}


void GlobalObject::installWeakRef(ExecutionState& state)
{
    m_weakRef = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakRef, builtinWeakRefConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_weakRef->setGlobalIntrinsicObject(state);

    m_weakRefPrototype = new Object(state, m_objectPrototype);
    m_weakRefPrototype->setGlobalIntrinsicObject(state, true);
    m_weakRefPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakRef, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakRefPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().deref),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().deref, builtinWeakRefDeRef, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // WeakRef.prototype.deref
    m_weakRefPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakRef.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakRef->setFunctionPrototype(state, m_weakRefPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakRef),
                      ObjectPropertyDescriptor(m_weakRef, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
