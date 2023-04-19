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

class TemporalDate {
public:
    TemporalDate(int16_t year, int8_t month, int8_t day)
        : m_year(year)
        , m_month(month)
        , m_day(day)
    {
    }

    TemporalDate()
        : m_year(0)
        , m_month(0)
        , m_day(0)
    {
    }

    int16_t m_year;
    int8_t m_month;
    int8_t m_day;
};

class TemporalTime {
public:
    TemporalTime(int8_t hour, int8_t minute, int8_t second, int16_t millisecond, int16_t microsecond, int16_t nanosecond)
        : m_hour(hour)
        , m_minute(minute)
        , m_second(second)
        , m_millisecond(millisecond)
        , m_microsecond(microsecond)
        , m_nanosecond(nanosecond)
    {
    }

    TemporalTime()
        : m_hour(0)
        , m_minute(0)
        , m_second(0)
        , m_millisecond(0)
        , m_microsecond(0)
        , m_nanosecond(0)
    {
    }

    int8_t m_hour;
    int8_t m_minute;
    int8_t m_second;
    int16_t m_millisecond;
    int16_t m_microsecond;
    int16_t m_nanosecond;
};

enum RoundingMode {
    CEIL,
    FLOOR,
    EXPAND,
    TRUNC,
    HALF_CEIL,
    HALF_FLOOR,
    HALF_EXPAND,
    HALF_TRUNC,
    HALF_EVEN
};

enum UnsignedRoundingMode {
    INF,
    ZERO,
    HALF_INF,
    HALF_ZERO,
    UNSIGNED_HALF_EVEN
};

static UnsignedRoundingMode positiveUnsignedRoundingMode[] = { INF, ZERO, INF, ZERO, HALF_INF, HALF_ZERO, HALF_INF, HALF_ZERO };
static UnsignedRoundingMode negativeUnsignedRoundingMode[] = { ZERO, INF, INF, ZERO, HALF_ZERO, HALF_INF, HALF_INF, HALF_ZERO };

class TemporalObject : public Temporal {
public:
    class TimeZone : public gc {
    public:
        TimeZone(bool z, String* offsetString, String* name);
        bool z;
        String* offsetString;
        String* name;
    };

    class DateTime : public gc {
    public:
        DateTime(int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, String* calendar, TimeZone* tz);

        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int millisecond;
        int microsecond;
        int nanosecond;
        String* calendar;
        TimeZone* tz;
    };

    enum DateTimeUnits {
        YEAR_UNIT,
        MONTH_UNIT,
        WEEK_UNIT,
        DAY_UNIT,
        HOUR_UNIT,
        MINUTE_UNIT,
        SECOND_UNIT,
        MILLISECOND_UNIT,
        MICROSECOND_UNIT,
        NANOSECOND_UNIT,
        DAY_LENGTH
    };

    explicit TemporalObject(ExecutionState& state);
    explicit TemporalObject(ExecutionState& state, Object* proto);
    virtual bool hasCalendar() const
    {
        return false;
    }
    bool isTemporalObject() const
    {
        return true;
    }
    virtual TemporalCalendarObject* getCalendar() const
    {
        return nullptr;
    }
    static Value toISODateTime(ExecutionState& state, DateObject& d);
    static Value toISODate(ExecutionState& state, DateObject& d);
    static Value toISOTime(ExecutionState& state, DateObject& d);

    static String* dateTimeUnitString(ExecutionState& state, DateTimeUnits unit);

    static TemporalObject::DateTime parseValidIso8601String(ExecutionState& state, std::string isoString, bool parseTimeZone);
    static TemporalObject::DateTime parseTemporalInstantString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalZonedDateTimeString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalDateString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalDateTimeString(ExecutionState& state, const std::string& isoString);
    static std::map<TemporalObject::DateTimeUnits, int> parseTemporalDurationString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalYearMonthString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::DateTime parseTemporalMonthDayString(ExecutionState& state, const std::string& isoString);
    static TemporalObject::TimeZone parseTemporalTimeZoneString(ExecutionState& state, const std::string& isoString);
    static std::string getNNumberFromString(ExecutionState& state, std::string& isoString, int n, unsigned int& index);
    static std::map<TemporalObject::DateTimeUnits, int> getSeconds(ExecutionState& state, std::string& isoString, unsigned int& index);
    static std::string offset(ExecutionState& state, std::string& isoString, unsigned int& index);
    static std::string tzComponent(ExecutionState& state, std::string& isoString, unsigned int& index);
    static TemporalObject::TimeZone parseTimeZoneOffset(ExecutionState& state, std::string& isoString, unsigned int& index);
    static bool isNumber(const std::string& s)
    {
        return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
    }

