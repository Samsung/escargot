#if defined(ENABLE_TEMPORAL)
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

#include "Escargot.h"
#include "TemporalZonedDateTimeObject.h"
#include "TemporalPlainDateObject.h"
#include "TemporalPlainTimeObject.h"
#include "TemporalPlainDateTimeObject.h"
#include "TemporalDurationObject.h"
#include "TemporalInstantObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

bool TemporalZonedDateTimeObject::ComputedTimeZone::equals(const ComputedTimeZone& src) const
{
    return offset() == src.offset() && (timeZoneName()->equals(src.timeZoneName()) || Intl::canonicalTimeZoneID(timeZoneName()) == Intl::canonicalTimeZoneID(src.timeZoneName()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-timezoneequals
static bool timeZoneEquals(const TemporalZonedDateTimeObject::ComputedTimeZone& one, const TemporalZonedDateTimeObject::ComputedTimeZone& two)
{
    // If one is two, return true.
    if (one.equals(two)) {
        return true;
    }
    // Let offsetMinutesOne be ! ParseTimeZoneIdentifier(one).[[OffsetMinutes]].
    // Let offsetMinutesTwo be ! ParseTimeZoneIdentifier(two).[[OffsetMinutes]].
    // If offsetMinutesOne is empty and offsetMinutesTwo is empty, then
    if (!one.hasOffsetTimeZoneName() && !two.hasOffsetTimeZoneName()) {
        // Let recordOne be GetAvailableNamedTimeZoneIdentifier(one).
        // Let recordTwo be GetAvailableNamedTimeZoneIdentifier(two).
        // If recordOne is not empty and recordTwo is not empty and recordOne.[[PrimaryIdentifier]] is recordTwo.[[PrimaryIdentifier]], return true.
        return one.timeZoneName()->equals(two.timeZoneName());
    } else {
        // Else,
        // If offsetMinutesOne is not empty and offsetMinutesTwo is not empty and offsetMinutesOne = offsetMinutesTwo, return true.
        if (one.hasOffsetTimeZoneName() && two.hasOffsetTimeZoneName()) {
            return one.offset() == two.offset();
        }
    }
    return false;
}

void TemporalZonedDateTimeObject::init(ExecutionState& state, ComputedTimeZone timeZone)
{
    m_timeZone = timeZone;
    Int128 timezoneAppliedEpochNanoseconds = *m_epochNanoseconds + timeZone.offset();

    if (!ISO8601::isValidEpochNanoseconds(*m_epochNanoseconds)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
    }

    m_icuCalendar = m_calendarID.createICUCalendar(state);

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalZonedDateTimeObject* self = (TemporalZonedDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(m_icuCalendar, ISO8601::ExactTime(*m_epochNanoseconds).floorEpochMilliseconds(), &status);
    CHECK_ICU()

    auto tz = m_timeZone.timeZoneName()->toUTF16StringData();
    ucal_setTimeZone(m_icuCalendar, tz.data(), tz.length(), &status);
    CHECK_ICU()

    auto calendar = calendarID();
    if (calendar.isISO8601()) {
        *m_plainDateTime = Temporal::toPlainDateTime(timezoneAppliedEpochNanoseconds);
    } else {
        int64_t underMicrosecondValue = int64_t(timezoneAppliedEpochNanoseconds % 1000000);
        auto y = calendar.year(state, m_icuCalendar);
        auto m = calendar.ordinalMonth(state, m_icuCalendar);
        CHECK_ICU()
        auto d = ucal_get(m_icuCalendar, UCAL_DAY_OF_MONTH, &status);
        CHECK_ICU()

        auto h = ucal_get(m_icuCalendar, UCAL_HOUR_OF_DAY, &status);
        CHECK_ICU()
        auto mm = ucal_get(m_icuCalendar, UCAL_MINUTE, &status);
        CHECK_ICU()
        auto s = ucal_get(m_icuCalendar, UCAL_SECOND, &status);
        CHECK_ICU()
        auto ms = ucal_get(m_icuCalendar, UCAL_MILLISECOND, &status);
        CHECK_ICU()

        *m_plainDateTime = ISO8601::PlainDateTime(ISO8601::PlainDate(y, m, d), ISO8601::PlainTime(h, mm, s, ms, underMicrosecondValue / 1000, underMicrosecondValue % 1000));
    }
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, TimeZone timeZone, Calendar calendar)
    : DerivedObject(state, proto)
    , m_epochNanoseconds(new(PointerFreeGC) Int128(epochNanoseconds))
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_timeZone()
    , m_calendarID(calendar)
    , m_icuCalendar(nullptr)
{
    ComputedTimeZone computedTimeZone;
    if (timeZone.hasTimeZoneName()) {
        Int128 timezoneAppliedEpoch = Temporal::getEpochNanosecondsFor(state, timeZone, epochNanoseconds, TemporalDisambiguationOption::Compatible);
        computedTimeZone = ComputedTimeZone(false, timeZone.timeZoneName(), int64_t(epochNanoseconds - timezoneAppliedEpoch));
    } else {
        StringBuilder tz;
        Temporal::formatOffsetTimeZoneIdentifier(state, int(timeZone.offset() / ISO8601::ExactTime::nsPerMinute), tz);
        computedTimeZone = ComputedTimeZone(true, tz.finalize(), timeZone.offset());
    }

    init(state, computedTimeZone);
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, ComputedTimeZone timeZone, Calendar calendar)
    : DerivedObject(state, proto)
    , m_epochNanoseconds(new(PointerFreeGC) Int128(epochNanoseconds))
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_timeZone()
    , m_calendarID(calendar)
    , m_icuCalendar(nullptr)
{
    init(state, timeZone);
}

static int64_t computeTimeZoneOffset(ExecutionState& state, UCalendar* cal)
{
    UErrorCode status = U_ZERO_ERROR;
    auto zoneOffset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    CHECK_ICU();
    auto dstOffset = ucal_get(cal, UCAL_DST_OFFSET, &status);
    CHECK_ICU();
    return (zoneOffset + dstOffset) * int64_t(ISO8601::ExactTime::nsPerMillisecond);
}

String* TemporalZonedDateTimeObject::toString(ExecutionState& state, Value options)
{
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetTemporalShowCalendarNameOption reads "calendarName", GetTemporalFractionalSecondDigitsOption reads "fractionalSecondDigits", and GetRoundingModeOption reads "roundingMode").
    // Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let showOffset be ? GetTemporalShowOffsetOption(resolvedOptions).
    auto showOffset = Temporal::getTemporalShowOffsetOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Let showTimeZone be ? GetTemporalShowTimeZoneNameOption(resolvedOptions).
    auto showTimeZone = Temporal::getTemporalShowTimeZoneNameOption(state, resolvedOptions);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);

    // Return TemporalZonedDateTimeToString(zonedDateTime, precision.[[Precision]], showCalendar, showTimeZone, showOffset, precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto result = Temporal::roundISODateTime(state, ISO8601::PlainDateTime(plainDate(), plainTime()), precision.increment, precision.unit, roundingMode);
    StringBuilder sb;
    sb.appendString(TemporalPlainDateObject::temporalDateToString(result.plainDate(), m_calendarID, TemporalShowCalendarNameOption::Never));
    sb.appendChar('T');
    sb.appendString(TemporalPlainTimeObject::temporalTimeToString(result.plainTime(), precision.precision));

    if (showOffset != TemporalShowOffsetOption::Never) {
        auto offsetNanoseconds = TemporalDurationObject::roundTimeDurationToIncrement(state, m_timeZone.offset(), ISO8601::ExactTime::nsPerMinute, ISO8601::RoundingMode::HalfExpand);
        auto offsetMinutes = int(offsetNanoseconds / ISO8601::ExactTime::nsPerMinute);
        Temporal::formatOffsetTimeZoneIdentifier(state, offsetMinutes, sb);
    }

    if (showTimeZone != TemporalShowTimeZoneNameOption::Never) {
        // If showTimeZone is critical, let flag be "!"; else let flag be the empty String.
        // Let timeZoneString be the string-concatenation of the code unit 0x005B (LEFT SQUARE BRACKET), flag, timeZone, and the code unit 0x005D (RIGHT SQUARE BRACKET).
        sb.appendChar('[');
        if (showTimeZone == TemporalShowTimeZoneNameOption::Critical) {
            sb.appendChar('!');
        }
        sb.appendString(m_timeZone.timeZoneName());
        sb.appendChar(']');
    }

    Temporal::formatCalendarAnnotation(sb, calendarID(), showCalendar);

    return sb.finalize();
}

bool TemporalZonedDateTimeObject::equals(ExecutionState& state, Value otherInput)
{
    // Let zonedDateTime be the this value.
    // Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
    // Set other to ? ToTemporalZonedDateTime(other).
    auto other = Temporal::toTemporalZonedDateTime(state, otherInput, Value());
    // If zonedDateTime.[[EpochNanoseconds]] ≠ other.[[EpochNanoseconds]], return false.
    if (epochNanoseconds() != other->epochNanoseconds()) {
        return false;
    }
    // If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, return false.
    if (!timeZoneEquals(timeZone(), other->timeZone())) {
        return false;
    }
    // Return CalendarEquals(zonedDateTime.[[Calendar]], other.[[Calendar]]).
    return calendarID() == other->calendarID();
}

int TemporalZonedDateTimeObject::hoursInDay(ExecutionState& state)
{
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let isoDateTime be GetISODateTimeFor(timeZone, zonedDateTime.[[EpochNanoseconds]]).
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, *m_epochNanoseconds);
    // Let today be isoDateTime.[[ISODate]].
    auto today = isoDateTime.plainDate();
    // Let tomorrow be BalanceISODate(today.[[Year]], today.[[Month]], today.[[Day]] + 1).
    auto tomorrow = Temporal::balanceISODate(state, today.year(), today.month(), today.day() + 1);
    // Let todayNs be ? GetStartOfDay(timeZone, today).
    auto todayNs = Temporal::getStartOfDay(state, timeZone, today);
    // Let tomorrowNs be ? GetStartOfDay(timeZone, tomorrow).
    auto tomorrowNs = Temporal::getStartOfDay(state, timeZone, tomorrow);
    // Let diff be TimeDurationFromEpochNanosecondsDifference(tomorrowNs, todayNs).
    // Return 𝔽(TotalTimeDuration(diff, hour)).
    return (int32_t)((tomorrowNs - todayNs) / ISO8601::ExactTime::nsPerHour);
}

TemporalInstantObject* TemporalZonedDateTimeObject::toInstant(ExecutionState& state)
{
    // Return ! CreateTemporalInstant(zonedDateTime.[[EpochNanoseconds]]).
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), epochNanoseconds());
}

