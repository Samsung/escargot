#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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

#include "Escargot.h"
#include "TemporalPlainDateTimeObject.h"
#include "TemporalPlainDateObject.h"
#include "TemporalPlainTimeObject.h"
#include "TemporalDurationObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, ISO8601::PlainTime plainTime, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime(plainDate, plainTime))
    , m_calendarID(calendar)
{
    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(),
                                         plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.microsecond(), plainTime.microsecond(), plainTime.nanosecond())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid date-time");
    }

    m_icuCalendar = calendar.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(m_icuCalendar, ISO8601::ExactTime::fromISOPartsAndOffset(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.microsecond(), 0).epochMilliseconds(), &status);
    CHECK_ICU()

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateTimeObject* self = (TemporalPlainDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, unsigned underMicrosecondValue, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_calendarID(calendar)
    , m_icuCalendar(icuCalendar)
{
    ASSERT(underMicrosecondValue < 1000 * 1000);
    UErrorCode status = U_ZERO_ERROR;

    int32_t y;
    if (calendar.shouldUseICUExtendedYear()) {
        y = ucal_get(m_icuCalendar, UCAL_EXTENDED_YEAR, &status);
    } else {
        y = ucal_get(m_icuCalendar, UCAL_YEAR, &status);
    }
    CHECK_ICU()
    auto m = ucal_get(m_icuCalendar, UCAL_MONTH, &status) + 1;
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

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateTimeObject* self = (TemporalPlainDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

String* TemporalPlainDateTimeObject::toString(ExecutionState& state, Value options)
{
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetTemporalShowCalendarNameOption reads "calendarName", GetTemporalFractionalSecondDigitsOption reads "fractionalSecondDigits", and GetRoundingModeOption reads "roundingMode").
    // Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);
    // Let result be RoundISODateTime(plainDateTime.[[ISODateTime]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto result = Temporal::roundISODateTime(state, ISO8601::PlainDateTime(computeISODate(state), plainTime()), precision.increment, precision.unit, roundingMode);
    // If ISODateTimeWithinLimits(result) is false, throw a RangeError exception.
    // Return ISODateTimeToString(result, plainDateTime.[[Calendar]], precision.[[Precision]], showCalendar).
    StringBuilder sb;
    sb.appendString(TemporalPlainDateObject::temporalDateToString(result.plainDate(), m_calendarID, showCalendar));
    sb.appendChar('T');
    sb.appendString(TemporalPlainTimeObject::temporalTimeToString(result.plainTime(), precision.precision));
    return sb.finalize();
}

ISO8601::PlainDate TemporalPlainDateTimeObject::computeISODate(ExecutionState& state)
{
    if (!m_calendarID.isISO8601()) {
        return Temporal::computeISODate(state, m_icuCalendar);
    }
    return plainDate();
}

TemporalPlainDateTimeObject* TemporalPlainDateTimeObject::with(ExecutionState& state, Value temporalDateTimeLike, Value options)
{
    // If ? IsPartialTemporalObject(temporalDateTimeLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalDateTimeLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalDateTimeLike");
    }
    // Let calendar be plainDateTime.[[Calendar]].
    auto calendar = m_calendarID;
    // Let fields be ISODateToFields(calendar, plainDateTime.[[ISODateTime]].[[ISODate]], date).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    MonthCode mc;
    mc.monthNumber = isoDate.month();
    fields.monthCode = mc;
    // Set fields.[[Hour]] to plainDateTime.[[ISODateTime]].[[Time]].[[Hour]].
    // Set fields.[[Minute]] to plainDateTime.[[ISODateTime]].[[Time]].[[Minute]].
    // Set fields.[[Second]] to plainDateTime.[[ISODateTime]].[[Time]].[[Second]].
    // Set fields.[[Millisecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Millisecond]].
    // Set fields.[[Microsecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Microsecond]].
    // Set fields.[[Nanosecond]] to plainDateTime.[[ISODateTime]].[[Time]].[[Nanosecond]].
    fields.day = isoDate.day();
    fields.hour = plainTime().hour();
    fields.minute = plainTime().minute();
    fields.second = plainTime().second();
    fields.millisecond = plainTime().millisecond();
    fields.microsecond = plainTime().microsecond();
    fields.nanosecond = plainTime().nanosecond();
    // Let partialDateTime be ? PrepareCalendarFields(calendar, temporalDateTimeLike, « year, month, month-code, day », « hour, minute, second, millisecond, microsecond, nanosecond », partial).
    CalendarField ds[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
    CalendarField fs[6] = { CalendarField::Hour, CalendarField::Minute, CalendarField::Second,
                            CalendarField::Millisecond, CalendarField::Microsecond, CalendarField::Nanosecond };
    auto partialDateTime = Temporal::prepareCalendarFields(state, calendar, temporalDateTimeLike.asObject(), ds, 4, fs, 6, nullptr, SIZE_MAX);
    // Set fields to CalendarMergeFields(calendar, fields, partialDateTime).
    fields = Temporal::calendarMergeFields(state, calendar, fields, partialDateTime);
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let result be ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
    // Return ? CreateTemporalDateTime(result, calendar).
    auto result = Temporal::interpretTemporalDateTimeFields(state, calendar, fields, overflow);
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(),
                                           result.plainDate(), result.plainTime(), calendar);
}

TemporalPlainDateTimeObject* TemporalPlainDateTimeObject::withPlainTime(ExecutionState& state, Value plainTimeLike)
{
    // Let time be ? ToTimeRecordOrMidnight(plainTimeLike).
    auto time = Temporal::toTimeRecordOrMidnight(state, plainTimeLike);
    // Let isoDateTime be CombineISODateAndTimeRecord(plainDateTime.[[ISODateTime]].[[ISODate]], time).
    auto isoDate = computeISODate(state);
    // Return ? CreateTemporalDateTime(isoDateTime, plainDateTime.[[Calendar]]).
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(),
                                           isoDate, time, calendarID());
}

