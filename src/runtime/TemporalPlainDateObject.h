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

#ifndef __EscargotTemporalPlainDateObject__
#define __EscargotTemporalPlainDateObject__

#include "runtime/TemporalObject.h"

namespace Escargot {

struct TemporalPlainDateGetter {
    static Value era(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value eraYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value dayOfWeek(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value dayOfYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value weekOfYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value yearOfWeek(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value daysInWeek(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value daysInMonth(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value daysInYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value monthsInYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value inLeapYear(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
    static Value monthCode(ExecutionState& state, ISO8601::PlainDate plainDate, Calendar calendarID, UCalendar* ucalendar);
};

class TemporalPlainDateObject : public DerivedObject {
public:
    TemporalPlainDateObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar, bool checkBoundery = true);
    TemporalPlainDateObject(ExecutionState& state, Object* proto, std::pair<UCalendar*, Optional<ISO8601::PlainDate>> fieldResolveResult, Calendar calendar, bool checkBoundery = true);
    TemporalPlainDateObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar);

    virtual bool isTemporalPlainDateObject() const override
    {
        return true;
    }

    ISO8601::PlainDate plainDate() const
    {
        return *m_plainDate;
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

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
    String* toString(ExecutionState& state, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-temporaldatetostring
    static String* temporalDateToString(ISO8601::PlainDate plainDate, Calendar calendar, TemporalShowCalendarNameOption showCalendar);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
    bool equals(ExecutionState& state, Value other);

    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodate
    enum class AddDurationToDateOperation {
        Add,
        Subtract
    };
    TemporalPlainDateObject* addDurationToDate(ExecutionState& state, AddDurationToDateOperation operation, Value temporalDurationLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
    TemporalPlainDateObject* with(ExecutionState& state, Value temporalDateLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.withcalendar
    TemporalPlainDateObject* withCalendar(ExecutionState& state, Value calendarLike);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
    TemporalDurationObject* since(ExecutionState& state, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
    TemporalDurationObject* until(ExecutionState& state, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.compare
    static int compare(ExecutionState& state, Value one, Value two);
    static int compareISODate(ExecutionState& state, TemporalPlainDateObject* one, TemporalPlainDateObject* two);
    static ISO8601::PlainDate toPlainDate(ExecutionState& state, const ISO8601::Duration& duration);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
    TemporalPlainDateTimeObject* toPlainDateTime(ExecutionState& state, Value temporalTime);
    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainmonthday
    TemporalPlainMonthDayObject* toPlainMonthDay(ExecutionState& state);
    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainyearmonth
    TemporalPlainYearMonthObject* toPlainYearMonth(ExecutionState& state);
    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tozoneddatetime
    TemporalZonedDateTimeObject* toZonedDateTime(ExecutionState& state, Value item);


    ISO8601::PlainDate computeISODate(ExecutionState& state);

protected:
    // https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindate
    enum class DifferenceTemporalPlainDate {
        Until,
        Since
    };
    ISO8601::Duration differenceTemporalPlainDate(ExecutionState& state, DifferenceTemporalPlainDate operation, Value other, Value options);

    ISO8601::PlainDate* m_plainDate;
    Calendar m_calendarID;
    UCalendar* m_icuCalendar;
};

} // namespace Escargot

#endif
#endif
