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
#include "runtime/DateObject.h"

namespace Escargot {

static int commonMonthOffsetLookUpTable[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
static int leapMonthOffsetLookUpTable[12] = { 0, 3, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6 };
static int monthDayLookUpTable[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

class TemporalDate {
public:
    TemporalDate(short mYear, char mMonth, char mDay);
    short m_year;
    char m_month;
    char m_day;
};

class TemporalTime {
public:
    TemporalTime(char mHour, char mMinute, char mSecond, short mMillisecond, short mMicrosecond, short mNanosecond);
    TemporalTime();
    char m_hour;
    char m_minute;
    char m_second;
    short m_millisecond;
    short m_microsecond;
    short m_nanosecond;
};

class TemporalObject : public Temporal {
public:
    struct DateTime {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int millisecond;
        int microsecond;
        int nanosecond;
        std::string calendar;
    };

    explicit TemporalObject(ExecutionState& state);
    explicit TemporalObject(ExecutionState& state, Object* proto);
    static Value toISODateTime(ExecutionState& state, DateObject& d);
    static Value toISODate(ExecutionState& state, DateObject& d);
    static Value toISOTime(ExecutionState& state, DateObject& d);
    static TemporalObject::DateTime parseValidIso8601String(ExecutionState& state, std::string isoString);
    static TemporalObject::DateTime parseTemporalDateString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalDateTimeString(ExecutionState& state, const std::string& isoString);
    static std::map<std::string, int> parseTemporalDurationString(ExecutionState& state, const std::string& isoString);
    static std::string getNNumberFromString(std::string& isoString, int n, unsigned int& index);
    static std::map<std::string, int> getSeconds(ExecutionState& state, std::string& isoString, unsigned int& index);
    static void offset(ExecutionState& state, std::string& isoString, unsigned int& index);
    static std::string tzComponent(ExecutionState& state, std::string& isoString, unsigned int& index);
    static bool isNumber(const std::string& s)
    {
        return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
    }
};

class TemporalCalendar : public Temporal {
public:
    explicit TemporalCalendar(ExecutionState& state);
    explicit TemporalCalendar(ExecutionState& state, Object* proto);

    bool isTemporalCalendarObject() const override
    {
        return true;
    }

    static Value getterHelper(ExecutionState& state, const Value& callee, Object* thisValue, Value* argv);

    static TemporalCalendar* createTemporalCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget = Optional<Object*>());
    static bool isBuiltinCalendar(String* id);
    static Value getBuiltinCalendar(ExecutionState& state, String* id);
    static Value getISO8601Calendar(ExecutionState& state);
    static ValueVector calendarFields(ExecutionState& state, const Value& calendar, const ValueVector& fieldNames);
    static Value calendarYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarMonth(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarMonthCode(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDay(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDayOfWeek(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDayOfYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarWeekOfYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDaysInWeek(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDaysInMonth(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarDaysInYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarMonthsInYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value calendarInLeapYear(ExecutionState& state, Object* calendar, const Value& dateLike);
    static Value toTemporalCalendar(ExecutionState& state, const Value& calendar);
    static Value toTemporalCalendarWithISODefault(ExecutionState& state, const Value& calendar);
    static Value getTemporalCalendarWithISODefault(ExecutionState& state, const Value& item);
    static Value dateFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options);
    static int toISOWeekOfYear(ExecutionState& state, const int year, const int month, const int day);
    static Value parseTemporalCalendarString(ExecutionState& state, const Value& isoString);
    static Value ISODaysInYear(ExecutionState& state, const int year);
    static Value ISODaysInMonth(ExecutionState& state, const int year, const int month);
    static bool isIsoLeapYear(ExecutionState& state, const int year);
    static std::string buildISOMonthCode(ExecutionState& state, const int month);
    static int ISOYear(ExecutionState& state, const Value& temporalObject);
    static int ISOMonth(ExecutionState& state, const Value& temporalObject);
    static std::string ISOMonthCode(ExecutionState& state, const Value& temporalObject);
    static int ISODay(ExecutionState& state, const Value& temporalObject);

    static Value defaultMergeFields(ExecutionState& state, const Value& fields, const Value& additionalFields);
    String* getIdentifier() const;
    void setIdentifier(String* identifier);

    static int dayOfYear(ExecutionState& state, const Value& epochDays) { return DateObject::daysInYear(DateObject::makeDate(state, epochDays, Value(0)).toInt32(state)) + 1; }

private:
    String* m_identifier;
};

class TemporalPlainDate : public Temporal, private TemporalDate {
public:
    explicit TemporalPlainDate(ExecutionState& state, int isoYear, int isoMonth, int isoDay, Optional<Value> calendarLike);
    explicit TemporalPlainDate(ExecutionState& state, Object* proto, short isoYear, char isoMonth, char isoDay, Optional<Value> calendarLike);

    bool isTemporalPlainDateObject() const override
    {
        return true;
    }

    static std::map<std::string, int> createISODateRecord(ExecutionState& state, const int year, const int month, const int day);
    static Value createTemporalDate(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const Value& calendar, Optional<Object*> newTarget = nullptr);
    static bool isValidISODate(ExecutionState& state, const int year, const int month, const int day);

    static Value toTemporalDate(ExecutionState& state, const Value& item, Optional<Object*> options = nullptr);
    static std::map<std::string, int> balanceISODate(ExecutionState& state, int year, int month, int day);


    short year() const
    {
        return m_year;
    }
    char month() const
    {
        return m_month;
    }
    char day() const
    {
        return m_day;
    }
    Value calendar() const { return m_calendar; }

private:
    Value m_calendar;
};

class TemporalPlainTime : public Temporal, private TemporalTime {
public:
    explicit TemporalPlainTime(ExecutionState& state);
    explicit TemporalPlainTime(ExecutionState& state, Object* proto);

    bool isTemporalPlainTimeObject() const override
    {
        return true;
    }

    static Value toTemporalTime(ExecutionState& state, const Value& item, Value options);
    static std::map<std::string, Value> toPartialTime(ExecutionState& state, const Value& temporalTimeLike);
    static std::map<std::string, int> regulateTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, const Value& overflow);
    static Value createTemporalTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, Optional<Object*> newTarget);
    static bool isValidTime(ExecutionState& state, const int h, const int m, const int s, const int ms, const int us, const int ns);
    static std::map<std::string, int> balanceTime(ExecutionState& state, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond);
    static std::map<std::string, int> constrainTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static std::map<std::string, int> toTemporalTimeRecord(ExecutionState& state, const Value& temporalTimeLike);
    static int compareTemporalTime(ExecutionState& state, const int time1[6], const int time2[6]);

    void setTime(char h, char m, char s, short ms, short qs, short ns);
    void setCalendar(ExecutionState& state, String* calendar, Optional<Object*> newTarget);

    char getHour() const
    {
        return m_hour;
    }
    char getMinute() const
    {
        return m_minute;
    }
    char getSecond() const
    {
        return m_second;
    }
    short getMillisecond() const
    {
        return m_millisecond;
    }
    short getMicrosecond() const
    {
        return m_microsecond;
    }
    short getNanosecond() const
    {
        return m_nanosecond;
    }
    TemporalCalendar* getCalendar() const
    {
        return m_calendar;
    }

private:
    TemporalCalendar* m_calendar;
};

class TemporalPlainDateTime : public Temporal, private TemporalDate, private TemporalTime {
public:
    explicit TemporalPlainDateTime(ExecutionState& state);
    explicit TemporalPlainDateTime(ExecutionState& state, Object* proto, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond);

    bool isTemporalPlainDateTimeObject() const override
    {
        return true;
    }

    static Value getEpochFromISOParts(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond);
    static bool ISODateTimeWithinLimits(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond);
    static std::map<std::string, std::string> interpretTemporalDateTimeFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options);
    static Value toTemporalDateTime(ExecutionState& state, const Value& item, const Value& options = Value());
    static std::map<std::string, int> balanceISODateTime(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond);
    static Value createTemporalDateTime(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond, const Value& calendar, Optional<Object*> newTarget);

    short getYear() const
    {
        return m_year;
    }
    char getMonth() const
    {
        return m_month;
    }
    char getDay() const
    {
        return m_day;
    }
    char getHour() const
    {
        return m_hour;
    }
    char getMinute() const
    {
        return m_minute;
    }
    char getSecond() const
    {
        return m_second;
    }
    short getMillisecond() const
    {
        return m_millisecond;
    }
    short getMicrosecond() const
    {
        return m_microsecond;
    }
    short getNanosecond() const
    {
        return m_nanosecond;
    }

    Value getCalendar() const
    {
        return Value(m_calendar);
    }
    void setCalendar(const Value mCalendar)
    {
        m_calendar = mCalendar.asObject()->asTemporalCalendarObject();
    }

private:
    TemporalCalendar* m_calendar;
};

class TemporalZonedDateTime : public Temporal {
public:
    explicit TemporalZonedDateTime(ExecutionState& state);
    explicit TemporalZonedDateTime(ExecutionState& state, Object* proto);

    bool isTemporalZonedDateTimeObject() const override
    {
        return true;
    }

    const BigInt* getNanoseconds() const
    {
        return m_nanoseconds;
    }

    void setNanoseconds(BigInt* mNanoseconds)
    {
        m_nanoseconds = mNanoseconds;
    }

    const Value& getTimeZone() const
    {
        return m_timeZone;
    }

    void setTimeZone(const Value& mTimeZone)
    {
        m_timeZone = mTimeZone;
    }

    const Value getCalendar() const
    {
        return Value(m_calendar);
    }

    void setCalendar(const Value mCalendar)
    {
        m_calendar = mCalendar.asObject()->asTemporalCalendarObject();
    }

private:
    BigInt* m_nanoseconds;
    Value m_timeZone;
    TemporalCalendar* m_calendar;
};

class TemporalDuration : public Temporal, private TemporalDate, private TemporalTime {
public:
    explicit TemporalDuration(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds);
    explicit TemporalDuration(ExecutionState& state, Object* proto, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanosecond);
    static bool isValidDuration(const int fields[]);
    static int durationSign(const int fields[]);
    static std::map<std::string, int> createDurationRecord(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds);
    static Value createTemporalDuration(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds, Optional<Object*> newTarget);

    static Value toTemporalDuration(ExecutionState& state, const Value& item);
    static std::map<std::string, int> toTemporalDurationRecord(ExecutionState& state, const Value& temporalDurationLike);
    static std::map<std::string, Value> toTemporalPartialDurationRecord(ExecutionState& state, const Value& temporalDurationLike);
    static Value createNegatedTemporalDuration(ExecutionState& state, const Value& duration);

    bool isTemporalDurationObject() const override
    {
        return true;
    }
    int getYear() const
    {
        return m_year;
    }
    int getMonth() const
    {
        return m_month;
    }
    int getWeek() const
    {
        return m_week;
    }
    int getDay() const
    {
        return m_day;
    }
    int getHour() const
    {
        return m_hour;
    }
    int getMinute() const
    {
        return m_minute;
    }
    int getSecond() const
    {
        return m_second;
    }
    int getMillisecond() const
    {
        return m_millisecond;
    }
    int getMicrosecond() const
    {
        return m_microsecond;
    }
    int getNanosecond() const
    {
        return m_nanosecond;
    }
    TemporalCalendar* getCalendar() const
    {
        return m_calendar;
    }

private:
    int m_week;
    TemporalCalendar* m_calendar;
};

class TemporalInstant : public Temporal {
public:
    explicit TemporalInstant(ExecutionState& state);
    explicit TemporalInstant(ExecutionState& state, Object* proto);

    static bool isValidEpochNanoseconds(const Value& epochNanoseconds);
    static Value createTemporalInstant(ExecutionState& state, const Value& epochNanoseconds, Optional<Object*> newTarget = nullptr);

    bool isTemporalInstantObject() const override
    {
        return true;
    }

    const BigInt* getNanoseconds() const
    {
        return m_nanoseconds;
    }

    void setNanoseconds(BigInt* mNanoseconds)
    {
        m_nanoseconds = mNanoseconds;
    }

private:
    BigInt* m_nanoseconds;
};

class TemporalPlainYearMonth : public Temporal {
public:
    explicit TemporalPlainYearMonth(ExecutionState& state);
    explicit TemporalPlainYearMonth(ExecutionState& state, Object* proto);

    static std::map<std::string, int> balanceISOYearMonth(ExecutionState& state, int year, int month);
};

class TemporalTimeZone : public Temporal {
public:
    explicit TemporalTimeZone(ExecutionState& state);
    explicit TemporalTimeZone(ExecutionState& state, Object* proto);

    static std::map<std::string, int> getISOPartsFromEpoch(ExecutionState& state, const Value& epochNanoseconds);
    static Value getOffsetNanosecondsFor(ExecutionState& state, const Value& timeZone, const Value& instant);
    static Value builtinTimeZoneGetPlainDateTimeFor(ExecutionState& state, const Value& timeZone, const Value& instant, const Value& calendar);
};

} // namespace Escargot

#endif
#endif
