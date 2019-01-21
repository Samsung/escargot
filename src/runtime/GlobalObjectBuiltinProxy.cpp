/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
    auto strings = &state.context()->staticStrings();

    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: calling a builtin Proxy constructor without new is forbidden");
        return Value();
    }

    Value target = argv[0];
    Value handler = argv[1];

    return ProxyObject::createProxy(state, target, handler);
}

void GlobalObject::installProxy(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_proxy = new FunctionObject(state, NativeFunctionInfo(strings->Proxy, builtinProxyConstructor, 2, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                     return new Object(state);
                                 }),
                                 FunctionObject::__ForBuiltin__);
    m_proxy->markThisObjectDontNeedStructureTransitionTable(state);
    m_proxy->setPrototype(state, m_functionPrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Proxy),
                      ObjectPropertyDescriptor(m_proxy, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}

#endif // ESCARGOT_ENABLE_PROXY
