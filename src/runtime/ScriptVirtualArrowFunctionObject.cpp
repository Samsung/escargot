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
    m_prototype = homeObject;
    return FunctionObjectProcessCallGenerator::processCall<ScriptVirtualArrowFunctionObject, false, false, false, ScriptVirtualArrowFunctionObjectThisValueBinder, FunctionObjectNewTargetBinder, FunctionObjectReturnValueBinder>(state, this, thisValue, 0, nullptr, nullptr);
}

Value ScriptVirtualArrowFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This function cannot be invoked");
    ASSERT_NOT_REACHED();
    return Value();
}

Value ScriptVirtualArrowFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This function cannot be invoked");
    ASSERT_NOT_REACHED();
    return Value();
}
} // namespace Escargot