    static Value toRelativeTemporalObject(ExecutionState& state, Object* options);
    static Object* mergeLargestUnitOption(ExecutionState& state, const Value& option, TemporalObject::DateTimeUnits largestUnit);
    static UnsignedRoundingMode getUnsignedRoundingMode(ExecutionState& state, RoundingMode roundingMode, bool isNegative);
    static BigInt* roundNumberToIncrementAsIfPositive(ExecutionState& state, BigInt* x, int64_t increment, RoundingMode roundingMode);
    static BigInt* applyUnsignedRoundingMode(ExecutionState& state, BigInt* x, BigInt* r1, BigInt* r2, UnsignedRoundingMode unsignedRoundingMode);
    static int64_t floor(double num);
    static int64_t modulo(int64_t num1, int64_t num2);
};

class TemporalCalendarObject : public TemporalObject {
public:
    explicit TemporalCalendarObject(ExecutionState& state);
    explicit TemporalCalendarObject(ExecutionState& state, Object* proto, String* identifier = nullptr);

    bool isTemporalCalendarObject() const override
    {
        return true;
    }

    static Value getterHelper(ExecutionState& state, const Value& callee, Object* thisValue, Value* argv);

    static TemporalCalendarObject* createTemporalCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget = nullptr);
    static bool isBuiltinCalendar(String* id);
    static Value getBuiltinCalendar(ExecutionState& state, String* id);
    static Value getISO8601Calendar(ExecutionState& state);
    static ValueVector calendarFields(ExecutionState& state, const Value& calendar, const ValueVector& fieldNames);
    static Value calendarDateAdd(ExecutionState& state, const Value& calendar, const Value& date, const Value& duration, const Value& options = Value(), Value dateAdd = Value());
    static Value calendarDateUntil(ExecutionState& state, const Value& calendar, const Value& first, const Value& second, const Value& options, Value dateUntil = Value());
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
    static Value toTemporalCalendar(ExecutionState& state, Value calendar);
    static Value toTemporalCalendarWithISODefault(ExecutionState& state, const Value& calendar);
    static Value getTemporalCalendarWithISODefault(ExecutionState& state, const Value& item);
    static Value dateFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options = Value());
    static Value calendarYearMonthFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options);
    static Value calendarMonthDayFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options);
    static int toISOWeekOfYear(ExecutionState& state, int year, int month, int day);
    static Value parseTemporalCalendarString(ExecutionState& state, const Value& isoString);
    static bool calendarEquals(const TemporalCalendarObject& firstCalendar, const TemporalCalendarObject& secondCalendar);
    static Value ISODaysInMonth(ExecutionState& state, int year, int month);
    static bool isIsoLeapYear(ExecutionState& state, int year);
    static std::string buildISOMonthCode(ExecutionState& state, int month);
    static int resolveISOMonth(ExecutionState& state, const Value& fields);
    static std::map<TemporalObject::DateTimeUnits, int> ISODateFromFields(ExecutionState& state, Value fields, const Value& options);
    static int ISOYear(ExecutionState& state, const Value& temporalObject);
    static int ISOMonth(ExecutionState& state, const Value& temporalObject);
    static std::string ISOMonthCode(ExecutionState& state, const Value& temporalObject);
    static int ISODay(ExecutionState& state, const Value& temporalObject);
    static Value defaultMergeFields(ExecutionState& state, const Value& fields, const Value& additionalFields);
    String* getIdentifier() const
    {
        return m_identifier;
    }

    static int dayOfYear(ExecutionState& state, const Value& epochDays) { return DateObject::daysInYear(DateObject::makeDate(state, epochDays, Value(0)).toInt32(state)) + 1; }

    bool operator==(const TemporalCalendarObject& rhs) const;
    bool operator!=(const TemporalCalendarObject& rhs) const;

