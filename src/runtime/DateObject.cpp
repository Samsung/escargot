/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "DateObject.h"
#include "Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

#define RESOLVECACHE(state)  \
    if (m_isCacheDirty) {    \
        resolveCache(state); \
    }

#define LEAP ((int16_t)1)
enum { JAN,
       FEB,
       MAR,
       APR,
       MAY,
       JUN,
       JUL,
       AUG,
       SEP,
       OCT,
       NOV,
       DEC,
       INVALID };

enum { SUN,
       MON,
       TUE,
       WED,
       THU,
       FRI,
       SAT };

static constexpr int8_t daysInMonth[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static constexpr int16_t _firstDay(int mon)
{
    return (mon == 0) ? 0 : (_firstDay(mon - 1) + daysInMonth[mon - 1]);
}

static constexpr int16_t firstDayOfMonth[2][13] = {
    {
        _firstDay(JAN), _firstDay(FEB), _firstDay(MAR),
        _firstDay(APR), _firstDay(MAY), _firstDay(JUN),
        _firstDay(JUL), _firstDay(AUG), _firstDay(SEP),
        _firstDay(OCT), _firstDay(NOV), _firstDay(DEC),
        _firstDay(INVALID),
    },
    {
        _firstDay(JAN), _firstDay(FEB), _firstDay(MAR) + LEAP,
        _firstDay(APR) + LEAP, _firstDay(MAY) + LEAP, _firstDay(JUN) + LEAP,
        _firstDay(JUL) + LEAP, _firstDay(AUG) + LEAP, _firstDay(SEP) + LEAP,
        _firstDay(OCT) + LEAP, _firstDay(NOV) + LEAP, _firstDay(DEC) + LEAP,
        _firstDay(INVALID) + LEAP,
    },
};

static const char days[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* invalidDate = "Invalid Date";

DateObject::DateObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->datePrototype());
    setTimeValueAsNaN();
}


void DateObject::initCachedUTC(ExecutionState& state, DateObject* d)
{
    state.context()->vmInstance()->setCachedUTC(d);
}


time64_t DateObject::currentTime()
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) == 0) {
        return time.tv_sec * const_Date_msPerSecond + time.tv_nsec / const_Date_nsPerMs;
    } else {
        return TIME64NAN;
    }
}


void DateObject::setTimeValue(time64_t t)
{
    m_primitiveValue = t;
    if (IS_VALID_TIME(m_primitiveValue)) {
        m_isCacheDirty = true;
    }
}


void DateObject::setTimeValue(ExecutionState& state, const Value& v)
{
    Value pv = v.toPrimitive(state);
    if (pv.isNumber()) {
        setTimeValue(DateObject::timeClip(state, pv.asNumber()));
    } else {
        String* istr = v.toString(state);
        setTimeValue(parseStringToDate(state, istr));
    }
}


void DateObject::setTimeValue(ExecutionState& state, int year, int month,
                              int date, int hour, int minute, int64_t second,
                              int64_t millisecond, bool convertToUTC)
{
    int yearComputed = year + floor(month / (double)const_Date_monthsPerYear);
    int monthComputed = month % const_Date_monthsPerYear;
    if (monthComputed < 0)
        monthComputed = (monthComputed + const_Date_monthsPerYear) % const_Date_monthsPerYear;

    time64_t primitiveValue = timeinfoToMs(state, yearComputed, monthComputed, date, hour, minute, second, millisecond);

    if (convertToUTC) {
        primitiveValue = applyLocalTimezoneOffset(state, primitiveValue);
    }

    if (LIKELY(IS_VALID_TIME(primitiveValue)) && IS_IN_TIME_RANGE(primitiveValue)) {
        m_primitiveValue = primitiveValue;
    } else {
        setTimeValueAsNaN();
    }
    m_isCacheDirty = true;
}


// code from WebKit 3369f50e501f85e27b6e7baffd0cc7ac70931cc3
// WTF/wtf/DateMath.cpp:340
static int equivalentYearForDST(int year)
{
    // It is ok if the cached year is not the current year as long as the rules
    // for DST did not change between the two years; if they did the app would need
    // to be restarted.
    static int minYear = 2010;
    int maxYear = 2037;

    int difference;
    if (year > maxYear)
        difference = minYear - year;
    else if (year < minYear)
        difference = maxYear - year;
    else
        return year;

    int quotient = difference / 28;
    int product = quotient * 28;

    year += product;
    ASSERT((year >= minYear && year <= maxYear) || (product - year == static_cast<int>(std::numeric_limits<double>::quiet_NaN())));
    return year;
}


