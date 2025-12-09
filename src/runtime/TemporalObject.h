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

template <typename T>
T nonNegativeModulo(T x, int y)
{
    T result = x % y;
    if (!result)
        return 0;
    if (result < 0)
        result += y;
    return result;
}

template <typename T>
T intFloor(T x, int64_t y)
{
    if (x > 0) {
        return x / y;
    }
    if (x % y) {
        return x / y - 1;
    } else {
        return x / y;
    }
}

// https://github.com/tc39/proposal-intl-era-monthcode
class Calendar {
public:
    // sync with 'canonicalCodeForDisplayNames'
#define CALENDAR_ID_RECORDS(F)                                                      \
    F(ISO8601, "iso8601", "iso8601", "iso8601")                                     \
    F(Buddhist, "buddhist", "buddhist", "buddhist")                                 \
    F(Chinese, "chinese", "chinese", "chinese")                                     \
    F(Coptic, "coptic", "coptic", "coptic")                                         \
    F(Dangi, "dangi", "dangi", "dangi")                                             \
    F(Ethiopian, "ethiopic", "ethiopic", "ethiopic")                                \
    F(EthiopianAmeteAlem, "ethioaa", "ethiopic-amete-alem", "ethioaa")              \
    F(Gregorian, "gregory", "gregorian", "gregory")                                 \
    F(Hebrew, "hebrew", "hebrew", "hebrew")                                         \
    F(Indian, "indian", "indian", "indian")                                         \
    F(Islamic, "islamic", "islamic", "islamic")                                     \
    F(IslamicCivil, "islamic-civil", "islamic-civil", "islamic-civil")              \
    F(IslamicCivilLegacy, "islamicc", "islamic-civil", "islamic-civil")             \
    F(IslamicRGSA, "islamic-rgsa", "islamic-rgsa", "islamic-rgsa")                  \
    F(IslamicTabular, "islamic-tbla", "islamic-tbla", "islamic-tbla")               \
    F(IslamicUmmAlQura, "islamic-umalqura", "islamic-umalqura", "islamic-umalqura") \
    F(Japanese, "japanese", "japanese", "japanese")                                 \
    F(Persian, "persian", "persian", "persian")                                     \
    F(ROC, "roc", "roc", "roc")

    enum class ID : int32_t {
#define DEFINE_FIELD(name, string, icuString, fullName) name,
        CALENDAR_ID_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD
    };

    Calendar(ID id = ID::ISO8601)
        : m_id(id)
    {
    }

    bool operator==(const Calendar& c) const
    {
        return toICUString() == c.toICUString();
    }

    bool operator!=(const Calendar& c) const
    {
        return !operator==(c);
    }

    bool isISO8601() const
    {
        return m_id == ID::ISO8601;
    }

    ID id() const
    {
        return m_id;
    }

    bool isEraRelated() const;
    bool shouldUseICUExtendedYear() const;
    bool hasLeapMonths() const;
    bool hasEpagomenalMonths() const;
    bool sameAsGregoryExceptHandlingEraAndYear() const;

    // https://tc39.es/proposal-intl-era-monthcode/#table-epoch-years
    int32_t epochISOYear() const;

    // icu4c base year of chinese, dangi, roc are differ with icu4x
    int diffYearDueToICU4CAndSpecDiffer() const;

    // in icu4c, hebrew calendar needs UCAL_ORIDINAL_CODE for everywhere
    UCalendarDateFields icuNonOridnalMonthCode() const;

    static Optional<Calendar> fromString(ISO8601::CalendarID);
    static Optional<Calendar> fromString(String* str);
    String* toString() const;
    std::string toICUString() const;
    UCalendar* createICUCalendar(ExecutionState& state);
    void lookupICUEra(ExecutionState& state, const std::function<bool(size_t idx, const std::string& icuEra)>& fn) const;