private:
    String* m_identifier;
};


class TemporalPlainDateTimeObject;

class TemporalPlainDateObject : public TemporalObject {
public:
    bool isTemporalPlainDateObject() const override
    {
        return true;
    }

    static std::map<TemporalObject::DateTimeUnits, int> createISODateRecord(ExecutionState& state, int year, int month, int day);
    static Value createTemporalDate(ExecutionState& state, int isoYear, int isoMonth, int isoDay, const Value& calendar, Optional<Object*> newTarget = nullptr);
    static Value createFromPlainDateTimeObject(ExecutionState& state, TemporalPlainDateTimeObject* plainDateTime);
    static std::map<TemporalObject::DateTimeUnits, int> regulateISODate(ExecutionState& state, int year, int month, int day, const Value& overflow);
    static bool isValidISODate(ExecutionState& state, int year, int month, int day);
    static Value toTemporalDate(ExecutionState& state, const Value& item, Optional<Object*> options = nullptr);
    static std::map<TemporalObject::DateTimeUnits, int> balanceISODate(ExecutionState& state, int year, int month, int day);
    static int compareISODate(int firstYear, int firstMonth, int firstDay, int secondYear, int secondMonth, int secondDay);

    int16_t year() const
    {
        return m_date.m_year;
    }
    int8_t month() const
    {
        return m_date.m_month;
    }
    int8_t day() const
    {
        return m_date.m_day;
    }
    TemporalCalendarObject* calendar() const
    {
        return m_calendar;
    }
    const TemporalDate& date()
    {
        return m_date;
    }
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }
    bool hasCalendar() const override
    {
        return true;
    }

private:
    explicit TemporalPlainDateObject(ExecutionState& state, Object* proto, const TemporalDate& date, TemporalCalendarObject* calendar);

    TemporalDate m_date;
    TemporalCalendarObject* m_calendar;
};

class TemporalPlainTimeObject : public TemporalObject {
public:
    enum Operation {
        ADD = 1,
        SUBTRACT = -1,
    };

    bool isTemporalPlainTimeObject() const override
    {
        return true;
    }

    static std::map<TemporalObject::DateTimeUnits, int> differenceTime(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> first, std::map<TemporalObject::DateTimeUnits, int> second);
    static Value toTemporalTime(ExecutionState& state, const Value& item, Value options);
    static std::map<TemporalObject::DateTimeUnits, Value> toPartialTime(ExecutionState& state, const Value& temporalTimeLike);
    static std::map<TemporalObject::DateTimeUnits, int> regulateTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, const Value& overflow);
    static Value createFromPlainDateTimeObject(ExecutionState& state, TemporalPlainDateTimeObject* plainDateTime);
    static Value createTemporalTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, Optional<Object*> newTarget = nullptr);
    static bool isValidTime(ExecutionState& state, int h, int m, int s, int ms, int us, int ns);
    static std::map<TemporalObject::DateTimeUnits, int> balanceTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static std::map<TemporalObject::DateTimeUnits, int> constrainTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static std::map<TemporalObject::DateTimeUnits, int> toTemporalTimeRecord(ExecutionState& state, const Value& temporalTimeLike);
    static int compareTemporalTime(ExecutionState& state, const int time1[6], const int time2[6]);
    static std::map<TemporalObject::DateTimeUnits, int> addTime(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int>& first, std::map<TemporalObject::DateTimeUnits, int>& second);
    static Value addDurationToOrSubtractDurationFromPlainTime(ExecutionState& state, Operation operation, TemporalPlainTimeObject* temporalTime, const Value& temporalDurationLike);

    int8_t getHour() const
    {
        return m_time.m_hour;
    }
    int8_t getMinute() const
    {
        return m_time.m_minute;
    }
    int8_t getSecond() const
    {
        return m_time.m_second;
    }
    int16_t getMillisecond() const
    {
        return m_time.m_millisecond;
    }
    int16_t getMicrosecond() const
    {
        return m_time.m_microsecond;
    }
    int16_t getNanosecond() const
    {
        return m_time.m_nanosecond;
    }
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }
    const TemporalTime& time()
    {
        return m_time;
    }
    bool hasCalendar() const override
    {
        return true;
    }

