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
#include "PointerValue.h"
#include "FunctionObject.h"
#include "ErrorObject.h"

namespace Escargot {

Value PointerValue::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(!isCallable());
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_NOT_Callable);
    ASSERT_NOT_REACHED();

    // never get here. but I add return statement for removing compile warning
    return Value(Value::EmptyValue);
}

Object* PointerValue::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    ASSERT(!isConstructor());
    if (isFunctionObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Not_Constructor_Function, asFunctionObject()->codeBlock()->functionName());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Not_Constructor);
    ASSERT_NOT_REACHED();

    // never get here. but I add return statement for removing compile warning
    return nullptr;
}
}
