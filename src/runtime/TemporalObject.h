#if defined(ESCARGOT_ENABLE_TEMPORAL)
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

#ifndef __EscargotTemporalObject__
#define __EscargotTemporalObject__

#include "Escargot.h"
#include "runtime/Object.h"
#include "runtime/GlobalObject.h"
#include "runtime/Temporal.h"

namespace Escargot {

static int commonMonthOffsetLookUpTable[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
static int leapMonthOffsetLookUpTable[12] = { 0, 3, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6 };
static int monthDayLookUpTable[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

class TemporalObject : public Temporal {
public:
    explicit TemporalObject(ExecutionState& state);
    explicit TemporalObject(ExecutionState& state, Object* proto);
    static Value toISODateTime(ExecutionState& state, DateObject& d);
    static Value toISODate(ExecutionState& state, DateObject& d);
    static Value toISOTime(ExecutionState& state, DateObject& d);
    static std::map<std::string, std::string> parseValidIso8601String(ExecutionState& state, const Value& str);
};

class TemporalCalendar : public Temporal {
public:
    explicit TemporalCalendar(ExecutionState& state);
    explicit TemporalCalendar(ExecutionState& state, Object* proto);

    bool isTemporalCalendarObject() const override
    {
        return true;
    }

    static TemporalCalendar* createTemporalCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget = Optional<Object*>());
    static bool isBuiltinCalendar(String* id);
    static Value getBuiltinCalendar(ExecutionState& state, String* id);
    static Value getISO8601Calendar(ExecutionState& state);
    static Value toTemporalCalendar(ExecutionState& state, const Value& calendar);
    static Value toTemporalCalendarWithISODefault(ExecutionState& state, const Value& calendar);
    static Value parseTemporalCalendarString(ExecutionState& state, const Value& isoString);
    static Value ISODaysInMonth(ExecutionState& state, const Value& year, const Value& month);
    static bool isIsoLeapYear(ExecutionState& state, const Value& year);
    static Value defaultMergeFields(ExecutionState& state, const Value& fields, const Value& additionalFields);
    String* getIdentifier() const;
    void setIdentifier(String* identifier);

private:
    String* m_identifier;
};

class TemporalPlainDate : public Temporal {
public:
    explicit TemporalPlainDate(ExecutionState& state, Value isoYear, Value isoMonth, Value isoDay, Optional<Value> calendarLike);
    explicit TemporalPlainDate(ExecutionState& state, Object* proto, Value isoYear, Value isoMonth, Value isoDay, Optional<Value> calendarLike);

    static Value createTemporalDate(ExecutionState& state, const Value& isoYear, const Value& isoMonth, const Value& isoDay, const Value& calendar, Optional<Object*> newTarget);
    static bool isValidISODate(ExecutionState& state, const Value& year, const Value& month, const Value& day);

    bool hasInitializedTemporalDate() const
    {
        return true;
    }

    int year() const { return m_y; }
    int month() const { return m_m; }
    int day() const { return m_d; }

private:
    int m_y;
    int m_m;
    int m_d;
    Value m_calendar;
};

class TemporalPlainTime : public Temporal {
public:
    explicit TemporalPlainTime(ExecutionState& state);
    explicit TemporalPlainTime(ExecutionState& state, Object* proto);

    static Value createTemporalTime(ExecutionState& state, size_t argc, Value* argv, Optional<Object*> newTarget);
    static bool isValidTime(ExecutionState& state, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond);

    void setTime(char h, char m, char s, short ms, short qs, short ns);
    void setCalendar(ExecutionState& state, String* calendar, Optional<Object*> newTarget);


private:
    short m_year;
    char m_month;
    char m_day;
    char m_hour;
    char m_minute;
    char m_second;
    short m_millisecond;
    short m_microsecond;
    short m_nanosecond;

    TemporalCalendar* calendar;
};

class TemporalPlainDateTime : public Temporal {
public:
    explicit TemporalPlainDateTime(ExecutionState& state);
    explicit TemporalPlainDateTime(ExecutionState& state, Object* proto);

    static Value getEpochFromISOParts(ExecutionState& state, const Value& year, const Value& month, const Value& day, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond);
    static bool ISODateTimeWithinLimits(ExecutionState& state, const Value& year, const Value& month, const Value& day, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond);
};

} // namespace Escargot

#endif
#endif
