/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Global.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/ShadowRealmObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#if defined(ESCARGOT_ENABLE_SHADOWREALM)

//  https://tc39.es/proposal-shadowrealm/#sec-shadowrealm
static Value builtinShadowRealmConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    // Let O be ? OrdinaryCreateFromConstructor(NewTarget, "%ShadowRealm.prototype%", « [[ShadowRealm]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->shadowRealmPrototype();
    });
    ShadowRealmObject* O = new ShadowRealmObject(state, proto);

    // Let callerContext be the running execution context.
    // Perform ? InitializeHostDefinedRealm().
    // Let innerContext be the running execution context.
    Context* innerContext = new Context(state.context()->vmInstance());
    // Remove innerContext from the execution context stack and restore callerContext as the running execution context.
    // Let realmRec be the Realm of innerContext.
    // Set O.[[ShadowRealm]] to realmRec.
    O->setRealmContext(innerContext);
    // Perform ? HostInitializeShadowRealm(realmRec, innerContext, O).
    // Assert: realmRec.[[GlobalObject]] is an ordinary object.
    ASSERT(innerContext->globalObject()->isOrdinary());
    // Return O.
    return O;
}

static Value builtinShadowRealmEvaluate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    const Value& O = thisValue;
    // Perform ? ValidateShadowRealmObject(O).
    if (!O.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "this value must be an Object");
    }
    if (!O.asObject()->isShadowRealmObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "this value must be a ShadowRealm object");
    }

    // If sourceText is not a String, throw a TypeError exception.
    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "sourceText must be a String");
    }
    // Let callerRealm be the current Realm Record.
    Context* callerRealm = state.context();
    // Let evalRealm be O.[[ShadowRealm]].
    Context* evalRealm = O.asObject()->asShadowRealmObject()->realmContext();
    // Return ? PerformShadowRealmEval(sourceText, callerRealm, evalRealm).
    return ShadowRealmObject::performShadowRealmEval(state, argv[0], callerRealm, evalRealm);
}

void GlobalObject::initializeShadowRealm(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->shadowRealm(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().ShadowRealm), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installShadowRealm(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_shadowRealm = new NativeFunctionObject(state, NativeFunctionInfo(strings->ShadowRealm, builtinShadowRealmConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_shadowRealm->setGlobalIntrinsicObject(state);

    m_shadowRealmPrototype = new PrototypeObject(state);
    m_shadowRealmPrototype->setGlobalIntrinsicObject(state, true);

    m_shadowRealm->setFunctionPrototype(state, m_shadowRealmPrototype);

    m_shadowRealmPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                    ObjectPropertyDescriptor(String::fromASCII("ShadowRealm"), ObjectPropertyDescriptor::ConfigurablePresent));

    m_shadowRealmPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->evaluate),
                                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->evaluate, builtinShadowRealmEvaluate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().ShadowRealm),
                        ObjectPropertyDescriptor(m_shadowRealm, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

#else

void GlobalObject::initializeShadowRealm(ExecutionState& state)
{
    // dummy initialize function
}

#endif

} // namespace Escargot
