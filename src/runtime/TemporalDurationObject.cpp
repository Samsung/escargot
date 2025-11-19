#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TemporalDurationObject.h"
#include "TemporalZonedDateTimeObject.h"
#include "TemporalPlainDateObject.h"
#include "TemporalPlainDateTimeObject.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, Object* proto,
                                               const Value& years, const Value& months, const Value& weeks, const Value& days,
                                               const Value& hours, const Value& minutes, const Value& seconds, const Value& milliseconds,
                                               const Value& microseconds, const Value& nanoseconds)
    : DerivedObject(state, proto)
{
    // https://tc39.es/proposal-temporal/#sec-temporal.duration
    // If NewTarget is undefined, throw a TypeError exception.
    // If years is undefined, let y be 0; else let y be ? ToIntegerIfIntegral(years).
    // If months is undefined, let mo be 0; else let mo be ? ToIntegerIfIntegral(months).
    // If weeks is undefined, let w be 0; else let w be ? ToIntegerIfIntegral(weeks).
    // If days is undefined, let d be 0; else let d be ? ToIntegerIfIntegral(days).
    // If hours is undefined, let h be 0; else let h be ? ToIntegerIfIntegral(hours).
    // If minutes is undefined, let m be 0; else let m be ? ToIntegerIfIntegral(minutes).
    // If seconds is undefined, let s be 0; else let s be ? ToIntegerIfIntegral(seconds).
    // If milliseconds is undefined, let ms be 0; else let ms be ? ToIntegerIfIntegral(milliseconds).
    // If microseconds is undefined, let mis be 0; else let mis be ? ToIntegerIfIntegral(microseconds).
    // If nanoseconds is undefined, let ns be 0; else let ns be ? ToIntegerIfIntegral(nanoseconds).
    // Return ? CreateTemporalDuration(y, mo, w, d, h, m, s, ms, mis, ns, NewTarget).
#define DEFINE_SETTER(name, Name, names, Names, index, category) \
    if (!names.isUndefined()) {                                  \
        m_duration[index] = names.toIntegerIfIntergral(state);   \
    }
    PLAIN_DATETIME_UNITS(DEFINE_SETTER)
#undef DEFINE_SETTER
}

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, const ISO8601::Duration& duration)
    : DerivedObject(state, state.context()->globalObject()->temporalDurationPrototype())
    , m_duration(duration)
{
}

ISO8601::Duration TemporalDurationObject::temporalDurationFromInternal(ExecutionState& state, ISO8601::InternalDuration internalDuration, ISO8601::DateTimeUnit largestUnit)
{
    // Let days, hours, minutes, seconds, milliseconds, and microseconds be 0.
    double days = 0, hours = 0, minutes = 0, seconds = 0;
    Int128 milliseconds = 0, microseconds = 0;

    // Let sign be TimeDurationSign(internalDuration.[[Time]]).
    // Let nanoseconds be abs(internalDuration.[[Time]]).
    int32_t sign;
    Int128 nanoseconds = internalDuration.time();
    if (ISO8601::toDateTimeCategory(largestUnit) == ISO8601::DateTimeUnitCategory::Date) {
        sign = internalDuration.sign();
        nanoseconds = std::abs(nanoseconds);
    } else {
        sign = internalDuration.timeDurationSign();
        if (sign == -1 && internalDuration.dateDuration().sign() == 1) {
            if (largestUnit == ISO8601::DateTimeUnit::Hour) {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerDay;
            } else if (largestUnit == ISO8601::DateTimeUnit::Minute) {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerHour;
            } else if (largestUnit == ISO8601::DateTimeUnit::Second) {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerMinute;
            } else if (largestUnit == ISO8601::DateTimeUnit::Millisecond) {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerSecond;
            } else if (largestUnit == ISO8601::DateTimeUnit::Microsecond) {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerMillisecond;
            } else {
                nanoseconds = nanoseconds + ISO8601::ExactTime::nsPerMicrosecond;
            }
        } else if (sign == 1 && internalDuration.dateDuration().sign() == -1) {
            if (largestUnit == ISO8601::DateTimeUnit::Hour) {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerDay;
            } else if (largestUnit == ISO8601::DateTimeUnit::Minute) {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerHour;
            } else if (largestUnit == ISO8601::DateTimeUnit::Second) {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerMinute;
            } else if (largestUnit == ISO8601::DateTimeUnit::Millisecond) {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerSecond;
            } else if (largestUnit == ISO8601::DateTimeUnit::Microsecond) {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerMillisecond;
            } else {
                nanoseconds = nanoseconds - ISO8601::ExactTime::nsPerMicrosecond;
            }
        }
    }

    // If TemporalUnitCategory(largestUnit) is date, then
    if (ISO8601::toDateTimeCategory(largestUnit) == ISO8601::DateTimeUnitCategory::Date) {
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        // Set milliseconds to floor(microseconds / 1000).
        // Set microseconds to microseconds modulo 1000.
        // Set seconds to floor(milliseconds / 1000).
        // Set milliseconds to milliseconds modulo 1000.
        // Set minutes to floor(seconds / 60).
        // Set seconds to seconds modulo 60.
        // Set hours to floor(minutes / 60).
        // Set minutes to minutes modulo 60.
        // Set days to floor(hours / 24).
        // Set hours to hours modulo 24.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double)(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
        hours = std::trunc(minutes / 60);
        minutes = std::fmod(minutes, 60);
        days = std::trunc(hours / 24);
        hours = std::fmod(hours, 24);
    } else if (largestUnit == ISO8601::DateTimeUnit::Hour) {
        // Else if largestUnit is hour, then
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        // Set milliseconds to floor(microseconds / 1000).
        // Set microseconds to microseconds modulo 1000.
        // Set seconds to floor(milliseconds / 1000).
        // Set milliseconds to milliseconds modulo 1000.
        // Set minutes to floor(seconds / 60).
        // Set seconds to seconds modulo 60.
        // Set hours to floor(minutes / 60).
        // Set minutes to minutes modulo 60.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double)(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
        hours = std::trunc(minutes / 60);
        minutes = std::fmod(minutes, 60);
    } else if (largestUnit == ISO8601::DateTimeUnit::Minute) {
        // Else if largestUnit is minute, then
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        // Set milliseconds to floor(microseconds / 1000).
        // Set microseconds to microseconds modulo 1000.
        // Set seconds to floor(milliseconds / 1000).
        // Set milliseconds to milliseconds modulo 1000.
        // Set minutes to floor(seconds / 60).
        // Set seconds to seconds modulo 60.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double)(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = std::trunc(seconds / 60);
        seconds = std::fmod(seconds, 60);
    } else if (largestUnit == ISO8601::DateTimeUnit::Second) {
        // Else if largestUnit is second, then
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        // Set milliseconds to floor(microseconds / 1000).
        // Set microseconds to microseconds modulo 1000.
        // Set seconds to floor(milliseconds / 1000).
        // Set milliseconds to milliseconds modulo 1000.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = (double)(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
    } else if (largestUnit == ISO8601::DateTimeUnit::Millisecond) {
        // Else if largestUnit is millisecond, then
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        // Set milliseconds to floor(microseconds / 1000).
        // Set microseconds to microseconds modulo 1000.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
    } else if (largestUnit == ISO8601::DateTimeUnit::Microsecond) {
        // Else if largestUnit is microsecond, then
        // Set microseconds to floor(nanoseconds / 1000).
        // Set nanoseconds to nanoseconds modulo 1000.
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
    } else {
        // Assert: largestUnit is nanosecond.
    }

    if (ISO8601::toDateTimeCategory(largestUnit) == ISO8601::DateTimeUnitCategory::Date) {
        if (hours) {
            hours *= sign;
        }
        if (minutes) {
            minutes *= sign;
        }
        if (seconds) {
            seconds *= sign;
        }
        if (milliseconds) {
            milliseconds *= sign;
        }
        if (microseconds) {
            microseconds *= sign;
        }
        if (nanoseconds) {
            nanoseconds *= sign;
        }
    } else {
        // remove -0.0
        if (std::signbit(days) && !days) {
            days = 0;
        }
        if (std::signbit(hours) && !hours) {
            hours = 0;
        }
        if (std::signbit(minutes) && !minutes) {
            minutes = 0;
        }
        if (std::signbit(seconds) && !seconds) {
            seconds = 0;
        }
    }

    // Return ? CreateTemporalDuration(internalDuration.[[Date]].[[Years]], internalDuration.[[Date]].[[Months]],
    // internalDuration.[[Date]].[[Weeks]], internalDuration.[[Date]].[[Days]] + days × sign,
    // hours × sign, minutes × sign, seconds × sign, milliseconds × sign, microseconds × sign, nanoseconds × sign).
    auto value = ISO8601::Duration{ internalDuration.dateDuration().years(), internalDuration.dateDuration().months(), internalDuration.dateDuration().weeks(), internalDuration.dateDuration().days() + days * sign, hours, minutes,
                                    static_cast<double>(seconds), static_cast<double>(milliseconds), static_cast<double>(microseconds), static_cast<double>(nanoseconds) };

    if (!value.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid Duration value");
    }

    return value;
}