    void setYear(ExecutionState& state, UCalendar* calendar, int32_t year);
    void setYear(ExecutionState& state, UCalendar* calendar, String* era, int32_t year);

    int32_t year(ExecutionState& state, UCalendar* calendar);
    int32_t eraYear(ExecutionState& state, UCalendar* calendar);
    String* era(ExecutionState& state, UCalendar* calendar);

private:
    UCalendar* createICUCalendar(ExecutionState& state, const std::string& name);
    ID m_id;
};

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

inline TemporalUnit toTemporalUnit(ISO8601::DateTimeUnit u)
{
    return static_cast<TemporalUnit>(u);
}

struct MonthCode {
    unsigned monthNumber = 0;
    bool isLeapMonth = false;
};

#define CALENDAR_FIELD_RECORDS(F)                   \
    F(era, Era, Optional<String*>)                  \
    F(eraYear, EraYear, Optional<int>)              \
    F(year, Year, Optional<int>)                    \
    F(month, Month, Optional<unsigned>)             \
    F(monthCode, MonthCode, Optional<MonthCode>)    \
    F(day, Day, Optional<unsigned>)                 \
    F(hour, Hour, Optional<unsigned>)               \
    F(minute, Minute, Optional<unsigned>)           \
    F(second, Second, Optional<unsigned>)           \
    F(millisecond, Millisecond, Optional<unsigned>) \
    F(microsecond, Microsecond, Optional<unsigned>) \
    F(nanosecond, Nanosecond, Optional<unsigned>)   \
    F(offset, Offset, Optional<String*>)            \
    F(timeZone, TimeZone, Optional<TimeZone>)

enum class CalendarField {
#define DEFINE_FIELD(name, Name, type) Name,
    CALENDAR_FIELD_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD
};

struct CalendarFieldsRecord {
#define DEFINE_FIELD(name, Name, type) type name;
    CALENDAR_FIELD_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD

    void setValue(ExecutionState& state, CalendarField f, Value value);
};

enum class TemporalOverflowOption : uint8_t {
    Constrain,
    Reject
};

enum class TemporalShowCalendarNameOption : uint8_t {
    Auto,
    Always,
    Never,
    Critical
};

enum class TemporalDisambiguationOption : uint8_t {
    Compatible,
    Earlier,
    Later,
    Reject,
};

enum class TemporalShowOffsetOption : uint8_t {
    Auto,
    Never
};

enum class TemporalShowTimeZoneNameOption : uint8_t {
    Auto,
    Never,
    Critical
};

enum class TemporalOffsetOption : uint8_t {
    Prefer,
    Use,
    Ignore,
    Reject,
};

enum class TemporalOffsetBehaviour : uint8_t {
    Exact,
    Wall,
    Option
};

enum class TemporalMatchBehaviour : uint8_t {
    MatchExactly,
    MatchMinutes
};

enum class TemporalDirectionOption : uint8_t {
    Next,
    Previous
};

enum class TemporalKind : uint8_t {
    PlainDate,
    PlainYearMonth,
    PlainMonthDay,
    PlainTime,
    PlainDateTime,
    Instant,
    ZonedDateTime
};

class Temporal {
public:
    static ISO8601::PlainDate computeISODate(ExecutionState& state, UCalendar* ucal);
    static TimeZone parseTimeZone(ExecutionState& state, String* input, bool allowISODateTimeString = true);
    static void formatSecondsStringFraction(StringBuilder& builder, Int128 fraction, Value precision);
    static ISO8601::PlainDateTime toPlainDateTime(Int128 epochNanoseconds);
    static int timeDurationSign(Int128 t)
    {
        if (t < 0) {
            return -1;
        } else if (t > 0) {
            return 1;
        }
        return 0;
    }

    static int64_t computeTimeZoneOffset(ExecutionState& state, String* name, int64_t epochMilli);
    static int64_t computeTimeZoneOffset(ExecutionState& state, String* name, const ISO8601::PlainDate& localDate, Optional<ISO8601::PlainTime> localTime, bool duplicateTimeAsDST = false);