TemporalPlainDateObject* TemporalZonedDateTimeObject::toPlainDate(ExecutionState& state)
{
    // Let isoDateTime be GetISODateTimeFor(zonedDateTime.[[TimeZone]], zonedDateTime.[[EpochNanoseconds]]).
    TimeZone timeZone = this->timeZone();
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, *m_epochNanoseconds);
    // Return ! CreateTemporalDate(isoDateTime.[[ISODate]], zonedDateTime.[[Calendar]]).
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(), isoDateTime.plainDate(), calendarID());
}

TemporalPlainTimeObject* TemporalZonedDateTimeObject::toPlainTime(ExecutionState& state)
{
    // Let isoDateTime be GetISODateTimeFor(zonedDateTime.[[TimeZone]], zonedDateTime.[[EpochNanoseconds]]).
    TimeZone timeZone = this->timeZone();
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, *m_epochNanoseconds);
    // Return ! CreateTemporalTime(isoDateTime.[[Time]]).
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(), isoDateTime.plainTime());
}

TemporalPlainDateTimeObject* TemporalZonedDateTimeObject::toPlainDateTime(ExecutionState& state)
{
    // Let isoDateTime be GetISODateTimeFor(zonedDateTime.[[TimeZone]], zonedDateTime.[[EpochNanoseconds]]).
    TimeZone timeZone = this->timeZone();
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, *m_epochNanoseconds);
    // Return ! CreateTemporalDateTime(isoDateTime, zonedDateTime.[[Calendar]]).
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(), isoDateTime.plainDate(), isoDateTime.plainTime(), calendarID());
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::with(ExecutionState& state, Value temporalZonedDateTimeLike, Value options)
{
    // If ? IsPartialTemporalObject(temporalZonedDateTimeLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalZonedDateTimeLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalZonedDateTimeLike");
    }
    // Let epochNs be zonedDateTime.[[EpochNanoseconds]].
    auto epochNs = epochNanoseconds();
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let calendar be zonedDateTime.[[Calendar]].
    auto calendar = calendarID();
    // Let offsetNanoseconds be GetOffsetNanosecondsFor(timeZone, epochNs).
    auto offsetNanoseconds = Temporal::getOffsetNanosecondsFor(state, timeZone, epochNs);
    // Let isoDateTime be GetISODateTimeFor(timeZone, epochNs).
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, epochNs);
    // Let fields be ISODateToFields(calendar, isoDateTime.[[ISODate]], date).
    CalendarFieldsRecord fields;
    fields.year = isoDateTime.plainDate().year();
    fields.month = isoDateTime.plainDate().month();
    fields.day = isoDateTime.plainDate().day();
    // Set fields.[[Hour]] to plainDateTime.[[ISODateTime]].[[Time]].[[Hour]].
    // Set fields.[[Minute]] to plainDateTime.[[ISODateTime]].[[Time]].[[Minute]].
    // Set fields.[[Second]] to plainDateTime.[[ISODateTime]].[[Time]].[[Second]].
    // Set fields.[[Millisecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Millisecond]].
    // Set fields.[[Microsecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Microsecond]].
    // Set fields.[[Nanosecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Nanosecond]].
    fields.hour = plainTime().hour();
    fields.minute = plainTime().minute();
    fields.second = plainTime().second();
    fields.millisecond = plainTime().millisecond();
    fields.microsecond = plainTime().microsecond();
    fields.nanosecond = plainTime().nanosecond();
    // Set fields.[[OffsetString]] to FormatUTCOffsetNanoseconds(offsetNanoseconds).
    StringBuilder sb;
    Temporal::formatOffsetTimeZoneIdentifier(state, int32_t(offsetNanoseconds / ISO8601::ExactTime::nsPerMinute), sb);
    fields.offset = sb.finalize();
    // Let partialZonedDateTime be ? PrepareCalendarFields(calendar, temporalZonedDateTimeLike, « year, month, month-code, day », « hour, minute, second, millisecond, microsecond, nanosecond, offset », partial).
    CalendarField ds[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
    CalendarField fs[7] = { CalendarField::Hour, CalendarField::Minute, CalendarField::Second,
                            CalendarField::Millisecond, CalendarField::Microsecond, CalendarField::Nanosecond,
                            CalendarField::Offset };
    auto partialZonedDateTime = Temporal::prepareCalendarFields(state, calendar, temporalZonedDateTimeLike.asObject(), ds, 4, fs, 7, nullptr, SIZE_MAX);
    // Set fields to CalendarMergeFields(calendar, fields, partialZonedDateTime).
    fields = Temporal::calendarMergeFields(state, calendar, fields, partialZonedDateTime);
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let disambiguation be ? GetTemporalDisambiguationOption(resolvedOptions).
    auto disambiguation = Temporal::getTemporalDisambiguationOption(state, resolvedOptions);
    // Let offset be ? GetTemporalOffsetOption(resolvedOptions, prefer).
    auto offset = Temporal::getTemporalOffsetOption(state, resolvedOptions, TemporalOffsetOption::Prefer);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let dateTimeResult be ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
    auto dateTimeResult = Temporal::interpretTemporalDateTimeFields(state, calendar, fields, overflow);
    // Let newOffsetNanoseconds be ! ParseDateTimeUTCOffset(fields.[[OffsetString]]).
    auto newOffsetNanoseconds = ISO8601::parseUTCOffset(fields.offset.value(), {}).value();
    // Let epochNanoseconds be ? InterpretISODateTimeOffset(dateTimeResult.[[ISODate]], dateTimeResult.[[Time]], option, newOffsetNanoseconds, timeZone, disambiguation, offset, match-exactly).
    auto epochNanoseconds = Temporal::interpretISODateTimeOffset(state, dateTimeResult.plainDate(), dateTimeResult.plainTime(),
                                                                 TemporalOffsetBehaviour::Option, newOffsetNanoseconds, timeZone, false, disambiguation, offset, TemporalMatchBehaviour::MatchExactly);
    // Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds, timeZone, calendar);
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::withPlainTime(ExecutionState& state, Value temporalTimeLike)
{
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let calendar be zonedDateTime.[[Calendar]].
    auto calendar = calendarID();
    // Let isoDateTime be GetISODateTimeFor(timeZone, zonedDateTime.[[EpochNanoseconds]]).
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, epochNanoseconds());
    // If plainTimeLike is undefined, then
    Int128 epochNs;
    if (temporalTimeLike.isUndefined()) {
        // Let epochNs be ? GetStartOfDay(timeZone, isoDateTime.[[ISODate]]).
        epochNs = Temporal::getStartOfDay(state, timeZone, isoDateTime.plainDate());
    } else {
        // Else,
        // Let plainTime be ? ToTemporalTime(plainTimeLike).
        auto plainTime = Temporal::toTemporalTime(state, temporalTimeLike, Value());
        // Let resultISODateTime be CombineISODateAndTimeRecord(isoDateTime.[[ISODate]], plainTime.[[Time]]).
        // Let epochNs be ? GetEpochNanosecondsFor(timeZone, resultISODateTime, compatible).
        epochNs = Temporal::getEpochNanosecondsFor(state, timeZone, ISO8601::PlainDateTime(isoDateTime.plainDate(), plainTime->plainTime()), TemporalDisambiguationOption::Compatible);
    }

    // Return ! CreateTemporalZonedDateTime(epochNs, timeZone, calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNs, timeZone, calendar);
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::withTimeZone(ExecutionState& state, Value timeZoneLike)
{
    // Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneLike).
    auto timeZone = Temporal::toTemporalTimezoneIdentifier(state, timeZoneLike);
    // Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], timeZone, zonedDateTime.[[Calendar]]).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds(), timeZone, calendarID());
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::withCalendar(ExecutionState& state, Value calendarLike)
{
    // Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
    auto calendar = Temporal::toTemporalCalendarIdentifier(state, calendarLike);
    // Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds(), timeZone(), calendar);
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::addDurationToZonedDateTime(ExecutionState& state, TemporalZonedDateTimeObject::AddDurationToZonedDateTimeOperation operation, Value temporalDurationLike, Value options)
{
    // Let duration be ? ToTemporalDuration(temporalDurationLike).
    ISO8601::Duration duration = Temporal::toTemporalDuration(state, temporalDurationLike)->duration();
    // If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if (operation == AddDurationToZonedDateTimeOperation::Subtract) {
        duration = TemporalDurationObject::createNegatedTemporalDuration(duration);
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let calendar be zonedDateTime.[[Calendar]].
    auto calendar = calendarID();
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let internalDuration be ToInternalDurationRecord(duration).
    auto internalDuration = TemporalDurationObject::toInternalDurationRecord(duration);
    // Let epochNanoseconds be ? AddZonedDateTime(zonedDateTime.[[EpochNanoseconds]], timeZone, calendar, internalDuration, overflow).
    auto epochNanoseconds = Temporal::addZonedDateTime(state, this->epochNanoseconds(), timeZone, calendar, internalDuration, overflow);
    // Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds, timeZone, calendar);
}

TemporalDurationObject* TemporalZonedDateTimeObject::differenceTemporalZonedDateTime(ExecutionState& state, DifferenceTemporalZonedDateTime operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalZonedDateTime(other).
    auto other = Temporal::toTemporalZonedDateTime(state, otherInput, Value());
    // If CalendarEquals(zonedDateTime.[[Calendar]], other.[[Calendar]]) is false, then
    if (other->calendarID() != calendarID()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "calendar is not same");
    }

    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », nanosecond, hour).
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalZonedDateTime::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::DateTime, nullptr, 0, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    // If TemporalUnitCategory(settings.[[LargestUnit]]) is time, then
    if (ISO8601::toDateTimeCategory(settings.largestUnit) == ISO8601::DateTimeUnitCategory::Time) {
        // Let internalDuration be DifferenceInstant(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        auto internalDuration = TemporalInstantObject::differenceInstant(state, epochNanoseconds(), other->epochNanoseconds(), settings.roundingIncrement, settings.smallestUnit, settings.roundingMode);
        // Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
        auto result = TemporalDurationObject::temporalDurationFromInternal(state, internalDuration, settings.largestUnit);
        // If operation is since, set result to CreateNegatedTemporalDuration(result).
        if (operation == DifferenceTemporalZonedDateTime::Since) {
            result = TemporalDurationObject::createNegatedTemporalDuration(result);
        }
        // Return result.
        return new TemporalDurationObject(state, result);
    }
    // If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, then
    if (!timeZoneEquals(timeZone(), other->timeZone())) {
        // Throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "timeZone is not same");
    }
    // If zonedDateTime.[[EpochNanoseconds]] = other.[[EpochNanoseconds]], then
    if (epochNanoseconds() == other->epochNanoseconds()) {
        // Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
        return new TemporalDurationObject(state, {});
    }

    // Let internalDuration be ? DifferenceZonedDateTimeWithRounding(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]], settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
    auto internalDuration = differenceZonedDateTimeWithRounding(state, epochNanoseconds(), other->epochNanoseconds(), timeZone(), calendarID(), settings.largestUnit, settings.roundingIncrement, settings.smallestUnit, settings.roundingMode);
    // Let result be ! TemporalDurationFromInternal(internalDuration, hour).
    ISO8601::Duration result;
    if (settings.smallestUnit <= ISO8601::DateTimeUnit::Day) {
        result = TemporalDurationObject::temporalDurationFromInternal(state, internalDuration, settings.smallestUnit);
    } else {
        result = TemporalDurationObject::temporalDurationFromInternal(state, internalDuration, ISO8601::DateTimeUnit::Hour);
    }
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalZonedDateTime::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }
    // Return result.
    return new TemporalDurationObject(state, result);
}