ISO8601::Duration TemporalDurationObject::createNegatedTemporalDuration(ISO8601::Duration duration)
{
    // Return ! CreateTemporalDuration(-duration.[[Years]], -duration.[[Months]], -duration.[[Weeks]], -duration.[[Days]],
    // -duration.[[Hours]], -duration.[[Minutes]], -duration.[[Seconds]], -duration.[[Milliseconds]], -duration.[[Microseconds]], -duration.[[Nanoseconds]]).
    for (size_t i = 0; i < 10; i++) {
        // avoiding -0.0
        if (duration[i]) {
            duration[i] = -duration[i];
        }
    }
    return duration;
}

static void appendInteger(StringBuilder& builder, double value)
{
    ASSERT(std::isfinite(value));

    auto absValue = std::abs(value);
    if (absValue <= 9007199254740991.0 /* MAX_SAFE_INTEGER */) {
        builder.appendString(String::fromDouble(absValue));
        return;
    }

    BigIntData bi(absValue);
    std::string s = bi.toNonGCStdString();
    builder.appendString(String::fromASCII(s.data(), s.length()));
}

static void appendInteger(StringBuilder& builder, Int128 value)
{
    std::string s = std::to_string(std::abs(value));
    builder.appendString(String::fromASCII(s.data(), s.length()));
}

String* TemporalDurationObject::temporalDurationToString(ISO8601::Duration duration, Value precision)
{
    auto balancedMicroseconds = static_cast<Int128>(static_cast<int64_t>(duration.microseconds())) + static_cast<Int128>(static_cast<int64_t>(std::trunc(duration.nanoseconds() / 1000)));
    auto balancedNanoseconds = static_cast<Int128>(static_cast<int64_t>(duration.nanoseconds())) % 1000;
    auto balancedMilliseconds = static_cast<Int128>(static_cast<int64_t>(duration.milliseconds())) + (balancedMicroseconds / 1000);
    balancedMicroseconds = balancedMicroseconds % 1000;
    auto balancedSeconds = static_cast<Int128>(static_cast<int64_t>(duration.seconds())) + (balancedMilliseconds / 1000);
    balancedMilliseconds = balancedMilliseconds % 1000;

    StringBuilder builder;

    auto sign = duration.sign();
    if (sign < 0)
        builder.appendChar('-');

    builder.appendChar('P');
    if (duration.years()) {
        appendInteger(builder, duration.years());
        builder.appendChar('Y');
    }
    if (duration.months()) {
        appendInteger(builder, duration.months());
        builder.appendChar('M');
    }
    if (duration.weeks()) {
        appendInteger(builder, duration.weeks());
        builder.appendChar('W');
    }
    if (duration.days()) {
        appendInteger(builder, duration.days());
        builder.appendChar('D');
    }

    // The zero value is displayed in seconds.
    auto usesSeconds = balancedSeconds || balancedMilliseconds || balancedMicroseconds || balancedNanoseconds || !sign || !precision.isString() || !precision.asString()->equals("auto");
    if (!duration.hours() && !duration.minutes() && !usesSeconds)
        return builder.finalize();

    builder.appendChar('T');
    if (duration.hours()) {
        appendInteger(builder, duration.hours());
        builder.appendChar('H');
    }
    if (duration.minutes()) {
        appendInteger(builder, duration.minutes());
        builder.appendChar('M');
    }
    if (usesSeconds) {
        appendInteger(builder, balancedSeconds);

        // Int128 fraction = static_cast<int64_t>(std::abs(balancedMilliseconds) * 1e6 + std::abs(balancedMicroseconds) * 1e3 + std::abs(balancedNanoseconds));
        Int128 fraction = std::abs(balancedMilliseconds) * 1000000 + std::abs(balancedMicroseconds) * 1000 + std::abs(balancedNanoseconds);
        Temporal::formatSecondsStringFraction(builder, static_cast<int64_t>(fraction), precision);

        builder.appendChar('S');
    }

    return builder.finalize();
}

