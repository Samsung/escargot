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

#ifndef __EscargotTemporalDurationObject__
#define __EscargotTemporalDurationObject__

#include "runtime/TemporalObject.h"
#include "util/ISO8601.h"

namespace Escargot {

class TemporalDurationObject : public DerivedObject {
public:
    TemporalDurationObject(ExecutionState& state, Object* proto,
                           const Value& years, const Value& months, const Value& weeks, const Value& days,
                           const Value& hours, const Value& minutes, const Value& seconds, const Value& milliseconds,
                           const Value& microseconds, const Value& nanoseconds);

    TemporalDurationObject(ExecutionState& state, const ISO8601::Duration& duration);

    // https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationfrominternal
    static ISO8601::Duration temporalDurationFromInternal(ExecutionState& state, ISO8601::InternalDuration internalDuration, ISO8601::DateTimeUnit largestUnit);

    // https://tc39.es/proposal-temporal/#sec-temporal-createnegatedtemporalduration
    static ISO8601::Duration createNegatedTemporalDuration(ISO8601::Duration duration);

    virtual bool isTemporalDurationObject() const override
    {
        return true;
    }

    const ISO8601::Duration& duration() const
    {
        return m_duration;
    }

    double operator[](size_t idx) const
    {
        return m_duration[static_cast<unsigned>(idx)];
    }

    double operator[](ISO8601::DateTimeUnit idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    double& operator[](size_t idx)
    {
        return m_duration[static_cast<unsigned>(idx)];
    }

    double& operator[](ISO8601::DateTimeUnit idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    // https://tc39.es/proposal-temporal/#sec-durationsign
    int sign() const
    {
        return m_duration.sign();
    }

    String* toString(ExecutionState& state, Value options);
    static String* temporalDurationToString(ISO8601::Duration duration, Value precision);

    // https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecord
    static ISO8601::InternalDuration toInternalDurationRecord(ISO8601::Duration duration);

    // https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays
    static ISO8601::InternalDuration toInternalDurationRecordWith24HourDays(ExecutionState& state, ISO8601::Duration duration);

    // https://tc39.es/proposal-temporal/#sec-temporal-roundtimeduration
    static Int128 roundTimeDuration(ExecutionState& state, Int128 timeDuration, unsigned increment, ISO8601::DateTimeUnit unit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-roundtimedurationtoincrement
    static Int128 roundTimeDurationToIncrement(ExecutionState& state, Int128 d, Int128 increment, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-timedurationfromcomponents
    static Int128 timeDurationFromComponents(ExecutionState& state, double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds);

    // https://tc39.es/proposal-temporal/#sec-temporal-add24hourdaystonormalizedtimeduration
    static Int128 add24HourDaysToTimeDuration(ExecutionState& state, Int128 d, double days);

private:
    ISO8601::Duration m_duration;
};

} // namespace Escargot

#endif
#endif
