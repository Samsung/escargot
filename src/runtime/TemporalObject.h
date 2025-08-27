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

namespace Escargot {

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

    // https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochnanoseconds
    static Int128 systemUTCEpochNanoseconds();
    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidepochnanoseconds
    static bool isValidEpochNanoseconds(Int128 s);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
    static TemporalDurationObject* toTemporalDuration(ExecutionState& state, const Value& item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
    static TemporalInstantObject* toTemporalInstant(ExecutionState& state, Value item);
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