String* TemporalDurationObject::toString(ExecutionState& state, Value options)
{
    // Let duration be the this value.
    // Perform ? RequireInternalSlot(duration, [[InitializedTemporalDuration]]).
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetTemporalFractionalSecondDigitsOption reads "fractionalSecondDigits" and GetRoundingModeOption reads "roundingMode").
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour or minute, throw a RangeError exception.
    if (smallestUnit && (smallestUnit.value() == TemporalUnit::Hour || smallestUnit.value() == TemporalUnit::Minute)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit value");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);
    // If precision.[[Unit]] is nanosecond and precision.[[Increment]] = 1, then
    if (precision.unit == ISO8601::DateTimeUnit::Nanosecond && precision.increment == 1) {
        // Return TemporalDurationToString(duration, precision.[[Precision]]).
        return temporalDurationToString(m_duration, precision.precision);
    }

    // Let largestUnit be DefaultTemporalLargestUnit(duration).
    auto largestUnit = m_duration.defaultTemporalLargestUnit();
    // Let internalDuration be ToInternalDurationRecord(duration).
    ISO8601::InternalDuration internalDuration = toInternalDurationRecord(m_duration);
    // Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto timeDuration = roundTimeDuration(state, internalDuration.time(), precision.increment, precision.unit, roundingMode);
    // Set internalDuration to CombineDateAndTimeDuration(internalDuration.[[Date]], timeDuration).
    internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(internalDuration.dateDuration(), timeDuration);
    // Let roundedLargestUnit be LargerOfTwoTemporalUnits(largestUnit, second).
    auto roundedLargestUnit = Temporal::largerOfTwoTemporalUnits(largestUnit, ISO8601::DateTimeUnit::Second);
    // Let roundedDuration be ? TemporalDurationFromInternal(internalDuration, roundedLargestUnit).
    auto roundedDuration = temporalDurationFromInternal(state, internalDuration, roundedLargestUnit);
    // Return TemporalDurationToString(roundedDuration, precision.[[Precision]]).
    return temporalDurationToString(roundedDuration, precision.precision);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
TemporalDurationObject* TemporalDurationObject::with(ExecutionState& state, Value temporalDurationLike)
{
    // Let duration be the this value.
    // Perform ? RequireInternalSlot(duration, [[InitializedTemporalDuration]]).
    // Let temporalDurationLike be ? ToTemporalPartialDurationRecord(temporalDurationLike).
    auto temporalPartialDuration = toTemporalPartialDurationRecord(state, temporalDurationLike);

    // Step 4 to 23
    ISO8601::Duration newDuration;
    size_t idx = 0;
    for (auto n : temporalPartialDuration) {
        if (n) {
            newDuration[idx] = n.value();
        } else {
            newDuration[idx] = m_duration[idx];
        }
        idx++;
    }

    // Return ? CreateTemporalDuration(years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds).
    if (!newDuration.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid duration value");
    }

    return new TemporalDurationObject(state, newDuration);
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalrelativetooption
struct TemporalRelativeToOptionRecord {
    Optional<TemporalPlainDateObject*> plainRelativeTo;
    Optional<TemporalZonedDateTimeObject*> zonedRelativeTo;
};

static TemporalRelativeToOptionRecord getTemporalRelativeToOption(ExecutionState& state, Object* options)
{
    // Let value be ? Get(options, "relativeTo").
    Value value = options->get(state, state.context()->staticStrings().lazyRelativeTo()).value(state, options);
    // If value is undefined, return the Record { [[PlainRelativeTo]]: undefined, [[ZonedRelativeTo]]: undefined }.
    if (value.isUndefined()) {
        return {};
    }

    Calendar calendar;
    ISO8601::PlainDate isoDate;
    ISO8601::PlainTime time;
    Optional<TimeZone> timeZone;

    // Let offsetBehaviour be option.
    TemporalOffsetBehaviour offsetBehaviour = TemporalOffsetBehaviour::Option;
    // Let matchBehaviour be match-exactly.
    TemporalMatchBehaviour matchBehaviour = TemporalMatchBehaviour::MatchExactly;
    // If value is an Object, then
    if (value.isObject()) {
        // If value has an [[InitializedTemporalZonedDateTime]] internal slot, then
        if (value.asObject()->isTemporalZonedDateTimeObject()) {
            // Return the Record { [[PlainRelativeTo]]: undefined, [[ZonedRelativeTo]]: value }.
            return { NullOption, value.asObject()->asTemporalZonedDateTimeObject() };
        } else if (value.asObject()->isTemporalPlainDateObject()) {
            // If value has an [[InitializedTemporalDate]] internal slot, then
            // Return the Record { [[PlainRelativeTo]]: value, [[ZonedRelativeTo]]: undefined }.
            return { value.asObject()->asTemporalPlainDateObject(), NullOption };
        } else if (value.asObject()->isTemporalPlainDateTimeObject()) {
            // If value has an [[InitializedTemporalDateTime]] internal slot, then
            auto pdt = value.asObject()->asTemporalPlainDateTimeObject();
            // Let plainDate be ! CreateTemporalDate(value.[[ISODateTime]].[[ISODate]], value.[[Calendar]]).
            // Return the Record { [[PlainRelativeTo]]: plainDate, [[ZonedRelativeTo]]: undefined }.
            return { new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                                 pdt->computeISODate(state), pdt->calendarID()),
                     NullOption };
        }
        // Let calendar be ? GetTemporalCalendarIdentifierWithISODefault(value).
        auto calendar = Temporal::getTemporalCalendarIdentifierWithISODefault(state, value);
        // Let fields be ? PrepareCalendarFields(calendar, value, « year, month, month-code, day », « hour, minute, second, millisecond, microsecond, nanosecond, offset, time-zone », «»).
        CalendarField calendarFieldNames[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
        CalendarField noncalendarFieldNames[8] = { CalendarField::Hour, CalendarField::Minute, CalendarField::Second, CalendarField::Millisecond,
                                                   CalendarField::Microsecond, CalendarField::Nanosecond, CalendarField::Offset, CalendarField::TimeZone };
        auto fields = Temporal::prepareCalendarFields(state, calendar, value.asObject(), calendarFieldNames, 4, noncalendarFieldNames, 8, nullptr, 0);
        // Let result be ? InterpretTemporalDateTimeFields(calendar, fields, constrain).
        auto result = Temporal::interpretTemporalDateTimeFields(state, calendar, fields, TemporalOverflowOption::Constrain);
        // Let timeZone be fields.[[TimeZone]].
        timeZone = fields.timeZone;
        // Let offsetString be fields.[[OffsetString]].
        // If offsetString is unset, then
        if (!fields.offset) {
            // Set offsetBehaviour to wall.
            offsetBehaviour = TemporalOffsetBehaviour::Wall;
        }
        // Let isoDate be result.[[ISODate]].
        isoDate = result.plainDate();
        // Let time be result.[[Time]].
        time = result.plainTime();
    } else {
        constexpr auto msg = "The value you gave for GetTemporalRelativeToOption is invalid";
        // If value is not a String, throw a TypeError exception.
        if (!value.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
        }

        // Let result be ? ParseISODateTime(value, « TemporalDateTimeString[+Zoned], TemporalDateTimeString[~Zoned] »).
        ISO8601::DateTimeParseOption option;
        option.allowTimeZoneTimeWithoutTime = true;
        option.parseSubMinutePrecisionForTimeZone = ISO8601::DateTimeParseOption::SubMinutePrecisionForTimeZoneMode::Allow00;
        auto result = ISO8601::parseCalendarDateTime(value.asString(), option);
        if (!result) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        auto unwrappedResult = result.value();
        // Let offsetString be result.[[TimeZone]].[[OffsetString]].
        // Let annotation be result.[[TimeZone]].[[TimeZoneAnnotation]].
        // If annotation is empty, then
        //   Let timeZone be unset.
        if (std::get<2>(unwrappedResult)) {
            auto timeZoneRecord = std::get<2>(unwrappedResult).value();

            if (timeZoneRecord.m_z && !timeZoneRecord.m_nameOrOffset) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
            }

            if (timeZoneRecord.m_nameOrOffset && timeZoneRecord.m_nameOrOffset.id().value() == 0 && timeZoneRecord.m_offset) {
                auto epoch = ISO8601::ExactTime::fromPlainDateTime(ISO8601::PlainDateTime(std::get<0>(unwrappedResult),
                                                                                          std::get<1>(unwrappedResult) ? std::get<1>(unwrappedResult).value() : ISO8601::PlainTime()))
                                 .floorEpochMilliseconds();
                if (timeZoneRecord.m_offset.value() != Temporal::computeTimeZoneOffset(state, timeZoneRecord.m_nameOrOffset.get<0>(), epoch)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
                }
            }

            // Else,
            // Let timeZone be ? ToTemporalTimeZoneIdentifier(annotation).
            if (timeZoneRecord.m_nameOrOffset && timeZoneRecord.m_nameOrOffset.id().value() == 0) {
                timeZone = TimeZone(timeZoneRecord.m_nameOrOffset.get<0>());
            } else if (timeZoneRecord.m_nameOrOffset && timeZoneRecord.m_nameOrOffset.id().value() == 1) {
                timeZone = TimeZone(timeZoneRecord.m_nameOrOffset.get<1>());
            } else if (timeZoneRecord.m_offset) {
                timeZone = TimeZone(timeZoneRecord.m_offset.value());
            }
            // If result.[[TimeZone]].[[Z]] is true, then
            if (timeZoneRecord.m_z) {
                // Set offsetBehaviour to exact.
                offsetBehaviour = TemporalOffsetBehaviour::Exact;
            } else if (!timeZoneRecord.m_offset) {
                // Else if offsetString is empty, then
                // Set offsetBehaviour to wall.
                offsetBehaviour = TemporalOffsetBehaviour::Wall;
            }
            // Set matchBehaviour to match-minutes.
            matchBehaviour = TemporalMatchBehaviour::MatchMinutes;
            // If offsetString is not empty, then
            if (timeZoneRecord.m_offset) {
                // Let offsetParseResult be ParseText(StringToCodePoints(offsetString), UTCOffset[+SubMinutePrecision]).
                // If offsetParseResult contains more than one MinuteSecond Parse Node, set matchBehaviour to match-exactly.
                if (timeZoneRecord.m_offset.value() % ISO8601::ExactTime::nsPerHour) {
                    matchBehaviour = TemporalMatchBehaviour::MatchExactly;
                }
            }
        }
        // Let calendar be result.[[Calendar]].
        // If calendar is empty, set calendar to "iso8601".
        // Set calendar to ? CanonicalizeCalendar(calendar).
        if (std::get<3>(unwrappedResult)) {
            auto mayID = Calendar::fromString(std::get<3>(unwrappedResult).value());
            if (!mayID) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid CalendarID");
            }
            calendar = mayID.value();
        }
        // Let isoDate be CreateISODateRecord(result.[[Year]], result.[[Month]], result.[[Day]]).
        isoDate = std::get<0>(unwrappedResult);
        // Let time be result.[[Time]].
        if (std::get<1>(unwrappedResult)) {
            time = std::get<1>(unwrappedResult).value();
        }
    }

    // If timeZone is unset, then
    if (!timeZone) {
        // Let plainDate be ? CreateTemporalDate(isoDate, calendar).
        // Return the Record { [[PlainRelativeTo]]: plainDate, [[ZonedRelativeTo]]: undefined }.
        return { new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                             isoDate, calendar),
                 NullOption };
    }

    int64_t offsetNs = 0;
    // If offsetBehaviour is option, then
    if (offsetBehaviour == TemporalOffsetBehaviour::Option) {
        // Let offsetNs be ! ParseDateTimeUTCOffset(offsetString).
        if (timeZone.value().hasOffset()) {
            offsetNs = timeZone.value().offset();
        }
    }
    // Else,
    //   Let offsetNs be 0.
    // Let epochNanoseconds be ? InterpretISODateTimeOffset(isoDate, time, offsetBehaviour, offsetNs, timeZone, compatible, reject, matchBehaviour).
    auto epochNanoseconds = Temporal::interpretISODateTimeOffset(state, isoDate, time, offsetBehaviour, offsetNs, timeZone.value(), false, TemporalDisambiguationOption::Compatible, TemporalOffsetOption::Reject, matchBehaviour);
    // Let zonedRelativeTo be ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
    // Return the Record { [[PlainRelativeTo]]: undefined, [[ZonedRelativeTo]]: zonedRelativeTo }.
    return { NullOption,
             new TemporalZonedDateTimeObject(state, state.context()->globalObject()->temporalZonedDateTimePrototype(), epochNanoseconds, timeZone.value(), calendar) };
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetimewithtotal
static double differenceZonedDateTimeWithTotal(ExecutionState& state, Int128 ns1, Int128 ns2, TimeZone timeZone, Calendar calendar, ISO8601::DateTimeUnit unit)
{
    // If TemporalUnitCategory(unit) is time, then
    if (ISO8601::toDateTimeCategory(unit) == ISO8601::DateTimeUnitCategory::Time) {
        // Let difference be TimeDurationFromEpochNanosecondsDifference(ns2, ns1).
        Int128 difference = Temporal::timeDurationFromEpochNanosecondsDifference(ns2, ns1);
        // Return TotalTimeDuration(difference, unit).
        return Temporal::totalTimeDuration(state, difference, toTemporalUnit(unit));
    }
    // Let difference be ? DifferenceZonedDateTime(ns1, ns2, timeZone, calendar, unit).
    auto difference = TemporalZonedDateTimeObject::differenceZonedDateTime(state, ns1, ns2, timeZone, calendar, unit);
    // Let dateTime be GetISODateTimeFor(timeZone, ns1).
    auto dateTime = Temporal::getISODateTimeFor(state, timeZone, ns1);
    // Return ? TotalRelativeDuration(difference, ns1, ns2, dateTime, timeZone, calendar, unit).
    return Temporal::totalRelativeDuration(state, difference, ns1, ns2, dateTime, timeZone, calendar, unit);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differenceplaindatetimewithtotal
static double differencePlainDateTimeWithTotal(ExecutionState& state, ISO8601::PlainDateTime isoDateTime1, ISO8601::PlainDateTime isoDateTime2, Calendar calendar, ISO8601::DateTimeUnit unit)
{
    // If CompareISODateTime(isoDateTime1, isoDateTime2) = 0, then
    if (isoDateTime1 == isoDateTime2) {
        return 0;
    }

    // If ISODateTimeWithinLimits(isoDateTime1) is false or ISODateTimeWithinLimits(isoDateTime2) is false, throw a RangeError exception.
    if (!ISO8601::isoDateTimeWithinLimits(isoDateTime1) || !ISO8601::isoDateTimeWithinLimits(isoDateTime2)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid datetime in DifferencePlainDateTimeWithTotal");
    }
    // Let diff be DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, unit).
    auto diff = Temporal::differenceISODateTime(state, isoDateTime1, isoDateTime2, calendar, unit);
    // If unit is nanosecond, return diff.[[Time]].
    if (unit == ISO8601::DateTimeUnit::Nanosecond) {
        return double(diff.time());
    }
    // Let originEpochNs be GetUTCEpochNanoseconds(isoDateTime1).
    auto originEpochNs = ISO8601::ExactTime::fromPlainDateTime(isoDateTime1).epochNanoseconds();
    // Let destEpochNs be GetUTCEpochNanoseconds(isoDateTime2).
    auto destEpochNs = ISO8601::ExactTime::fromPlainDateTime(isoDateTime2).epochNanoseconds();
    // Return ? TotalRelativeDuration(diff, originEpochNs, destEpochNs, isoDateTime1, unset, calendar, unit).
    return Temporal::totalRelativeDuration(state, diff, originEpochNs, destEpochNs, isoDateTime1, NullOption, calendar, unit);
}

double TemporalDurationObject::total(ExecutionState& state, Value totalOfInput)
{
    Object* totalOf;
    // If totalOf is undefined, throw a TypeError exception.
    if (totalOfInput.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid totalOfInput value");
        ASSERT_NOT_REACHED();
    }
    // If totalOf is a String, then
    if (totalOfInput.isString()) {
        // Let paramString be totalOf.
        // Set totalOf to OrdinaryObjectCreate(null).
        totalOf = new Object(state, Object::PrototypeIsNull);
        // Perform ! CreateDataPropertyOrThrow(totalOf, "unit", paramString).
        totalOf->directDefineOwnProperty(state, state.context()->staticStrings().lazyUnit(),
                                         ObjectPropertyDescriptor(totalOfInput, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    } else {
        // Else,
        // Set totalOf to ? GetOptionsObject(totalOf).
        totalOf = Intl::getOptionsObject(state, totalOfInput).value();
    }

    // Let relativeToRecord be ? GetTemporalRelativeToOption(totalOf).
    auto relativeToRecord = getTemporalRelativeToOption(state, totalOf);
    // Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
    auto zonedRelativeTo = relativeToRecord.zonedRelativeTo;
    // Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].
    auto plainRelativeTo = relativeToRecord.plainRelativeTo;
    // Let unit be ? GetTemporalUnitValuedOption(totalOf, "unit", required).
    auto unit = Temporal::getTemporalUnitValuedOption(state, totalOf, state.context()->staticStrings().lazyUnit().string(), Value(Value::EmptyValue));
    // Perform ? ValidateTemporalUnitValue(unit, datetime).
    Temporal::validateTemporalUnitValue(state, unit, ISO8601::DateTimeUnitCategory::DateTime, nullptr, 0);
    // If zonedRelativeTo is not undefined, then
    double total;
    if (zonedRelativeTo) {
        // Let internalDuration be ToInternalDurationRecord(duration).
        ISO8601::InternalDuration internalDuration = toInternalDurationRecord(m_duration);
        // Let timeZone be zonedRelativeTo.[[TimeZone]].
        TimeZone timeZone = zonedRelativeTo->timeZone();
        // Let calendar be zonedRelativeTo.[[Calendar]].
        auto calendar = zonedRelativeTo->calendarID();
        // Let relativeEpochNs be zonedRelativeTo.[[EpochNanoseconds]].
        auto relativeEpochNs = zonedRelativeTo->epochNanoseconds();
        // Let targetEpochNs be ? AddZonedDateTime(relativeEpochNs, timeZone, calendar, internalDuration, constrain).
        auto targetEpochNs = Temporal::addZonedDateTime(state, relativeEpochNs, timeZone, calendar, internalDuration, TemporalOverflowOption::Constrain);
        // Let total be ? DifferenceZonedDateTimeWithTotal(relativeEpochNs, targetEpochNs, timeZone, calendar, unit).
        total = differenceZonedDateTimeWithTotal(state, relativeEpochNs, targetEpochNs, timeZone, calendar, toDateTimeUnit(unit.value()));
    } else if (plainRelativeTo) {
        // Else if plainRelativeTo is not undefined, then
        // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(state, m_duration);
        // Let targetTime be AddTime(MidnightTimeRecord(), internalDuration.[[Time]]).
        auto targetTime = Temporal::balanceTime(0, 0, 0, 0, 0, internalDuration.time());
        // Let calendar be plainRelativeTo.[[Calendar]].
        auto calendar = plainRelativeTo->calendarID();
        // Let dateDuration be ! AdjustDateDurationRecord(internalDuration.[[Date]], targetTime.[[Days]]).
        auto dateDuration = Temporal::adjustDateDurationRecord(state, internalDuration.dateDuration(), targetTime.days(), NullOption, NullOption);
        // Let targetDate be ? CalendarDateAdd(calendar, plainRelativeTo.[[ISODate]], dateDuration, constrain).
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UCalendar> newCal(calendar.createICUCalendar(state), [](UCalendar* r) {
            ucal_close(r);
        });
        auto inputDate = plainRelativeTo->computeISODate(state);
        ucal_setMillis(newCal.get(), ISO8601::ExactTime::fromPlainDate(inputDate).floorEpochMilliseconds(), &status);
        CHECK_ICU();
        auto targetDatePair = Temporal::calendarDateAdd(state, calendar, plainRelativeTo->computeISODate(state), newCal.get(), dateDuration, TemporalOverflowOption::Constrain);
        ucal_close(targetDatePair.first);
        auto targetDate = targetDatePair.second;
        // Let isoDateTime be CombineISODateAndTimeRecord(plainRelativeTo.[[ISODate]], MidnightTimeRecord()).
        ISO8601::PlainDateTime isoDateTime(inputDate, ISO8601::PlainTime());
        // Let targetDateTime be CombineISODateAndTimeRecord(targetDate, targetTime).
        ISO8601::PlainDateTime targetDateTime(targetDate, ISO8601::PlainTime(targetTime.hours(), targetTime.minutes(), targetTime.seconds(), targetTime.milliseconds(), targetTime.microseconds(), targetTime.nanoseconds()));
        // Let total be ? DifferencePlainDateTimeWithTotal(isoDateTime, targetDateTime, calendar, unit).
        total = differencePlainDateTimeWithTotal(state, isoDateTime, targetDateTime, calendar, toDateTimeUnit(unit.value()));
    } else {
        // Else,
        // Let largestUnit be DefaultTemporalLargestUnit(duration).
        auto largestUnit = m_duration.defaultTemporalLargestUnit();
        // If IsCalendarUnit(largestUnit) is true, or IsCalendarUnit(unit) is true, throw a RangeError exception.
        if (Temporal::isCalendarUnit(largestUnit) || Temporal::isCalendarUnit(toDateTimeUnit(unit.value()))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid unit");
        }
        // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        auto internalDuration = toInternalDurationRecordWith24HourDays(state, m_duration);
        // Let total be TotalTimeDuration(internalDuration.[[Time]], unit).
        return Temporal::totalTimeDuration(state, internalDuration.time(), unit.value());
    }

    // Return 𝔽(total).
    return total;
}

TemporalDurationObject* TemporalDurationObject::round(ExecutionState& state, Value roundToInput)
{
    Object* roundTo;
    // If roundTo is undefined, then
    if (roundToInput.isUndefined()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid roundTo value");
        ASSERT_NOT_REACHED();
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
        roundTo = Intl::getOptionsObject(state, roundToInput).value();
    }

    // Let smallestUnitPresent be true.
    bool smallestUnitPresent = true;
    // Let largestUnitPresent be true.
    bool largestUnitPresent = true;

    // Let largestUnit be ? GetTemporalUnitValuedOption(roundTo, "largestUnit", unset).
    auto largestUnit = Temporal::getTemporalUnitValuedOption(state, roundTo, state.context()->staticStrings().lazyLargestUnit().string(), NullOption);
    // Let relativeToRecord be ? GetTemporalRelativeToOption(roundTo).
    auto relativeToRecord = getTemporalRelativeToOption(state, roundTo);
    // Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
    auto zonedRelativeTo = relativeToRecord.zonedRelativeTo;
    // Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].
    auto plainRelativeTo = relativeToRecord.plainRelativeTo;

    // Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    auto roundingIncrement = Temporal::getRoundingIncrementOption(state, roundTo);
    // Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
    auto roundingMode = Temporal::getRoundingModeOption(state, roundTo, state.context()->staticStrings().lazyHalfExpand().string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, roundTo,
                                                              state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, datetime).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::DateTime, nullptr, 0);

    // If smallestUnit is unset, then
    if (!smallestUnit) {
        // Set smallestUnitPresent to false.
        smallestUnitPresent = false;
        // Set smallestUnit to nanosecond.
        smallestUnit = TemporalUnit::Nanosecond;
    }

    // Let existingLargestUnit be DefaultTemporalLargestUnit(duration).
    auto existingLargestUnit = m_duration.defaultTemporalLargestUnit();
    // Let defaultLargestUnit be LargerOfTwoTemporalUnits(existingLargestUnit, smallestUnit).
    auto defaultLargestUnit = Temporal::largerOfTwoTemporalUnits(existingLargestUnit, toDateTimeUnit(smallestUnit.value()));

    // If largestUnit is unset, then
    if (!largestUnit) {
        // Set largestUnitPresent to false.
        largestUnitPresent = false;
        // Set largestUnit to defaultLargestUnit.
        largestUnit = toTemporalUnit(defaultLargestUnit);
    } else if (largestUnit.value() == TemporalUnit::Auto) {
        // Else if largestUnit is auto, then
        // Set largestUnit to defaultLargestUnit.
        largestUnit = toTemporalUnit(defaultLargestUnit);
    }

    // If smallestUnitPresent is false and largestUnitPresent is false, then
    if (!smallestUnitPresent && !largestUnitPresent) {
        // Throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid unit");
    }

    // If LargerOfTwoTemporalUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
    if (Temporal::largerOfTwoTemporalUnits(toDateTimeUnit(largestUnit.value()), toDateTimeUnit(smallestUnit.value())) != toDateTimeUnit(largestUnit.value())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid unit");
    }
    // Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
    auto maximum = Temporal::maximumTemporalDurationRoundingIncrement(toDateTimeUnit(smallestUnit.value()));
    // If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
    if (maximum) {
        Temporal::validateTemporalRoundingIncrement(state, roundingIncrement, maximum.value(), false);
    }
    // If roundingIncrement > 1, and largestUnit is not smallestUnit, and TemporalUnitCategory(smallestUnit) is date, throw a RangeError exception.
    if (roundingIncrement > 1 && largestUnit.value() != smallestUnit.value() && ISO8601::toDateTimeCategory(toDateTimeUnit(smallestUnit.value())) == ISO8601::DateTimeUnitCategory::Date) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid unit");
    }
    // If zonedRelativeTo is not undefined, then
    if (zonedRelativeTo) {
        // Let internalDuration be ToInternalDurationRecord(duration).
        ISO8601::InternalDuration internalDuration = toInternalDurationRecord(m_duration);
        // Let timeZone be zonedRelativeTo.[[TimeZone]].
        TimeZone timeZone = zonedRelativeTo->timeZone();
        // Let calendar be zonedRelativeTo.[[Calendar]].
        auto calendar = zonedRelativeTo->calendarID();
        // Let relativeEpochNs be zonedRelativeTo.[[EpochNanoseconds]].
        auto relativeEpochNs = zonedRelativeTo->epochNanoseconds();
        // Let targetEpochNs be ? AddZonedDateTime(relativeEpochNs, timeZone, calendar, internalDuration, constrain).
        auto targetEpochNs = Temporal::addZonedDateTime(state, relativeEpochNs, timeZone, calendar, internalDuration, TemporalOverflowOption::Constrain);
        // Set internalDuration to ? DifferenceZonedDateTimeWithRounding(relativeEpochNs, targetEpochNs, timeZone, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        internalDuration = TemporalZonedDateTimeObject::differenceZonedDateTimeWithRounding(state, relativeEpochNs, targetEpochNs, timeZone, calendar, toDateTimeUnit(largestUnit.value()),
                                                                                            roundingIncrement, toDateTimeUnit(smallestUnit.value()), roundingMode);
        // If TemporalUnitCategory(largestUnit) is date, set largestUnit to hour.
        if (ISO8601::toDateTimeCategory(toDateTimeUnit(largestUnit.value())) == ISO8601::DateTimeUnitCategory::Date) {
            largestUnit = TemporalUnit::Hour;
        }
        // Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
        auto resultDuration = temporalDurationFromInternal(state, internalDuration, toDateTimeUnit(largestUnit.value()));
        if (!resultDuration.isValid()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Result duration is out of range");
        }
        return new TemporalDurationObject(state, resultDuration);
    }
    // If plainRelativeTo is not undefined, then
    if (plainRelativeTo) {
        // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(state, m_duration);
        // Let targetTime be AddTime(MidnightTimeRecord(), internalDuration.[[Time]]).
        auto targetTime = Temporal::balanceTime(0, 0, 0, 0, 0, internalDuration.time());
        // Let calendar be plainRelativeTo.[[Calendar]].
        auto calendar = plainRelativeTo->calendarID();
        // Let dateDuration be ! AdjustDateDurationRecord(internalDuration.[[Date]], targetTime.[[Days]]).
        auto dateDuration = Temporal::adjustDateDurationRecord(state, internalDuration.dateDuration(), targetTime.days(), NullOption, NullOption);
        // Let targetDate be ? CalendarDateAdd(calendar, plainRelativeTo.[[ISODate]], dateDuration, constrain).
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UCalendar> newCal(calendar.createICUCalendar(state), [](UCalendar* r) {
            ucal_close(r);
        });
        auto inputDate = plainRelativeTo->computeISODate(state);
        ucal_setMillis(newCal.get(), ISO8601::ExactTime::fromPlainDate(inputDate).floorEpochMilliseconds(), &status);
        CHECK_ICU();
        auto targetDatePair = Temporal::calendarDateAdd(state, calendar, plainRelativeTo->computeISODate(state), newCal.get(), dateDuration, TemporalOverflowOption::Constrain);
        ucal_close(targetDatePair.first);
        auto targetDate = targetDatePair.second;
        // Let isoDateTime be CombineISODateAndTimeRecord(plainRelativeTo.[[ISODate]], MidnightTimeRecord()).
        ISO8601::PlainDateTime isoDateTime(plainRelativeTo->computeISODate(state), ISO8601::PlainTime());
        // Let targetDateTime be CombineISODateAndTimeRecord(targetDate, targetTime).
        ISO8601::PlainDateTime targetDateTime(targetDate, ISO8601::PlainTime(targetTime.hours(), targetTime.minutes(), targetTime.seconds(), targetTime.milliseconds(), targetTime.microseconds(), targetTime.nanoseconds()));
        // Set internalDuration to ? DifferencePlainDateTimeWithRounding(isoDateTime, targetDateTime, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        internalDuration = Temporal::differencePlainDateTimeWithRounding(state, isoDateTime, targetDateTime, calendar, toDateTimeUnit(largestUnit.value()),
                                                                         roundingIncrement, toDateTimeUnit(smallestUnit.value()), roundingMode);
        // Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
        auto resultDuration = temporalDurationFromInternal(state, internalDuration, toDateTimeUnit(largestUnit.value()));
        if (!resultDuration.isValid()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Result duration is out of range");
        }
        return new TemporalDurationObject(state, resultDuration);
    }
    // If IsCalendarUnit(existingLargestUnit) is true, or IsCalendarUnit(largestUnit) is true, throw a RangeError exception.
    if (Temporal::isCalendarUnit(existingLargestUnit) || Temporal::isCalendarUnit(toDateTimeUnit(largestUnit.value()))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid unit");
    }
    // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
    ISO8601::InternalDuration internalDuration = toInternalDurationRecordWith24HourDays(state, m_duration);
    // If smallestUnit is day, then
    if (smallestUnit.value() == TemporalUnit::Day) {
        // Let fractionalDays be TotalTimeDuration(internalDuration.[[Time]], day).
        auto fractionalDays = Temporal::totalTimeDuration(state, internalDuration.time(), TemporalUnit::Day);
        // Let days be RoundNumberToIncrement(fractionalDays, roundingIncrement, roundingMode).
        auto days = ISO8601::roundNumberToIncrement(fractionalDays, roundingIncrement, roundingMode);
        // Let dateDuration be ? CreateDateDurationRecord(0, 0, 0, days).
        ISO8601::Duration dateDuration = ISO8601::Duration{ 0.0, 0.0, 0.0, double(days), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        // Set internalDuration to CombineDateAndTimeDuration(dateDuration, 0).
        internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, 0);
    } else {
        // Else,
        // Let timeDuration be ? RoundTimeDuration(internalDuration.[[Time]], roundingIncrement, smallestUnit, roundingMode).
        auto timeDuration = roundTimeDuration(state, internalDuration.time(), roundingIncrement, toDateTimeUnit(smallestUnit.value()), roundingMode);
        // Set internalDuration to CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
        internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration{}, timeDuration);
    }

    // Return ? TemporalDurationFromInternal(internalDuration, largestUnit).
    auto resultDuration = temporalDurationFromInternal(state, internalDuration, toDateTimeUnit(largestUnit.value()));
    if (!resultDuration.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Result duration is out of range");
    }
    return new TemporalDurationObject(state, resultDuration);
}