// Make timeinfo which assumes UTC timezone offset to
// timeinfo which assumes local timezone offset
// e.g. return (t - 32400*1000) on KST zone
time64_t DateObject::applyLocalTimezoneOffset(ExecutionState& state, time64_t t)
{
#ifdef ENABLE_ICU
    if (state.context()->vmInstance()->timezone() == NULL) {
        state.context()->vmInstance()->setTimezone();
    }
#endif

#ifdef ENABLE_ICU
    UErrorCode succ = U_ZERO_ERROR;
#endif
    int32_t stdOffset, dstOffset;

// roughly check range before calling yearFromTime function
#ifdef ENABLE_ICU
    stdOffset = state.context()->vmInstance()->timezone()->getRawOffset();
#else
    stdOffset = 0;
#endif
    if (!IS_IN_TIME_RANGE(t - stdOffset)) {
        return TIME64NAN;
    }

    // Find the equivalent year because ECMAScript spec says
    // The implementation should not try to determine
    // whether the exact time was subject to daylight saving time,
    // but just whether daylight saving time would have been in effect
    // if the current daylight saving time algorithm had been used at the time.
    int realYear = yearFromTime(t);
    int equivalentYear = equivalentYearForDST(realYear);

    time64_t msBetweenYears = (realYear != equivalentYear) ? (timeFromYear(equivalentYear) - timeFromYear(realYear)) : 0;

    t += msBetweenYears;
#ifdef ENABLE_ICU
    state.context()->vmInstance()->timezone()->getOffset(t, true, stdOffset, dstOffset, succ);
#else
    dstOffset = 0;
#endif
    t -= msBetweenYears;
#ifdef ENABLE_ICU
    // range check should be completed by caller function
    if (succ == U_ZERO_ERROR) {
        return t - (stdOffset + dstOffset);
    }
    return TIME64NAN;
#else
    return t - (stdOffset + dstOffset);
#endif
}


time64_t DateObject::timeinfoToMs(ExecutionState& state, int year, int mon, int day, int hour, int minute, int64_t second, int64_t millisecond)
{
    return (daysToMs(year, mon, day) + hour * const_Date_msPerHour + minute * const_Date_msPerMinute + second * const_Date_msPerSecond + millisecond);
}


static const struct KnownZone {
    char tzName[4];
    int tzOffset;
} known_zones[] = {
    { "UT", 0 },
    { "GMT", 0 },
    { "EST", -300 },
    { "EDT", -240 },
    { "CST", -360 },
    { "CDT", -300 },
    { "MST", -420 },
    { "MDT", -360 },
    { "PST", -480 },
    { "PDT", -420 }
};

// returns 0-11 (Jan-Dec); -1 on failure
static int findMonth(const char* monthStr)
{
    ASSERT(monthStr);
    char needle[4];
    for (int i = 0; i < 3; ++i) {
        if (!*monthStr)
            return -1;
        //        needle[i] = static_cast<char>(toASCIILower(*monthStr++));
        needle[i] = (*monthStr++) | (uint8_t)0x20;
    }
    needle[3] = '\0';
    const char* haystack = "janfebmaraprmayjunjulaugsepoctnovdec";
    const char* str = strstr(haystack, needle);
    if (str) {
        int position = static_cast<int>(str - haystack);
        if (position % 3 == 0)
            return position / 3;
    }
    return -1;
}

inline bool isASCIISpace(char c)
{
    return c <= ' ' && (c == ' ' || (c <= 0xD && c >= 0x9));
}

inline bool isASCIIDigit(char c)
{
    return c >= '0' && c <= '9';
}

inline static void skipSpacesAndComments(const char*& s)
{
    int nesting = 0;
    char ch;
    while ((ch = *s)) {
        if (!isASCIISpace(ch)) {
            if (ch == '(')
                nesting++;
            else if (ch == ')' && nesting > 0)
                nesting--;
            else if (nesting == 0)
                break;
        }
        s++;
    }
}

static bool parseInt(const char* string, char** stopPosition, int base, int* result)
{
    long longResult = strtol(string, stopPosition, base);
    // Avoid the use of errno as it is not available on Windows CE
    if (string == *stopPosition || longResult <= std::numeric_limits<int>::min() || longResult >= std::numeric_limits<int>::max())
        return false;
    *result = static_cast<int>(longResult);
    return true;
}

static bool parseLong(const char* string, char** stopPosition, int base, long* result, int digits = 0)
{
    if (digits == 0) {
        *result = strtol(string, stopPosition, base);
        // Avoid the use of errno as it is not available on Windows CE
        if (string == *stopPosition || *result == std::numeric_limits<long>::min() || *result == std::numeric_limits<long>::max())
            return false;
        return true;
    } else {
        strtol(string, stopPosition, base); // for compute stopPosition

        char s[4]; // 4 is temporary number for case (digit == 3)..
        s[0] = string[0];
        s[1] = string[1];
        s[2] = string[2];
        s[3] = '\0';

        *result = strtol(s, NULL, base);
        if (string == *stopPosition || *result == std::numeric_limits<long>::min() || *result == std::numeric_limits<long>::max())
            return false;
        return true;
    }
}

