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
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"

namespace Escargot {

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
    int32_t sign = internalDuration.sign();

    // Let nanoseconds be abs(internalDuration.[[Time]]).
    auto nanoseconds = std::abs(internalDuration.time());

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

    // Return ? CreateTemporalDuration(internalDuration.[[Date]].[[Years]], internalDuration.[[Date]].[[Months]],
    // internalDuration.[[Date]].[[Weeks]], internalDuration.[[Date]].[[Days]] + days × sign,
    // hours × sign, minutes × sign, seconds × sign, milliseconds × sign, microseconds × sign, nanoseconds × sign).
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

    return ISO8601::Duration{ internalDuration.dateDuration().years(), internalDuration.dateDuration().months(), internalDuration.dateDuration().weeks(), internalDuration.dateDuration().days() + days * sign, hours, minutes,
                              static_cast<double>(seconds), static_cast<double>(milliseconds), static_cast<double>(microseconds), static_cast<double>(nanoseconds) };
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


} // namespace Escargot

#endif