ISO8601::PartialDuration TemporalDurationObject::toTemporalPartialDurationRecord(ExecutionState& state, Value temporalDurationLike)
{
    // If temporalDurationLike is not an Object, then
    if (!temporalDurationLike.isObject()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "toTemporalPartialDurationRecord needs Object");
    }

    // Let result be a new partial Duration Record with each field set to undefined.
    ISO8601::PartialDuration result;
    // NOTE: The following steps read properties and perform independent validation in alphabetical order.
    // Let days be ? Get(temporalDurationLike, "days").
    // If days is not undefined, set result.[[Days]] to ? ToIntegerIfIntegral(days).
    // Let hours be ? Get(temporalDurationLike, "hours").
    // If hours is not undefined, set result.[[Hours]] to ? ToIntegerIfIntegral(hours).
    // Let microseconds be ? Get(temporalDurationLike, "microseconds").
    // If microseconds is not undefined, set result.[[Microseconds]] to ? ToIntegerIfIntegral(microseconds).
    // Let milliseconds be ? Get(temporalDurationLike, "milliseconds").
    // If milliseconds is not undefined, set result.[[Milliseconds]] to ? ToIntegerIfIntegral(milliseconds).
    // Let minutes be ? Get(temporalDurationLike, "minutes").
    // If minutes is not undefined, set result.[[Minutes]] to ? ToIntegerIfIntegral(minutes).
    // Let months be ? Get(temporalDurationLike, "months").
    // If months is not undefined, set result.[[Months]] to ? ToIntegerIfIntegral(months).
    // Let nanoseconds be ? Get(temporalDurationLike, "nanoseconds").
    // If nanoseconds is not undefined, set result.[[Nanoseconds]] to ? ToIntegerIfIntegral(nanoseconds).
    // Let seconds be ? Get(temporalDurationLike, "seconds").
    // If seconds is not undefined, set result.[[Seconds]] to ? ToIntegerIfIntegral(seconds).
    // Let weeks be ? Get(temporalDurationLike, "weeks").
    // If weeks is not undefined, set result.[[Weeks]] to ? ToIntegerIfIntegral(weeks).
    // Let years be ? Get(temporalDurationLike, "years").
    // If years is not undefined, set result.[[Years]] to ? ToIntegerIfIntegral(years).

    Object* s = temporalDurationLike.asObject();
    Value v;
