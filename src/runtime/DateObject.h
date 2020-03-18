/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotDateObject__
#define __EscargotDateObject__

#include "runtime/Object.h"

namespace Escargot {

#define TIME64NAN (1LL << 63)
#define IS_VALID_TIME(time) ((time != TIME64NAN) ? true : false)
#define IS_IN_TIME_RANGE(millisec) \
    (millisec <= const_Date_MaximumDatePrimitiveValue && millisec >= -const_Date_MaximumDatePrimitiveValue)

typedef int64_t time64_t;

static const int const_Date_daysPerWeek = 7;
static const int const_Date_daysPerYear = 365;
static const int const_Date_daysPerLeapYear = const_Date_daysPerYear + 1;
static const int const_Date_monthsPerYear = 12;
static const int64_t const_Date_MaximumDatePrimitiveValue = 8.64e15;
static const int64_t const_Date_nsPerMs = 1e6;
static const int64_t const_Date_hoursPerDay = 24;
static const int64_t const_Date_minutesPerHour = 60;
static const int64_t const_Date_secondsPerMinute = 60;
static const int64_t const_Date_secondsPerHour = const_Date_secondsPerMinute * const_Date_minutesPerHour;
static const int64_t const_Date_msPerSecond = 1000;
static const int64_t const_Date_msPerMinute = const_Date_msPerSecond * const_Date_secondsPerMinute;
static const int64_t const_Date_msPerHour = const_Date_msPerSecond * const_Date_secondsPerHour;
static const int64_t const_Date_msPerDay = const_Date_msPerHour * const_Date_hoursPerDay;


class DateObject : public Object {
public:
    explicit DateObject(ExecutionState& state);
    explicit DateObject(ExecutionState& state, Object* proto);

    static void initCachedUTC(ExecutionState& state, DateObject* d);

    static time64_t currentTime();

    double primitiveValue()
    {
        if (LIKELY(IS_VALID_TIME(m_primitiveValue)))
            return m_primitiveValue;
        return std::numeric_limits<double>::quiet_NaN();
    }
    inline void setTimeValueAsNaN()
    {
        m_primitiveValue = TIME64NAN;
    }

    inline bool isValid()
    {
        return IS_VALID_TIME(m_primitiveValue);
    }

    virtual bool isDateObject() const
    {
        return true;
    }

    static time64_t timeClip(ExecutionState& state, double V)
    {
        if (std::isinf(V) || std::isnan(V)) {
            return TIME64NAN;
        } else if (!IS_IN_TIME_RANGE(V)) {
            return TIME64NAN;
        } else {
            return Value(V).toInteger(state);
        }
    }

    void setTimeValue(time64_t t);
    void setTimeValue(ExecutionState& state, const Value& str);
    void setTimeValue(ExecutionState& state, int year, int month, int date, int hour, int minute, int64_t second, int64_t millisecond, bool convertToUTC = true);

    static time64_t timeinfoToMs(ExecutionState& state, int year, int month, int day, int hour, int minute, int64_t second, int64_t millisecond); //
    static time64_t applyLocalTimezoneOffset(ExecutionState& state, time64_t t); //

    String* toDateString(ExecutionState& state);
    String* toTimeString(ExecutionState& state);
    String* toFullString(ExecutionState& state);
    String* toISOString(ExecutionState& state);
    String* toUTCString(ExecutionState& state, String* functionName);
    String* toLocaleDateString(ExecutionState& state);
    String* toLocaleTimeString(ExecutionState& state);
    String* toLocaleFullString(ExecutionState& state);
    int getDate(ExecutionState& state);
    int getDay(ExecutionState& state);
    int getFullYear(ExecutionState& state);
    int getHours(ExecutionState& state);
    int getMilliseconds(ExecutionState& state);
    int getMinutes(ExecutionState& state);
    int getMonth(ExecutionState& state);
    int getSeconds(ExecutionState& state);
    int getTimezoneOffset(ExecutionState& state);
    int getUTCDate(ExecutionState& state);
    int getUTCDay(ExecutionState& state);
    int getUTCFullYear(ExecutionState& state);
    int getUTCHours(ExecutionState& state);
    int getUTCMilliseconds(ExecutionState& state);
    int getUTCMinutes(ExecutionState& state);
    int getUTCMonth(ExecutionState& state);
    int getUTCSeconds(ExecutionState& state);


    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty(ExecutionState& state)
    {
        return "Date";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);
    }

    struct timeinfo {
        timeinfo()
            : year(0)
            , month(0)
            , mday(0)
            , hour(0)
            , min(0)
            , sec(0)
            , millisec(0)
            , wday(0)
            , gmtoff(0)
            , isdst(0)
        {
        }
        int year;
        int month;
        int mday;

        int hour;
        int min;
        int sec;
        int millisec;

        int wday;
        int gmtoff;
        int isdst;
        // int yday;
    };

    time64_t m_primitiveValue; // 1LL << 63 is reserved for represent NaN
    struct timeinfo m_cachedLocal;
    bool m_isCacheDirty : 1;

    void resolveCache(ExecutionState& state);
    static time64_t parseStringToDate(ExecutionState& state, String* istr);
    static time64_t parseStringToDate_1(ExecutionState& state, String* istr, bool& haveTZ, int& offset);
    static time64_t parseStringToDate_2(ExecutionState& state, String* istr, bool& haveTZ);
    static int daysInYear(int year);
    static int daysFromMonth(int year, int month);
    static int daysFromYear(int year);
    static int daysFromTime(time64_t t); // return the number of days after 1970.1.1
    static time64_t daysToMs(int year, int month, int date);
    static time64_t timeFromYear(int year) { return const_Date_msPerDay * daysFromYear(year); }
    static int yearFromTime(time64_t t);
    static void getYMDFromTime(time64_t t, struct timeinfo& cachedLocal);
    static bool inLeapYear(int year);
};

class DatePrototypeObject : public DateObject {
public:
    DatePrototypeObject(ExecutionState& state, Object* proto)
        : DateObject(state, proto)
    {
    }

    virtual bool isDatePrototypeObject() const override
    {
        return true;
    }

    virtual const char* internalClassProperty(ExecutionState& state) override
    {
        return "Object";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        DateObject::fillGCDescriptor(desc);
    }
};
} // namespace Escargot
#endif
