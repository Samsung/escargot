#if defined(ENABLE_TEMPORAL)
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

#include "runtime/Context.h"
#include "runtime/Object.h"
#include "runtime/GlobalObject.h"
#include "runtime/DateObject.h"
#include "intl/Intl.h"
#include "util/Int128.h"
#include "util/ISO8601.h"

namespace Escargot {

class TimeZone {
public:
    TimeZone()
    {
    }

    TimeZone(String* timeZone)
        : m_timeZoneName(timeZone)
    {
    }

    TimeZone(int64_t offset)
        : m_offset(offset)
    {
    }

    bool empty() const
    {
        return !hasTimeZoneName() && !hasOffset();
    }

    bool hasTimeZoneName() const
    {
        return !!m_timeZoneName;
    }

    bool hasOffset() const
    {
        return !!m_offset;
    }

    String* timeZoneName()
    {
        return m_timeZoneName.value();
    }

    int64_t offset()
    {
        return m_offset.value();
    }

private:
    Optional<String*> m_timeZoneName;
    Optional<int64_t> m_offset;
};

struct TimeRecord {
    static TimeRecord noonTimeRecord()
    {
        return TimeRecord(0, 12, 0, 0, 0, 0, 0);
    }

    TimeRecord(int d, int h, int m, int s, int ms, int microsec, int ns)
        : days(d)
        , hour(h)
        , minute(m)
        , second(s)
        , millisecond(ms)
        , microsecond(microsec)
        , nanosecond(ns)
    {
    }

    int days = 0;
    int hour = 12;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
    int microsecond = 0;
    int nanosecond = 0;
};

struct ISODate {
    ISODate(int y, int m, int d)
        : year(y)
        , month(m)
        , day(d)
    {
    }

    int year = 0;
    int month = 0;
    int day = 0;
};

struct ISODateTime {
    ISODateTime(const ISODate& d, const TimeRecord& t)
        : date(d)
        , time(t)
    {
    }

    MAKE_STACK_ALLOCATED();

    ISODate date;
    TimeRecord time;
};

struct YearWeek {
    static const int UndefinedValue = std::numeric_limits<int>::max();

    YearWeek(int w, int y)
        : week(w)
        , year(y)
    {
    }

    Value weekValue() const
    {
        if (week == UndefinedValue) {
            return Value();
        }
        return Value(week);
    }

    Value yearValue() const
    {
        if (year == UndefinedValue) {
            return Value();
        }
        return Value(year);
    }

    int week = UndefinedValue;
    int year = UndefinedValue;
};

struct CalendarDate {
    static const int EraYearUndefinedValue = std::numeric_limits<int>::max();

    CalendarDate(int y, int mon, String* monCode, int d, int dow, int doy, const YearWeek& yw, int diw, int dim, int diy, int miy, bool leapYear)
        : era(nullptr)
        , eraYear(EraYearUndefinedValue)
        , year(y)
        , month(mon)
        , monthCode(monCode)
        , day(d)
        , dayOfWeek(dow)
        , dayOfYear(doy)
        , weekOfYear(yw)
        , daysInWeek(diw)
        , daysInMonth(dim)
        , daysInYear(diy)
        , monthsInYear(miy)
        , inLeapYear(leapYear)
    {
    }

    MAKE_STACK_ALLOCATED();

    Value eraValue() const
    {
        if (!era) {
            return Value();
        }
        return Value(era);
    }

    Value eraYearValue() const
    {
        if (eraYear == EraYearUndefinedValue) {
            return Value();
        }
        return Value(eraYear);
    }

