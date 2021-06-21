/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "ScriptClassConstructorFunctionObject.h"

#include "FunctionObjectInlines.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ScriptVirtualArrowFunctionObject.h"

namespace Escargot {
ScriptClassConstructorFunctionObject::ScriptClassConstructorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock,
                                                                           LexicalEnvironment* outerEnvironment, Object* homeObject, String* classSourceCode, const Optional<AtomicString>& name, bool needsToSetHomeObjectForInstance)
    : ScriptFunctionObject(state, proto, codeBlock, outerEnvironment, name ? 3 : 2)
    , m_needsToSetHomeObjectForInstance(needsToSetHomeObjectForInstance)
    , m_homeObject(homeObject)
    , m_classSourceCode(classSourceCode)
{
    ASSERT(!codeBlock->isGenerator()); // Class constructor may not be a generator

    if (name) {
        m_structure = state.context()->defaultStructureForClassConstructorFunctionObjectWithName();
        m_prototypeIndex = 2;

        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2 < m_structure->propertyCount());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionLength()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = name.value().string();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
    } else {
        m_structure = state.context()->defaultStructureForClassConstructorFunctionObject();
        m_prototypeIndex = 1;

        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionLength()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
    }
}

Value ScriptClassConstructorFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Class constructor cannot be invoked without 'new'");
    return Value();
}

class ScriptClassConstructorFunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& callerState, ExecutionState& calleeState, ScriptClassConstructorFunctionObject* self, const Value& thisArgument, bool isStrict)
    {
        // Let envRec be localEnv’s EnvironmentRecord.
        // Assert: The next step never returns an abrupt completion because envRec.[[thisBindingStatus]] is not "uninitialized".
        // Return envRec.BindThisValue(thisValue).
        if (self->constructorKind() == ScriptFunctionObject::ConstructorKind::Base) {
            FunctionEnvironmentRecord* r = calleeState.lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            r->bindThisValue(calleeState, thisArgument);
            r->functionObject()->asScriptClassConstructorFunctionObject()->initInstanceFieldMembers(calleeState, thisArgument.asObject());
        }

        return thisArgument;
    }
};

class ScriptClassConstructorFunctionObjectNewTargetBinderWithConstruct {
public:
    void operator()(ExecutionState& callerState, ExecutionState& calleeState, FunctionObject* self, Object* newTarget, FunctionEnvironmentRecord* record)
    {
        record->setNewTarget(newTarget);
    }
};

class ScriptClassConstructorFunctionObjectReturnValueBinderWithConstruct {
public:
    Value operator()(ExecutionState& callerState, ExecutionState& calleeState, ScriptClassConstructorFunctionObject* self, const Value& interpreterReturnValue, const Value& thisArgument, FunctionEnvironmentRecord* record)
    {
        // Let result be OrdinaryCallEvaluateBody(F, argumentsList).
        const Value& result = interpreterReturnValue;
        // If result.[[type]] is return, then
        // If Type(result.[[value]]) is Object, return NormalCompletion(result.[[value]]).
        if (result.isObject()) {
            return result;
        }
        // If kind is "base", return NormalCompletion(thisArgument).
        if (self->constructorKind() == ScriptFunctionObject::ConstructorKind::Base) {
            return thisArgument;
        }
        // If result.[[value]] is not undefined, throw a TypeError exception.
        if (!result.isUndefined()) {
            ErrorObject::throwBuiltinError(callerState, ErrorObject::TypeError, ErrorObject::Messages::InvalidDerivedConstructorReturnValue);
        }
        // Else, ReturnIfAbrupt(result).
        // Return envRec.GetThisBinding().
        return record->getThisBinding(calleeState);
    }
};