private:
    explicit TemporalPlainTimeObject(ExecutionState& state, Object* proto, const TemporalTime& time, TemporalCalendarObject* calendar);

    TemporalTime m_time;
    TemporalCalendarObject* m_calendar;
};

class TemporalPlainDateTimeObject : public TemporalObject {
public:
    bool isTemporalPlainDateTimeObject() const override
    {
        return true;
    }

    static uint64_t getEpochFromISOParts(ExecutionState& state, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static bool ISODateTimeWithinLimits(ExecutionState& state, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static std::map<TemporalObject::DateTimeUnits, int> interpretTemporalDateTimeFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options);
    static Value toTemporalDateTime(ExecutionState& state, const Value& item, const Value& options = Value());
    static std::map<TemporalObject::DateTimeUnits, int> balanceISODateTime(ExecutionState& state, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static Value createTemporalDateTime(ExecutionState& state, int isoYear, int isoMonth, int isoDay, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, const Value& calendar, Optional<Object*> newTarget = nullptr);
    static Value createFromPlainDateObject(ExecutionState& state, TemporalPlainDateObject* plainDate);
    static std::map<TemporalObject::DateTimeUnits, int> addDateTime(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int>& first, const Value& calendar, std::map<TemporalObject::DateTimeUnits, int>& second, Object* options);
    static std::map<TemporalObject::DateTimeUnits, int> differenceISODateTime(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> first, std::map<TemporalObject::DateTimeUnits, int> second, const Value& calendar, TemporalObject::DateTimeUnits largestUnit, const Value& options);
    static Value addDurationToOrSubtractDurationFromPlainDateTime(ExecutionState& state, TemporalPlainTimeObject::Operation operation, TemporalPlainDateTimeObject* temporalDateTime, const Value& temporalDurationLike, const Value& options);

    int16_t getYear() const
    {
        return m_date.m_year;
    }
    int8_t getMonth() const
    {
        return m_date.m_month;
    }
    int8_t getDay() const
    {
        return m_date.m_day;
    }
    int8_t getHour() const
    {
        return m_time.m_hour;
    }
    int8_t getMinute() const
    {
        return m_time.m_minute;
    }
    int8_t getSecond() const
    {
        return m_time.m_second;
    }
    int16_t getMillisecond() const
    {
        return m_time.m_millisecond;
    }
    int16_t getMicrosecond() const
    {
        return m_time.m_microsecond;
    }
    int16_t getNanosecond() const
    {
        return m_time.m_nanosecond;
    }
    const TemporalDate& date()
    {
        return m_date;
    }
    const TemporalTime& time()
    {
        return m_time;
    }
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }

    bool hasCalendar() const override
    {
        return true;
    }

private:
    explicit TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, const TemporalDate& date, const TemporalTime& time, TemporalCalendarObject* calendar);

    TemporalDate m_date;
    TemporalTime m_time;
    TemporalCalendarObject* m_calendar;
};

class TemporalZonedDateTimeObject : public TemporalObject {
public:
    enum OffsetBehaviour {
        OPTION,
        EXACT,
        WALL
    };

    enum MatchBehaviour {
        EXACTLY,
        MINUTES
    };