time64_t DateObject::parseStringToDate_1(ExecutionState& state, String* istr, bool& haveTZ, int& offset)
{
    haveTZ = false;
    offset = 0;

    long month = -1;
    const char* dateString = istr->toUTF8StringData().data();
    const char* wordStart = dateString;

    skipSpacesAndComments(dateString);

    while (*dateString && !isASCIIDigit(*dateString)) {
        if (isASCIISpace(*dateString) || *dateString == '(') {
            if (dateString - wordStart >= 3)
                month = findMonth(wordStart);
            skipSpacesAndComments(dateString);
            wordStart = dateString;
        } else
            dateString++;
    }

    if (month == -1 && wordStart != dateString)
        month = findMonth(wordStart);

    skipSpacesAndComments(dateString);

    if (!*dateString)
        return TIME64NAN;

    char* newPosStr;
    long int day;
    if (!parseLong(dateString, &newPosStr, 10, &day))
        return TIME64NAN;
    dateString = newPosStr;

    if (!*dateString)
        return TIME64NAN;

    if (day < 0)
        return TIME64NAN;

    int year = 0;
    if (day > 31) {
        // ### where is the boundary and what happens below?
        if (*dateString != '/')
            return TIME64NAN;
        // looks like a YYYY/MM/DD date
        if (!*++dateString)
            return TIME64NAN;
        if (day <= std::numeric_limits<int>::min() || day >= std::numeric_limits<int>::max())
            return TIME64NAN;
        year = static_cast<int>(day);
        if (!parseLong(dateString, &newPosStr, 10, &month))
            return TIME64NAN;
        month -= 1;
        dateString = newPosStr;
        if (*dateString++ != '/' || !*dateString)
            return TIME64NAN;
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return TIME64NAN;
        dateString = newPosStr;
    } else if (*dateString == '/' && month == -1) {
        dateString++;
        // This looks like a MM/DD/YYYY date, not an RFC date.
        month = day - 1; // 0-based
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return TIME64NAN;
        if (day < 1 || day > 31)
            return TIME64NAN;
        dateString = newPosStr;
        if (*dateString == '/')
            dateString++;
        if (!*dateString)
            return TIME64NAN;
    } else {
        if (*dateString == '-')
            dateString++;

        skipSpacesAndComments(dateString);

        if (*dateString == ',')
            dateString++;

        if (month == -1) { // not found yet
            month = findMonth(dateString);
            if (month == -1)
                return TIME64NAN;

            while (*dateString && *dateString != '-' && *dateString != ',' && !isASCIISpace(*dateString))
                dateString++;

            if (!*dateString)
                return TIME64NAN;

            // '-99 23:12:40 GMT'
            if (*dateString != '-' && *dateString != '/' && *dateString != ',' && !isASCIISpace(*dateString))
                return TIME64NAN;
            dateString++;
        }
    }

    if (month < 0 || month > 11)
        return TIME64NAN;

    // '99 23:12:40 GMT'
    if (year <= 0 && *dateString) {
        if (!parseInt(dateString, &newPosStr, 10, &year))
            return TIME64NAN;
    }

    // Don't fail if the time is missing.
    long hour = 0;
    long minute = 0;
    long second = 0;
    if (!*newPosStr)
        dateString = newPosStr;
    else {
        // ' 23:12:40 GMT'
        if (!(isASCIISpace(*newPosStr) || *newPosStr == ',')) {
            if (*newPosStr != ':')
                return TIME64NAN;
            // There was no year; the number was the hour.
            year = -1;
        } else {
            // in the normal case (we parsed the year), advance to the next number
            dateString = ++newPosStr;
            skipSpacesAndComments(dateString);
        }

        parseLong(dateString, &newPosStr, 10, &hour);
        // Do not check for errno here since we want to continue
        // even if errno was set becasue we are still looking
        // for the timezone!

        // Read a number? If not, this might be a timezone name.
        if (newPosStr != dateString) {
            dateString = newPosStr;

            if (hour < 0 || hour > 23)
                return TIME64NAN;

            if (!*dateString)
                return TIME64NAN;

            // ':12:40 GMT'
            if (*dateString++ != ':')
                return TIME64NAN;

            if (!parseLong(dateString, &newPosStr, 10, &minute))
                return TIME64NAN;
            dateString = newPosStr;

            if (minute < 0 || minute > 59)
                return TIME64NAN;

            // ':40 GMT'
            if (*dateString && *dateString != ':' && !isASCIISpace(*dateString))
                return TIME64NAN;

            // seconds are optional in rfc822 + rfc2822
            if (*dateString == ':') {
                dateString++;

                if (!parseLong(dateString, &newPosStr, 10, &second))
                    return TIME64NAN;
                dateString = newPosStr;

                if (second < 0 || second > 59)
                    return TIME64NAN;
            }

            skipSpacesAndComments(dateString);

            if (strncasecmp(dateString, "AM", 2) == 0) {
                if (hour > 12)
                    return TIME64NAN;
                if (hour == 12)
                    hour = 0;
                dateString += 2;
                skipSpacesAndComments(dateString);
            } else if (strncasecmp(dateString, "PM", 2) == 0) {
                if (hour > 12)
                    return TIME64NAN;
                if (hour != 12)
                    hour += 12;
                dateString += 2;
                skipSpacesAndComments(dateString);
            }
        }
    }

    // The year may be after the time but before the time zone.
    if (isASCIIDigit(*dateString) && year == -1) {
        if (!parseInt(dateString, &newPosStr, 10, &year))
            return TIME64NAN;
        dateString = newPosStr;
        skipSpacesAndComments(dateString);
    }

    // Don't fail if the time zone is missing.
    // Some websites omit the time zone (4275206).
    if (*dateString) {
        if (strncasecmp(dateString, "GMT", 3) == 0 || strncasecmp(dateString, "UTC", 3) == 0) {
            dateString += 3;
            haveTZ = true;
        }
        if (*dateString == '+' || *dateString == '-') {
            int o;
            if (!parseInt(dateString, &newPosStr, 10, &o))
                return TIME64NAN;
            dateString = newPosStr;

            if (o < -9959 || o > 9959)
                return TIME64NAN;

            int sgn = (o < 0) ? -1 : 1;
            o = abs(o);
            if (*dateString != ':') {
                if (o >= 24)
                    offset = ((o / 100) * 60 + (o % 100)) * sgn;
                else
                    offset = o * 60 * sgn;
            } else { // GMT+05:00
                ++dateString; // skip the ':'
                int o2;
                if (!parseInt(dateString, &newPosStr, 10, &o2))
                    return TIME64NAN;
                dateString = newPosStr;
                offset = (o * 60 + o2) * sgn;
            }
            haveTZ = true;
        } else {
            size_t arrlenOfTZ = sizeof(known_zones) / sizeof(struct KnownZone);
            for (size_t i = 0; i < arrlenOfTZ; ++i) {
                if (0 == strncasecmp(dateString, known_zones[i].tzName, strlen(known_zones[i].tzName))) {
                    offset = known_zones[i].tzOffset;
                    dateString += strlen(known_zones[i].tzName);
                    haveTZ = true;
                    break;
                }
            }
        }
    }
    skipSpacesAndComments(dateString);

    if (*dateString && year == -1) {
        if (!parseInt(dateString, &newPosStr, 10, &year))
            return TIME64NAN;
        dateString = newPosStr;
        skipSpacesAndComments(dateString);
    }

    // Trailing garbage
    if (*dateString)
        return TIME64NAN;

    // Y2K: Handle 2 digit years.
    if (year >= 0 && year < 100) {
        if (year < 50)
            year += 2000;
        else
            year += 1900;
    }

    return timeinfoToMs(state, year, month, day, hour, minute, second, 0);
}