Value ScriptClassConstructorFunctionObject::construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget)
{
    // Assert: Type(newTarget) is Object.
    Object* thisArgument = nullptr;
    // Let kind be F’s [[ConstructorKind]] internal slot.
    ConstructorKind kind = constructorKind();

    // If kind is "base", then
    if (kind == ConstructorKind::Base) {
        // Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget, [](ExecutionState& state, Context* constructorRealm) -> Object* {
            return constructorRealm->globalObject()->objectPrototype();
        });
        // Set the [[Prototype]] internal slot of obj to proto.
        thisArgument = new Object(state, proto);
        // ReturnIfAbrupt(thisArgument).

        // if there is class private names on CodeBlock,
        // we should store homeObject for check nested private class field classes
        /*
        var C = class {
          get #m() { return 'outer class'; }
          method() { return this.#m; }
          B = class {
            method(o) {
              return o.#m;
            }
            #m = 'test262';
          }
        }
        let c = new C();
        let innerB = new c.B();
        print(innerB.method(c)) // throw exception
        */
        if (m_needsToSetHomeObjectForInstance) {
            thisArgument->setHomeObject(homeObject());
        }
    }

    // Let calleeContext be PrepareForOrdinaryCall(F, newTarget).
    // Assert: calleeContext is now the running execution context.
    // If kind is "base", perform OrdinaryCallBindThis(F, calleeContext, thisArgument).
    // -> perform OrdinaryCallBindThis at ScriptClassConstructorFunctionObjectThisValueBinder

    // Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    // If kind is "base", return NormalCompletion(thisArgument).
    // If result.[[value]] is not undefined, throw a TypeError exception.
    // Else, ReturnIfAbrupt(result).
    // Return envRec.GetThisBinding().
    // -> perform at ScriptClassConstructorFunctionObjectReturnValueBinderWithConstruct
    return FunctionObjectProcessCallGenerator::processCall<ScriptClassConstructorFunctionObject, true, true, true, ScriptClassConstructorFunctionObjectThisValueBinder,
                                                           ScriptClassConstructorFunctionObjectNewTargetBinderWithConstruct, ScriptClassConstructorFunctionObjectReturnValueBinderWithConstruct>(state, this, thisArgument, argc, argv, newTarget)
        .asObject();
}

void ScriptClassConstructorFunctionObject::initInstanceFieldMembers(ExecutionState& state, Object* instance)
{
    size_t s = m_instanceFieldInitData.size();
    Object* homeObject = getFunctionPrototype(state).isObject() ? getFunctionPrototype(state).asObject() : nullptr;
    for (size_t i = 0; i < s; i++) {
        Value name = std::get<0>(m_instanceFieldInitData[i]);
        Value value = std::get<1>(m_instanceFieldInitData[i]);
        size_t kind = std::get<2>(m_instanceFieldInitData[i]);
        switch (kind) {
        case ScriptClassConstructorFunctionObject::NotPrivate:
        case ScriptClassConstructorFunctionObject::PrivateFieldValue:
            if (!value.isUndefined()) {
                value = value.asPointerValue()->asScriptVirtualArrowFunctionObject()->call(state, Value(instance), homeObject);
            }
            break;
        default:
            break;
        }
        if (kind == ScriptClassConstructorFunctionObject::NotPrivate) {
            instance->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, name), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
        } else if (kind == ScriptClassConstructorFunctionObject::PrivateFieldValue) {
            instance->privateFieldAdd(state, AtomicString(state, name.asString()), value);
        } else if (kind == ScriptClassConstructorFunctionObject::PrivateFieldMethod) {
            instance->privateFieldAdd(state, AtomicString(state, name.asString()), value);
        } else if (kind == ScriptClassConstructorFunctionObject::PrivateFieldGetter) {
            instance->privateAccessorAdd(state, AtomicString(state, name.asString()), value.asFunction(), true, false);
        } else if (kind == ScriptClassConstructorFunctionObject::PrivateFieldSetter) {
            instance->privateAccessorAdd(state, AtomicString(state, name.asString()), value.asFunction(), false, true);
        } else {
            ASSERT_NOT_REACHED();
        }
    }
}

bool ScriptClassConstructorFunctionObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    bool ret = ScriptFunctionObject::deleteOwnProperty(state, P);
    if (ret && !P.isUIntType() && P.objectStructurePropertyName() == state.context()->staticStrings().name) {
        m_prototypeIndex = m_structure->findProperty(state.context()->staticStrings().name).first;
    }
    return ret;
}

} // namespace Escargot
