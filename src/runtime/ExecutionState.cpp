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
#include "ExecutionState.h"
#include "ExecutionContext.h"
#include "Context.h"

namespace Escargot {

void ExecutionState::throwException(const Value& e)
{
    context()->throwException(*this, e);
}

ExecutionState* ExecutionState::parent()
{
    if (m_parent & 1) {
        return (ExecutionState*)(m_parent - 1);
    } else {
        return rareData()->m_parent;
    }
}

ExecutionStateRareData* ExecutionState::ensureRareData()
{
    if (m_parent & 1) {
        ExecutionState* p = parent();
        m_rareData = new ExecutionStateRareData();
        m_rareData->m_parent = p;
    }
    return rareData();
}
}
