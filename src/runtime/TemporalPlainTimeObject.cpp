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
#include "TemporalPlainTimeObject.h"
#include "TemporalDurationObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

TemporalPlainTimeObject::TemporalPlainTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainTime plainTime)
    : DerivedObject(state, proto)
    , m_plainTime(new(PointerFreeGC) ISO8601::PlainTime(plainTime))
{
}

ISO8601::PartialPlainTime TemporalPlainTimeObject::toTemporalTimeRecord(ExecutionState& state, Value temporalTimeLike, Optional<bool> completeness)
{
    // If completeness is not present, set completeness to complete.
    if (!completeness) {
        completeness = true;
    }

    ISO8601::PartialPlainTime result;
    // If completeness is complete, then
    if (completeness.value()) {
        // Let result be a new TemporalTimeLike Record with each field set to 0.
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    result.set##capitalizedName(0);
        PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
    } else {
        // Else,
        // Let result be a new TemporalTimeLike Record with each field set to unset.
    }

    // Let any be false.
    bool any = false;

    // Step 5..16
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName)                                                         \
    {                                                                                                                  \
        Value value = Object::getV(state, temporalTimeLike, state.context()->staticStrings().lazy##capitalizedName()); \
        if (!value.isUndefined()) {                                                                                    \
            any = true;                                                                                                \
            result.set##capitalizedName(value.toIntegerWithTruncation(state));                                         \
        }                                                                                                              \
    }
    PLAIN_TIME_UNITS_ALPHABET_ORDER(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
    // If any is false, throw a TypeError exception.
    if (!any) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "At least one field required for ToTemporalTimeRecord");
    }
    // Return result.
    return result;
}

inline int64_t clamping(int64_t v, int64_t lower, int64_t upper)
{
    if (v < lower) {
        return lower;
    }
    if (v > upper) {
        return upper;
    }
    return v;
}

ISO8601::PlainTime TemporalPlainTimeObject::regulateTime(ExecutionState& state, int64_t hour, int64_t minute, int64_t second,
                                                         int64_t millisecond, int64_t microsecond, int64_t nanosecond, TemporalOverflowOption overflow)
{
    // If overflow is constrain, then
    if (overflow == TemporalOverflowOption::Constrain) {
        // Set hour to the result of clamping hour between 0 and 23.
        hour = clamping(hour, 0, 23);
        // Set minute to the result of clamping minute between 0 and 59.
        minute = clamping(minute, 0, 59);
        // Set second to the result of clamping second between 0 and 59.
        second = clamping(second, 0, 59);
        // Set millisecond to the result of clamping millisecond between 0 and 999.
        millisecond = clamping(millisecond, 0, 999);
        // Set microsecond to the result of clamping microsecond between 0 and 999.
        microsecond = clamping(microsecond, 0, 999);
        // Set nanosecond to the result of clamping nanosecond between 0 and 999.
        nanosecond = clamping(nanosecond, 0, 999);
    } else {
        // Else,
        // Assert: overflow is reject.
        // If IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond) is false, throw a RangeError exception.
        if (!Temporal::isValidTime(hour, minute, second, millisecond, microsecond, nanosecond)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid time");
        }
    }

    // Return CreateTimeRecord(hour, minute, second, millisecond, microsecond, nanosecond).
    return ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

ISO8601::Duration TemporalPlainTimeObject::roundTime(ExecutionState& state, ISO8601::PlainTime plainTime, double increment, ISO8601::DateTimeUnit unit, ISO8601::RoundingMode roundingMode)
{
    auto fractionalSecond = [](ISO8601::PlainTime plainTime) -> double {
        return plainTime.second() + plainTime.millisecond() * 1e-3 + plainTime.microsecond() * 1e-6 + plainTime.nanosecond() * 1e-9;
    };

    double quantity = 0;
    switch (unit) {
    case ISO8601::DateTimeUnit::Day: {
        double length = 8.64 * 1e13;
        quantity = (((((plainTime.hour() * 60.0 + plainTime.minute()) * 60.0 + plainTime.second()) * 1000.0 + plainTime.millisecond()) * 1000.0 + plainTime.microsecond()) * 1000.0 + plainTime.nanosecond()) / length;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return ISO8601::Duration({ 0.0, 0.0, 0.0, result, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });
    }
    case ISO8601::DateTimeUnit::Hour: {
        quantity = (fractionalSecond(plainTime) / 60.0 + plainTime.minute()) / 60.0 + plainTime.hour();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(result, 0.0, 0.0, 0.0, 0.0, 0.0);
    }
    case ISO8601::DateTimeUnit::Minute: {
        quantity = fractionalSecond(plainTime) / 60.0 + plainTime.minute();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(plainTime.hour(), result, 0.0, 0.0, 0.0, 0.0);
    }
    case ISO8601::DateTimeUnit::Second: {
        quantity = fractionalSecond(plainTime);
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(plainTime.hour(), plainTime.minute(), result, 0.0, 0.0, 0.0);
    }
    case ISO8601::DateTimeUnit::Millisecond: {
        quantity = plainTime.millisecond() + plainTime.microsecond() * 1e-3 + plainTime.nanosecond() * 1e-6;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), result, 0.0, 0.0);
    }
    case ISO8601::DateTimeUnit::Microsecond: {
        quantity = plainTime.microsecond() + plainTime.nanosecond() * 1e-3;
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), result, 0.0);
    }
    case ISO8601::DateTimeUnit::Nanosecond: {
        quantity = plainTime.nanosecond();
        auto result = roundNumberToIncrement(quantity, increment, roundingMode);
        return Temporal::balanceTime(plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), result);
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return ISO8601::Duration();
}