ISO8601::InternalDuration TemporalZonedDateTimeObject::differenceZonedDateTimeWithRounding(ExecutionState& state, Int128 ns1, Int128 ns2, TimeZone timeZone, Calendar calendar,
                                                                                           ISO8601::DateTimeUnit largestUnit, unsigned roundingIncrement, ISO8601::DateTimeUnit smallestUnit, ISO8601::RoundingMode roundingMode)
{
    // If TemporalUnitCategory(largestUnit) is time, then
    if (ISO8601::toDateTimeCategory(largestUnit) == ISO8601::DateTimeUnitCategory::Time) {
        // Return DifferenceInstant(ns1, ns2, roundingIncrement, smallestUnit, roundingMode).
        return TemporalInstantObject::differenceInstant(state, ns1, ns2, roundingIncrement, smallestUnit, roundingMode);
    }

    // Let difference be ? DifferenceZonedDateTime(ns1, ns2, timeZone, calendar, largestUnit).
    auto difference = differenceZonedDateTime(state, ns1, ns2, timeZone, calendar, largestUnit);
    // If smallestUnit is nanosecond and roundingIncrement = 1, return difference.
    if (smallestUnit == ISO8601::DateTimeUnit::Nanosecond && roundingIncrement == 1) {
        return difference;
    }
    // Let dateTime be GetISODateTimeFor(timeZone, ns1).
    auto dateTime = Temporal::getISODateTimeFor(state, timeZone, ns1);
    // Return ? RoundRelativeDuration(difference, ns1, ns2, dateTime, timeZone, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
    return Temporal::roundRelativeDuration(state, difference, ns2, Temporal::toPlainDateTime(ns1), timeZone, calendar, toTemporalUnit(largestUnit), roundingIncrement, toTemporalUnit(smallestUnit), roundingMode);
}

