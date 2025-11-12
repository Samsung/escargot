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

#ifndef __EscargotTemporalZonedDateTimeObject__
#define __EscargotTemporalZonedDateTimeObject__

#include "runtime/TemporalObject.h"
#include "runtime/TemporalPlainDateObject.h"
#include "util/ISO8601.h"

namespace Escargot {

class TemporalZonedDateTimeObject : public DerivedObject {
public:
    class ComputedTimeZone {
    public:
        ComputedTimeZone(bool hasOffsetTimeZoneName = false, String* timeZoneName = String::emptyString(), int64_t offset = 0)
            : m_hasOffsetTimeZoneName(hasOffsetTimeZoneName)
            , m_timeZoneName(timeZoneName)
            , m_offset(offset)
        {
        }

        bool hasOffsetTimeZoneName() const
        {
            return m_hasOffsetTimeZoneName;
        }

        String* timeZoneName() const
        {
            ASSERT(m_timeZoneName->length());
            return m_timeZoneName;
        }

        int64_t offset() const
        {
            return m_offset;
        }

        bool equals(const ComputedTimeZone& src) const
        {
            return m_timeZoneName->equals(src.timeZoneName()) && offset() == src.offset();
        }

        operator TimeZone() const
        {
            if (hasOffsetTimeZoneName()) {
                return TimeZone(offset());
            } else {
                return TimeZone(m_timeZoneName);
            }
        }

    private:
        bool m_hasOffsetTimeZoneName;
        String* m_timeZoneName;
        int64_t m_offset;
    };

    TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, TimeZone timeZone, Calendar calendar);
    TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, ComputedTimeZone timeZone, Calendar calendar);

    virtual bool isTemporalZonedDateTimeObject() const override
    {
        return true;
    }

    ISO8601::PlainDate plainDate() const
    {
        return m_plainDateTime->plainDate();
    }

    ISO8601::PlainTime plainTime() const
    {
        return m_plainDateTime->plainTime();
    }

    ISO8601::PlainDateTime plainDateTime() const
    {
        return *m_plainDateTime;
    }

    Int128 epochNanoseconds() const
    {
        return *m_epochNanoseconds;
    }

    Calendar calendarID() const
    {
        return m_calendarID;
    }

    ComputedTimeZone timeZone() const
    {
        return m_timeZone;
    }

    Value era(ExecutionState& state)
    {
        return TemporalPlainDateGetter::era(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value eraYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::eraYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value dayOfWeek(ExecutionState& state)
    {
        return TemporalPlainDateGetter::dayOfWeek(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value dayOfYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::dayOfYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value weekOfYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::weekOfYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value yearOfWeek(ExecutionState& state)
    {
        return TemporalPlainDateGetter::yearOfWeek(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value daysInWeek(ExecutionState& state)
    {
        return TemporalPlainDateGetter::daysInWeek(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value daysInMonth(ExecutionState& state)
    {
        return TemporalPlainDateGetter::daysInMonth(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value daysInYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::daysInYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value monthsInYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::monthsInYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value inLeapYear(ExecutionState& state)
    {
        return TemporalPlainDateGetter::inLeapYear(state, plainDate(), calendarID(), m_icuCalendar);
    }
    Value monthCode(ExecutionState& state)
    {
        return TemporalPlainDateGetter::monthCode(state, plainDate(), calendarID(), m_icuCalendar);
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
    String* toString(ExecutionState& state, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
    bool equals(ExecutionState& state, Value otherInput);

    // https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
    int hoursInDay(ExecutionState& state);

    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
    TemporalInstantObject* toInstant(ExecutionState& state);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
    TemporalPlainDateObject* toPlainDate(ExecutionState& state);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
    TemporalPlainTimeObject* toPlainTime(ExecutionState& state);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
    TemporalPlainDateTimeObject* toPlainDateTime(ExecutionState& state);

    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
    TemporalZonedDateTimeObject* with(ExecutionState& state, Value temporalZonedDateTimeLike, Value options);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
    TemporalZonedDateTimeObject* withPlainTime(ExecutionState& state, Value temporalTimeLike);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
    TemporalZonedDateTimeObject* withTimeZone(ExecutionState& state, Value timeZoneLike);
    // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
    TemporalZonedDateTimeObject* withCalendar(ExecutionState& state, Value calendarLike);

    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtozoneddatetime
    enum class AddDurationToZonedDateTimeOperation {
        Add,
        Subtract
    };
    TemporalZonedDateTimeObject* addDurationToZonedDateTime(ExecutionState& state, AddDurationToZonedDateTimeOperation operation, Value temporalDurationLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalzoneddatetime
    enum class DifferenceTemporalZonedDateTime {
        Until,
        Since
    };
    TemporalDurationObject* differenceTemporalZonedDateTime(ExecutionState& state, DifferenceTemporalZonedDateTime operation, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetimewithrounding
    ISO8601::InternalDuration differenceZonedDateTimeWithRounding(ExecutionState& state, Int128 ns1, Int128 ns2, TimeZone timeZone, Calendar calendar,
                                                                  ISO8601::DateTimeUnit largestUnit, unsigned roundingIncrement, ISO8601::DateTimeUnit smallestUnit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetime
    ISO8601::InternalDuration differenceZonedDateTime(ExecutionState& state, Int128 ns1, Int128 ns2, TimeZone timeZone, Calendar calendar, ISO8601::DateTimeUnit largestUnit);

private:
    void init(ExecutionState& state, ComputedTimeZone timeZone);
    Int128* m_epochNanoseconds;
    ISO8601::PlainDateTime* m_plainDateTime; // stores timezone applied value
    ComputedTimeZone m_timeZone;
    Calendar m_calendarID;
    UCalendar* m_icuCalendar;
};

} // namespace Escargot

#endif
#endif
