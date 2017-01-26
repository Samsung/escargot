#include "Escargot.h"
#include "DateObject.h"
#include "Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

static const int64_t MaximumDatePrimitiveValue = 8640000000000000;


static constexpr double hoursPerDay = 24.0;
static constexpr double minutesPerHour = 60.0;
static constexpr double secondsPerMinute = 60.0;
static constexpr double secondsPerHour = secondsPerMinute * minutesPerHour;
static constexpr double msPerSecond = 1000.0;
static constexpr double msPerMinute = msPerSecond * secondsPerMinute;
static constexpr double msPerHour = msPerSecond * secondsPerHour;
static constexpr double msPerDay = msPerHour * hoursPerDay;

DateObject::DateObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->datePrototype());
    setTimeValueAsNaN();
}

double DateObject::currentTime()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time.tv_sec * msPerSecond + floor(time.tv_nsec / 1000000);
}

void DateObject::setTimeValue(ExecutionState& state, const Value& str)
{
    String* istr = str.toString(state);

    time64IncludingNaN primitiveValue = parseStringToDate(state, istr);
    if (std::isnan(primitiveValue)) {
        setTimeValueAsNaN();
    } else {
        m_primitiveValue = (time64_t)primitiveValue;
        m_isCacheDirty = true;
        m_hasValidDate = true;
    }
}