    explicit TemporalZonedDateTimeObject(ExecutionState& state, const BigInt* nanoseconds, const TemporalTimeZoneObject* timeZone, TemporalCalendarObject* calendar);
    explicit TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, const BigInt* nanoseconds, const TemporalTimeZoneObject* timeZone, TemporalCalendarObject* calendar);

    TemporalPlainDateTimeObject* toTemporalPlainDateTime(ExecutionState& state);

    static Value createTemporalZonedDateTime(ExecutionState& state, const BigInt& epochNanoseconds, const TemporalTimeZoneObject* timeZone, const TemporalCalendarObject* calendar, Optional<Object*> newTarget = nullptr);
    static Value toTemporalZonedDateTime(ExecutionState& state, const Value& item, const Value& options = Value());
    static Value interpretISODateTimeOffset(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int>& dateTime, TemporalZonedDateTimeObject::OffsetBehaviour offsetBehaviour, time64_t offsetNanoseconds, const Value& timeZone, const Value& disambiguation, const Value& offsetOption, TemporalZonedDateTimeObject::MatchBehaviour matchBehaviour);
    static BigInt* addZonedDateTime(ExecutionState& state, const BigInt* epochNanoseconds, const TemporalTimeZoneObject* timeZone, const TemporalCalendarObject* calendar, std::map<TemporalObject::DateTimeUnits, int> duration, const Value& options = Value());
    static std::map<TemporalObject::DateTimeUnits, int> differenceZonedDateTime(ExecutionState& state, BigInt* ns1, BigInt* ns2, const Value& timeZone, const Value& calendar, TemporalObject::DateTimeUnits largestUnit, const Value& options);
    static std::map<TemporalObject::DateTimeUnits, int> nanosecondsToDays(ExecutionState& state, int64_t nanoseconds, const Value relativeTo);
    static Value addDurationToOrSubtractDurationFromZonedDateTime(ExecutionState& state, TemporalPlainTimeObject::Operation operation, TemporalZonedDateTimeObject* zonedDateTimeObject, const Value& temporalDurationLike, const Value& options);

    bool isTemporalZonedDateTimeObject() const override
    {
        return true;
    }

    const BigInt* getNanoseconds() const
    {
        return m_nanoseconds;
    }

    const TemporalTimeZoneObject* getTimeZone() const
    {
        return m_timeZone;
    }

    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }
    bool hasCalendar() const override
    {
        return true;
    }

private:
    const BigInt* m_nanoseconds;
    const TemporalTimeZoneObject* m_timeZone;
    TemporalCalendarObject* m_calendar;
};

class TemporalDurationObject : public TemporalObject, private TemporalDate, private TemporalTime {
public:
    explicit TemporalDurationObject(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds);
    explicit TemporalDurationObject(ExecutionState& state, Object* proto, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanosecond);
    static bool isValidDuration(const int fields[]);
    static TemporalObject::DateTimeUnits defaultTemporalLargestUnit(std::map<TemporalObject::DateTimeUnits, int> temporalObject);
    static BigInt* totalDurationNanoseconds(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> duration, int offsetShift);
    static int durationSign(const int fields[]);
    static std::map<TemporalObject::DateTimeUnits, int> createDurationRecord(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds);
    static std::map<TemporalObject::DateTimeUnits, int> createTimeDurationRecord(ExecutionState& state, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds);
    static Value createTemporalDuration(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds, Optional<Object*> newTarget = nullptr);

    static Value toTemporalDuration(ExecutionState& state, const Value& item);
    static std::map<TemporalObject::DateTimeUnits, int> toTemporalDurationRecord(ExecutionState& state, const Value& temporalDurationLike);
    static std::map<TemporalObject::DateTimeUnits, Value> toTemporalPartialDurationRecord(ExecutionState& state, const Value& temporalDurationLike);
    static Value createNegatedTemporalDuration(ExecutionState& state, const Value& duration);
    static std::map<TemporalObject::DateTimeUnits, int> balanceDuration(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> duration, TemporalObject::DateTimeUnits largestUnit, const Value& relativeTo = Value());
    static std::map<TemporalObject::DateTimeUnits, int> balancePossiblyInfiniteDuration(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> duration, TemporalObject::DateTimeUnits largestUnit, const Value& relativeTo = Value());
    static std::map<TemporalObject::DateTimeUnits, int> addDuration(ExecutionState& state, std::map<TemporalObject::DateTimeUnits, int> first, std::map<TemporalObject::DateTimeUnits, int> second, const Value& relativeTo);
    static Value addDurationToOrSubtractDurationFromDuration(ExecutionState& state, TemporalPlainTimeObject::Operation operation, TemporalDurationObject* duration, const Value& other, const Value& options);

