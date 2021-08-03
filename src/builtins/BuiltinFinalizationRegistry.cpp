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
#include "runtime/FinalizationRegistryObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

#define RESOLVE_THIS_BINDING_TO_FINALIZATIONREGISTRY(NAME, BUILT_IN_METHOD)                                                                                                                                                                                               \
    if (!thisValue.isObject() || !thisValue.asObject()->isFinalizationRegistryObject()) {                                                                                                                                                                                 \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().FinalizationRegistry.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                     \
    FinalizationRegistryObject* NAME = thisValue.asObject()->asFinalizationRegistryObject();

static Value builtinFinalizationRegistryConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

    return new FinalizationRegistryObject(state, proto, argv[0].asObject(), state.resolveCallee()->getFunctionRealm(state));
}

static Value builtinfinalizationRegistryRegister(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_FINALIZATIONREGISTRY(finalRegistry, stringRegister);

    if (argc == 0 || !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "target is not object");
    }
    if (argv[0] == argv[1]) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "target and heldValue is the same");
    }

    Optional<Object*> unregisterToken;
    if (argc >= 3) {
        if (argv[2].isObject()) {
            unregisterToken = argv[2].asObject();
        } else if (!argv[2].isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "unregisterToken is not undefined");
        }
    }
    finalRegistry->setCell(state, argv[0].asObject(), argv[1], unregisterToken);
    return Value();
}

static Value builtinfinalizationRegistryUnregister(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_FINALIZATIONREGISTRY(finalRegistry, unregister);
    if (argc == 0 || !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "unregisterToken is not object");
    }
    return Value(finalRegistry->deleteCell(state, argv[0].asObject()));
}

static Value builtinfinalizationRegistryCleanupSome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_FINALIZATIONREGISTRY(finalRegistry, cleanupSome);

    // If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception
    Object* callback = nullptr;
    if (argc && !argv[0].isUndefined()) {
        if (!argv[0].isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "callback is not callable");
        }
        callback = argv[0].asObject();
    }

    finalRegistry->cleanupSome(state, callback);

    return Value();
}

void GlobalObject::initializeFinalizationRegistry(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->finalizationRegistry();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().FinalizationRegistry), nativeData, Value(Value::EmptyValue));
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

    // FinalizationRegistry.prototype.register
    m_finalizationRegistryPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringRegister),
                                                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringRegister, builtinfinalizationRegistryRegister, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // https://github.com/tc39/proposal-cleanup-some
    // FinalizationRegistry.prototype.cleanupSome
    m_finalizationRegistryPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().cleanupSome),
                                                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cleanupSome, builtinfinalizationRegistryCleanupSome, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_finalizationRegistry->setFunctionPrototype(state, m_finalizationRegistryPrototype);
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().FinalizationRegistry),
                        ObjectPropertyDescriptor(m_finalizationRegistry, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent)));
}
} // namespace Escargot