ISO8601::Duration TemporalPlainTimeObject::differenceTemporalPlainTime(ExecutionState& state, DifferenceTemporalPlainTime operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalTime(other).
    auto other = Temporal::toTemporalTime(state, otherInput, Value());
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, time, « », nanosecond, hour).
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalPlainTime::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::Time, nullptr, 0, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    // Let timeDuration be DifferenceTime(temporalTime.[[Time]], other.[[Time]]).
    auto timeDuration = Temporal::differenceTime(plainTime(), other->plainTime());
    // Set timeDuration to ! RoundTimeDuration(timeDuration, settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
    timeDuration = TemporalDurationObject::roundTimeDuration(state, timeDuration, settings.roundingIncrement, settings.smallestUnit, settings.roundingMode);
    // Let duration be CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration({}, timeDuration);
    // Let result be ! TemporalDurationFromInternal(duration, settings.[[LargestUnit]]).
    auto result = TemporalDurationObject::temporalDurationFromInternal(state, duration, settings.largestUnit);
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalPlainTime::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }
    // Return result.
    return result;
}

TemporalPlainTimeObject* TemporalPlainTimeObject::addDurationToTime(ExecutionState& state, AddDurationToTimeOperation operation, Value temporalDurationLike)
{
    // Let duration be ? ToTemporalDuration(temporalDurationLike).
    auto duration = Temporal::toTemporalDuration(state, temporalDurationLike)->duration();
    // If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if (operation == AddDurationToTimeOperation::Subtract) {
        duration = TemporalDurationObject::createNegatedTemporalDuration(duration);
    }

    if (!duration.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid duration");
    }

    // Let internalDuration be ToInternalDurationRecord(duration).
    // Let result be AddTime(temporalTime.[[Time]], internalDuration.[[Time]]).
    // Return ! CreateTemporalTime(result).
    auto result = Temporal::balanceTime(int64_t(duration.hours()) + int64_t(plainTime().hour()), int64_t(duration.minutes()) + int64_t(plainTime().minute()), int64_t(duration.seconds()) + int64_t(plainTime().second()),
                                        int64_t(duration.milliseconds()) + int64_t(plainTime().millisecond()), int64_t(duration.microseconds()) + int64_t(plainTime().microsecond()), int64_t(duration.nanoseconds()) + int64_t(plainTime().nanosecond()));
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(), ISO8601::PlainTime(result.hours(), result.minutes(), result.seconds(), result.milliseconds(), result.microseconds(), result.nanoseconds()));
}

TemporalPlainTimeObject* TemporalPlainTimeObject::with(ExecutionState& state, Value temporalTimeLike, Value options)
{
    // Let plainTime be the this value.
    // Perform ? RequireInternalSlot(plainTime, [[InitializedTemporalTime]]).
    // If ? IsPartialTemporalObject(temporalTimeLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalTimeLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalTimeLike");
    }
    // Let partialTime be ? ToTemporalTimeRecord(temporalTimeLike, partial).
    auto partialTime = toTemporalTimeRecord(state, temporalTimeLike, false);
    // Step 5..16
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    int64_t name;                                              \
    if (partialTime.name()) {                                  \
        name = partialTime.name().value();                     \
    } else {                                                   \
        name = plainTime().name();                             \
    }
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD

    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let result be ? RegulateTime(hour, minute, second, millisecond, microsecond, nanosecond, overflow).
    auto result = regulateTime(state, hour, minute, second, millisecond, microsecond, nanosecond, overflow);
    // Return ! CreateTemporalTime(result).
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(), result);
}

