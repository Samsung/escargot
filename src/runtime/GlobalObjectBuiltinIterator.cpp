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

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

static Value builtinIteratorIterator(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return thisValue;
}

void GlobalObject::installIterator(ExecutionState& state)
{
    m_iteratorPrototype = new Object(state);
    m_iteratorPrototype->markThisObjectDontNeedStructureTransitionTable(state);

    FunctionObject* fn = new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.iterator]")), builtinIteratorIterator, 0, nullptr, NativeFunctionInfo::Strict));
    m_iteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                          ObjectPropertyDescriptor(fn,
                                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