ISO8601::InternalDuration TemporalZonedDateTimeObject::differenceZonedDateTime(ExecutionState& state, Int128 ns1, Int128 ns2, TimeZone timeZone, Calendar calendar, ISO8601::DateTimeUnit largestUnit)
{
    // If ns1 = ns2, return CombineDateAndTimeDuration(ZeroDateDuration(), 0).
    if (ns1 == ns2) {
        return {};
    }
    // Let startDateTime be GetISODateTimeFor(timeZone, ns1).
    auto startDateTime = Temporal::getISODateTimeFor(state, timeZone, ns1);
    // Let endDateTime be GetISODateTimeFor(timeZone, ns2).
    auto endDateTime = Temporal::getISODateTimeFor(state, timeZone, ns2);
    // If CompareISODate(startDateTime.[[ISODate]], endDateTime.[[ISODate]]) = 0, then
    if (startDateTime.plainDate().compare(endDateTime.plainDate()) == 0) {
        // Let timeDuration be TimeDurationFromEpochNanosecondsDifference(ns2, ns1).
        // Return CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
        Int128 timeDuration = Temporal::timeDurationFromEpochNanosecondsDifference(ns2, ns1);
        return ISO8601::InternalDuration(ISO8601::Duration(), timeDuration);
    }

    int sign;
    // If ns2 - ns1 < 0, let sign be -1; else let sign be 1.
    if (ns2 - ns1 < 0) {
        sign = -1;
    } else {
        sign = 1;
    }
    int maxDayCorrection;
    // If sign = 1, let maxDayCorrection be 2; else let maxDayCorrection be 1.
    if (sign == 1) {
        maxDayCorrection = 2;
    } else {
        maxDayCorrection = 1;
    }
    // Let dayCorrection be 0.
    int dayCorrection = 0;
    // Let timeDuration be DifferenceTime(startDateTime.[[Time]], endDateTime.[[Time]]).
    auto timeDuration = Temporal::differenceTime(startDateTime.plainTime(), endDateTime.plainTime());
    // If TimeDurationSign(timeDuration) = -sign, set dayCorrection to dayCorrection + 1.
    if (Temporal::timeDurationSign(timeDuration) == -sign) {
        dayCorrection = dayCorrection + 1;
    }

    ISO8601::PlainDateTime intermediateDateTime = ISO8601::PlainDateTime(ISO8601::PlainDate(), ISO8601::PlainTime());
    // Let success be false.
    bool success = false;
    // Repeat, while dayCorrection ≤ maxDayCorrection and success is false,
    while (dayCorrection <= maxDayCorrection && !success) {
        // Let intermediateDate be BalanceISODate(endDateTime.[[ISODate]].[[Year]], endDateTime.[[ISODate]].[[Month]], endDateTime.[[ISODate]].[[Day]] - dayCorrection × sign).
        auto intermediateDate = Temporal::balanceISODate(state, endDateTime.plainDate().year(), endDateTime.plainDate().month(),
                                                         endDateTime.plainDate().day() - dayCorrection * sign);
        // Let intermediateDateTime be CombineISODateAndTimeRecord(intermediateDate, startDateTime.[[Time]]).
        intermediateDateTime = ISO8601::PlainDateTime(intermediateDate, startDateTime.plainTime());
        // Let intermediateNs be ? GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible).
        auto intermediateNs = Temporal::getEpochNanosecondsFor(state, timeZone, intermediateDateTime,
                                                               TemporalDisambiguationOption::Compatible);
        // Set timeDuration to TimeDurationFromEpochNanosecondsDifference(ns2, intermediateNs).
        Int128 timeDuration = Temporal::timeDurationFromEpochNanosecondsDifference(ns2, intermediateNs);
        // Let timeSign be TimeDurationSign(timeDuration).
        auto timeSign = Temporal::timeDurationSign(timeDuration);
        // If sign ≠ -timeSign, then
        if (sign != -timeSign) {
            // Set success to true.
            success = true;
        }
        // Set dayCorrection to dayCorrection + 1.
        dayCorrection++;
    }
    // Let dateLargestUnit be LargerOfTwoTemporalUnits(largestUnit, day).
    auto dateLargestUnit = Temporal::largerOfTwoTemporalUnits(largestUnit, ISO8601::DateTimeUnit::Day);
    // Let dateDifference be CalendarDateUntil(calendar, startDateTime.[[ISODate]], intermediateDateTime.[[ISODate]], dateLargestUnit).
    auto dateDifference = Temporal::calendarDateUntil(calendar, startDateTime.plainDate(), intermediateDateTime.plainDate(), toTemporalUnit(dateLargestUnit));
    // Return CombineDateAndTimeDuration(dateDifference, timeDuration).
    return ISO8601::InternalDuration(dateDifference, timeDuration);
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::round(ExecutionState& state, Value roundToInput)
{
    Optional<Object*> roundTo;
    // If roundTo is undefined, then
    if (roundToInput.isUndefined()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid roundTo value");
    } else if (roundToInput.isString()) {
        // If roundTo is a String, then
        // Let paramString be roundTo.
        // Set roundTo to OrdinaryObjectCreate(null).
        // Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        roundTo = new Object(state, Object::PrototypeIsNull);
        roundTo->directDefineOwnProperty(state, state.context()->staticStrings().lazySmallestUnit(),
                                         ObjectPropertyDescriptor(roundToInput, ObjectPropertyDescriptor::AllPresent));
    } else {
        // Else,
        // Set roundTo to ? GetOptionsObject(roundTo).
        roundTo = Intl::getOptionsObject(state, roundToInput);
    }

    // Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    auto roundingIncrement = Temporal::getRoundingIncrementOption(state, roundTo);
    // Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
    auto roundingMode = Temporal::getRoundingModeOption(state, roundTo, state.context()->staticStrings().lazyHalfExpand().string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", required).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, roundTo,
                                                              state.context()->staticStrings().lazySmallestUnit().string(), Value(Value::EmptyValue));
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time, « day »).
    TemporalUnit extraValues[1] = { TemporalUnit::Day };
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, extraValues, 1);
    Optional<unsigned> maximum;
    bool inclusive;
    // If smallestUnit is day, then
    if (smallestUnit == TemporalUnit::Day) {
        // Let maximum be 1.
        maximum = 1;
        // Let inclusive be true.
        inclusive = true;
    } else {
        // Else,
        // Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        maximum = Temporal::maximumTemporalDurationRoundingIncrement(toDateTimeUnit(smallestUnit.value()));
        // Assert: maximum is not unset.
        // Let inclusive be false.
        inclusive = false;
    }


    // Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive).
    Temporal::validateTemporalRoundingIncrement(state, roundingIncrement, maximum.value(), inclusive);
    // If smallestUnit is nanosecond and roundingIncrement = 1, then
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1) {
        // Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]]).
        return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds(), timeZone(), calendarID());
    }

    // Let thisNs be zonedDateTime.[[EpochNanoseconds]].
    auto thisNs = epochNanoseconds();
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let calendar be zonedDateTime.[[Calendar]].
    auto calendar = calendarID();
    // Let isoDateTime be GetISODateTimeFor(timeZone, thisNs).
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, thisNs);

    Int128 epochNanoseconds;
    // If smallestUnit is day, then
    if (smallestUnit == TemporalUnit::Day) {
        // Let dateStart be isoDateTime.[[ISODate]].
        auto dateStart = isoDateTime.plainDate();
        // Let dateEnd be BalanceISODate(dateStart.[[Year]], dateStart.[[Month]], dateStart.[[Day]] + 1).
        auto dateEnd = Temporal::balanceISODate(state, dateStart.year(), dateStart.month(), dateStart.day() + 1);
        // Let startNs be ? GetStartOfDay(timeZone, dateStart).
        auto startNs = Temporal::getStartOfDay(state, timeZone, dateStart);
        // Assert: thisNs ≥ startNs.
        // Let endNs be ? GetStartOfDay(timeZone, dateEnd).
        auto endNs = Temporal::getStartOfDay(state, timeZone, dateEnd);
        // Assert: thisNs < endNs.
        // Let dayLengthNs be ℝ(endNs - startNs).
        Int128 dayLengthNs = endNs - startNs;
        // Let dayProgressNs be TimeDurationFromEpochNanosecondsDifference(thisNs, startNs).
        Int128 dayProgressNs = Temporal::timeDurationFromEpochNanosecondsDifference(thisNs, startNs);
        // Let roundedDayNs be ! RoundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode).
        auto roundedDayNs = TemporalDurationObject::roundTimeDurationToIncrement(state, dayProgressNs, dayLengthNs, roundingMode);
        // Let epochNanoseconds be AddTimeDurationToEpochNanoseconds(roundedDayNs, startNs).
        epochNanoseconds = roundedDayNs + startNs;
    } else {
        // Else,
        // Let roundResult be RoundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode).
        auto roundResult = Temporal::roundISODateTime(state, isoDateTime, roundingIncrement, toDateTimeUnit(smallestUnit.value()), roundingMode);
        // Let offsetNanoseconds be GetOffsetNanosecondsFor(timeZone, thisNs).
        auto offsetNanoseconds = Temporal::getOffsetNanosecondsFor(state, timeZone, thisNs);
        // Let epochNanoseconds be ? InterpretISODateTimeOffset(roundResult.[[ISODate]], roundResult.[[Time]], option, offsetNanoseconds, timeZone, compatible, prefer, match-exactly).
        epochNanoseconds = Temporal::interpretISODateTimeOffset(state, roundResult.plainDate(), roundResult.plainTime(), TemporalOffsetBehaviour::Option,
                                                                offsetNanoseconds, timeZone, false, TemporalDisambiguationOption::Compatible, TemporalOffsetOption::Prefer, TemporalMatchBehaviour::MatchExactly);
    }
    // Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds, timeZone, calendar);
}

