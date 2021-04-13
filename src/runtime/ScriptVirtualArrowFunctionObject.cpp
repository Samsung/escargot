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
#include "ScriptVirtualArrowFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

class ScriptVirtualArrowFunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& callerState, ExecutionState& calleeState, ScriptVirtualArrowFunctionObject* self, const Value& thisValue, bool isStrict)
    {
        return thisValue;
    }
};

Value ScriptVirtualArrowFunctionObject::call(ExecutionState& state, const Value& thisValue, Object* homeObject)
{
    // we should retain homeObject because sub functions can use this later
    ASSERT(m_prototype == nullptr || m_prototype == homeObject);
    m_prototype = homeObject;

    if (interpretedCodeBlock()->parameterCount()) {
        // virtual parameter for static field initialization
        // should have only one parameter which is for class constructor
        ASSERT(interpretedCodeBlock()->parameterCount() == 1 && !!homeObject);
        Value arg = homeObject;
        return FunctionObjectProcessCallGenerator::processCall<ScriptVirtualArrowFunctionObject, false, false, false, ScriptVirtualArrowFunctionObjectThisValueBinder, FunctionObjectNewTargetBinder, FunctionObjectReturnValueBinder>(state, this, thisValue, 1, &arg, nullptr);
    } else {
        return FunctionObjectProcessCallGenerator::processCall<ScriptVirtualArrowFunctionObject, false, false, false, ScriptVirtualArrowFunctionObjectThisValueBinder, FunctionObjectNewTargetBinder, FunctionObjectReturnValueBinder>(state, this, thisValue, 0, nullptr, nullptr);
    }
}

Value ScriptVirtualArrowFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This function cannot be invoked");
    ASSERT_NOT_REACHED();
    return Value();
}

Value ScriptVirtualArrowFunctionObject::construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This function cannot be invoked");
    ASSERT_NOT_REACHED();
    return Value();
}
} // namespace Escargot