void DateObject::setTimeValue(ExecutionState& state, int year, int month, int date, int hour, int minute, int64_t second, int64_t millisecond, bool convertToUTC)
{
    int ym = year + floor(month / 12.0);
    int mn = month % 12;
    if (mn < 0)
        mn = (mn + 12) % 12;

    time64IncludingNaN primitiveValue = ymdhmsToSeconds(state, ym, mn, date, hour, minute, second) * msPerSecond + millisecond;

    if (convertToUTC) {
        primitiveValue = applyLocalTimezoneOffset(state, (time64_t)primitiveValue);
    }

    if (LIKELY(!std::isnan(primitiveValue)) && primitiveValue <= MaximumDatePrimitiveValue && primitiveValue >= -MaximumDatePrimitiveValue) {
        m_hasValidDate = true;
        m_primitiveValue = (time64_t)primitiveValue;
        resolveCache(state);
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
    int product = (quotient)*28;

    year += product;
    ASSERT((year >= minYear && year <= maxYear) || (product - year == static_cast<int>(std::numeric_limits<double>::quiet_NaN())));
    return year;
}

time64IncludingNaN DateObject::applyLocalTimezoneOffset(ExecutionState& state, time64_t t)
{
    if (state.context()->vmInstance()->timezone() == NULL) {
        state.context()->vmInstance()->setTimezone(icu::TimeZone::createTimeZone(state.context()->vmInstance()->timezoneID()));
    }

    UErrorCode succ = U_ZERO_ERROR;
    time64IncludingNaN primitiveValue = t;
    int32_t stdOffset, dstOffset;

    // roughly check range before calling yearFromTime function
    stdOffset = state.context()->vmInstance()->timezone()->getRawOffset();
    primitiveValue -= stdOffset;
    if (primitiveValue > MaximumDatePrimitiveValue || primitiveValue < -MaximumDatePrimitiveValue) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Find the equivalent year because ECMAScript spec says
    // The implementation should not try to determine
    // whether the exact time was subject to daylight saving time,
    // but just whether daylight saving time would have been in effect
    // if the current daylight saving time algorithm had been used at the time.
    primitiveValue = t;
    int year = yearFromTime(t);
    int equivalentYear = equivalentYearForDST(year);
    if (year != equivalentYear) {
        primitiveValue = primitiveValue - timeFromYear(year) + timeFromYear(equivalentYear);
    }

    state.context()->vmInstance()->timezone()->getOffset(primitiveValue, true, stdOffset, dstOffset, succ);

    primitiveValue = t - stdOffset - dstOffset;

    // range check should be completed by caller function
    if (succ == U_ZERO_ERROR) {
        return primitiveValue;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

time64_t DateObject::ymdhmsToSeconds(ExecutionState& state, int year, int mon, int day, int hour, int minute, int64_t second)
{
    return (makeDay(year, mon, day) * msPerDay + (hour * msPerHour + minute * msPerMinute + second * msPerSecond /* + millisecond */)) / 1000.0;
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

time64IncludingNaN DateObject::parseStringToDate_1(ExecutionState& state, String* istr, bool& haveTZ, int& offset)
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
        return std::numeric_limits<double>::quiet_NaN();

    char* newPosStr;
    long int day;
    if (!parseLong(dateString, &newPosStr, 10, &day))
        return std::numeric_limits<double>::quiet_NaN();
    dateString = newPosStr;

    if (!*dateString)
        return std::numeric_limits<double>::quiet_NaN();

    if (day < 0)
        return std::numeric_limits<double>::quiet_NaN();

    int year = 0;
    if (day > 31) {
        // ### where is the boundary and what happens below?
        if (*dateString != '/')
            return std::numeric_limits<double>::quiet_NaN();
        // looks like a YYYY/MM/DD date
        if (!*++dateString)
            return std::numeric_limits<double>::quiet_NaN();
        if (day <= std::numeric_limits<int>::min() || day >= std::numeric_limits<int>::max())
            return std::numeric_limits<double>::quiet_NaN();
        year = static_cast<int>(day);
        if (!parseLong(dateString, &newPosStr, 10, &month))
            return std::numeric_limits<double>::quiet_NaN();
        month -= 1;
        dateString = newPosStr;
        if (*dateString++ != '/' || !*dateString)
            return std::numeric_limits<double>::quiet_NaN();
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return std::numeric_limits<double>::quiet_NaN();
        dateString = newPosStr;
    } else if (*dateString == '/' && month == -1) {
        dateString++;
        // This looks like a MM/DD/YYYY date, not an RFC date.
        month = day - 1; // 0-based
        if (!parseLong(dateString, &newPosStr, 10, &day))
            return std::numeric_limits<double>::quiet_NaN();
        if (day < 1 || day > 31)
            return std::numeric_limits<double>::quiet_NaN();
        dateString = newPosStr;
        if (*dateString == '/')
            dateString++;
        if (!*dateString)
            return std::numeric_limits<double>::quiet_NaN();
    } else {
        if (*dateString == '-')
            dateString++;

        skipSpacesAndComments(dateString);

        if (*dateString == ',')
            dateString++;

        if (month == -1) { // not found yet
            month = findMonth(dateString);
            if (month == -1)
                return std::numeric_limits<double>::quiet_NaN();

            while (*dateString && *dateString != '-' && *dateString != ',' && !isASCIISpace(*dateString))
                dateString++;

            if (!*dateString)
                return std::numeric_limits<double>::quiet_NaN();

            // '-99 23:12:40 GMT'
            if (*dateString != '-' && *dateString != '/' && *dateString != ',' && !isASCIISpace(*dateString))
                return std::numeric_limits<double>::quiet_NaN();
            dateString++;
        }
    }

    if (month < 0 || month > 11)
        return std::numeric_limits<double>::quiet_NaN();

    // '99 23:12:40 GMT'
    if (year <= 0 && *dateString) {
        if (!parseInt(dateString, &newPosStr, 10, &year))
            return std::numeric_limits<double>::quiet_NaN();
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
                return std::numeric_limits<double>::quiet_NaN();
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
                return std::numeric_limits<double>::quiet_NaN();

            if (!*dateString)
                return std::numeric_limits<double>::quiet_NaN();

            // ':12:40 GMT'
            if (*dateString++ != ':')
                return std::numeric_limits<double>::quiet_NaN();

            if (!parseLong(dateString, &newPosStr, 10, &minute))
                return std::numeric_limits<double>::quiet_NaN();
            dateString = newPosStr;

            if (minute < 0 || minute > 59)
                return std::numeric_limits<double>::quiet_NaN();

            // ':40 GMT'
            if (*dateString && *dateString != ':' && !isASCIISpace(*dateString))
                return std::numeric_limits<double>::quiet_NaN();

            // seconds are optional in rfc822 + rfc2822
            if (*dateString == ':') {
                dateString++;

                if (!parseLong(dateString, &newPosStr, 10, &second))
                    return std::numeric_limits<double>::quiet_NaN();
                dateString = newPosStr;

                if (second < 0 || second > 59)
                    return std::numeric_limits<double>::quiet_NaN();
            }

            skipSpacesAndComments(dateString);

            if (strncasecmp(dateString, "AM", 2) == 0) {
                if (hour > 12)
                    return std::numeric_limits<double>::quiet_NaN();
                if (hour == 12)
                    hour = 0;
                dateString += 2;
                skipSpacesAndComments(dateString);
            } else if (strncasecmp(dateString, "PM", 2) == 0) {
                if (hour > 12)
                    return std::numeric_limits<double>::quiet_NaN();
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
            return std::numeric_limits<double>::quiet_NaN();
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
                return std::numeric_limits<double>::quiet_NaN();
            dateString = newPosStr;

            if (o < -9959 || o > 9959)
                return std::numeric_limits<double>::quiet_NaN();

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
                    return std::numeric_limits<double>::quiet_NaN();
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
            return std::numeric_limits<double>::quiet_NaN();
        dateString = newPosStr;
        skipSpacesAndComments(dateString);
    }

    // Trailing garbage
    if (*dateString)
        return std::numeric_limits<double>::quiet_NaN();

    // Y2K: Handle 2 digit years.
    if (year >= 0 && year < 100) {
        if (year < 50)
            year += 2000;
        else
            year += 1900;
    }

    return ymdhmsToSeconds(state, year, month, day, hour, minute, second) * msPerSecond;
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
static char* parseES5TimePortion(char* currentPosition, long& hours, long& minutes, double& seconds, long& timeZoneSeconds, bool& haveTZ)
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
        seconds = intSeconds;
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
            seconds += fracSeconds * pow(10.0, static_cast<double>(-numFracDigits));
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

time64IncludingNaN DateObject::parseStringToDate_2(ExecutionState& state, String* istr, bool& haveTZ)
{
    haveTZ = true;
    const char* dateString = istr->toUTF8StringData().data();

    static const long daysPerMonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // The year must be present, but the other fields may be omitted - see ES5.1 15.9.1.15.
    int year = 0;
    long month = 1;
    long day = 1;
    long hours = 0;
    long minutes = 0;
    double seconds = 0;
    long timeZoneSeconds = 0;

    // Parse the date YYYY[-MM[-DD]]
    char* currentPosition = parseES5DatePortion(dateString, year, month, day);
    if (!currentPosition)
        return std::numeric_limits<double>::quiet_NaN();
    // Look for a time portion.
    if (*currentPosition == 'T') {
        haveTZ = false;
        // Parse the time HH:mm[:ss[.sss]][Z|(+|-)00:00]
        currentPosition = parseES5TimePortion(currentPosition + 1, hours, minutes, seconds, timeZoneSeconds, haveTZ);
        if (!currentPosition)
            return std::numeric_limits<double>::quiet_NaN();
    }
    // Check that we have parsed all characters in the string.
    if (*currentPosition)
        return std::numeric_limits<double>::quiet_NaN();

    // A few of these checks could be done inline above, but since many of them are interrelated
    // we would be sacrificing readability to "optimize" the (presumably less common) failure path.
    if (month < 1 || month > 12)
        return std::numeric_limits<double>::quiet_NaN();
    if (day < 1 || day > daysPerMonth[month - 1])
        return std::numeric_limits<double>::quiet_NaN();
    if (month == 2 && day > 28 && daysInYear(year) != 366)
        return std::numeric_limits<double>::quiet_NaN();
    if (hours < 0 || hours > 24)
        return std::numeric_limits<double>::quiet_NaN();
    if (hours == 24 && (minutes || seconds))
        return std::numeric_limits<double>::quiet_NaN();
    if (minutes < 0 || minutes > 59)
        return std::numeric_limits<double>::quiet_NaN();
    if (seconds < 0 || seconds >= 61)
        return std::numeric_limits<double>::quiet_NaN();
    if (seconds > 60) {
        // Discard leap seconds by clamping to the end of a minute.
        seconds = 60;
    }

    double dateSeconds = ymdhmsToSeconds(state, year, month - 1, day, hours, minutes, (int)seconds) - timeZoneSeconds;
    return dateSeconds * msPerSecond + (seconds - (int)seconds) * msPerSecond;
}

time64IncludingNaN DateObject::parseStringToDate(ExecutionState& state, String* istr)
{
    bool haveTZ;
    int offset;
    time64IncludingNaN primitiveValue = parseStringToDate_2(state, istr, haveTZ);
    if (!std::isnan(primitiveValue)) {
        if (!haveTZ) { // add local timezone offset
            primitiveValue = applyLocalTimezoneOffset(state, primitiveValue);
        }
    } else {
        primitiveValue = parseStringToDate_1(state, istr, haveTZ, offset);
        if (!std::isnan(primitiveValue)) {
            if (!haveTZ) {
                primitiveValue = applyLocalTimezoneOffset(state, primitiveValue);
            } else {
                primitiveValue = primitiveValue - (offset * msPerMinute);
            }
        }
    }

    if (primitiveValue <= MaximumDatePrimitiveValue && primitiveValue >= -MaximumDatePrimitiveValue) {
        return primitiveValue;
    } else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}


int DateObject::daysInYear(int year)
{
    long y = year;
    if (y % 4 != 0) {
        return 365;
    } else if (y % 100 != 0) {
        return 366;
    } else if (y % 400 != 0) {
        return 365;
    } else { // y % 400 == 0
        return 366;
    }
}

int DateObject::dayFromYear(int year) // day number of the first day of year 'y'
{
    return 365 * (year - 1970) + floor((year - 1969) / 4.0) - floor((year - 1901) / 100.0) + floor((year - 1601) / 400.0);
}

int DateObject::yearFromTime(time64_t t)
{
    long estimate = ceil(t / msPerDay / 365.0) + 1970;

    while (makeDay(estimate, 0, 1) * msPerDay > t) {
        estimate--;
    }

    while (makeDay(estimate + 1, 0, 1) * msPerDay <= t) {
        estimate++;
    }


    return estimate;
}

int DateObject::inLeapYear(time64_t t)
{
    int days = daysInYear(yearFromTime(t));
    if (days == 365) {
        return 0;
    } else if (days == 366) {
        return 1;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

int DateObject::dayFromMonth(int year, int month)
{
    int ds[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (daysInYear(year) == 366) {
        ds[1] = 29;
    }
    int retval = 0;
    for (int i = 0; i < month; i++) {
        retval += ds[i];
    }
    return retval;
}

int DateObject::monthFromTime(time64_t t)
{
    int dayWithinYear = day(t) - dayFromYear(yearFromTime(t));
    int leap = inLeapYear(t);
    if (dayWithinYear < 0) {
        RELEASE_ASSERT_NOT_REACHED();
    } else if (dayWithinYear < 31) {
        return 0;
    } else if (dayWithinYear < 59 + leap) {
        return 1;
    } else if (dayWithinYear < 90 + leap) {
        return 2;
    } else if (dayWithinYear < 120 + leap) {
        return 3;
    } else if (dayWithinYear < 151 + leap) {
        return 4;
    } else if (dayWithinYear < 181 + leap) {
        return 5;
    } else if (dayWithinYear < 212 + leap) {
        return 6;
    } else if (dayWithinYear < 243 + leap) {
        return 7;
    } else if (dayWithinYear < 273 + leap) {
        return 8;
    } else if (dayWithinYear < 304 + leap) {
        return 9;
    } else if (dayWithinYear < 334 + leap) {
        return 10;
    } else if (dayWithinYear < 365 + leap) {
        return 11;
    } else {
        RELEASE_ASSERT_NOT_REACHED();
    }
}

int DateObject::dateFromTime(time64_t t)
{
    int dayWithinYear = day(t) - dayFromYear(yearFromTime(t));
    int leap = inLeapYear(t);
    int retval = dayWithinYear - leap;
    switch (monthFromTime(t)) {
    case 0:
        return retval + 1 + leap;
    case 1:
        return retval - 30 + leap;
    case 2:
        return retval - 58;
    case 3:
        return retval - 89;
    case 4:
        return retval - 119;
    case 5:
        return retval - 150;
    case 6:
        return retval - 180;
    case 7:
        return retval - 211;
    case 8:
        return retval - 242;
    case 9:
        return retval - 272;
    case 10:
        return retval - 303;
    case 11:
        return retval - 333;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

int DateObject::daysFromTime(time64_t t)
{
    if (t < 0)
        t -= (msPerDay - 1);
    return static_cast<int>(t / msPerDay);
}

void DateObject::computeLocalTime(ExecutionState& state, time64_t t, struct tm& tm)
{
    if (state.context()->vmInstance()->timezone() == NULL) {
        state.context()->vmInstance()->setTimezone(icu::TimeZone::createTimeZone(state.context()->vmInstance()->timezoneID()));
    }

    UErrorCode succ = U_ZERO_ERROR;
    int32_t stdOffset, dstOffset;
    state.context()->vmInstance()->timezone()->getOffset(t, false, stdOffset, dstOffset, succ);

    tm.tm_isdst = dstOffset == 0 ? 0 : 1;
    tm.tm_gmtoff = (stdOffset + dstOffset) / 1000;

    t = t + stdOffset + dstOffset;

    int days = daysFromTime(t);
    int timeInDay = static_cast<int>(t - days * msPerDay);
    ASSERT(timeInDay >= 0);
    tm.tm_year = yearFromTime(t) - 1900;
    tm.tm_mon = monthFromTime(t);
    tm.tm_mday = dateFromTime(t);
    int weekday = (days + 4) % 7;
    tm.tm_wday = weekday >= 0 ? weekday : weekday + 7;
    tm.tm_hour = timeInDay / (int)msPerHour;
    tm.tm_min = (timeInDay / (int)msPerMinute) % 60;
    tm.tm_sec = (timeInDay / (int)msPerSecond) % 60;
    // tm.tm_yday
}

time64_t DateObject::makeDay(int year, int month, int date)
{
    // TODO: have to check whether year or month is infinity
    //  if(year == infinity || month == infinity){
    //      return nan;
    //  }

    // adjustment on argument[0],[1] is performed at setTimeValue(with 7 arguments) function
    ASSERT(0 <= month && month < 12);
    long ym = year;
    int mn = month;
    time64_t t = timeFromYear(ym) + dayFromMonth(ym, mn) * msPerDay;
    return day(t) + date - 1;
}

void DateObject::resolveCache(ExecutionState& state)
{
    if (m_isCacheDirty) {
        int realyear = yearFromTime(m_primitiveValue);
        int equivalentYear = equivalentYearForDST(realyear);
        time64_t primitiveValueForDST = m_primitiveValue;

        if (realyear != equivalentYear) {
            primitiveValueForDST = primitiveValueForDST + (timeFromYear(equivalentYear) - timeFromYear(realyear));
        }
        computeLocalTime(state, primitiveValueForDST, m_cachedTM);
        m_cachedTM.tm_year += (realyear - equivalentYear);
        m_isCacheDirty = false;
    }
}

String* DateObject::toDateString(ExecutionState& state)
{
    static char days[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    resolveCache(state);
    char buffer[512];
    if (!std::isnan(primitiveValue())) {
        snprintf(buffer, 512, "%s %s %02d %d", days[getDay(state)], months[getMonth(state)], getDate(state), getFullYear(state));
        return new ASCIIString(buffer);
    } else {
        return new ASCIIString("Invalid Date");
    }
}

String* DateObject::toTimeString(ExecutionState& state)
{
    resolveCache(state);
    char buffer[512];
    if (!std::isnan(primitiveValue())) {
        int gmt = getTimezoneOffset(state) / -36;
        snprintf(buffer, 512, "%02d:%02d:%02d GMT%s%04d (%s)", getHours(state), getMinutes(state), (int)getSeconds(state), (gmt < 0) ? "-" : "+", std::abs(gmt), m_cachedTM.tm_isdst ? tzname[1] : tzname[0]);
        return new ASCIIString(buffer);
    } else {
        return new ASCIIString("Invalid Date");
    }
}

String* DateObject::toFullString(ExecutionState& state)
{
    if (!std::isnan(primitiveValue())) {
        resolveCache(state);
        StringBuilder builder;
        builder.appendString(toDateString(state));
        builder.appendChar(' ');
        builder.appendString(toTimeString(state));
        return builder.finalize();
    } else {
        return new ASCIIString("Invalid Date");
    }
}

String* DateObject::toISOString(ExecutionState& state)
{
    char buffer[512];
    if (!std::isnan(primitiveValue())) {
        resolveCache(state);
        if (getUTCFullYear(state) >= 0 && getUTCFullYear(state) <= 9999) {
            snprintf(buffer, 512, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", getUTCFullYear(state), getUTCMonth(state) + 1, getUTCDate(state), getUTCHours(state), getUTCMinutes(state), getUTCSeconds(state), getUTCMilliseconds(state));
        } else {
            snprintf(buffer, 512, "%+07d-%02d-%02dT%02d:%02d:%02d.%03dZ", getUTCFullYear(state), getUTCMonth(state) + 1, getUTCDate(state), getUTCHours(state), getUTCMinutes(state), getUTCSeconds(state), getUTCMilliseconds(state));
        }
        return new ASCIIString(buffer);
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toISOString.string(), errorMessage_GlobalObject_InvalidDate);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

int DateObject::getDate(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_mday;
}

int DateObject::getDay(ExecutionState& state)
{
    int ret = ((int)day(m_primitiveValue - getTimezoneOffset(state) * msPerSecond) + 4) % 7;
    return (ret < 0) ? (ret + 7) % 7 : ret;
}

int DateObject::getFullYear(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_year + 1900;
}

int DateObject::getHours(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_hour;
}

int DateObject::getMilliseconds(ExecutionState& state)
{
    int ret = m_primitiveValue % (int)msPerSecond;
    return (ret < 0) ? (ret + (int)msPerSecond) % (int)msPerSecond : ret;
}

int DateObject::getMinutes(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_min;
}

int DateObject::getMonth(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_mon;
}

int DateObject::getSeconds(ExecutionState& state)
{
    resolveCache(state);
    return m_cachedTM.tm_sec;
}

int DateObject::getTimezoneOffset(ExecutionState& state)
{
    resolveCache(state);
    return -1 * m_cachedTM.tm_gmtoff;
}

void DateObject::setTime(double t)
{
    if (std::isnan(t)) {
        setTimeValueAsNaN();
        return;
    }

    if (t <= MaximumDatePrimitiveValue && t >= -MaximumDatePrimitiveValue) {
        m_primitiveValue = floor(t);
        m_isCacheDirty = true;
        m_hasValidDate = true;
    } else {
        setTimeValueAsNaN();
    }
}

int DateObject::getUTCDate(ExecutionState& state)
{
    return dateFromTime(m_primitiveValue);
}

int DateObject::getUTCDay(ExecutionState& state)
{
    int ret = (int)(day(m_primitiveValue) + 4) % 7;
    return (ret < 0) ? (ret + 7) % 7 : ret;
}

int DateObject::getUTCFullYear(ExecutionState& state)
{
    return yearFromTime(m_primitiveValue);
}

int DateObject::getUTCHours(ExecutionState& state)
{
    int ret = (long long)floor(m_primitiveValue / msPerHour) % (int)hoursPerDay;
    return (ret < 0) ? (ret + (int)hoursPerDay) % (int)hoursPerDay : ret;
}

int DateObject::getUTCMilliseconds(ExecutionState& state)
{
    int ret = m_primitiveValue % (int)msPerSecond;
    return (ret < 0) ? (ret + (int)msPerSecond) % (int)msPerSecond : ret;
}

int DateObject::getUTCMinutes(ExecutionState& state)
{
    int ret = (long long)floor(m_primitiveValue / msPerMinute) % (int)minutesPerHour;
    return (ret < 0) ? (ret + (int)minutesPerHour) % (int)minutesPerHour : ret;
}

int DateObject::getUTCMonth(ExecutionState& state)
{
    return monthFromTime(m_primitiveValue);
}

int DateObject::getUTCSeconds(ExecutionState& state)
{
    int ret = (long long)floor(m_primitiveValue / msPerSecond) % (int)secondsPerMinute;
    return (ret < 0) ? (ret + (int)secondsPerMinute) % (int)secondsPerMinute : ret;
}

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