#define SET_ONCE(name)                                                                                    \
    v = s->get(state, ObjectPropertyName(state.context()->staticStrings().lazy##name())).value(state, s); \
    if (!v.isUndefined()) {                                                                               \
        result.set##name(v.toIntegerIfIntergral(state));                                                  \
    }

    SET_ONCE(Days);
    SET_ONCE(Hours);
    SET_ONCE(Microseconds);
    SET_ONCE(Milliseconds);
    SET_ONCE(Minutes);
    SET_ONCE(Months);
    SET_ONCE(Nanoseconds);
    SET_ONCE(Seconds);
    SET_ONCE(Weeks);
    SET_ONCE(Years);

#undef SET_ONCE

    // If years is undefined, and months is undefined, and weeks is undefined, and days is undefined, and hours is undefined, and minutes is undefined, and seconds is undefined, and milliseconds is undefined, and microseconds is undefined, and nanoseconds is undefined, throw a TypeError exception.
    bool allUndefined = true;
    for (const auto& s : result) {
        if (s.hasValue()) {
            allUndefined = false;
            break;
        }
    }

    if (allUndefined) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "toTemporalPartialDurationRecord needs at least one duration value");
    }

    // Return result.
    return result;
}

ISO8601::InternalDuration TemporalDurationObject::toInternalDurationRecord(ISO8601::Duration duration)
{
    ISO8601::Duration dateDuration{
        duration[0], duration[1], duration[2], duration[3], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    };
    auto t = duration.totalNanoseconds(ISO8601::DateTimeUnit::Hour);
    return ISO8601::InternalDuration(dateDuration, t);
}

