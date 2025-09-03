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

#ifndef __EscargotTemporalInstantObject__
#define __EscargotTemporalInstantObject__

#include "runtime/TemporalObject.h"

namespace Escargot {

class TemporalInstantObject : public DerivedObject {
public:
    TemporalInstantObject(ExecutionState& state, Object* proto, Int128 nanoseconds);

    virtual bool isTemporalInstantObject() const override
    {
        return true;
    }

    Value epochMilliseconds() const;
    Int128 epochNanoseconds() const
    {
        return *m_nanoseconds;
    }

    String* toString(ExecutionState& state, Value options);
    static String* toString(ExecutionState& state, Int128 epochNanoseconds, TimeZone timeZone, Value precision);
    TemporalInstantObject* round(ExecutionState& state, Value roundTo);

private:
    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoinstant
    enum class AddDurationOperation {
        Add,
        Subtract
    };
    TemporalInstantObject* addDurationToInstant(AddDurationOperation operation, const Value& temporalDurationLike);

    Int128* m_nanoseconds; // [[EpochNanoseconds]]
};

} // namespace Escargot

#endif
#endif
