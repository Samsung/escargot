/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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
#include "PrototypeObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

PrototypeObject::PrototypeObject(ExecutionState& state)
    : DerivedObject(state)
{
}

PrototypeObject::PrototypeObject(ExecutionState& state, Object* proto, size_t defaultSpace)
    : DerivedObject(state, proto, defaultSpace)
{
}

PrototypeObject::PrototypeObject(ExecutionState& state, ForGlobalBuiltin)
    : DerivedObject(state, Object::PrototypeIsNull)
{
}

void PrototypeObject::markAsPrototypeObject(ExecutionState& state)
{
    // this is the hottest marking site in the engine: every `new F()` re-marks
    // F.prototype, and an ordinary object used as a prototype is vtag-rewritten
    // into this class, so it lands here too. no class in this hierarchy answers
    // an array index from anywhere but its ObjectStructure, so the virtual
    // hasIndexedPropertyOutsideStructure() query is a provable false here and
    // must not be paid -- an indirect call per construction is not free
    ASSERT(!hasIndexedPropertyOutsideStructure());
    if (UNLIKELY(structure()->hasIndexPropertyName() && !isIndexedPropertyDirtyAsPrototype())) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state, this, true);
    }
}

bool ImmutablePrototypeObject::setPrototype(ExecutionState& state, const Value& proto)
{
    // Let current be ? O.[[GetPrototypeOf]]().
    Value current = getPrototype(state);
    // If SameValue(V, current) is true, return true.
    if (current == proto) {
        return true;
    }

    // Return false.
    return false;
}

} // namespace Escargot
