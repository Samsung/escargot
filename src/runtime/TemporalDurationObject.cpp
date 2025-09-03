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
#include "TemporalDurationObject.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"

namespace Escargot {

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, Object* proto,
                                               const Value& years, const Value& months, const Value& weeks, const Value& days,
                                               const Value& hours, const Value& minutes, const Value& seconds, const Value& milliseconds,
                                               const Value& microseconds, const Value& nanoseconds)
    : DerivedObject(state, proto)
{
    // https://tc39.es/proposal-temporal/#sec-temporal.duration
    // If NewTarget is undefined, throw a TypeError exception.
    // If years is undefined, let y be 0; else let y be ? ToIntegerIfIntegral(years).
    // If months is undefined, let mo be 0; else let mo be ? ToIntegerIfIntegral(months).
    // If weeks is undefined, let w be 0; else let w be ? ToIntegerIfIntegral(weeks).
    // If days is undefined, let d be 0; else let d be ? ToIntegerIfIntegral(days).
    // If hours is undefined, let h be 0; else let h be ? ToIntegerIfIntegral(hours).
    // If minutes is undefined, let m be 0; else let m be ? ToIntegerIfIntegral(minutes).
    // If seconds is undefined, let s be 0; else let s be ? ToIntegerIfIntegral(seconds).
    // If milliseconds is undefined, let ms be 0; else let ms be ? ToIntegerIfIntegral(milliseconds).
    // If microseconds is undefined, let mis be 0; else let mis be ? ToIntegerIfIntegral(microseconds).
    // If nanoseconds is undefined, let ns be 0; else let ns be ? ToIntegerIfIntegral(nanoseconds).
    // Return ? CreateTemporalDuration(y, mo, w, d, h, m, s, ms, mis, ns, NewTarget).
#define DEFINE_SETTER(name, Name, names, Names, index, category) \
    if (!names.isUndefined()) {                                  \
        m_duration[index] = names.toIntegerIfIntergral(state);   \
    }
    PLAIN_DATETIME_UNITS(DEFINE_SETTER)
#undef DEFINE_SETTER
}

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, const ISO8601::Duration& duration)
    : DerivedObject(state, state.context()->globalObject()->temporalDurationPrototype())
    , m_duration(duration)
{
}

} // namespace Escargot

#endif
