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
#include "PointerValue.h"
#include "FunctionObject.h"
#include "ErrorObject.h"

namespace Escargot {

size_t PointerValue::g_objectTag;
size_t PointerValue::g_prototypeObjectTag;
size_t PointerValue::g_arrayObjectTag;
size_t PointerValue::g_arrayPrototypeObjectTag;
size_t PointerValue::g_scriptFunctionObjectTag;
size_t PointerValue::g_objectRareDataTag;
// tag values for ScriptSimpleFunctionObject
#define DEFINE_SCRIPTSIMPLEFUNCTION_TAGS(STRICT, CLEAR, isStrict, isClear, SIZE) \
    size_t PointerValue::g_scriptSimpleFunctionObject##STRICT##CLEAR##SIZE##Tag;

DECLARE_SCRIPTSIMPLEFUNCTION_LIST(DEFINE_SCRIPTSIMPLEFUNCTION_TAGS);
#undef DEFINE_SCRIPTSIMPLEFUNCTION_TAGS

Value PointerValue::call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv)
{
    ASSERT(!isCallable());
    THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
}

Value PointerValue::construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget)
{
    ASSERT(!isConstructor());
    if (isFunctionObject()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::Not_Constructor_Function, asFunctionObject()->codeBlock()->functionName());
    }
    THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::Not_Constructor);
}
} // namespace Escargot
