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

class TemporalPlainDateObject : public DerivedObject {
public:
    TemporalPlainDateObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar);
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

    Value era(ExecutionState& state);
    Value eraYear(ExecutionState& state);
    Value dayOfWeek(ExecutionState& state);
    Value dayOfYear(ExecutionState& state);
    Value weekOfYear(ExecutionState& state);
    Value yearOfWeek(ExecutionState& state);
    Value daysInWeek(ExecutionState& state);
    Value daysInMonth(ExecutionState& state);
    Value daysInYear(ExecutionState& state);
    Value monthsInYear(ExecutionState& state);
    Value inLeapYear(ExecutionState& state);
    Value monthCode(ExecutionState& state);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
    String* toString(ExecutionState& state, Value options);

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

private:
    ISO8601::PlainDate computeISODate(ExecutionState& state);

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