TemporalPlainDateTimeObject* TemporalPlainDateTimeObject::withCalendar(ExecutionState& state, Value calendarLike)
{
    // Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
    auto calendar = Temporal::toTemporalCalendarIdentifier(state, calendarLike);
    auto icuCalendar = calendar.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(icuCalendar, ucal_getMillis(m_icuCalendar, &status), &status);

    // Return ! CreateTemporalDateTime(plainDateTime.[[ISODateTime]], calendar).
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(),
                                           icuCalendar, plainTime().microsecond() * 1000 + plainTime().nanosecond(), calendar);
}

TemporalPlainDateTimeObject* TemporalPlainDateTimeObject::addDurationToDateTime(ExecutionState& state, AddDurationToDateTimeOperation operation, Value temporalDurationLike, Value options)
{
    // Let duration be ? ToTemporalDuration(temporalDurationLike).
    ISO8601::Duration duration = Temporal::toTemporalDuration(state, temporalDurationLike)->duration();
    // If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if (operation == AddDurationToDateTimeOperation::Subtract) {
        duration = TemporalDurationObject::createNegatedTemporalDuration(duration);
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
    auto internalDuration = TemporalDurationObject::toInternalDurationRecordWith24HourDays(state, duration);
    // Let timeResult be AddTime(dateTime.[[ISODateTime]].[[Time]], internalDuration.[[Time]]).
    auto timeResult = Temporal::balanceTime(Int128(plainTime().hour()), Int128(plainTime().minute()), Int128(plainTime().second()),
                                            Int128(plainTime().millisecond()), Int128(plainTime().microsecond()), Int128(internalDuration.time()) + Int128(plainTime().nanosecond()));

    // Let dateDuration be ? AdjustDateDurationRecord(internalDuration.[[Date]], timeResult.[[Days]]).
    auto dateDuration = Temporal::adjustDateDurationRecord(state, internalDuration.dateDuration(), timeResult.days(), NullOption, NullOption);

    auto isoDate = computeISODate(state);
    // Let addedDate be ? CalendarDateAdd(dateTime.[[Calendar]], dateTime.[[ISODateTime]].[[ISODate]], dateDuration, overflow).
    auto addedDate = Temporal::calendarDateAdd(state, calendarID(), isoDate, m_icuCalendar, dateDuration, overflow);
    ucal_close(addedDate.first);
    // Let result be CombineISODateAndTimeRecord(addedDate, timeResult).
    // Return ? CreateTemporalDateTime(result, dateTime.[[Calendar]]).
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(),
                                           addedDate.second, ISO8601::PlainTime(timeResult.hours(), timeResult.minutes(), timeResult.seconds(), timeResult.milliseconds(), timeResult.microseconds(), timeResult.nanoseconds()), calendarID());
}