static char* parseES5DatePortion(const char* currentPosition, int& year, long& month, long& day)
{
    char* postParsePosition;

    // This is a bit more lenient on the year string than ES5 specifies:
    // instead of restricting to 4 digits (or 6 digits with mandatory +/-),
    // it accepts any integer value. Consider this an implementation fallback.
    if (!parseInt(currentPosition, &postParsePosition, 10, &year))
        return 0;

    // Check for presence of -MM portion.
    if (*postParsePosition != '-')
        return postParsePosition;
    currentPosition = postParsePosition + 1;

    if (!isASCIIDigit(*currentPosition))
        return 0;
    if (!parseLong(currentPosition, &postParsePosition, 10, &month))
        return 0;
    if ((postParsePosition - currentPosition) != 2)
        return 0;

    // Check for presence of -DD portion.
    if (*postParsePosition != '-')
        return postParsePosition;
    currentPosition = postParsePosition + 1;

    if (!isASCIIDigit(*currentPosition))
        return 0;
    if (!parseLong(currentPosition, &postParsePosition, 10, &day))
        return 0;
    if ((postParsePosition - currentPosition) != 2)
        return 0;
    return postParsePosition;
}
static char* parseES5TimePortion(char* currentPosition, long& hours, long& minutes, long& milliSeconds, long& timeZoneSeconds, bool& haveTZ)
{
    char* postParsePosition;
    if (!isASCIIDigit(*currentPosition))
        return 0;
    if (!parseLong(currentPosition, &postParsePosition, 10, &hours))
        return 0;
    if (*postParsePosition != ':' || (postParsePosition - currentPosition) != 2)
        return 0;
    currentPosition = postParsePosition + 1;

    if (!isASCIIDigit(*currentPosition))
        return 0;
    if (!parseLong(currentPosition, &postParsePosition, 10, &minutes))
        return 0;
    if ((postParsePosition - currentPosition) != 2)
        return 0;
    currentPosition = postParsePosition;

    // Seconds are optional.
    if (*currentPosition == ':') {
        ++currentPosition;

        long intSeconds;
        if (!isASCIIDigit(*currentPosition))
            return 0;
        if (!parseLong(currentPosition, &postParsePosition, 10, &intSeconds))
            return 0;
        if ((postParsePosition - currentPosition) != 2)
            return 0;
        milliSeconds = intSeconds * const_Date_msPerSecond;
        if (*postParsePosition == '.') {
            currentPosition = postParsePosition + 1;

            // In ECMA-262-5 it's a bit unclear if '.' can be present without milliseconds, but
            // a reasonable interpretation guided by the given examples and RFC 3339 says "no".
            // We check the next character to avoid reading +/- timezone hours after an invalid decimal.
            if (!isASCIIDigit(*currentPosition))
                return 0;

            long fracSeconds;
            if (!parseLong(currentPosition, &postParsePosition, 10, &fracSeconds, 3))
                return 0;

            long numFracDigits = std::min((long)(postParsePosition - currentPosition), 3L);
            milliSeconds += fracSeconds * pow(10.0, static_cast<double>(3 - numFracDigits));
        }
        currentPosition = postParsePosition;
    }

    if (*currentPosition == 'Z') {
        haveTZ = true;
        return currentPosition + 1;
    }

    bool tzNegative;
    if (*currentPosition == '-')
        tzNegative = true;
    else if (*currentPosition == '+')
        tzNegative = false;
    else
        return currentPosition; // no timezone
    ++currentPosition;
    haveTZ = true;

    long tzHours;
    long tzHoursAbs;
    long tzMinutes;

    if (!isASCIIDigit(*currentPosition))
        return 0;
    if (!parseLong(currentPosition, &postParsePosition, 10, &tzHours))
        return 0;
    if (postParsePosition - currentPosition == 4) {
        tzMinutes = tzHours % 100;
        tzHours = tzHours / 100;
        tzHoursAbs = labs(tzHours);
    } else if (postParsePosition - currentPosition == 2) {
        if (*postParsePosition != ':')
            return 0;

        tzHoursAbs = labs(tzHours);
        currentPosition = postParsePosition + 1;

        if (!isASCIIDigit(*currentPosition))
            return 0;
        if (!parseLong(currentPosition, &postParsePosition, 10, &tzMinutes))
            return 0;
        if ((postParsePosition - currentPosition) != 2)
            return 0;
    } else
        return 0;

    currentPosition = postParsePosition;

    if (tzHoursAbs > 24)
        return 0;
    if (tzMinutes < 0 || tzMinutes > 59)
        return 0;

    timeZoneSeconds = 60 * (tzMinutes + (60 * tzHoursAbs));
    if (tzNegative)
        timeZoneSeconds = -timeZoneSeconds;

    return currentPosition;
}

