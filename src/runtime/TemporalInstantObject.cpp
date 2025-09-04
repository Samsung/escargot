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
#include "TemporalInstantObject.h"
#include "TemporalDurationObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

TemporalInstantObject::TemporalInstantObject(ExecutionState& state, Object* proto, Int128 n)
    : DerivedObject(state, proto)
    , m_nanoseconds(new(PointerFreeGC) Int128(n))
{
}

Value TemporalInstantObject::epochMilliseconds() const
{
    Int128 s = *m_nanoseconds;
    s /= 1000000;
    return Value(static_cast<int64_t>(s));
}


String* TemporalInstantObject::toString(ExecutionState& state, Value options)
{
    const char* msg = "Invalid options value";
    // Let resolvedOptions be ? GetOptionsObject(options).
    Optional<Object*> resolvedOptions = Intl::getOptionsObject(state, options);
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Let timeZone be ? Get(resolvedOptions, "timeZone").
    Value timeZone;
    TimeZone computedTimeZone;
    if (resolvedOptions) {
        timeZone = resolvedOptions->get(state, ObjectPropertyName(state.context()->staticStrings().lazyTimeZone())).value(state, resolvedOptions.value());
    }

    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit.hasValue() && smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    // If timeZone is not undefined, then
    if (!timeZone.isUndefined()) {
        // Set timeZone to ? ToTemporalTimeZoneIdentifier(timeZone).
        computedTimeZone = Temporal::toTemporalTimezoneIdentifier(state, timeZone);
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);
    // Let roundedNs be RoundTemporalInstant(instant.[[EpochNanoseconds]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    // Let roundedInstant be ! CreateTemporalInstant(roundedNs).
    Int128 maximum = ISO8601::resolveNanosecondsValueByUnit(precision.unit);

    Temporal::validateTemporalRoundingIncrement(state, precision.increment, maximum, true);

    auto roundedInstant = ISO8601::ExactTime(*m_nanoseconds).round(state, precision.increment, precision.unit, roundingMode);
    // Return TemporalInstantToString(roundedInstant, timeZone, precision.[[Precision]]).
    return TemporalInstantObject::toString(state, roundedInstant.epochNanoseconds(), computedTimeZone, precision.precision);
}