    bool isTemporalDurationObject() const override
    {
        return true;
    }
    int getYear() const
    {
        return m_year;
    }
    int8_t getMonth() const
    {
        return m_month;
    }
    int getWeek() const
    {
        return m_week;
    }
    int8_t getDay() const
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
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }

private:
    int m_week;
    TemporalCalendarObject* m_calendar;
};

class TemporalInstantObject : public TemporalObject {
public:
    static const int64_t dayToNanosecond = 86400000000000;
    static const int64_t HourToNanosecond = 3600000000000;
    static const int64_t MinuteToNanosecond = 60000000000;
    static const int64_t SecondToNanosecond = 1000000000;
    static const int64_t MillisecondToNanosecond = 1000000;
    static const int64_t MicrosecondToNanosecond = 1000;

    explicit TemporalInstantObject(ExecutionState& state);
    explicit TemporalInstantObject(ExecutionState& state, Object* proto);

    static bool isValidEpochNanoseconds(const Value& epochNanoseconds);
    static Value createTemporalInstant(ExecutionState& state, const Value& epochNanoseconds, Optional<Object*> newTarget = nullptr);
    static Value toTemporalInstant(ExecutionState& state, const Value& item);
    static Value parseTemporalInstant(ExecutionState& state, const std::string& isoString);
    static int compareEpochNanoseconds(ExecutionState& state, const BigInt& firstNanoSeconds, const BigInt& secondNanoSeconds);

    static int64_t offsetStringToNanoseconds(ExecutionState& state, String* offset);
    static BigInt* addInstant(ExecutionState& state, const BigInt* epochNanoseconds, std::map<TemporalObject::DateTimeUnits, int>& duration);
    static BigInt* differenceInstant(ExecutionState& state, const BigInt* ns1, BigInt* ns2, int roundingIncrement, TemporalObject::DateTimeUnits smallestUnit, RoundingMode roundingMode);
    static BigInt* roundTemporalInstant(ExecutionState& state, BigInt* ns, int increment, TemporalObject::DateTimeUnits unit, RoundingMode roundingMode);
    static Value addDurationToOrSubtractDurationFromInstant(ExecutionState& state, TemporalPlainTimeObject::Operation operation, TemporalInstantObject* instant, const Value& other, const Value& options);

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

class TemporalPlainYearMonthObject : public TemporalObject {
public:
    explicit TemporalPlainYearMonthObject(ExecutionState& state);
    explicit TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, int isoYear = 0, int isoMonth = 0, TemporalCalendarObject* calendar = nullptr, int referenceISODay = 0);

    static Value createTemporalYearMonth(ExecutionState& state, int isoYear, int isoMonth, const Value& calendar, int referenceISODay, Optional<Object*> newTarget = nullptr);
    static bool isoYearMonthWithinLimits(int isoYear, int isoMonth);
    static Value toTemporalYearMonth(ExecutionState& state, const Value& item, const Value& options);
    static Value addDurationToOrSubtractDurationFromPlainYearMonth(ExecutionState& state, TemporalPlainTimeObject::Operation operation, TemporalPlainYearMonthObject* yearMonth, const Value& temporalDurationLike, const Value& options);

    int getIsoYear() const
    {
        return m_isoYear;
    }
    int getIsoMonth() const
    {
        return m_isoMonth;
    }
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }
    int getReferenceIsoDay() const
    {
        return m_referenceISODay;
    }
    bool hasCalendar() const override
    {
        return true;
    }

private:
    int m_isoYear;
    int m_isoMonth;
    TemporalCalendarObject* m_calendar;
    int m_referenceISODay;
};

