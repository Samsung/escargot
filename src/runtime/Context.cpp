/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    m_defaultStructureForNotConstructorFunctionObject = m_instance->m_defaultStructureForNotConstructorFunctionObject;
    m_defaultStructureForFunctionObjectInStrictMode = m_instance->m_defaultStructureForFunctionObjectInStrictMode;
    m_defaultStructureForNotConstructorFunctionObjectInStrictMode = m_instance->m_defaultStructureForNotConstructorFunctionObjectInStrictMode;
    m_defaultStructureForBuiltinFunctionObject = m_instance->m_defaultStructureForBuiltinFunctionObject;
    m_defaultStructureForFunctionPrototypeObject = m_instance->m_defaultStructureForFunctionPrototypeObject;
    m_defaultStructureForBindedFunctionObject = m_instance->m_defaultStructureForBindedFunctionObject;
    m_defaultStructureForArrayObject = m_instance->m_defaultStructureForArrayObject;
    m_defaultStructureForStringObject = m_instance->m_defaultStructureForStringObject;
    m_defaultStructureForRegExpObject = m_instance->m_defaultStructureForRegExpObject;
    m_defaultStructureForArgumentsObject = m_instance->m_defaultStructureForArgumentsObject;
    m_defaultStructureForArgumentsObjectInStrictMode = m_instance->m_defaultStructureForArgumentsObjectInStrictMode;
#if ESCARGOT_ENABLE_PROMISE
    m_jobQueue = instance->m_jobQueue;
#endif
    m_virtualIdentifierCallback = nullptr;
    m_virtualIdentifierCallbackPublic = nullptr;

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