time64_t DateObject::parseStringToDate_2(ExecutionState& state, String* istr, bool& haveTZ)
{
    haveTZ = true;
    const char* dateString = istr->toUTF8StringData().data();

    static const long const_Date_daysPerMonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // The year must be present, but the other fields may be omitted - see ES5.1 15.9.1.15.
    int year = 0;
    long month = 1;
    long day = 1;
    long hours = 0;
    long minutes = 0;
    long milliSeconds = 0;
    long timeZoneSeconds = 0;

    // Parse the date YYYY[-MM[-DD]]
    char* currentPosition = parseES5DatePortion(dateString, year, month, day);
    if (!currentPosition)
        return TIME64NAN;
    // Look for a time portion.
    if (*currentPosition == 'T') {
        haveTZ = false;
        // Parse the time HH:mm[:ss[.sss]][Z|(+|-)00:00]
        currentPosition = parseES5TimePortion(currentPosition + 1, hours, minutes, milliSeconds, timeZoneSeconds, haveTZ);
        if (!currentPosition)
            return TIME64NAN;
    }
    // Check that we have parsed all characters in the string.
    while (*currentPosition) {
        if (std::isspace(*currentPosition)) {
            currentPosition++;
        } else {
            break;
        }
    }
    if (*currentPosition)
        return TIME64NAN;

    // A few of these checks could be done inline above, but since many of them are interrelated
    // we would be sacrificing readability to "optimize" the (presumably less common) failure path.
    if (month < 1 || month > 12)
        return TIME64NAN;
    if (day < 1 || day > const_Date_daysPerMonth[month - 1])
        return TIME64NAN;
    if (month == 2 && day > 28 && daysInYear(year) != 366)
        return TIME64NAN;
    if (hours < 0 || hours > 24)
        return TIME64NAN;
    if (hours == 24 && (minutes || milliSeconds))
        return TIME64NAN;
    if (minutes < 0 || minutes > 59)
        return TIME64NAN;
    if (milliSeconds < 0 || milliSeconds >= const_Date_msPerMinute + const_Date_msPerSecond)
        return TIME64NAN;
    if (milliSeconds > const_Date_msPerMinute) {
        // Discard leap seconds by clamping to the end of a minute.
        milliSeconds = const_Date_msPerMinute;
    }
    time64_t date = timeinfoToMs(state, year, month - 1, day, hours, minutes, (int64_t)(milliSeconds / const_Date_msPerSecond), milliSeconds % const_Date_msPerSecond) - timeZoneSeconds * const_Date_msPerSecond;
    return date;
}

