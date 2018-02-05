/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#if ESCARGOT_ENABLE_PROXY

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "ProxyObject.h"
#include "ArrayObject.h"
#include "JobQueue.h"
#include "SandBox.h"

namespace Escargot {

// $26.2.1 The Proxy Constructor
static Value builtinProxyConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // TODO: Implement a wrapped Object.
    return Value();
}

void GlobalObject::installProxy(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_proxy = new FunctionObject(state, NativeFunctionInfo(strings->Proxy, builtinProxyConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                     return new ProxyObject(state);
			                     }),
			                     FunctionObject::__ForBuiltin__);
    m_proxy->markThisObjectDontNeedStructureTransitionTable(state);
    m_proxy->setPrototype(state, m_functionPrototype);
    m_proxyPrototype = m_objectPrototype;
    m_proxyPrototype = new ProxyObject(state);
    m_proxyPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_proxyPrototype->setPrototype(state, m_objectPrototype);
    m_promisePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // TODO: 26.2.2.1 Implement revocable
    
    defineOwnProperty(state, ObjectPropertyName(strings->Proxy),
                      ObjectPropertyDescriptor(m_proxy, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

}

}

#endif // ESCARGOT_ENABLE_PROXY