    // FIXME types of members could be changed to smaller types
    // these members could be rearranged for better alignment as well
    String* era = nullptr;
    int eraYear = EraYearUndefinedValue;
    int year = 0;
    int month = 0;
    String* monthCode = nullptr;
    int day = 0;
    int dayOfWeek = 0;
    int dayOfYear = 0;
    YearWeek weekOfYear;
    int daysInWeek = 0;
    int daysInMonth = 0;
    int daysInYear = 0;
    int monthsInYear = 0;
    bool inLeapYear = false;
};

enum class TemporalUnit : uint8_t {
#define DEFINE_TYPE(name, Name, names, Names, index, category) Name,
    PLAIN_DATETIME_UNITS(DEFINE_TYPE)
#undef DEFINE_TYPE
        Auto
};

inline ISO8601::DateTimeUnit toDateTimeUnit(TemporalUnit u)
{
    ASSERT(u != TemporalUnit::Auto);
    return static_cast<ISO8601::DateTimeUnit>(u);
}

inline Optional<ISO8601::DateTimeUnit> toDateTimeUnit(Optional<TemporalUnit> u)
{
    if (u) {
        return toDateTimeUnit(u.value());
    }
    return NullOption;
}

class Temporal {
public:
    /* TODO ParseISODateTime
    struct ParseResult {
        Optional<char> sign;

        Optional<String*> dateYear;
        Optional<String*> dateMonth;
        Optional<String*> dateDay;
        Optional<String*> timeHour;
        Optional<String*> timeMinute;
        Optional<String*> timeSecond;
        Optional<String*> timeFraction;
    };

    enum class Production {
        AnnotationValue,
        DateMonth,
        TemporalDateTimeString,
        TemporalDurationString,
        TemporalInstantString,
        TemporalMonthDayString,
        TemporalTimeString,
        TemporalYearMonthString,
        TemporalZonedDateTimeString,
        TimeZoneIdentifier,
    };
    */

    static double epochDayNumberForYear(const double year);
    static int epochTimeToEpochYear(const double time);
    static double epochTimeForYear(const double year);
    static int mathematicalInLeapYear(const double time);
    static int mathematicalDaysInYear(const int year);

    static bool isValidISODate(ExecutionState& state, const int year, const int month, const int day);
    static int ISODayOfWeek(const ISODate& date);
    static int ISODayOfYear(const ISODate& date);
    static int ISODaysInMonth(const int year, const int month);
    static YearWeek ISOWeekOfYear(const ISODate& date);
    static bool ISODateWithinLimits(ExecutionState& state, const ISODate& date);
    static bool ISODateTimeWithinLimits(ExecutionState& state, const ISODateTime& dateTime);

    static Int128 nsMaxInstant()
    {
        Int128 ret = 864000000;
        ret *= 10000000000000;
        return ret;
    }
    static Int128 nsMinInstant()
    {
        Int128 ret = -864000000;
        ret *= 10000000000000;
        return ret;
    }
    static Int128 nsMaxConstant()
    {
        Int128 ret = 86400000864;
        ret *= 100000000000;
        return ret;
    }
    static Int128 nsMinConstant()
    {
        Int128 ret = -86400000864;
        ret *= 100000000000;
        return ret;
    }
    static int64_t nsPerDay();

    static Value createTemporalDate(ExecutionState& state, const ISODate& isoDate, String* calendar, Optional<Object*> newTarget);
    static Value toTemporalDate(ExecutionState& state, Value item, Value options = Value());

    static void formatSecondsStringFraction(StringBuilder& builder, Int128 fraction, Value precision);

    // https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochnanoseconds
    static Int128 systemUTCEpochNanoseconds();
    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidepochnanoseconds
    static bool isValidEpochNanoseconds(Int128 s);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
    static TemporalDurationObject* toTemporalDuration(ExecutionState& state, const Value& item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
    static TemporalInstantObject* toTemporalInstant(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalfractionalseconddigitsoption
    // NullOption means AUTO
    static Optional<unsigned> getTemporalFractionalSecondDigitsOption(ExecutionState& state, Optional<Object*> resolvedOptions);

    // https://tc39.es/proposal-temporal/#sec-temporal-getroundingmodeoption
    static ISO8601::RoundingMode getRoundingModeOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* fallback);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalunitvaluedoption
    static Optional<TemporalUnit> getTemporalUnitValuedOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* key, Optional<Value> defaultValue /* give DefaultValue to EmptyValue means Required = true*/);