time64_t DateObject::parseStringToDate(ExecutionState& state, String* istr)
{
    bool haveTZ;
    int offset;
    time64_t primitiveValue = parseStringToDate_2(state, istr, haveTZ);
    if (IS_VALID_TIME(primitiveValue)) {
        if (!haveTZ) { // add local timezone offset
            primitiveValue = applyLocalTimezoneOffset(state, primitiveValue);
        }
    } else {
        primitiveValue = parseStringToDate_1(state, istr, haveTZ, offset);
        if (IS_VALID_TIME(primitiveValue)) {
            if (!haveTZ) {
                primitiveValue = applyLocalTimezoneOffset(state, primitiveValue);
            } else {
                primitiveValue = primitiveValue - (offset * const_Date_msPerMinute);
            }
        }
    }

    if (IS_VALID_TIME(primitiveValue) && IS_IN_TIME_RANGE(primitiveValue)) {
        return primitiveValue;
    } else {
        return TIME64NAN;
    }
}


inline int DateObject::daysInYear(int year)
{
    if (year % 4 != 0) {
        return const_Date_daysPerYear;
    } else if (year % 100 != 0) {
        return const_Date_daysPerLeapYear;
    } else if (year % 400 != 0) {
        return const_Date_daysPerYear;
    } else { // year % 400 == 0
        return const_Date_daysPerLeapYear;
    }
}

// The number of days from (1970.1.1) to (year.1.1)
inline int DateObject::daysFromYear(int year)
{
    if (LIKELY(year >= 1970)) {
        return 365 * (year - 1970)
            + (year - 1969) / 4
            - (year - 1901) / 100
            + (year - 1601) / 400;
    } else {
        return 365 * (year - 1970)
            + floor((year - 1969) / 4.0)
            - floor((year - 1901) / 100.0)
            + floor((year - 1601) / 400.0);
    }
}


int DateObject::yearFromTime(time64_t t)
{
    int estimate = floor(t / const_Date_msPerDay / 365.2425) + 1970;
    time64_t yearAsMs = daysToMs(estimate, 0, 1);
    if (yearAsMs > t) {
        estimate--;
    }
    if (yearAsMs + daysInYear(estimate) * const_Date_msPerDay <= t) {
        estimate++;
    }
    return estimate;
}


inline bool DateObject::inLeapYear(int year)
{
    int days = daysInYear(year);
    // assume 'days' is 365 or 366
    return (bool)(days - const_Date_daysPerYear);
}


void DateObject::getYMDFromTime(time64_t t, struct timeinfo& cachedLocal)
{
    int estimate = floor(t / const_Date_msPerDay / 365.2425) + 1970;
    time64_t yearAsMs = daysToMs(estimate, 0, 1);
    if (yearAsMs > t) {
        estimate--;
    }
    if (yearAsMs + daysInYear(estimate) * const_Date_msPerDay <= t) {
        estimate++;
    }

    cachedLocal.year = estimate;

    int dayWithinYear = daysFromTime(t) - daysFromYear(cachedLocal.year);
    ASSERT(0 <= dayWithinYear && dayWithinYear < const_Date_daysPerLeapYear);

    int leap = (int)inLeapYear(cachedLocal.year);

    for (int i = 1; i <= 12; i++) {
        if (dayWithinYear < firstDayOfMonth[leap][i]) {
            cachedLocal.month = i - 1;
            break;
        }
    }

    cachedLocal.mday = dayWithinYear + 1 - firstDayOfMonth[leap][cachedLocal.month];
}


int DateObject::daysFromMonth(int year, int month)
{
    int leap = (int)inLeapYear(year);

    return firstDayOfMonth[leap][month];
}