// https://tc39.es/proposal-temporal/#sec-temporal-add24hourdaystonormalizedtimeduration
Int128 TemporalDurationObject::add24HourDaysToTimeDuration(ExecutionState& state, Int128 d, double days)
{
    CheckedInt128 daysInNanoseconds = checkedCastDoubleToInt128(days) * ISO8601::ExactTime::nsPerDay;
    CheckedInt128 result = d + daysInNanoseconds;
    ASSERT(!result.hasOverflowed());
    if (std::abs(result) > ISO8601::InternalDuration::maxTimeDuration) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Total time in duration is out of range");
        return {};
    }
    return result;
}

Int128 TemporalDurationObject::addTimeDuration(ExecutionState& state, Int128 one, Int128 two)
{
    // Let result be one + two.
    auto result = one + two;
    // If abs(result) > maxTimeDuration, throw a RangeError exception.
    if (std::abs(result) > ISO8601::InternalDuration::maxTimeDuration) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Total time in duration is out of range");
        return {};
    }
    // Return result.
    return result;
}

ISO8601::Duration TemporalDurationObject::addDurations(ExecutionState& state, AddDurationsOperation operation, Value otherInput)
{
    // Set other to ? ToTemporalDuration(other).
    auto otherDurationObject = Temporal::toTemporalDuration(state, otherInput);
    ISO8601::Duration other = otherDurationObject->duration();
    // If operation is subtract, set other to CreateNegatedTemporalDuration(other).
    if (operation == AddDurationsOperation::Subtract) {
        other = createNegatedTemporalDuration(otherDurationObject->duration());
    }
    // Let largestUnit1 be DefaultTemporalLargestUnit(duration).
    auto largestUnit1 = m_duration.defaultTemporalLargestUnit();
    // Let largestUnit2 be DefaultTemporalLargestUnit(other).
    auto largestUnit2 = other.defaultTemporalLargestUnit();
    // Let largestUnit be LargerOfTwoTemporalUnits(largestUnit1, largestUnit2).
    auto largestUnit = Temporal::largerOfTwoTemporalUnits(largestUnit1, largestUnit2);
    // If IsCalendarUnit(largestUnit) is true, throw a RangeError exception.
    if (Temporal::isCalendarUnit(largestUnit)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "can not add/subtract Calendar Unit duration");
    }
    // Let d1 be ToInternalDurationRecordWith24HourDays(duration).
    auto d1 = toInternalDurationRecordWith24HourDays(state, m_duration);
    // Let d2 be ToInternalDurationRecordWith24HourDays(other).
    auto d2 = toInternalDurationRecordWith24HourDays(state, other);
    // Let timeResult be ? AddTimeDuration(d1.[[Time]], d2.[[Time]]).
    auto timeResult = addTimeDuration(state, d1.time(), d2.time());
    // Let result be CombineDateAndTimeDuration(ZeroDateDuration(), timeResult).
    auto result = ISO8601::InternalDuration::combineDateAndTimeDuration({}, timeResult);
    // Return ? TemporalDurationFromInternal(result, largestUnit).
    auto resultDuration = temporalDurationFromInternal(state, result, largestUnit);
    if (!resultDuration.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Result duration is out of range");
    }
    return resultDuration;
}