ISO8601::Duration TemporalPlainDateTimeObject::differenceTemporalPlainDateTime(ExecutionState& state, DifferenceTemporalPlainDateTime operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalDateTime(other).
    auto other = Temporal::toTemporalDateTime(state, otherInput, options);
    // If CalendarEquals(dateTime.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
    if (other->calendarID() != calendarID()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "other calendar is not same");
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », nanosecond, day).
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalPlainDateTime::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::DateTime, nullptr, 0, TemporalUnit::Nanosecond, TemporalUnit::Day);
    // If CompareISODateTime(dateTime.[[ISODateTime]], other.[[ISODateTime]]) = 0, then
    auto isoDateTime1 = computeISODate(state);
    auto isoDateTime2 = other->computeISODate(state);

    if (isoDateTime1 == isoDateTime2 && plainTime() == other->plainTime()) {
        // Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
        return {};
    }
    // Let internalDuration be ? DifferencePlainDateTimeWithRounding(dateTime.[[ISODateTime]], other.[[ISODateTime]], dateTime.[[Calendar]], settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
    auto internalDuration = Temporal::differencePlainDateTimeWithRounding(state, ISO8601::PlainDateTime(isoDateTime1, plainTime()), ISO8601::PlainDateTime(isoDateTime2, other->plainTime()),
                                                                          calendarID(), settings.largestUnit, settings.roundingIncrement, settings.smallestUnit, settings.roundingMode);
    // Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
    auto result = TemporalDurationObject::temporalDurationFromInternal(state, internalDuration, settings.largestUnit);
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalPlainDateTime::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }
    // Return result.
    return result;
}

TemporalPlainDateTimeObject* TemporalPlainDateTimeObject::round(ExecutionState& state, Value roundToInput)
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

    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetRoundingIncrementOption reads "roundingIncrement" and GetRoundingModeOption reads "roundingMode").
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
        // Return ! CreateTemporalDateTime(plainDateTime.[[ISODateTime]], plainDateTime.[[Calendar]]).
        return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(), computeISODate(state), plainTime(), calendarID());
    }

    // Let result be RoundISODateTime(plainDateTime.[[ISODateTime]], roundingIncrement, smallestUnit, roundingMode).
    auto result = Temporal::roundISODateTime(state, ISO8601::PlainDateTime(computeISODate(state), plainTime()), roundingIncrement, toDateTimeUnit(smallestUnit).value(), roundingMode);
    // Return ? CreateTemporalDateTime(result, plainDateTime.[[Calendar]]).
    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporalPlainDateTimePrototype(), result.plainDate(), result.plainTime(), calendarID());
}

bool TemporalPlainDateTimeObject::equals(ExecutionState& state, Value otherInput)
{
    // Set other to ? ToTemporalDateTime(other).
    auto other = Temporal::toTemporalDateTime(state, otherInput, Value());

    // If CompareISODateTime(plainDateTime.[[ISODateTime]], other.[[ISODateTime]]) ≠ 0, return false.
    auto isoDateTime1 = computeISODate(state);
    auto isoDateTime2 = other->computeISODate(state);
    if (isoDateTime1 != isoDateTime2 || plainTime() != other->plainTime()) {
        return false;
    }
    // Return CalendarEquals(plainDateTime.[[Calendar]], other.[[Calendar]]).
    return calendarID() == other->calendarID();
}

} // namespace Escargot

#endif
