#ifndef __EscargotDateObject__
#define __EscargotDateObject__

#include "runtime/Object.h"

namespace Escargot {

typedef int64_t time64_t;
typedef double time64IncludingNaN;

class DateObject : public Object {
public:
    DateObject(ExecutionState& state);

    static double currentTime();

    double primitiveValue()
    {
        if (LIKELY(m_hasValidDate))
            return m_primitiveValue;
        return std::numeric_limits<double>::quiet_NaN();
    }

    void setPrimitiveValue(double primitiveValue)
    {
        if (std::isnan(primitiveValue)) {
            setTimeValueAsNaN();
            return;
        }
        m_primitiveValue = primitiveValue;
    }

    void setTimeValueAsNaN()
    {
        m_primitiveValue = 0;
        m_hasValidDate = false;
    }

    virtual bool isDateObject()
    {
        return true;
    }

    static time64IncludingNaN applyLocalTimezoneOffset(ExecutionState& state, time64_t t);
    static time64_t ymdhmsToSeconds(ExecutionState& state, int year, int month, int day, int hour, int minute, int64_t second);
    static time64IncludingNaN timeClip(ExecutionState& state, double V)
    {
        if (std::isinf(V) || std::isnan(V)) {
            return nan("0");
        } else if (std::abs(V) >= 8640000000000000.0) {
            return nan("0");
        } else {
            return Value(V).toInteger(state);
        }
    }

    String* toDateString(ExecutionState& state);
    String* toTimeString(ExecutionState& state);
    String* toFullString(ExecutionState& state);
    String* toISOString(ExecutionState& state);
    int getDate(ExecutionState& state);
    int getDay(ExecutionState& state);
    int getFullYear(ExecutionState& state);
    int getHours(ExecutionState& state);
    int64_t getMilliseconds(ExecutionState& state);
    int getMinutes(ExecutionState& state);
    int getMonth(ExecutionState& state);
    int64_t getSeconds(ExecutionState& state);
    int getTimezoneOffset(ExecutionState& state);
    int getUTCDate(ExecutionState& state);
    int getUTCDay(ExecutionState& state);
    int getUTCFullYear(ExecutionState& state);
    int getUTCHours(ExecutionState& state);
    int64_t getUTCMilliseconds(ExecutionState& state);
    int getUTCMinutes(ExecutionState& state);
    int getUTCMonth(ExecutionState& state);
    int64_t getUTCSeconds(ExecutionState& state);

    void setTimeValue(ExecutionState& state, const Value& str);
    void setTimeValue(ExecutionState& state, time64IncludingNaN t)
    {
        m_hasValidDate = true;
        setPrimitiveValue(t);
    }
    void setTimeValue(ExecutionState& state, int year, int month, int date, int hour, int minute, int64_t second, int64_t millisecond, bool convertToUTC = true);
    inline bool isValid()
    {
        return m_hasValidDate;
    }
    void setTime(time64IncludingNaN t);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Date";
    }

private:
    struct tm m_cachedTM; // it stores time disregarding timezone
    int64_t m_primitiveValue; // it stores timevalue regarding timezone
    bool m_isCacheDirty;
    bool m_hasValidDate; // function get***() series (in ESValue.cpp) should check if the timevalue is valid with this flag
    int m_timezone;

    void resolveCache(ExecutionState& state);
    static time64IncludingNaN parseStringToDate(ExecutionState& state, String* istr);
    static time64IncludingNaN parseStringToDate_1(ExecutionState& state, String* istr, bool& haveTZ, int& offset);
    static time64IncludingNaN parseStringToDate_2(ExecutionState& state, String* istr, bool& haveTZ);
    static constexpr double hoursPerDay = 24.0;
    static constexpr double minutesPerHour = 60.0;
    static constexpr double secondsPerMinute = 60.0;
    static constexpr double secondsPerHour = secondsPerMinute * minutesPerHour;
    static constexpr double msPerSecond = 1000.0;
    static constexpr double msPerMinute = msPerSecond * secondsPerMinute;
    static constexpr double msPerHour = msPerSecond * secondsPerHour;
    static constexpr double msPerDay = msPerHour * hoursPerDay;
    static double day(time64_t t) { return floor(t / msPerDay); }
    static int timeWithinDay(time64_t t) { return t % (int)msPerDay; }
    static int daysInYear(int year);
    static int dayFromYear(int year);
    static time64_t timeFromYear(int year) { return msPerDay * dayFromYear(year); }
    static int yearFromTime(time64_t t);
    static int inLeapYear(time64_t t);
    static int dayFromMonth(int year, int month);
    static int monthFromTime(time64_t t);
    static int dateFromTime(time64_t t);
    static int daysFromTime(time64_t t); // return the number of days after 1970.1.1
    static time64_t makeDay(int year, int month, int date);
    static void computeLocalTime(ExecutionState& state, time64_t t, struct tm& tm);
};
}

#endif