    // https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochnanoseconds
    static Int128 systemUTCEpochNanoseconds();

    // https://tc39.es/proposal-temporal/#sec-temporal-systemdatetime
    static ISO8601::PlainDateTime systemDateTime(ExecutionState& state, Value temporalTimeZoneLike);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
    static TemporalDurationObject* toTemporalDuration(ExecutionState& state, const Value& item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
    static TemporalInstantObject* toTemporalInstant(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
    static TemporalPlainTimeObject* toTemporalTime(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
    static TemporalPlainDateObject* toTemporalDate(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaldatetime
    static TemporalPlainDateTimeObject* toTemporalDateTime(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
    static TemporalPlainYearMonthObject* toTemporalYearMonth(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
    static TemporalPlainMonthDayObject* toTemporalMonthDay(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
    static TemporalZonedDateTimeObject* toTemporalZonedDateTime(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totimerecordormidnight
    static ISO8601::PlainTime toTimeRecordOrMidnight(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalfractionalseconddigitsoption
    // NullOption means AUTO
    static Optional<unsigned> getTemporalFractionalSecondDigitsOption(ExecutionState& state, Optional<Object*> resolvedOptions);

    // https://tc39.es/proposal-temporal/#sec-temporal-getroundingmodeoption
    static ISO8601::RoundingMode getRoundingModeOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* fallback);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalunitvaluedoption
    static Optional<TemporalUnit> getTemporalUnitValuedOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* key, Optional<Value> defaultValue /* give DefaultValue to EmptyValue means Required = true*/);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowoffsetoption
    static TemporalShowOffsetOption getTemporalShowOffsetOption(ExecutionState& state, Optional<Object*> resolvedOptions);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowtimezonenameoption
    static TemporalShowTimeZoneNameOption getTemporalShowTimeZoneNameOption(ExecutionState& state, Optional<Object*> resolvedOptions);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporaldisambiguationoption
    static TemporalDisambiguationOption getTemporalDisambiguationOption(ExecutionState& state, Optional<Object*> resolvedOptions);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloffsetoption
    static TemporalOffsetOption getTemporalOffsetOption(ExecutionState& state, Optional<Object*> resolvedOptions, TemporalOffsetOption fallback);

    // https://tc39.es/proposal-temporal/#sec-temporal-getdirectionoption
    static TemporalDirectionOption getTemporalDirectionOption(ExecutionState& state, Optional<Object*> resolvedOptions);

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

    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidtime
    static bool isValidTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloverflowoption
    static TemporalOverflowOption getTemporalOverflowOption(ExecutionState& state, Optional<Object*> options);

    // https://tc39.es/proposal-temporal/#sec-temporal-ispartialtemporalobject
    static bool isPartialTemporalObject(ExecutionState& state, Value value);

    // https://tc39.es/proposal-temporal/#sec-temporal-balancetime
    static ISO8601::Duration balanceTime(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond);
    static ISO8601::Duration balanceTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond);
    static ISO8601::Duration balanceTime(Int128 hour, Int128 minute, Int128 second, Int128 millisecond, Int128 microsecond, Int128 nanosecond);

    // https://tc39.es/proposal-temporal/#sec-temporal-differencetime
    static Int128 differenceTime(ISO8601::PlainTime time1, ISO8601::PlainTime time2);

    // https://tc39.es/proposal-temporal/#sec-temporal-timedurationfromcomponents
    static Int128 timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
    static Calendar getTemporalCalendarIdentifierWithISODefault(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendaridentifier
    static Calendar toTemporalCalendarIdentifier(ExecutionState& state, Value temporalCalendarLike);

    // https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
    static CalendarFieldsRecord prepareCalendarFields(ExecutionState& state, Calendar calendar, Object* fields, CalendarField* calendarFieldNames, size_t calendarFieldNamesLength,
                                                      CalendarField* nonCalendarFieldNames, size_t nonCalendarFieldNamesLength, CalendarField* requiredFieldNames, size_t requiredFieldNamesLength /* SIZE_MAX means PARTIAL */);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendardatefromfields
    enum class CalendarDateFromFieldsMode {
        Date,
        YearMonth,
        MonthDay
    };
    static std::pair<UCalendar*, Optional<ISO8601::PlainDate>> calendarDateFromFields(ExecutionState& state, Calendar calendar, CalendarFieldsRecord fields, TemporalOverflowOption overflow, CalendarDateFromFieldsMode mode = CalendarDateFromFieldsMode::Date);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendarresolvefields
    static std::pair<UCalendar*, Optional<ISO8601::PlainDate>> calendarResolveFields(ExecutionState& state, Calendar calendar, CalendarFieldsRecord fields, TemporalOverflowOption overflow, CalendarDateFromFieldsMode mode);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendarmergefields
    static CalendarFieldsRecord calendarMergeFields(ExecutionState& state, Calendar calendar, const CalendarFieldsRecord& fields, const CalendarFieldsRecord& additionalFields);

    // https://tc39.es/proposal-temporal/#sec-temporal-interprettemporaldatetimefields
    static ISO8601::PlainDateTime interpretTemporalDateTimeFields(ExecutionState& state, Calendar calendar, const CalendarFieldsRecord& fields, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-interpretisodatetimeoffset
    static Int128 interpretISODateTimeOffset(ExecutionState& state, ISO8601::PlainDate isoDate, Optional<ISO8601::PlainTime> time,
                                             TemporalOffsetBehaviour offsetBehaviour, int64_t offsetNanoseconds, TimeZone timeZone, bool hasUTCDesignator, TemporalDisambiguationOption disambiguation,
                                             TemporalOffsetOption offsetOption, TemporalMatchBehaviour matchBehaviour);

    // https://tc39.es/proposal-temporal/#sec-temporal-getstartofday
    static Int128 getStartOfDay(ExecutionState& state, TimeZone timeZone, ISO8601::PlainDate isoDate);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowcalendarnameoption
    static TemporalShowCalendarNameOption getTemporalShowCalendarNameOption(ExecutionState& state, Optional<Object*> options);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
    // returns new "UCalendar*, ISODate"
    static std::pair<UCalendar*, ISO8601::PlainDate> calendarDateAdd(ExecutionState& state, Calendar calendar, ISO8601::PlainDate isoDate, UCalendar* icuDate, const ISO8601::Duration& duration, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil
    static ISO8601::Duration calendarDateUntil(Calendar calendar, ISO8601::PlainDate one, ISO8601::PlainDate two, TemporalUnit largestUnit);

    // https://tc39.es/proposal-temporal/#sec-temporal-balanceisoyearmonth
    static ISO8601::PlainYearMonth balanceISOYearMonth(double year, double month);

    // https://tc39.es/proposal-temporal/#sec-temporal-regulateisodate
    static Optional<ISO8601::PlainDate> regulateISODate(double year, double month, double day, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-roundrelativeduration
    static ISO8601::InternalDuration roundRelativeDuration(ExecutionState& state, ISO8601::InternalDuration duration, Int128 destEpochNs, ISO8601::PlainDateTime isoDateTime, Optional<TimeZone> timeZone, Calendar calendar, TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-addisodate
    static ISO8601::PlainDate isoDateAdd(ExecutionState& state, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-getepochnanosecondsfor
    static Int128 getEpochNanosecondsFor(ExecutionState& state, Optional<TimeZone> timeZone, ISO8601::PlainDateTime isoDateTime, TemporalDisambiguationOption disambiguation);
    static Int128 getEpochNanosecondsFor(ExecutionState& state, Optional<TimeZone> timeZone, Int128 epochNanoValue, TemporalDisambiguationOption disambiguation);

    // https://tc39.es/proposal-temporal/#sec-temporal-formatcalendarannotation
    static void formatCalendarAnnotation(StringBuilder& builder, Calendar calendar, TemporalShowCalendarNameOption showCalendar);

    // https://tc39.es/proposal-temporal/#sec-temporal-isoyearmonthwithinlimits
    static bool isoYearMonthWithinLimits(ISO8601::PlainDate plainDate);

    // https://tc39.es/proposal-temporal/#sec-temporal-balanceisodate
    static ISO8601::PlainDate balanceISODate(ExecutionState& state, double year, double month, double day);

    // https://tc39.es/proposal-temporal/#sec-temporal-adjustdatedurationrecord
    static ISO8601::Duration adjustDateDurationRecord(ExecutionState& state, ISO8601::Duration dateDuration, double days, Optional<double> weeks, Optional<double> months);

    // https://tc39.es/proposal-temporal/#sec-temporal-differenceplaindatetimewithrounding
    static ISO8601::InternalDuration differencePlainDateTimeWithRounding(ExecutionState& state, ISO8601::PlainDateTime isoDateTime1, ISO8601::PlainDateTime isoDateTime2, Calendar calendar,
                                                                         ISO8601::DateTimeUnit largestUnit, unsigned roundingIncrement, ISO8601::DateTimeUnit smallestUnit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-differenceisodatetime
    static ISO8601::InternalDuration differenceISODateTime(ExecutionState& state, ISO8601::PlainDateTime isoDateTime1, ISO8601::PlainDateTime isoDateTime2, Calendar calendar, ISO8601::DateTimeUnit largestUnit);

    // https://tc39.es/proposal-temporal/#sec-temporal-roundisodatetime
    static ISO8601::PlainDateTime roundISODateTime(ExecutionState& state, ISO8601::PlainDateTime isoDateTime, unsigned increment, ISO8601::DateTimeUnit sunit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-formatoffsettimezoneidentifier
    static void formatOffsetTimeZoneIdentifier(ExecutionState& state, int offsetMinutes, StringBuilder& sb, bool isSeparated = true);

    // https://tc39.es/proposal-temporal/#sec-temporal-getisodatetimefor
    static ISO8601::PlainDateTime getISODateTimeFor(ExecutionState& state, Optional<TimeZone> timeZone, Int128 epochNs);

    // https://tc39.es/proposal-temporal/#sec-temporal-getoffsetnanosecondsfor
    static int64_t getOffsetNanosecondsFor(ExecutionState& state, TimeZone timeZone, Int128 epochNs);

    // https://tc39.es/proposal-temporal/#sec-temporal-addinstant
    static Int128 addInstant(ExecutionState& state, Int128 epochNanoseconds, Int128 timeDuration);

    // https://tc39.es/proposal-temporal/#sec-temporal-addzoneddatetime
    static Int128 addZonedDateTime(ExecutionState& state, Int128 epochNanoseconds, TimeZone timeZone, Calendar calendar, ISO8601::InternalDuration duration, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-totaltimeduration
    static double totalTimeDuration(ExecutionState& state, Int128 timeDuration, TemporalUnit unit);

    // https://tc39.es/proposal-temporal/#sec-temporal-totalrelativeduration
    static double totalRelativeDuration(ExecutionState& state, const ISO8601::InternalDuration& duration, Int128 originEpochNs, Int128 destEpochNs, ISO8601::PlainDateTime isoDateTime, Optional<TimeZone> timeZone, Calendar calendar, ISO8601::DateTimeUnit unit);

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimevalue
    static std::pair<double, Optional<TemporalKind>> handleDateTimeValue(ExecutionState& state, IntlDateTimeFormatObject* format, Value x, bool allowZonedDateTime = false);
};

} // namespace Escargot

#endif
#endif
