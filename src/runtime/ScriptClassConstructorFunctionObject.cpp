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

namespace Escargot {
ScriptClassConstructorFunctionObject::ScriptClassConstructorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, Object* homeObject, String* classSourceCode)
    : ScriptFunctionObject(state, proto, codeBlock, outerEnvironment, 2)
    , m_homeObject(homeObject)
    , m_classSourceCode(classSourceCode)
{
    ASSERT(!codeBlock->isGenerator()); // Class constructor may not be a generator

    m_structure = state.context()->defaultStructureForClassConstructorFunctionObject();

    ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
    ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionLength()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
}

Value ScriptClassConstructorFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
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
            calleeState.lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->bindThisValue(calleeState, thisArgument);
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


Object* ScriptClassConstructorFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    // Assert: Type(newTarget) is Object.
    CodeBlock* cb = codeBlock();
    FunctionObject* constructor = this;

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
}
