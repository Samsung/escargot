/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 the V8 project authors. All rights reserved.
 * Copyright (C) 2010 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
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

#ifndef __EscargotISO8601__
#define __EscargotISO8601__

#include "runtime/BigInt.h"
#include "runtime/String.h"
#include "util/Int128.h"
#include "util/Variant.h"

namespace Escargot {
class ExecutionState;
namespace ISO8601 {

enum class DateTimeUnitCategory {
    Date,
    Time,
    DateTime
};

#define PLAIN_DATETIME_UNITS(F)                                                                     \
    F(year, Year, years, Years, 0, ISO8601::DateTimeUnitCategory::Date)                             \
    F(month, Month, months, Months, 1, ISO8601::DateTimeUnitCategory::Date)                         \
    F(week, Week, weeks, Weeks, 2, ISO8601::DateTimeUnitCategory::Date)                             \
    F(day, Day, days, Days, 3, ISO8601::DateTimeUnitCategory::Date)                                 \
    F(hour, Hour, hours, Hours, 4, ISO8601::DateTimeUnitCategory::Time)                             \
    F(minute, Minute, minutes, Minutes, 5, ISO8601::DateTimeUnitCategory::Time)                     \
    F(second, Second, seconds, Seconds, 6, ISO8601::DateTimeUnitCategory::Time)                     \
    F(millisecond, Millisecond, milliseconds, Milliseconds, 7, ISO8601::DateTimeUnitCategory::Time) \
    F(microsecond, Microsecond, microseconds, Microseconds, 8, ISO8601::DateTimeUnitCategory::Time) \
    F(nanosecond, Nanosecond, nanoseconds, Nanoseconds, 9, ISO8601::DateTimeUnitCategory::Time)

enum class DateTimeUnit : uint8_t {
#define DEFINE_TYPE(name, Name, names, Names, index, category) Name,
    PLAIN_DATETIME_UNITS(DEFINE_TYPE)
#undef DEFINE_TYPE
};

DateTimeUnitCategory toDateTimeCategory(DateTimeUnit u);

DateTimeUnit toDateTimeUnit(String*);
inline Optional<DateTimeUnit> toDateTimeUnit(Optional<String*> s)
{
    if (s) {
        return toDateTimeUnit(s.value());
    }
    return NullOption;
}

#define PLAIN_DATE_UNITS(F) \
    F(year, Year)           \
    F(month, Month)         \
    F(day, Day)

#define PLAIN_YEARMONTH_UNITS(F) \
    F(year, Year)                \
    F(month, Month)

#define PLAIN_TIME_UNITS(F)     \
    F(hour, Hour)               \
    F(minute, Minute)           \
    F(second, Second)           \
    F(millisecond, Millisecond) \
    F(microsecond, Microsecond) \
    F(nanosecond, Nanosecond)

#define PLAIN_TIME_UNITS_ALPHABET_ORDER(F) \
    F(hour, Hour)                          \
    F(microsecond, Microsecond)            \
    F(millisecond, Millisecond)            \
    F(minute, Minute)                      \
    F(nanosecond, Nanosecond)              \
    F(second, Second)

enum class RoundingMode : uint8_t {
    Ceil,
    Floor,
    Expand,
    Trunc,
    HalfCeil,
    HalfFloor,
    HalfExpand,
    HalfTrunc,
    HalfEven,
};

enum class UnsignedRoundingMode : uint8_t {
    Infinity,
    Zero,
    HalfInfinity,
    HalfZero,
    HalfEven
};

class ExactTime {
public:
    static constexpr Int128 dayRangeSeconds{ 8640000000000 }; // 1e8 days
    static constexpr Int128 nsPerMicrosecond{ 1000 };
    static constexpr Int128 nsPerMillisecond{ 1000000 };
    static constexpr Int128 nsPerSecond{ 1000000000 };
    static constexpr Int128 nsPerMinute = nsPerSecond * 60;
    static constexpr Int128 nsPerHour = nsPerMinute * 60;
    static constexpr Int128 nsPerDay = nsPerHour * 24;
    static constexpr Int128 minValue = -dayRangeSeconds * nsPerSecond;
    static constexpr Int128 maxValue = dayRangeSeconds * nsPerSecond;