int DateObject::daysFromTime(time64_t t)
{
    if (t < 0)
        t -= (const_Date_msPerDay - 1);
    return static_cast<int>(t / (double)const_Date_msPerDay);
}


// This function expects m_primitiveValue is valid.
void DateObject::resolveCache(ExecutionState& state)
{
#ifdef ENABLE_ICU
    if (state.context()->vmInstance()->timezone() == NULL) {
        state.context()->vmInstance()->setTimezone();
    }
#endif

    time64_t t = m_primitiveValue;
    int realYear = yearFromTime(t);
    int equivalentYear = equivalentYearForDST(realYear);

    time64_t msBetweenYears = (realYear != equivalentYear) ? (timeFromYear(equivalentYear) - timeFromYear(realYear)) : 0;

    t += msBetweenYears;

#ifdef ENABLE_ICU
    UErrorCode succ = U_ZERO_ERROR;
#endif
    int32_t stdOffset = 0, dstOffset = 0;
#ifdef ENABLE_ICU
    state.context()->vmInstance()->timezone()->getOffset(t, false, stdOffset, dstOffset, succ);
#endif

    m_cachedLocal.isdst = dstOffset == 0 ? 0 : 1;
    m_cachedLocal.gmtoff = -1 * (stdOffset + dstOffset) / const_Date_msPerMinute;

    t += (stdOffset + dstOffset);
    t -= msBetweenYears;

    getYMDFromTime(t, m_cachedLocal);

    int days = daysFromTime(t);
    int timeInDay = static_cast<int>(t - days * const_Date_msPerDay);

    ASSERT(timeInDay >= 0);

    int weekday = (days + 4) % const_Date_daysPerWeek;
    m_cachedLocal.wday = weekday >= 0 ? weekday : weekday + const_Date_daysPerWeek;
    // Do not cast const_Date_msPer[Hour|Minute|Second] into double
    m_cachedLocal.hour = timeInDay / const_Date_msPerHour;
    m_cachedLocal.min = (timeInDay / const_Date_msPerMinute) % const_Date_minutesPerHour;
    m_cachedLocal.sec = (timeInDay / const_Date_msPerSecond) % const_Date_secondsPerMinute;
    m_cachedLocal.millisec = (timeInDay) % const_Date_msPerSecond;

    m_isCacheDirty = false;
}


time64_t DateObject::daysToMs(int year, int month, int date)
{
    ASSERT(0 <= month && month < 12);
    time64_t t = timeFromYear(year) + daysFromMonth(year, month) * const_Date_msPerDay;
    return t + (date - 1) * const_Date_msPerDay;
}


String* DateObject::toDateString(ExecutionState& state)
{
    RESOLVECACHE(state);
    char buffer[32];
    if (IS_VALID_TIME(m_primitiveValue)) {
        snprintf(buffer, sizeof(buffer), "%s %s %02d %d", days[getDay(state)], months[getMonth(state)], getDate(state), getFullYear(state));
        return new ASCIIString(buffer);
    } else {
        return new ASCIIString(invalidDate);
    }
}

String* DateObject::toTimeString(ExecutionState& state)
{
    RESOLVECACHE(state);
    char buffer[32];
    if (IS_VALID_TIME(m_primitiveValue)) {
        int tzOffsetAsMin = -getTimezoneOffset(state); // 540
        int tzOffsetHour = (tzOffsetAsMin / const_Date_minutesPerHour);
        int tzOffsetMin = ((tzOffsetAsMin / (double)const_Date_minutesPerHour) - tzOffsetHour) * 60;
        tzOffsetHour *= 100;
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%s%04d (%s)", getHours(state), getMinutes(state), getSeconds(state), (tzOffsetAsMin < 0) ? "-" : "+", std::abs(tzOffsetHour + tzOffsetMin), m_cachedLocal.isdst ? tzname[1] : tzname[0]);
        return new ASCIIString(buffer);
    } else {
        return new ASCIIString(invalidDate);
    }
}

String* DateObject::toFullString(ExecutionState& state)
{
    if (IS_VALID_TIME(m_primitiveValue)) {
        StringBuilder builder;
        builder.appendString(toDateString(state));
        builder.appendChar(' ');
        builder.appendString(toTimeString(state));
        return builder.finalize();
    } else {
        return new ASCIIString(invalidDate);
    }
}

String* DateObject::toISOString(ExecutionState& state)
{
    const char* format[2] = { "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                              "%+07d-%02d-%02dT%02d:%02d:%02d.%03dZ" };
    char buffer[64];
    if (IS_VALID_TIME(m_primitiveValue)) {
        int formatSelect;
        if (getUTCFullYear(state) >= 0 && getUTCFullYear(state) <= 9999) {
            formatSelect = 0;
        } else {
            formatSelect = 1;
        }
        snprintf(buffer, sizeof(buffer), format[formatSelect], getUTCFullYear(state), getUTCMonth(state) + 1, getUTCDate(state), getUTCHours(state), getUTCMinutes(state), getUTCSeconds(state), getUTCMilliseconds(state));
        return new ASCIIString(buffer);
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toISOString.string(), errorMessage_GlobalObject_InvalidDate);
    }
    RELEASE_ASSERT_NOT_REACHED();
}