ISO8601::InternalDuration TemporalDurationObject::toInternalDurationRecordWith24HourDays(ExecutionState& state, ISO8601::Duration duration)
{
    // Let timeDuration be TimeDurationFromComponents(duration.[[Hours]], duration.[[Minutes]], duration.[[Seconds]], duration.[[Milliseconds]], duration.[[Microseconds]], duration.[[Nanoseconds]]).
    auto timeDuration = timeDurationFromComponents(state, duration.hours(), duration.minutes(), duration.seconds(), duration.milliseconds(), duration.microseconds(), duration.nanoseconds());
    // Set timeDuration to ! Add24HourDaysToTimeDuration(timeDuration, duration.[[Days]]).
    timeDuration = add24HourDaysToTimeDuration(state, timeDuration, duration.days());
    // Let dateDuration be ! CreateDateDurationRecord(duration.[[Years]], duration.[[Months]], duration.[[Weeks]], 0).
    ISO8601::Duration dateDuration = ISO8601::Duration{ duration.years(), duration.months(), duration.weeks(), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    return ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, timeDuration);
}

Int128 TemporalDurationObject::roundTimeDuration(ExecutionState& state, Int128 timeDuration, unsigned increment, ISO8601::DateTimeUnit unit, ISO8601::RoundingMode roundingMode)
{
    // Let divisor be the value in the "Length in Nanoseconds" column of the row of Table 21 whose "Value" column contains unit.
    Int128 divisor = ISO8601::lengthInNanoseconds(unit);
    // Return ? RoundTimeDurationToIncrement(timeDuration, divisor × increment, roundingMode)
    return TemporalDurationObject::roundTimeDurationToIncrement(state, timeDuration, divisor * increment, roundingMode);
}

Int128 TemporalDurationObject::roundTimeDurationToIncrement(ExecutionState& state, Int128 d, Int128 increment, ISO8601::RoundingMode roundingMode)
{
    // Let rounded be RoundNumberToIncrement(d, increment, roundingMode).
    Int128 rounded = ISO8601::roundNumberToIncrement(d, increment, roundingMode);
    // 2. If abs(rounded) > maxTimeDuration, throw a RangeError exception.
    if (std::abs(rounded) > ISO8601::InternalDuration::maxTimeDuration) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Rounded time duration exceeds maximum");
    }
    // 3. Return rounded.
    return rounded;
}

Int128 TemporalDurationObject::timeDurationFromComponents(ExecutionState& state, double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds)
{
    CheckedInt128 min = checkedCastDoubleToInt128(minutes) + checkedCastDoubleToInt128(hours) * Int128{ 60 };
    CheckedInt128 sec = checkedCastDoubleToInt128(seconds) + min * Int128{ 60 };
    CheckedInt128 millis = checkedCastDoubleToInt128(milliseconds) + sec * Int128{ 1000 };
    CheckedInt128 micros = checkedCastDoubleToInt128(microseconds) + millis * Int128{ 1000 };
    CheckedInt128 nanos = checkedCastDoubleToInt128(nanoseconds) + micros * Int128{ 1000 };
    if (nanos.hasOverflowed() || std::abs(nanos) > ISO8601::InternalDuration::maxTimeDuration) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "time duration exceeds maximum");
    }
    // The components come from a TemporalDuration, so as per
    // TemporalDuration::tryCreateIfValid(), these components can at most add up
    // to ISO8601::InternalDuration::maxTimeDuration
    ASSERT(!nanos.hasOverflowed());
    ASSERT(std::abs(nanos) <= ISO8601::InternalDuration::maxTimeDuration);
    return nanos;
}