String* TemporalInstantObject::toString(ExecutionState& state, Int128 epochNanoseconds, TimeZone timeZone, Value precision)
{
    auto epochMilli = ISO8601::ExactTime(epochNanoseconds).epochMilliseconds();
    int64_t offsetNanoseconds = 0;
    if (timeZone.hasOffset()) {
        offsetNanoseconds = timeZone.offset();
        epochMilli += (offsetNanoseconds / (1000 * 1000));
    } else if (timeZone.hasTimeZoneName()) {
        auto u16 = timeZone.timeZoneName()->toUTF16StringData();
        UErrorCode status = U_ZERO_ERROR;
        const char* msg = "Failed to get timeZone offset from ICU";
        auto ucalendar = ucal_open(u16.data(), u16.length(), "en", UCAL_GREGORIAN, &status);
        if (!ucalendar) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
            return String::emptyString();
        }

        ucal_setMillis(ucalendar, epochMilli, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        auto zoneOffset = ucal_get(ucalendar, UCAL_ZONE_OFFSET, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        auto dstOffset = ucal_get(ucalendar, UCAL_DST_OFFSET, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        ucal_close(ucalendar);
        offsetNanoseconds = zoneOffset + dstOffset;
        offsetNanoseconds *= 1000000;
    }

    DateObject::DateTimeInfo timeInfo;
    DateObject::computeTimeInfoFromEpoch(epochMilli, timeInfo);

    StringBuilder builder;
    int32_t year = timeInfo.year;
    int32_t month = timeInfo.month;
    int32_t day = timeInfo.mday;
    int32_t hour = timeInfo.hour;
    int32_t minute = timeInfo.min;
    int32_t second = timeInfo.sec;

    if (year > 9999 || year < 0) {
        builder.appendChar(year < 0 ? '-' : '+');
        auto s = pad('0', 6, std::to_string(std::abs(year)));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    } else {
        auto s = pad('0', 4, std::to_string(std::abs(year)));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    builder.appendChar('-');
    {
        auto s = pad('0', 2, std::to_string(month + 1));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }
    builder.appendChar('-');
    {
        auto s = pad('0', 2, std::to_string(day));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }
    builder.appendChar('T');
    {
        auto s = pad('0', 2, std::to_string(hour));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }
    builder.appendChar(':');
    {
        auto s = pad('0', 2, std::to_string(minute));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    Int128 fraction = epochNanoseconds % 1000000000;
    if (fraction < 0) {
        fraction += 1000000000;
    }

    if (!precision.isString() || !precision.asString()->equals("minute")) {
        builder.appendChar(':');
        auto s = pad('0', 2, std::to_string(second));
        builder.appendString(String::fromASCII(s.data(), s.length()));
        Temporal::formatSecondsStringFraction(builder, fraction, precision);
    }

    if (timeZone.empty()) {
        builder.appendChar('Z');
    } else {
        int64_t increment = 1000000000LL * 60;
        int64_t quotient = offsetNanoseconds / increment;
        int64_t remainder = offsetNanoseconds % increment;
        if (std::abs(remainder * 2) >= increment) {
            quotient += (offsetNanoseconds > 0 ? 1 : -1);
        }
        int32_t offsetMinutes = int32_t(quotient);
        // FormatOffsetTimeZoneIdentifier
        char sign = offsetMinutes >= 0 ? '+' : '-';
        int32_t absoluteMinutes = std::abs(offsetMinutes);
        int32_t hours = absoluteMinutes / 60;
        int32_t minutes = absoluteMinutes % 60;
        builder.appendChar(sign);
        {
            auto s = pad('0', 2, std::to_string(hours));
            builder.appendString(String::fromASCII(s.data(), s.length()));
        }
        builder.appendChar(':');
        {
            auto s = pad('0', 2, std::to_string(minutes));
            builder.appendString(String::fromASCII(s.data(), s.length()));
        }
    }

    return builder.finalize();
}

TemporalInstantObject* TemporalInstantObject::round(ExecutionState& state, Value roundTo)
{
    // Let instant be the this value.
    // Perform ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    const auto* msg = "Temporal.Instant.round needs object or string parameter";
    // If roundTo is undefined, then
    if (roundTo.isUndefined()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
    } else if (roundTo.isString()) {
        // If roundTo is a String, then
        // Let paramString be roundTo.
        auto paramString = roundTo.asString();
        // Set roundTo to OrdinaryObjectCreate(null).
        roundTo = new Object(state, Object::PrototypeIsNull);
        // Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        roundTo.asObject()->directDefineOwnProperty(state, state.context()->staticStrings().lazySmallestUnit(),
                                                    ObjectPropertyDescriptor(paramString, ObjectPropertyDescriptor::AllPresent));
    } else {
        // Else,
        // Set roundTo to ? GetOptionsObject(roundTo).
        if (!roundTo.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
        }
    }
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetRoundingIncrementOption reads "roundingIncrement" and GetRoundingModeOption reads "roundingMode").
    // Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
    auto roundingIncrement = Temporal::getRoundingIncrementOption(state, roundTo.asObject());
    // Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
    auto roundingMode = Temporal::getRoundingModeOption(state, roundTo.asObject(), state.context()->staticStrings().lazyHalfExpand().string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", required).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, roundTo.asObject(), state.context()->staticStrings().lazySmallestUnit().string(), Value(Value::EmptyValue)).value();
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, then
    // Let maximum be HoursPerDay.
    // Else if smallestUnit is minute, then
    // Let maximum be MinutesPerHour × HoursPerDay.
    // Else if smallestUnit is second, then
    // Let maximum be SecondsPerMinute × MinutesPerHour × HoursPerDay.
    // Else if smallestUnit is millisecond, then
    // Let maximum be ℝ(msPerDay).
    // Let maximum be 10**3 × ℝ(msPerDay).
    // Else,
    // Assert: smallestUnit is nanosecond.
    // Let maximum be nsPerDay.
    Int128 maximum = ISO8601::resolveNanosecondsValueByUnit(toDateTimeUnit(smallestUnit));

    // Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, true).
    Temporal::validateTemporalRoundingIncrement(state, roundingIncrement, maximum, true);
    // Let roundedNs be RoundTemporalInstant(instant.[[EpochNanoseconds]], roundingIncrement, smallestUnit, roundingMode).
    // Return ! CreateTemporalInstant(roundedNs).
    auto roundedInstant = ISO8601::ExactTime(*m_nanoseconds).round(state, roundingIncrement, toDateTimeUnit(smallestUnit), roundingMode);
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), roundedInstant.epochNanoseconds());
}

TemporalDurationObject* TemporalInstantObject::differenceTemporalInstant(ExecutionState& state, DifferenceTemporalInstantOperation operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalInstant(other).
    TemporalInstantObject* other = Temporal::toTemporalInstant(state, otherInput);
    // Let resolvedOptions be ? GetOptionsObject(options).
    Optional<Object*> resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, time, « », nanosecond, second).
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalInstantOperation::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::Time, nullptr, 0,
                                                    TemporalUnit::Nanosecond, TemporalUnit::Second);
    // Let internalDuration be DifferenceInstant(instant.[[EpochNanoseconds]], other.[[EpochNanoseconds]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
    auto internalDuration = differenceInstant(state, epochNanoseconds(), other->epochNanoseconds(), settings.roundingIncrement, settings.smallestUnit, settings.roundingMode);
    // Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
    auto result = TemporalDurationObject::temporalDurationFromInternal(state, internalDuration, settings.largestUnit);
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalInstantOperation::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }

    // Return result.
    return new TemporalDurationObject(state, result);
}

TemporalInstantObject* TemporalInstantObject::addDurationToInstant(AddDurationOperation operation, const Value& temporalDurationLike)
{
    TemporalInstantObject* instant = this;
    // TODO
    return nullptr;
}

ISO8601::InternalDuration TemporalInstantObject::differenceInstant(ExecutionState& state, Int128 ns1, Int128 ns2, unsigned roundingIncrement,
                                                                   ISO8601::DateTimeUnit smallestUnit, ISO8601::RoundingMode roundingMode)
{
    // Let timeDuration be TimeDurationFromEpochNanosecondsDifference(ns2, ns1).
    Int128 timeDuration = Temporal::timeDurationFromEpochNanosecondsDifference(ns2, ns1);
    // Set timeDuration to ! RoundTimeDuration(timeDuration, roundingIncrement, smallestUnit, roundingMode).
    timeDuration = TemporalDurationObject::roundTimeDuration(state, timeDuration, roundingIncrement, smallestUnit, roundingMode);
    // Return CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
    return ISO8601::InternalDuration(ISO8601::Duration(), timeDuration);
}

} // namespace Escargot

#endif
