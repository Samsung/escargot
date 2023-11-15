/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "ToStringRecursionPreventer.h"
#include "Context.h"

namespace Escargot {

ToStringRecursionPreventerItemAutoHolder::ToStringRecursionPreventerItemAutoHolder(ExecutionState& state, Object* obj)
    : m_preventer(state.context()->toStringRecursionPreventer())
{
    CHECK_STACK_OVERFLOW(state);
    m_preventer->pushItem(obj);
#ifndef NDEBUG
    m_object = obj;
#endif
}

ToStringRecursionPreventerItemAutoHolder::~ToStringRecursionPreventerItemAutoHolder()
{
#ifdef NDEBUG
    m_preventer->pop();
#else
    ASSERT(m_preventer->pop() == m_object);
#endif
}
} // namespace Escargot
