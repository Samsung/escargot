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
#include "BuiltinFunctionObject.h"
#include "Context.h"
#include "parser/CodeBlock.h"

namespace Escargot {

BuiltinFunctionObject::BuiltinFunctionObject(ExecutionState& state, CodeBlock* codeBlock)
    : NativeFunctionObject(state, codeBlock->isConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = codeBlock;
    m_homeObject = nullptr;

    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    if (isConstructor())
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(FunctionObject::codeBlock()->hasCallNativeFunctionCode());
}

BuiltinFunctionObject::BuiltinFunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : NativeFunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3)
{
    m_codeBlock = new CodeBlock(state.context(), info);
    m_homeObject = nullptr;

    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(isConstructor());
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
}

BuiltinFunctionObject::BuiltinFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinProxyConstructor)
    : NativeFunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new CodeBlock(state.context(), info);
    m_homeObject = nullptr;

    // The Proxy constructor does not have a prototype property
    m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->parameterCount()));
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());

    ASSERT(isConstructor());
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
}
}
