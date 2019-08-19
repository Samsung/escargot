/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "ScriptGeneratorFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

class ScriptGeneratorFunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& calleeState, ScriptGeneratorFunctionObject* self, const Value& thisArgument, bool isStrict)
    {
        Value thisValue = self->thisValue();
        if (thisValue.isEmpty()) {
            // OrdinaryCallBindThis ( F, calleeContext, thisArgument )
            // Let thisMode be the value of F’s [[ThisMode]] internal slot.
            // If thisMode is lexical, return NormalCompletion(undefined).
            // --> thisMode is always not lexcial because this is class ctor.
            // Let calleeRealm be the value of F’s [[Realm]] internal slot.
            // Let localEnv be the LexicalEnvironment of calleeContext.
            ASSERT(calleeState.context() == self->codeBlock()->context());

            if (isStrict) {
                // If thisMode is strict, let thisValue be thisArgument.
                return thisArgument;
            } else {
                // Else
                // if thisArgument is null or undefined, then
                // Let thisValue be calleeRealm.[[globalThis]]
                if (thisArgument.isUndefinedOrNull()) {
                    return calleeState.context()->globalObject();
                } else {
                    // Else
                    // Let thisValue be ToObject(thisArgument).
                    // Assert: thisValue is not an abrupt completion.
                    // NOTE ToObject produces wrapper objects using calleeRealm.
                    return thisArgument.toObject(calleeState);
                }
            }
        } else {
            ASSERT(self->codeBlock()->isArrowFunctionExpression());
            return thisValue;
        }
    }
};

Value ScriptGeneratorFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    return FunctionObjectProcessCallGenerator::processCall<ScriptGeneratorFunctionObject, true, false, false, false, ScriptGeneratorFunctionObjectThisValueBinder, FunctionObjectNewTargetBinder, FunctionObjectReturnValueBinder>(state, this, thisValue, argc, argv, nullptr);
}

Object* ScriptGeneratorFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Generator cannot be invoked with 'new'");
    ASSERT_NOT_REACHED();
    return nullptr;
}
}