ISO8601::Duration TemporalDurationObject::toDateDurationRecordWithoutTime(ExecutionState& state, ISO8601::Duration duration)
{
    // Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
    auto internalDuration = TemporalDurationObject::toInternalDurationRecordWith24HourDays(state, duration);
    // Let days be truncate(internalDuration.[[Time]] / nsPerDay).
    auto days = internalDuration.time() / ISO8601::ExactTime::nsPerDay;
    // Return ! CreateDateDurationRecord(internalDuration.[[Date]].[[Years]], internalDuration.[[Date]].[[Months]], internalDuration.[[Date]].[[Weeks]], days).
    ISO8601::Duration dateDuration = ISO8601::Duration{ duration.years(), duration.months(), duration.weeks(), static_cast<double>(days), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    return dateDuration;
}


static double dateDurationDays(ExecutionState& state, ISO8601::Duration dateDuration, TemporalPlainDateObject* plainRelativeTo)
{
    // Let yearsMonthsWeeksDuration be ! AdjustDateDurationRecord(dateDuration, 0).
    auto yearsMonthsWeeksDuration = Temporal::adjustDateDurationRecord(state, dateDuration, 0, NullOption, NullOption);
    // If DateDurationSign(yearsMonthsWeeksDuration) = 0, return dateDuration.[[Days]].
    if (yearsMonthsWeeksDuration.dateDurationSign() == 0) {
        return dateDuration.days();
    }
    // Let later be ? CalendarDateAdd(plainRelativeTo.[[Calendar]], plainRelativeTo.[[ISODate]], yearsMonthsWeeksDuration, constrain).
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> newCal(plainRelativeTo->calendarID().createICUCalendar(state), [](UCalendar* r) {
        ucal_close(r);
    });
    auto inputDate = plainRelativeTo->computeISODate(state);
    ucal_setMillis(newCal.get(), ISO8601::ExactTime::fromPlainDate(inputDate).floorEpochMilliseconds(), &status);
    CHECK_ICU();
    auto later = Temporal::calendarDateAdd(state, plainRelativeTo->calendarID(), inputDate, newCal.get(), yearsMonthsWeeksDuration, TemporalOverflowOption::Constrain);
    ucal_close(later.first);
    // Let epochDays1 be ISODateToEpochDays(plainRelativeTo.[[ISODate]].[[Year]], plainRelativeTo.[[ISODate]].[[Month]] - 1, plainRelativeTo.[[ISODate]].[[Day]]).
    auto epochDays1 = ISO8601::dateToDaysFrom1970(inputDate.year(), inputDate.month() - 1, inputDate.day());
    // Let epochDays2 be ISODateToEpochDays(later.[[Year]], later.[[Month]] - 1, later.[[Day]]).
    auto epochDays2 = ISO8601::dateToDaysFrom1970(later.second.year(), later.second.month() - 1, later.second.day());
    // Let yearsMonthsWeeksInDays be epochDays2 - epochDays1.
    auto yearsMonthsWeeksInDays = epochDays2 - epochDays1;
    // Return dateDuration.[[Days]] + yearsMonthsWeeksInDays.
    return dateDuration.days() + yearsMonthsWeeksInDays;
}

int TemporalDurationObject::compare(ExecutionState& state, Value oneInput, Value twoInput, Value options)
{
    // Set one to ? ToTemporalDuration(one).
    auto one = Temporal::toTemporalDuration(state, oneInput);
    // Set two to ? ToTemporalDuration(two).
    auto two = Temporal::toTemporalDuration(state, twoInput);
    // Let resolvedOptions be ? GetOptionsObject(options).
    if (!options.isObject() && !options.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid option value");
    }
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let relativeToRecord be ? GetTemporalRelativeToOption(resolvedOptions).
    TemporalRelativeToOptionRecord relativeToRecord;
    if (resolvedOptions) {
        relativeToRecord = getTemporalRelativeToOption(state, resolvedOptions.value());
    }
    // If one.[[Years]] = two.[[Years]], and one.[[Months]] = two.[[Months]], and one.[[Weeks]] = two.[[Weeks]], and one.[[Days]] = two.[[Days]], and one.[[Hours]] = two.[[Hours]], and one.[[Minutes]] = two.[[Minutes]], and one.[[Seconds]] = two.[[Seconds]], and one.[[Milliseconds]] = two.[[Milliseconds]], and one.[[Microseconds]] = two.[[Microseconds]], and one.[[Nanoseconds]] = two.[[Nanoseconds]], then
    if (one->duration() == two->duration()) {
        // Return +0𝔽.
        return 0;
    }

    // Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
    auto zonedRelativeTo = relativeToRecord.zonedRelativeTo;
    // Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].
    auto plainRelativeTo = relativeToRecord.plainRelativeTo;
    // Let largestUnit1 be DefaultTemporalLargestUnit(one).
    auto largestUnit1 = one->duration().defaultTemporalLargestUnit();
    // Let largestUnit2 be DefaultTemporalLargestUnit(two).
    auto largestUnit2 = two->duration().defaultTemporalLargestUnit();
    // Let duration1 be ToInternalDurationRecord(one).
    auto duration1 = toInternalDurationRecord(one->duration());
    // Let duration2 be ToInternalDurationRecord(two).
    auto duration2 = toInternalDurationRecord(two->duration());
    // If zonedRelativeTo is not undefined, and either TemporalUnitCategory(largestUnit1) or TemporalUnitCategory(largestUnit2) is date, then
    if (zonedRelativeTo && (ISO8601::toDateTimeCategory(largestUnit1) == ISO8601::DateTimeUnitCategory::Date || ISO8601::toDateTimeCategory(largestUnit2) == ISO8601::DateTimeUnitCategory::Date)) {
        // Let timeZone be zonedRelativeTo.[[TimeZone]].
        TimeZone timeZone = zonedRelativeTo->timeZone();
        // Let calendar be zonedRelativeTo.[[Calendar]].
        Calendar calendar = zonedRelativeTo->calendarID();
        // Let after1 be ? AddZonedDateTime(zonedRelativeTo.[[EpochNanoseconds]], timeZone, calendar, duration1, constrain).
        auto after1 = Temporal::addZonedDateTime(state, zonedRelativeTo->epochNanoseconds(), timeZone, calendar, duration1, TemporalOverflowOption::Constrain);
        // Let after2 be ? AddZonedDateTime(zonedRelativeTo.[[EpochNanoseconds]], timeZone, calendar, duration2, constrain).
        auto after2 = Temporal::addZonedDateTime(state, zonedRelativeTo->epochNanoseconds(), timeZone, calendar, duration2, TemporalOverflowOption::Constrain);
        // If after1 > after2, return 1𝔽.
        if (after1 > after2) {
            return 1;
        }
        // If after1 < after2, return -1𝔽.
        if (after1 < after2) {
            return -1;
        }
        // Return +0𝔽.
        return 0;
    }
    double days1;
    double days2;
    // If IsCalendarUnit(largestUnit1) is true or IsCalendarUnit(largestUnit2) is true, then
    if (Temporal::isCalendarUnit(largestUnit1) || Temporal::isCalendarUnit(largestUnit2)) {
        // If plainRelativeTo is undefined, throw a RangeError exception.
        if (!plainRelativeTo) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid relativeTo value");
        }
        // Let days1 be ? DateDurationDays(duration1.[[Date]], plainRelativeTo).
        days1 = dateDurationDays(state, duration1.dateDuration(), plainRelativeTo.value());
        // Let days2 be ? DateDurationDays(duration2.[[Date]], plainRelativeTo).
        days2 = dateDurationDays(state, duration2.dateDuration(), plainRelativeTo.value());
    } else {
        // Let days1 be one.[[Days]].
        days1 = one->duration().days();
        // Let days2 be two.[[Days]].
        days2 = two->duration().days();
    }

    // Let timeDuration1 be ? Add24HourDaysToTimeDuration(duration1.[[Time]], days1).
    auto timeDuration1 = add24HourDaysToTimeDuration(state, duration1.time(), days1);
    // Let timeDuration2 be ? Add24HourDaysToTimeDuration(duration2.[[Time]], days2).
    auto timeDuration2 = add24HourDaysToTimeDuration(state, duration2.time(), days2);
    // Return 𝔽(CompareTimeDuration(timeDuration1, timeDuration2)).
    if (timeDuration1 > timeDuration2) {
        return 1;
    }
    if (timeDuration1 < timeDuration2) {
        return -1;
    }
    return 0;
}

} // namespace Escargot

#endif
