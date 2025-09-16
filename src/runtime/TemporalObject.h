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
    F(EthiopianAmeteAlem, "ethioaa", "ethiopic-amete-alem", "ethiopic-amete-alem")  \
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

    static Optional<Calendar> fromString(ISO8601::CalendarID);
    static Optional<Calendar> fromString(String* str);
    String* toString() const;
    std::string toICUString() const;
    UCalendar* createICUCalendar(ExecutionState& state);
    void lookupICUEra(ExecutionState& state, const std::function<bool(size_t idx, const std::string& icuEra)>& fn) const;

private:
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
    F(timeZone, TimeZone, Optional<String*>)

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

class Temporal {
public:
    static void formatSecondsStringFraction(StringBuilder& builder, Int128 fraction, Value precision);

    // https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochnanoseconds
    static Int128 systemUTCEpochNanoseconds();

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
    static TemporalDurationObject* toTemporalDuration(ExecutionState& state, const Value& item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
    static TemporalInstantObject* toTemporalInstant(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
    static TemporalPlainTimeObject* toTemporalTime(ExecutionState& state, Value item, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
    static TemporalPlainDateObject* toTemporalDate(ExecutionState& state, Value item, Value options);

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

    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidtime
    static bool isValidTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloverflowoption
    static TemporalOverflowOption getTemporalOverflowOption(ExecutionState& state, Optional<Object*> options);

    // https://tc39.es/proposal-temporal/#sec-temporal-ispartialtemporalobject
    static bool isPartialTemporalObject(ExecutionState& state, Value value);

    // https://tc39.es/proposal-temporal/#sec-temporal-balancetime
    static ISO8601::Duration balanceTime(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond);
    static ISO8601::Duration balanceTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond);

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
    static UCalendar* calendarDateFromFields(ExecutionState& state, Calendar calendar, CalendarFieldsRecord fields, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendarmergefields
    static CalendarFieldsRecord calendarMergeFields(ExecutionState& state, Calendar calendar, const CalendarFieldsRecord& fields, const CalendarFieldsRecord& additionalFields);

    // https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowcalendarnameoption
    static TemporalShowCalendarNameOption getTemporalShowCalendarNameOption(ExecutionState& state, Optional<Object*> options);

    // https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
    // returns new "UCalendar*"
    static UCalendar* calendarDateAdd(ExecutionState& state, Calendar calendar, ISO8601::PlainDate isoDate, UCalendar* icuDate, const ISO8601::Duration& duration, TemporalOverflowOption overflow);
};

} // namespace Escargot

#endif
#endif