    ExactTime() = default;
    ExactTime(const ExactTime&) = default;
    explicit ExactTime(Int128 epochNanoseconds)
        : m_epochNanoseconds(epochNanoseconds)
    {
    }

    static ExactTime fromEpochMilliseconds(int64_t epochMilliseconds)
    {
        return ExactTime(Int128{ epochMilliseconds } * ExactTime::nsPerMillisecond);
    }
    static ExactTime fromISOPartsAndOffset(int32_t y, uint8_t mon, uint8_t d, unsigned h, unsigned min, unsigned s, unsigned ms, unsigned micros, unsigned ns, int64_t offset);

    int64_t epochMilliseconds() const
    {
        return static_cast<int64_t>(m_epochNanoseconds / ExactTime::nsPerMillisecond);
    }
    int64_t floorEpochMilliseconds() const
    {
        auto div = m_epochNanoseconds / ExactTime::nsPerMillisecond;
        auto rem = m_epochNanoseconds % ExactTime::nsPerMillisecond;
        if (rem && m_epochNanoseconds < 0)
            div -= 1;
        return static_cast<int64_t>(div);
    }
    Int128 epochNanoseconds() const
    {
        return m_epochNanoseconds;
    }

    int nanosecondsFraction() const
    {
        return static_cast<int>(m_epochNanoseconds % ExactTime::nsPerSecond);
    }

    // IsValidEpochNanoseconds ( epochNanoseconds )
    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidepochnanoseconds
    bool isValid() const
    {
        return m_epochNanoseconds >= ExactTime::minValue && m_epochNanoseconds <= ExactTime::maxValue;
    }

    ExactTime round(ExecutionState& state, unsigned increment, DateTimeUnit unit, RoundingMode roundingMode);

private:
    Int128 m_epochNanoseconds{};
};

class Duration {
    std::array<double, 10> m_data;

public:
    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldurationstring
    static Optional<Duration> parseDurationString(String* input);

    Duration()
    {
        m_data.fill(0);
    }

    Duration(const Duration& duration)
        : m_data(duration.m_data)
    {
    }

    template <typename T>
    Duration(std::initializer_list<T> list)
    {
        m_data.fill(0);
        size_t idx = 0;
        for (auto n : list) {
            m_data[idx++] = n;
        }
    }