String* DateObject::toUTCString(ExecutionState& state, String* functionName)
{
    const char* format = "%s, %02d %s %d %02d:%02d:%02d GMT";

    char buffer[64];
    if (IS_VALID_TIME(m_primitiveValue)) {
        snprintf(buffer, sizeof(buffer), format, days[getUTCDay(state)], getUTCDate(state),
                 months[getUTCMonth(state)], getUTCFullYear(state),
                 getUTCHours(state), getUTCMinutes(state), getUTCSeconds(state));
        return new ASCIIString(buffer);
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Date.string(), true, functionName, errorMessage_GlobalObject_InvalidDate);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String* DateObject::toLocaleDateString(ExecutionState& state)
{
    if (IS_VALID_TIME(m_primitiveValue)) {
#ifdef ENABLE_ICU
        icu::UnicodeString myString;
        icu::DateFormat* df = icu::DateFormat::createDateInstance(icu::DateFormat::MEDIUM, state.context()->vmInstance()->locale());
        df->format(primitiveValue(), myString);

        delete df;

        return new UTF16String(myString);
#else
        return toDateString(state);
#endif
    } else {
        return new ASCIIString(invalidDate);
    }
}

String* DateObject::toLocaleTimeString(ExecutionState& state)
{
    if (IS_VALID_TIME(m_primitiveValue)) {
#ifdef ENABLE_ICU
        icu::UnicodeString myString;
        icu::DateFormat* tf = icu::DateFormat::createTimeInstance(icu::DateFormat::MEDIUM, state.context()->vmInstance()->locale());
        tf->format(primitiveValue(), myString);

        delete tf;

        return new UTF16String(myString);
#else
        return toTimeString(state);
#endif
    } else {
        return new ASCIIString(invalidDate);
    }
}

String* DateObject::toLocaleFullString(ExecutionState& state)
{
    if (IS_VALID_TIME(m_primitiveValue)) {
        StringBuilder builder;
        builder.appendString(toLocaleDateString(state));
        builder.appendChar(' ');
        builder.appendString(toLocaleTimeString(state));
        return builder.finalize();
    } else {
        return new ASCIIString(invalidDate);
    }
}


int DateObject::getDate(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.mday;
}

int DateObject::getDay(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.wday;
}

int DateObject::getFullYear(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.year;
}

int DateObject::getHours(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.hour;
}

int DateObject::getMilliseconds(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.millisec;
}

int DateObject::getMinutes(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.min;
}

int DateObject::getMonth(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.month;
}

int DateObject::getSeconds(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.sec;
}

int DateObject::getTimezoneOffset(ExecutionState& state)
{
    RESOLVECACHE(state);
    return m_cachedLocal.gmtoff;
}


#define DECLARE_DATE_UTC_GETTER(Name)                                                        \
    int DateObject::getUTC##Name(ExecutionState& state)                                      \
    {                                                                                        \
        DateObject* cachedUTC = state.context()->vmInstance()->cachedUTC();                  \
        time64_t primitiveValueUTC                                                           \
            = m_primitiveValue + getTimezoneOffset(state) * const_Date_msPerMinute;          \
        if (!(cachedUTC->isValid()) || cachedUTC->primitiveValue() != primitiveValueUTC) {   \
            cachedUTC->setTimeValue(primitiveValueUTC);                                      \
            if (UNLIKELY(cachedUTC->getTimezoneOffset(state) != getTimezoneOffset(state))) { \
                primitiveValueUTC = m_primitiveValue                                         \
                    + cachedUTC->getTimezoneOffset(state) * const_Date_msPerMinute;          \
                cachedUTC->setTimeValue(primitiveValueUTC);                                  \
            }                                                                                \
        }                                                                                    \
        return cachedUTC->get##Name(state);                                                  \
    }

DECLARE_DATE_UTC_GETTER(Date);
DECLARE_DATE_UTC_GETTER(Day);
DECLARE_DATE_UTC_GETTER(FullYear);
DECLARE_DATE_UTC_GETTER(Hours);
DECLARE_DATE_UTC_GETTER(Milliseconds);
DECLARE_DATE_UTC_GETTER(Minutes);
DECLARE_DATE_UTC_GETTER(Month);
DECLARE_DATE_UTC_GETTER(Seconds);

void* DateObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(DateObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(DateObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(DateObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(DateObject, m_values));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(DateObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
