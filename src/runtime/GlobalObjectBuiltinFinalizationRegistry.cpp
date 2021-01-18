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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "FinalizationRegistryObject.h"
#include "NativeFunctionObject.h"

namespace Escargot {

Value builtinFinalizationRegistryConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }
    if (argc == 0 || !argv[0].isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "cleanup Callback is not callable");
    }

    // Let finalizationRegistry be ? OrdinaryCreateFromConstructor(NewTarget, "%FinalizationRegistryPrototype%", « [[Realm]], [[CleanupCallback]], [[Cells]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->finalizationRegistryPrototype();
    });
    Object* activeFunction = state.resolveCallee();
    FinalizationRegistryObject* FinalizationRegistry = new FinalizationRegistryObject(state, proto, argv[0], activeFunction);

    return FinalizationRegistry;
}

Value builtinfinalizationRegistryRegister(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isFinalizationRegistryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    FinalizationRegistryObject* finalRegisty = thisValue.asObject()->asFinalizationRegistryObject();

    if (argc == 0 || !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "target is not object");
    }
    if (argv[0] == argv[1]) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "target and heldValue is the same");
    }
    Value unregisterToken = argv[2];
    if (argc >= 3 && !unregisterToken.isObject()) {
        if (!unregisterToken.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "unregisterToken is not undefined");
        }
        unregisterToken = Value(Value::EmptyValue);
    }
    finalRegisty->setCell(state, argv[0], argv[1], unregisterToken);
    return Value();
}

Value builtinfinalizationRegistryUnregister(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isFinalizationRegistryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    FinalizationRegistryObject* finalRegisty = thisValue.asObject()->asFinalizationRegistryObject();

    if (argc == 0 || !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "unregisterToken is not object");
    }
    return Value(finalRegisty->deleteCell(state, argv[0]));
}

void GlobalObject::installFinalizationRegistry(ExecutionState& state)
{
    m_finalizationRegistry = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().FinalizationRegistry, builtinFinalizationRegistryConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_finalizationRegistry->setGlobalIntrinsicObject(state);

    m_finalizationRegistryPrototype = new Object(state, m_objectPrototype);
    m_finalizationRegistryPrototype->setGlobalIntrinsicObject(state, true);
    m_finalizationRegistryPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_finalizationRegistry, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_finalizationRegistryPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                                      ObjectPropertyDescriptor(Value(state.context()->staticStrings().FinalizationRegistry.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // FinalizationRegistry.prototype.unregister
    m_finalizationRegistryPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().unregister),
                                                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().unregister, builtinfinalizationRegistryUnregister, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // FinalizationRegistry.prototype.unregister
    m_finalizationRegistryPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringRegister),
                                                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringRegister, builtinfinalizationRegistryRegister, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_finalizationRegistry->setFunctionPrototype(state, m_finalizationRegistryPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().FinalizationRegistry),
                      ObjectPropertyDescriptor(m_finalizationRegistry, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent)));
}
} // namespace Escargot