TemporalZonedDateTimeObject* TemporalZonedDateTimeObject::startOfDay(ExecutionState& state)
{
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // Let calendar be zonedDateTime.[[Calendar]].
    auto calendar = calendarID();
    // Let isoDateTime be GetISODateTimeFor(timeZone, zonedDateTime.[[EpochNanoseconds]]).
    auto isoDateTime = Temporal::getISODateTimeFor(state, timeZone, epochNanoseconds());
    // Let epochNanoseconds be ? GetStartOfDay(timeZone, isoDateTime.[[ISODate]]).
    auto epochNanoseconds = Temporal::getStartOfDay(state, timeZone, isoDateTime.plainDate());
    // Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds, timeZone, calendar);
}

Value TemporalZonedDateTimeObject::getTimeZoneTransition(ExecutionState& state, Value directionParamInput)
{
    // Let timeZone be zonedDateTime.[[TimeZone]].
    TimeZone timeZone = this->timeZone();
    // If directionParam is undefined, throw a TypeError exception.
    if (directionParamInput.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid directionParam");
    }
    Optional<Object*> directionParam;
    // If directionParam is a String, then
    if (directionParamInput.isString()) {
        // Let paramString be directionParam.
        // Set directionParam to OrdinaryObjectCreate(null).
        // Perform ! CreateDataPropertyOrThrow(directionParam, "direction", paramString).
        directionParam = new Object(state, Object::PrototypeIsNull);
        directionParam->directDefineOwnProperty(state, state.context()->staticStrings().lazyDirection(),
                                                ObjectPropertyDescriptor(directionParamInput, ObjectPropertyDescriptor::AllPresent));

    } else {
        // Else,
        // Set directionParam to ? GetOptionsObject(directionParam).
        directionParam = Intl::getOptionsObject(state, directionParamInput);
    }

    // Let direction be ? GetDirectionOption(directionParam).
    auto direction = Temporal::getTemporalDirectionOption(state, directionParam);

    // If IsOffsetTimeZoneIdentifier(timeZone) is true, return null.
    if (timeZone.hasOffset()) {
        return Value(Value::Null);
    }

    // If direction is next, then
    UDate newEpoch = ISO8601::ExactTime(epochNanoseconds()).floorEpochMilliseconds();
    UErrorCode status = U_ZERO_ERROR;
    auto startOffset = computeTimeZoneOffset(state, m_icuCalendar);
    LocalResourcePointer<UCalendar> newCal(ucal_clone(m_icuCalendar, &status), [](UCalendar* r) {
        ucal_close(r);
    });

    UBool ret;

    while (true) {
        if (direction == TemporalDirectionOption::Next) {
            // Let transition be GetNamedTimeZoneNextTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
            ret = ucal_getTimeZoneTransitionDate(newCal.get(), UTimeZoneTransitionType::UCAL_TZ_TRANSITION_NEXT, &newEpoch, &status);
        } else {
            // Else,
            // Let transition be GetNamedTimeZonePreviousTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
            ret = ucal_getTimeZoneTransitionDate(newCal.get(), UTimeZoneTransitionType::UCAL_TZ_TRANSITION_PREVIOUS, &newEpoch, &status);
        }
        CHECK_ICU();

        if (!ret) {
            break;
        }

        ucal_setMillis(newCal.get(), newEpoch + (direction == TemporalDirectionOption::Next ? 1 : -1), &status);
        CHECK_ICU()

        if (startOffset != computeTimeZoneOffset(state, newCal.get())) {
            break;
        }
    }

    // If transition is null, return null.
    if (!ret) {
        return Value(Value::Null);
    }

    Int128 newEpochNs = int64_t(newEpoch) * ISO8601::ExactTime::nsPerMillisecond;
    if (!ISO8601::isValidEpochNanoseconds(newEpochNs)) {
        return Value(Value::Null);
    }

    // Return ! CreateTemporalZonedDateTime(transition, timeZone, zonedDateTime.[[Calendar]]).
    return new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), newEpochNs, timeZone, calendarID());
}

int TemporalZonedDateTimeObject::compare(ExecutionState& state, Value oneInput, Value twoInput)
{
    // Set one to ? ToTemporalZonedDateTime(one).
    // Set two to ? ToTemporalZonedDateTime(two).
    auto one = Temporal::toTemporalZonedDateTime(state, oneInput, Value());
    auto two = Temporal::toTemporalZonedDateTime(state, twoInput, Value());
    // Return 𝔽(CompareEpochNanoseconds(one.[[EpochNanoseconds]], two.[[EpochNanoseconds]])).
    auto epochNanosecondsOne = one->epochNanoseconds();
    auto epochNanosecondsTwo = two->epochNanoseconds();
    if (epochNanosecondsOne > epochNanosecondsTwo) {
        return 1;
    }
    if (epochNanosecondsOne < epochNanosecondsTwo) {
        return -1;
    }
    return 0;
}

} // namespace Escargot

#endif