class TemporalPlainMonthDayObject : public TemporalObject {
public:
    explicit TemporalPlainMonthDayObject(ExecutionState& state);
    explicit TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, int isoMonth = 0, int isoDay = 0, TemporalCalendarObject* calendar = nullptr, int referenceISOYear = 0);

    static Value createTemporalMonthDay(ExecutionState& state, int isoMonth, int isoDay, TemporalCalendarObject* calendar, int referenceISOYear, Optional<Object*> newTarget = nullptr);
    static Value toTemporalMonthDay(ExecutionState& state, const Value& item, const Value& options);

    bool isTemporalPlainMonthDayObject() const override
    {
        return true;
    }

    int getIsoMonth() const
    {
        return m_isoMonth;
    }
    int getIsoDay() const
    {
        return m_isoDay;
    }
    TemporalCalendarObject* getCalendar() const override
    {
        return m_calendar;
    }
    int getReferenceIsoYear() const
    {
        return m_referenceISOYear;
    }
    bool hasCalendar() const override
    {
        return true;
    }

private:
    int m_isoMonth;
    int m_isoDay;
    TemporalCalendarObject* m_calendar;
    int m_referenceISOYear;
};

class TemporalTimeZoneObject : public TemporalObject {
public:
    explicit TemporalTimeZoneObject(ExecutionState& state);
    explicit TemporalTimeZoneObject(ExecutionState& state, Object* proto, ASCIIString* identifier = new ASCIIString(""), const Value& offsetNanoseconds = Value());

    static std::map<TemporalObject::DateTimeUnits, int> getISOPartsFromEpoch(ExecutionState& state, const Value& epochNanoseconds);
    static Value createTemporalTimeZone(ExecutionState& state, const std::string& identifier, Optional<Object*> newTarget = nullptr);
    static bool isValidTimeZoneName(const std::string& timeZone);
    static std::string canonicalizeTimeZoneName(const std::string& timeZone);
    static std::string formatTimeZoneOffsetString(long long offsetNanoseconds);
    static std::string defaultTimeZone();
    static Value toTemporalTimeZone(ExecutionState& state, const Value& temporalTimeZoneLike);
    static std::map<TemporalObject::DateTimeUnits, int> getIANATimeZoneDateTimeParts(ExecutionState& state, const Value& epochNanoseconds);
    static int64_t getOffsetNanosecondsFor(ExecutionState& state, const Value& timeZone, const Value& instant);
    static Value builtinTimeZoneGetOffsetStringFor(ExecutionState& state, const Value& timeZone, const Value& instant);
    static Value builtinTimeZoneGetPlainDateTimeFor(ExecutionState& state, const Value& timeZone, const Value& instant, const Value& calendar);
    static Value getIANATimeZoneOffsetNanoseconds(ExecutionState& state, const Value& epochNanoseconds, const std::string& timeZoneIdentifier);
    static ValueVector getIANATimeZoneEpochValue(ExecutionState& state, const std::string& timeZoneIdentifier, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond);
    static Value getIANATimeZoneNextTransition(ExecutionState& state, const Value& epochNanoseconds, std::string timeZoneIdentifier);
    static Value getIANATimeZonePreviousTransition(ExecutionState& state, const Value& epochNanoseconds, std::string timeZoneIdentifier);
    static Value builtinTimeZoneGetInstantFor(ExecutionState& state, const Value& timeZone, const Value& dateTime, const Value& disambiguation);
    static Value disambiguatePossibleInstants(ExecutionState& state, ValueVector& possibleInstants, const Value& timeZone, TemporalPlainDateTimeObject* dateTime, const Value& disambiguation);
    static ValueVector getPossibleInstantsFor(ExecutionState& state, const Value& timeZone, const Value& dateTime);

    bool isTemporalTimeZoneObject() const override
    {
        return true;
    }

    const ASCIIString* getIdentifier() const
    {
        return m_identifier;
    }
    const Value& getOffsetNanoseconds() const
    {
        return m_offsetNanoseconds;
    }

private:
    static Value getIANATimeZoneTransition(ExecutionState& state, BigInt* rightNanos, BigInt* rightOffsetNs, BigInt* leftNanos, BigInt* leftOffsetNs, BigInt* cap, const std::string& timeZoneIdentifier);
    ASCIIString* m_identifier;
    Value m_offsetNanoseconds;
};

} // namespace Escargot

#endif
#endif