    int sign() const
    {
        for (const auto& v : m_data) {
            if (v < 0) {
                return -1;
            }
            if (v > 0) {
                return 1;
            }
        }
        return 0;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidduration
    bool isValid() const;

    // https://tc39.es/proposal-temporal/#sec-temporal-defaulttemporallargestunit
    DateTimeUnit defaultTemporalLargestUnit() const
    {
        size_t idx = 0;
        for (auto n : m_data) {
            if (n) {
                return static_cast<DateTimeUnit>(idx);
            }
            idx++;
        }
        return DateTimeUnit::Nanosecond;
    }

    static String* typeName(ExecutionState& state, ISO8601::DateTimeUnit t);
    Int128 totalNanoseconds(ISO8601::DateTimeUnit type) const;

    double operator[](size_t idx) const
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    double operator[](ISO8601::DateTimeUnit idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    double& operator[](size_t idx)
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    double& operator[](ISO8601::DateTimeUnit idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    std::array<double, 10>::iterator begin() { return m_data.begin(); }
    std::array<double, 10>::iterator end() { return m_data.end(); }

    std::array<double, 10>::const_iterator begin() const { return m_data.begin(); }
    std::array<double, 10>::const_iterator end() const { return m_data.end(); }

#define DEFINE_GETTER(name, Name, names, Names, index, category) \
    double names() const { return m_data[index]; }
    PLAIN_DATETIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER

#define DEFINE_SETTER(name, Name, names, Names, index, category) \
    void set##Names(double v) { m_data[index] = v; }
    PLAIN_DATETIME_UNITS(DEFINE_SETTER)
#undef DEFINE_SETTER
};

class PartialDuration {
    std::array<Optional<double>, 10> m_data;

public:
    Optional<double> operator[](size_t idx) const
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    Optional<double> operator[](ISO8601::DateTimeUnit idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    Optional<double>& operator[](size_t idx)
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    Optional<double>& operator[](ISO8601::DateTimeUnit idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    std::array<Optional<double>, 10>::const_iterator begin() const { return m_data.begin(); }
    std::array<Optional<double>, 10>::const_iterator end() const { return m_data.end(); }

#define DEFINE_GETTER(name, Name, names, Names, index, category) \
    Optional<double> names() const { return m_data[index]; }
    PLAIN_DATETIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER

#define DEFINE_SETTER(name, Name, names, Names, index, category) \
    void set##Names(Optional<double> v) { m_data[index] = v; }
    PLAIN_DATETIME_UNITS(DEFINE_SETTER)
#undef DEFINE_SETTER
};

// https://tc39.es/proposal-temporal/#sec-temporal-internal-duration-records
// Represents a duration as an ISO8601::Duration (in which all time fields
// are ignored) along with an Int128 time duration that represents the sum
// of all time fields. Used to avoid losing precision in intermediate calculations.
class InternalDuration final {
public:
    InternalDuration(Duration d, Int128 t)
        : m_dateDuration(d)
        , m_time(t)
    {
    }
    InternalDuration()
        : m_dateDuration(Duration())
        , m_time(0)
    {
    }
    static constexpr Int128 maxTimeDuration = 9007199254740992 * ExactTime::nsPerSecond - 1;

    int32_t sign() const;

    int32_t timeDurationSign() const
    {
        return m_time < 0 ? -1 : m_time > 0 ? 1
                                            : 0;
    }

    Int128 time() const { return m_time; }

    Duration dateDuration() const { return m_dateDuration; }

    static InternalDuration combineDateAndTimeDuration(Duration, Int128);

private:
    // Time fields are ignored
    Duration m_dateDuration;

    // A time duration is an integer in the inclusive interval from -maxTimeDuration
    // to maxTimeDuration, where
    // maxTimeDuration = 2**53 × 10**9 - 1 = 9,007,199,254,740,991,999,999,999.
    // It represents the portion of a Temporal.Duration object that deals with time
    // units, but as a combined value of total nanoseconds.
    Int128 m_time;
};

class PlainTime {
public:
    PlainTime()
        : m_millisecond(0)
        , m_microsecond(0)
        , m_nanosecond(0)
    {
    }

    PlainTime(unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond)
        : m_hour(hour)
        , m_minute(minute)
        , m_second(second)
        , m_millisecond(millisecond)
        , m_microsecond(microsecond)
        , m_nanosecond(nanosecond)
    {
    }

#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    unsigned name() const { return m_##name; }
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD

    bool operator==(const PlainTime& other) const
    {
        return m_hour == other.m_hour && m_minute == other.m_minute
            && m_second == other.m_second && m_millisecond == other.m_millisecond
            && m_microsecond == other.m_microsecond && m_nanosecond == other.m_nanosecond;
    }

    bool operator!=(const PlainTime& other) const
    {
        return !operator==(other);
    }

private:
    uint8_t m_hour{ 0 };
    uint8_t m_minute{ 0 };
    uint8_t m_second{ 0 };
    uint32_t m_millisecond : 10;
    uint32_t m_microsecond : 10;
    uint32_t m_nanosecond : 10;
};
static_assert(sizeof(PlainTime) <= sizeof(uint64_t), "");

class PartialPlainTime {
public:
    PartialPlainTime()
    {
    }

#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    Optional<int64_t> name() const { return m_##name; }
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD

#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    void set##capitalizedName(Optional<int64_t> v) { m_##name = v; }
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD

private:
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    Optional<int64_t> m_##name;
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
};

// Note that PlainDate does not include week unit.
// year can be negative. And month and day starts with 1.
class PlainDate {
public:
    PlainDate()
        : m_year(0)
        , m_month(1)
        , m_day(1)
    {
    }

    PlainDate(int32_t year, unsigned month, unsigned day)
        : m_year(year)
        , m_month(month)
        , m_day(day)
    {
    }

    bool operator==(const PlainDate& other) const
    {
        return m_year == other.m_year && m_month == other.m_month
            && m_day == other.m_day;
    }

    bool operator!=(const PlainDate& other) const
    {
        return !operator==(other);
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-compareisodate
    int compare(const PlainDate& other) const
    {
        if (year() > other.year())
            return 1;
        if (year() < other.year())
            return -1;
        if (month() > other.month())
            return 1;
        if (month() < other.month())
            return -1;
        if (day() > other.day())
            return 1;
        if (day() < other.day())
            return -1;
        return 0;
    }

    int32_t year() const { return m_year; }
    uint8_t month() const { return m_month; }
    uint8_t day() const { return m_day; }

private:
    int32_t m_year : 21; // ECMAScript max / min date's year can be represented <= 20 bits.
    int32_t m_month : 5; // Starts with 1.
    int32_t m_day : 6; // Starts with 1.
};
static_assert(sizeof(PlainDate) == sizeof(int32_t), "");

class PlainYearMonth {
public:
    PlainYearMonth(int32_t y = 0, int32_t m = 0)
        : m_year(y)
        , m_month(m)
    {
    }

    int32_t year() const { return m_year; }
    int32_t month() const { return m_month; }

private:
    int32_t m_year;
    int32_t m_month;
};

class PlainDateTime {
public:
    PlainDateTime(const PlainDate& pd, const PlainTime& pt)
        : m_plainDate(pd)
        , m_plainTime(pt)
    {
    }

    const PlainDate& plainDate() const
    {
        return m_plainDate;
    }

    const PlainTime& plainTime() const
    {
        return m_plainTime;
    }

    bool operator==(const PlainDateTime& other) const
    {
        return m_plainDate == other.plainDate() && m_plainTime == other.plainTime();
    }

private:
    PlainDate m_plainDate;
    PlainTime m_plainTime;
};

using CalendarID = std::string;
using TimeZoneID = String*;
using TimeZone = Variant<TimeZoneID, int64_t>;

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
// Record { [[Z]], [[OffsetString]], [[Name]] }
struct TimeZoneRecord {
    bool m_z;
    Optional<int64_t> m_offset;
    Variant<TimeZoneID, int64_t> m_nameOrOffset = Variant<TimeZoneID, int64_t>::empty();

    TimeZoneRecord(bool z = false, Optional<int64_t> offset = NullOption, Variant<TimeZoneID, int64_t> nameOrOffset = Variant<TimeZoneID, int64_t>::empty())
        : m_z(z)
        , m_offset(offset)
        , m_nameOrOffset(nameOrOffset)
    {
    }
};

static constexpr unsigned minCalendarLength = 3;
static constexpr unsigned maxCalendarLength = 8;
enum class RFC9557Flag : bool { None,
                                Critical }; // "Critical" = "!" flag
enum class RFC9557Key : bool { Calendar,
                               Other };
using RFC9557Value = std::string;
struct RFC9557Annotation {
    RFC9557Flag m_flag;
    RFC9557Key m_key;
    RFC9557Value m_value;
};

struct DateTimeParseOption {
    bool parseSubMinutePrecisionForTimeZone{ true };
    bool allowTimeZoneTimeWithoutTime{ false };
};

Optional<ExactTime> parseISODateTimeWithInstantFormat(String* input);
Optional<int64_t> parseUTCOffset(String* string, DateTimeParseOption option);
Optional<TimeZoneID> parseTimeZoneName(String* string);
Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>> parseTime(String* input);
Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>> parseDateTime(String* input);
Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>, Optional<CalendarID>>> parseCalendarDateTime(String* input, DateTimeParseOption option = {});
Optional<std::tuple<PlainDate, Optional<TimeZoneRecord>, Optional<CalendarID>>> parseCalendarYearMonth(String* input, DateTimeParseOption option = {});
Optional<std::tuple<PlainDate, Optional<TimeZoneRecord>, Optional<CalendarID>>> parseCalendarMonthDay(String* input, DateTimeParseOption option = {});
Optional<String*> parseCalendarString(String* input);

// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrement
double roundNumberToIncrement(double x, double increment, RoundingMode roundingMode);
Int128 roundNumberToIncrement(Int128 x, Int128 increment, RoundingMode roundingMode);

// https://tc39.es/proposal-temporal/#sec-roundNumbertoincrementasifpositive
Int128 roundNumberToIncrementAsIfPositive(Int128 x, Int128 increment, RoundingMode roundingMode);

// https://tc39.es/proposal-temporal/#sec-getunsignedroundingmode
UnsignedRoundingMode getUnsignedRoundingMode(RoundingMode roundingMode, bool isNegative);

Int128 lengthInNanoseconds(DateTimeUnit unit);
Int128 resolveNanosecondsValueByUnit(DateTimeUnit unit);

bool isLeapYear(int year);
bool isYearWithinLimits(double year);
double dateToDaysFrom1970(int year, int month, int day);
double daysFrom1970ToYear(int year);
uint8_t daysInMonth(int32_t year, uint8_t month);
uint8_t daysInMonth(uint8_t month);
// https://tc39.es/proposal-temporal/#sec-temporal-isodatewithinlimits
bool isoDateTimeWithinLimits(int32_t year, uint8_t month, uint8_t day);
// https://tc39.es/proposal-temporal/#sec-temporal-isodatetimewithinlimits
bool isDateTimeWithinLimits(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond);
bool isoDateTimeWithinLimits(Int128 t);
bool isoDateTimeWithinLimits(PlainDateTime t);

// https://tc39.es/proposal-temporal/#sec-temporal-isvalidepochnanoseconds
bool isValidEpochNanoseconds(Int128 s);
Optional<ISO8601::PlainDate> toPlainDate(const ISO8601::Duration& duration);

class TimeConstants {
public:
    static constexpr int64_t hoursPerDay = 24;
    static constexpr int64_t minutesPerHour = 60;
    static constexpr int64_t secondsPerMinute = 60;
    static constexpr int64_t msPerSecond = 1000;
    static constexpr int64_t msPerMonth = 2592000000;
    static constexpr int64_t secondsPerHour = secondsPerMinute * minutesPerHour;
    static constexpr int64_t secondsPerDay = secondsPerHour * hoursPerDay;
    static constexpr int64_t msPerMinute = msPerSecond * secondsPerMinute;
    static constexpr int64_t msPerHour = msPerSecond * secondsPerHour;
    static constexpr int64_t msPerDay = msPerSecond * secondsPerDay;
    static constexpr int64_t maxECMAScriptTime = 8.64E15;
    static constexpr int64_t minECMAScriptTime = -8.64E15;

    static constexpr int64_t daysIn4Years = 4 * 365 + 1;
    static constexpr int64_t daysIn100Years = 25 * daysIn4Years - 1;
    static constexpr int64_t daysIn400Years = 4 * daysIn100Years + 1;
    static constexpr int64_t days1970to2000 = 30 * 365 + 7;
    static constexpr int32_t daysOffset = 1000 * daysIn400Years + 5 * daysIn400Years - days1970to2000;
    static constexpr int32_t yearsOffset = 400000;
};

} // namespace ISO8601
} // namespace Escargot

#endif
