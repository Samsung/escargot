/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "Context.h"
#include "VMInstance.h"
#include "GlobalObject.h"
#include "StringObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "parser/CodeBlock.h"
#include "SandBox.h"
#include "ArrayObject.h"

namespace Escargot {

Context::Context(VMInstance* instance)
    : m_instance(instance)
    , m_atomicStringMap(&instance->m_atomicStringMap)
    , m_staticStrings(instance->m_staticStrings)
    , m_scriptParser(new ScriptParser(this))
    , m_compiledCodeBlocks(instance->m_compiledCodeBlocks)
    , m_bumpPointerAllocator(instance->m_bumpPointerAllocator)
    , m_regexpCache(&instance->m_regexpCache)
    , m_sandBoxStack(instance->m_sandBoxStack)
    , m_toStringRecursionPreventer(&instance->m_toStringRecursionPreventer)
{
    m_defaultStructureForObject = m_instance->m_defaultStructureForObject;
    m_defaultStructureForFunctionObject = m_instance->m_defaultStructureForFunctionObject;
    m_defaultStructureForClassFunctionObject = m_instance->m_defaultStructureForClassFunctionObject;
    m_defaultStructureForArrowFunctionObject = m_instance->m_defaultStructureForArrowFunctionObject;
    m_defaultStructureForNotConstructorFunctionObject = m_instance->m_defaultStructureForNotConstructorFunctionObject;
    m_defaultStructureForBuiltinFunctionObject = m_instance->m_defaultStructureForBuiltinFunctionObject;
    m_defaultStructureForFunctionPrototypeObject = m_instance->m_defaultStructureForFunctionPrototypeObject;
    m_defaultStructureForBoundFunctionObject = m_instance->m_defaultStructureForBoundFunctionObject;
    m_defaultStructureForArrayObject = m_instance->m_defaultStructureForArrayObject;
    m_defaultStructureForStringObject = m_instance->m_defaultStructureForStringObject;
    m_defaultStructureForSymbolObject = m_instance->m_defaultStructureForSymbolObject;
    m_defaultStructureForRegExpObject = m_instance->m_defaultStructureForRegExpObject;
    m_defaultStructureForArgumentsObject = m_instance->m_defaultStructureForArgumentsObject;
    m_defaultStructureForArgumentsObjectInStrictMode = m_instance->m_defaultStructureForArgumentsObjectInStrictMode;
    m_jobQueue = instance->m_jobQueue;

    m_virtualIdentifierCallback = nullptr;
    m_securityPolicyCheckCallback = nullptr;
    m_virtualIdentifierCallbackPublic = nullptr;
    m_securityPolicyCheckCallbackPublic = nullptr;

    ExecutionState stateForInit(this);
    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);

    auto temp = new ArrayObject(stateForInit);
    g_arrayObjectTag = *((size_t*)temp);
}

void Context::throwException(ExecutionState& state, const Value& exception)
{
    if (m_sandBoxStack.size()) {
        m_sandBoxStack.back()->throwException(state, exception);
    } else {
        ESCARGOT_LOG_ERROR("there is no sandbox but exception occurred");
        RELEASE_ASSERT_NOT_REACHED();
    }
}
}
