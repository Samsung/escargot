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
