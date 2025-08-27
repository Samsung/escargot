#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#include "TemporalInstantObject.h"

namespace Escargot {

TemporalInstantObject::TemporalInstantObject(ExecutionState& state, Object* proto, Int128 n)
    : DerivedObject(state, proto)
    , m_nanoseconds(new(PointerFreeGC) Int128(n))
{
}

Value TemporalInstantObject::epochMilliseconds() const
{
    Int128 s = *m_nanoseconds;
    s /= 1000000;
    return Value(static_cast<int64_t>(s));
}

TemporalInstantObject* TemporalInstantObject::addDurationToInstant(AddDurationOperation operation, const Value& temporalDurationLike)
{
    TemporalInstantObject* instant = this;
    // TODO
    return nullptr;
}

} // namespace Escargot

#endif