    // https://tc39.es/proposal-temporal/#sec-temporal-validatetemporalunitvaluedoption
    static void validateTemporalUnitValue(ExecutionState& state, Optional<TemporalUnit> value, ISO8601::DateTimeUnitCategory unitGroup, Optional<TemporalUnit*> extraValues, size_t extraValueSize);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimezoneidentifier
    static TimeZone toTemporalTimezoneIdentifier(ExecutionState& state, const Value& temporalTimeZoneLike);

    // https://tc39.es/proposal-temporal/#sec-temporal-tosecondsstringprecisionrecord
    struct StringPrecisionRecord {
        Value precision;
        ISO8601::DateTimeUnit unit;
        unsigned increment;
    };
    static StringPrecisionRecord toSecondsStringPrecisionRecord(ExecutionState& state, Optional<ISO8601::DateTimeUnit> smallestUnit, Optional<unsigned> fractionalDigitCount);

    // https://tc39.es/proposal-temporal/#sec-validatetemporalroundingincrement
    static void validateTemporalRoundingIncrement(ExecutionState& state, unsigned increment, Int128 dividend, bool inclusive);

    // https://tc39.es/proposal-temporal/#sec-temporal-getroundingincrementoption
    static unsigned getRoundingIncrementOption(ExecutionState& state, Optional<Object*> options);

    // https://tc39.es/proposal-temporal/#sec-temporal-getdifferencesettings
    struct DifferenceSettingsRecord {
        ISO8601::DateTimeUnit smallestUnit;
        ISO8601::DateTimeUnit largestUnit;
        ISO8601::RoundingMode roundingMode;
        unsigned roundingIncrement;
    };
    static DifferenceSettingsRecord getDifferenceSettings(ExecutionState& state, bool isSinceOperation, Optional<Object*> options, ISO8601::DateTimeUnitCategory unitGroup,
                                                          Optional<TemporalUnit*> disallowedUnits, size_t disallowedUnitsLength, TemporalUnit fallbackSmallestUnit, TemporalUnit smallestLargestDefaultUnit);

    // https://tc39.es/proposal-temporal/#sec-temporal-negateroundingmode
    static ISO8601::RoundingMode negateRoundingMode(ExecutionState& state, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-largeroftwotemporalunits
    static ISO8601::DateTimeUnit largerOfTwoTemporalUnits(ISO8601::DateTimeUnit u1, ISO8601::DateTimeUnit u2);

    // https://tc39.es/proposal-temporal/#sec-temporal-maximumtemporaldurationroundingincrement
    static Optional<unsigned> maximumTemporalDurationRoundingIncrement(ISO8601::DateTimeUnit unit);

    // https://tc39.es/proposal-temporal/#sec-temporal-timedurationfromepochnanosecondsdifference
    static Int128 timeDurationFromEpochNanosecondsDifference(Int128 one, Int128 two);

    // https://tc39.es/proposal-temporal/#sec-temporal-iscalendarunit
    static bool isCalendarUnit(ISO8601::DateTimeUnit unit);
};

class TemporalObject : public DerivedObject {
public:
    explicit TemporalObject(ExecutionState& state);
    explicit TemporalObject(ExecutionState& state, Object* proto);

    virtual bool isTemporalObject() const override
    {
        return true;
    }
};

class TemporalPlainDateObject : public TemporalObject {
public:
    explicit TemporalPlainDateObject(ExecutionState& state, Object* proto, const ISODate& date, String* calendar);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    virtual bool isTemporalPlainDateObject() const override
    {
        return true;
    }

    int year() const
    {
        return m_date.year;
    }

    int month() const
    {
        return m_date.month;
    }

    int day() const
    {
        return m_date.day;
    }

    String* calendar() const
    {
        return m_calendar;
    }

    const ISODate& date()
    {
        return m_date;
    }

private:
    String* m_calendar;
    ISODate m_date;
};

} // namespace Escargot

#endif
#endif
