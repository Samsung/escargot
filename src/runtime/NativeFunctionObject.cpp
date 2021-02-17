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
#include "NativeFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForGlobalBuiltin)
    : FunctionObject(state, state.context()->globalObject()->objectPrototype(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
    ASSERT(!NativeFunctionObject::isConstructor());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForBuiltinConstructor)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
    if (info.m_isConstructor) {
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
    }
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForBuiltinProxyConstructor)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    // The Proxy constructor does not have a prototype property
    m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionLength()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));

    ASSERT(NativeFunctionObject::isConstructor());
}

NativeFunctionObject::NativeFunctionObject(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info)
    : FunctionObject(structure, std::move(values), context->globalObject()->functionPrototype())
{
    m_codeBlock = new NativeCodeBlock(context, info);
}

bool NativeFunctionObject::isConstructor() const
{
    return nativeCodeBlock()->isNativeConstructor();
}

Value NativeFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(codeBlock()->isNativeCodeBlock());
    return processNativeFunctionCall<false>(state, thisValue, argc, argv, nullptr);
}

Value NativeFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget->isObject());
    ASSERT(newTarget->isConstructor());
    ASSERT(codeBlock()->isNativeCodeBlock());

    return processNativeFunctionCall<true>(state, Value(), argc, argv, newTarget);
}
} // namespace Escargot
