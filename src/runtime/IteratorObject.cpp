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

#include "Escargot.h"
#include "IteratorObject.h"
#include "Context.h"

namespace Escargot {

IteratorObject::IteratorObject(ExecutionState& state)
    : Object(state)
{
}

Value IteratorObject::next(ExecutionState& state)
{
    auto result = advance(state);
    Object* r = new Object(state);

    r->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(result.first, ObjectPropertyDescriptor::AllPresent));
    r->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().done), ObjectPropertyDescriptor(Value(result.second), ObjectPropertyDescriptor::AllPresent));
    return r;
}
}
