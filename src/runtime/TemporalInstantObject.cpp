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
    Optional<Object*> resolvedOptions;
    if (options.isObject()) {
        resolvedOptions = options.asObject();
    } else if (!options.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
    }

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
    if (smallestUnit.hasValue() && smallestUnit.value()->equals("hour")) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    // If timeZone is not undefined, then
    if (!timeZone.isUndefined()) {
        // Set timeZone to ? ToTemporalTimeZoneIdentifier(timeZone).
        computedTimeZone = Temporal::toTemporalTimezoneIdentifier(state, timeZone);
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, smallestUnit, digits);
    // Let roundedNs be RoundTemporalInstant(instant.[[EpochNanoseconds]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    // Let roundedInstant be ! CreateTemporalInstant(roundedNs).
    Int128 maximum = 0;
    constexpr int64_t hoursPerDay = 24;
    constexpr int64_t minutesPerHour = 60;
    constexpr int64_t secondsPerMinute = 60;
    constexpr int64_t msPerDay = hoursPerDay * minutesPerHour * secondsPerMinute * 1000;

    String* unit = precision.unit;
    if (unit->equals("hour")) {
        maximum = static_cast<Int128>(hoursPerDay);
    } else if (unit->equals("minute")) {
        maximum = static_cast<Int128>(minutesPerHour * hoursPerDay);
    } else if (unit->equals("second")) {
        maximum = static_cast<Int128>(secondsPerMinute * minutesPerHour * hoursPerDay);
    } else if (unit->equals("millisecond")) {
        maximum = static_cast<Int128>(msPerDay);
    } else if (unit->equals("microsecond")) {
        maximum = static_cast<Int128>(msPerDay * 1000);
    } else if (unit->equals("nanosecond")) {
        maximum = ISO8601::ExactTime::nsPerDay;
    }

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

        if ((precision.isString() && precision.asString()->equals("auto") && fraction) || (precision.isInt32() && precision.asInt32())) {
            auto padded = pad('0', 9, std::to_string(fraction));
            padded = '.' + padded;

            if (precision.isInt32()) {
                padded = padded.substr(0, padded.length() - (9 - precision.asInt32()));
            } else {
                auto lengthWithoutTrailingZeroes = padded.length();
                while (padded[lengthWithoutTrailingZeroes - 1] == '0') {
                    lengthWithoutTrailingZeroes--;
                }
                padded = padded.substr(0, lengthWithoutTrailingZeroes);
            }
            builder.appendString(String::fromASCII(padded.data(), padded.length()));
        }
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

TemporalInstantObject* TemporalInstantObject::addDurationToInstant(AddDurationOperation operation, const Value& temporalDurationLike)
{
    TemporalInstantObject* instant = this;
    // TODO
    return nullptr;
}

} // namespace Escargot

#endif