static String* temporalTimeToString(ISO8601::PlainTime plainTime, Value precision)
{
    if (precision.isString() && precision.asString()->equals("minute")) {
        std::string s;
        s = pad('0', 2, std::to_string(plainTime.hour()));
        s += ":";
        s += pad('0', 2, std::to_string(plainTime.minute()));
        return String::fromASCII(s.data(), s.length());
    }

    int64_t milliseconds = plainTime.millisecond();
    int64_t microseconds = plainTime.microsecond();
    int64_t nanoseconds = plainTime.nanosecond();
    int64_t fractionNanoseconds = milliseconds * 1000000 + microseconds * 1000 + nanoseconds;
    if (precision.isString() && precision.asString()->equals("auto")) {
        if (!fractionNanoseconds) {
            std::string s;
            s = pad('0', 2, std::to_string(plainTime.hour()));
            s += ":";
            s += pad('0', 2, std::to_string(plainTime.minute()));
            s += ":";
            s += pad('0', 2, std::to_string(plainTime.second()));
            return String::fromASCII(s.data(), s.length());
        }
        auto fraction = std::to_string(fractionNanoseconds);
        unsigned paddingLength = 9 - fraction.size();
        unsigned index = fraction.size();
        Optional<unsigned> validLength;
        while (index--) {
            if (fraction[index] != '0') {
                validLength = index + 1;
                break;
            }
        }
        if (validLength) {
            size_t popCount = fraction.size() - validLength.value();
            for (size_t i = 0; i < popCount; i++) {
                fraction.pop_back();
            }
        } else {
            fraction.clear();
        }

        std::string s;
        s = pad('0', 2, std::to_string(plainTime.hour()));
        s += ":";
        s += pad('0', 2, std::to_string(plainTime.minute()));
        s += ":";
        s += pad('0', 2, std::to_string(plainTime.second()));
        s += ".";
        s += pad('0', paddingLength, "");
        s += fraction;
        return String::fromASCII(s.data(), s.length());
    }
    int32_t precisionValue = 0;
    if (precision.isInt32()) {
        precisionValue = precision.asInt32();
    }
    if (!precisionValue) {
        std::string s;
        s = pad('0', 2, std::to_string(plainTime.hour()));
        s += ":";
        s += pad('0', 2, std::to_string(plainTime.minute()));
        s += ":";
        s += pad('0', 2, std::to_string(plainTime.second()));
        return String::fromASCII(s.data(), s.length());
    }
    auto fraction = std::to_string(fractionNanoseconds);
    int32_t paddingLength = 9 - fraction.size();
    paddingLength = std::min(paddingLength, precisionValue);
    precisionValue -= paddingLength;
    fraction.resize(precisionValue);

    std::string s;
    s = pad('0', 2, std::to_string(plainTime.hour()));
    s += ":";
    s += pad('0', 2, std::to_string(plainTime.minute()));
    s += ":";
    s += pad('0', 2, std::to_string(plainTime.second()));
    s += ".";
    s += pad('0', paddingLength, "");
    s += fraction;
    return String::fromASCII(s.data(), s.length());
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
String* TemporalPlainTimeObject::toString(ExecutionState& state, Value options)
{
    // Let plainTime be the this value.
    // Perform ? RequireInternalSlot(plainTime, [[InitializedTemporalTime]]).
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
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);
    // Let roundResult be RoundTime(plainTime.[[Time]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto roundResult = roundTime(state, plainTime(), precision.increment, precision.unit, roundingMode);
    // Return TimeRecordToString(roundResult, precision.[[Precision]]).
    return temporalTimeToString(ISO8601::PlainTime(roundResult.hours(), roundResult.minutes(), roundResult.seconds(), roundResult.milliseconds(),
                                                   roundResult.microseconds(), roundResult.nanoseconds()),
                                precision.precision);
}

TemporalDurationObject* TemporalPlainTimeObject::since(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainTime(state, DifferenceTemporalPlainTime::Since, other, options));
}

TemporalDurationObject* TemporalPlainTimeObject::until(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainTime(state, DifferenceTemporalPlainTime::Until, other, options));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
TemporalPlainTimeObject* TemporalPlainTimeObject::round(ExecutionState& state, Value roundToInput)
{
    // Let plainTime be the this value.
    // Perform ? RequireInternalSlot(plainTime, [[InitializedTemporalTime]]).
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
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
    auto maximum = Temporal::maximumTemporalDurationRoundingIncrement(toDateTimeUnit(smallestUnit.value()));
    // Assert: maximum is not unset.
    // Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
    Temporal::validateTemporalRoundingIncrement(state, roundingIncrement, maximum.value(), false);
    // Let result be RoundTime(plainTime.[[Time]], roundingIncrement, smallestUnit, roundingMode).
    // ExecutionState& state, ISO8601::PlainTime plainTime, double increment, ISO8601::DateTimeUnit unit, ISO8601::RoundingMode roundingMode
    auto result = roundTime(state, plainTime(), roundingIncrement, toDateTimeUnit(smallestUnit.value()), roundingMode);
    // Return ! CreateTemporalTime(result).
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(),
                                       ISO8601::PlainTime(result.hours(), result.minutes(), result.seconds(), result.milliseconds(), result.microseconds(), result.nanoseconds()));
}

bool TemporalPlainTimeObject::equals(ExecutionState& state, Value otherInput)
{
    auto other = Temporal::toTemporalTime(state, otherInput, Value());
    return plainTime() == other->plainTime();
}

int TemporalPlainTimeObject::compare(ExecutionState& state, Value one, Value two)
{
    auto time1 = Temporal::toTemporalTime(state, one, Value())->plainTime();
    auto time2 = Temporal::toTemporalTime(state, two, Value())->plainTime();

#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    if (time1.name() > time2.name())                           \
        return 1;                                              \
    if (time1.name() < time2.name())                           \
        return -1;
    PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
    return 0;
}

} // namespace Escargot

#endif
