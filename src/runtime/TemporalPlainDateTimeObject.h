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

#ifndef __EscargotTemporalPlainDateTimeObject__
#define __EscargotTemporalPlainDateTimeObject__

#include "runtime/TemporalObject.h"
#include "runtime/TemporalPlainDateObject.h"

namespace Escargot {

class TemporalPlainDateTimeObject : public DerivedObject {
public:
    TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainDate isoDate, ISO8601::PlainTime plainTime, Calendar calendar);
    TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, unsigned underMicrosecondValue, Calendar calendar);

    virtual bool isTemporalPlainDateTimeObject() const override
    {
        return true;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
    String* toString(ExecutionState& state, Value options);

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

    Calendar calendarID() const
    {
        return m_calendarID;
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

    ISO8601::PlainDate computeISODate(ExecutionState& state);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
    TemporalPlainDateTimeObject* with(ExecutionState& state, Value temporalDateTimeLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withplaintime
    TemporalPlainDateTimeObject* withPlainTime(ExecutionState& state, Value plainTimeLike);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withcalendar
    TemporalPlainDateTimeObject* withCalendar(ExecutionState& state, Value calendarLike);

    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodatetime
    enum class AddDurationToDateTimeOperation {
        Add,
        Subtract
    };
    TemporalPlainDateTimeObject* addDurationToDateTime(ExecutionState& state, AddDurationToDateTimeOperation operation, Value temporalDurationLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindatetime
    enum class DifferenceTemporalPlainDateTime {
        Until,
        Since
    };
    ISO8601::Duration differenceTemporalPlainDateTime(ExecutionState& state, DifferenceTemporalPlainDateTime operation, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
    TemporalPlainDateTimeObject* round(ExecutionState& state, Value roundToInput);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.equals
    bool equals(ExecutionState& state, Value otherInput);

    TemporalPlainDateObject* toPlainDate(ExecutionState& state);
    TemporalPlainTimeObject* toPlainTime(ExecutionState& state);
    TemporalZonedDateTimeObject* toZonedDateTime(ExecutionState& state, Value temporalTimeZoneLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.compare
    static int compare(ExecutionState& state, Value one, Value two);

private:
    ISO8601::PlainDateTime* m_plainDateTime;
    Calendar m_calendarID;
    UCalendar* m_icuCalendar;
};

} // namespace Escargot

#endif
#endif
