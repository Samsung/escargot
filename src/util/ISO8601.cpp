/*
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

#include "Escargot.h"
#include "ISO8601.h"
#include "runtime/Context.h"
#include "runtime/Object.h"

namespace Escargot {
namespace ISO8601 {

static constexpr int64_t nsPerHour = 1000LL * 1000 * 1000 * 60 * 60;
static constexpr int64_t nsPerMinute = 1000LL * 1000 * 1000 * 60;
static constexpr int64_t nsPerSecond = 1000LL * 1000 * 1000;
static constexpr int64_t nsPerMillisecond = 1000LL * 1000;
static constexpr int64_t nsPerMicrosecond = 1000LL;

static int32_t parseDecimalInt32(const std::string& characters)
{
    int32_t result = 0;
    for (auto character : characters) {
        ASSERT(isdigit(character));
        result = (result * 10) + character - '0';
    }
    return result;
}

// DurationHandleFractions ( fHours, minutes, fMinutes, seconds, fSeconds, milliseconds, fMilliseconds, microseconds, fMicroseconds, nanoseconds, fNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal-durationhandlefractions
static void handleFraction(Duration& duration, int factor, std::string fractionString, Duration::Type fractionType)
{
    auto fractionLength = fractionString.length();
    std::string padded("000000000");
    for (unsigned i = 0; i < fractionLength; i++) {
        padded[i] = fractionString[i];
    }

    int64_t fraction = static_cast<int64_t>(factor) * parseDecimalInt32(padded);
    if (!fraction) {
        return;
    }

    static constexpr int64_t divisor = 1000000000LL;
    if (fractionType == Duration::Type::Hours) {
        fraction *= 60;
        duration.setMinutes(fraction / divisor);
        fraction %= divisor;
        if (!fraction)
            return;
    }

    if (fractionType != Duration::Type::Seconds) {
        fraction *= 60;
        duration.setSeconds(fraction / divisor);
        fraction %= divisor;
        if (!fraction)
            return;
    }

    duration.setMilliseconds(fraction / nsPerMillisecond);
    duration.setMicroseconds(fraction % nsPerMillisecond / nsPerMicrosecond);
    duration.setNanoseconds(fraction % nsPerMicrosecond);
}

static double parseInt(std::string src)
{
    return std::stoi(src);
}

Optional<Duration> Duration::parseDurationString(String* input)
{
    // ISO 8601 duration strings are like "-P1Y2M3W4DT5H6M7.123456789S". Notes:
    // - case insensitive
    // - sign: + -
    // - separator: . ,
    // - T is present iff there is a time part
    // - integral parts can have any number of digits but fractional parts have at most 9
    // - hours and minutes can have fractional parts too, but only as the LAST part of the string

    ParserString buffer(input);

    if (buffer.lengthRemaining() < 3)
        return NullOption;

    Duration result;

    int factor = 1;
    if (*buffer == '+') {
        buffer.advance();
    } else if (*buffer == '-') {
        factor = -1;
        buffer.advance();
    }

    if (toupper(*buffer) != 'P') {
        return NullOption;
    }

    buffer.advance();
    for (unsigned datePartIndex = 0; datePartIndex < 4 && buffer.hasCharactersRemaining() && isASCIIDigit(*buffer); buffer.advance()) {
        unsigned digits = 1;
        while (digits < buffer.lengthRemaining() && isdigit(buffer[digits]))
            digits++;

        double integer = factor * parseInt(buffer.first(digits));
        buffer.advanceBy(digits);
        if (buffer.atEnd())
            return NullOption;

        switch (toupper(*buffer)) {
        case 'Y':
            if (datePartIndex) {
                return NullOption;
            }
            result.setYears(integer);
            datePartIndex = 1;
            break;
        case 'M':
            if (datePartIndex >= 2) {
                return NullOption;
            }
            result.setMonths(integer);
            datePartIndex = 2;
            break;
        case 'W':
            if (datePartIndex >= 3) {
                return NullOption;
            }
            result.setWeeks(integer);
            datePartIndex = 3;
            break;
        case 'D':
            result.setDays(integer);
            datePartIndex = 4;
            break;
        default:
            return NullOption;
        }
    }

    if (buffer.atEnd()) {
        return result;
    }

    if (buffer.lengthRemaining() < 3 || toupper(*buffer) != 'T') {
        return NullOption;
    }

    buffer.advance();
    for (unsigned timePartIndex = 0; timePartIndex < 3 && buffer.hasCharactersRemaining() && isASCIIDigit(*buffer); buffer.advance()) {
        unsigned digits = 1;
        while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits])) {
            digits++;
        }

        double integer = factor * parseInt(buffer.first(digits));
        buffer.advanceBy(digits);
        if (buffer.atEnd()) {
            return NullOption;
        }

        std::string fractionalPart;
        if (*buffer == '.' || *buffer == ',') {
            buffer.advance();
            digits = 0;
            while (digits < buffer.lengthRemaining() && isASCIIDigit(buffer[digits]))
                digits++;
            if (!digits || digits > 9) {
                return NullOption;
            }

            fractionalPart = buffer.first(digits);
            buffer.advanceBy(digits);
            if (buffer.atEnd()) {
                return NullOption;
            }
        }

        switch (toupper(*buffer)) {
        case 'H':
            if (timePartIndex)
                return NullOption;
            result.setHours(integer);
            if (fractionalPart.size()) {
                handleFraction(result, factor, fractionalPart, Duration::Type::Hours);
                timePartIndex = 3;
            } else {
                timePartIndex = 1;
            }
            break;
        case 'M':
            if (timePartIndex >= 2)
                return NullOption;
            result.setMinutes(integer);
            if (fractionalPart.size()) {
                handleFraction(result, factor, fractionalPart, Duration::Type::Minutes);
                timePartIndex = 3;
            } else {
                timePartIndex = 2;
            }
            break;
        case 'S':
            result.setSeconds(integer);
            if (fractionalPart.size()) {
                handleFraction(result, factor, fractionalPart, Duration::Type::Seconds);
            }
            timePartIndex = 3;
            break;
        default:
            return NullOption;
        }
    }

    if (buffer.hasCharactersRemaining()) {
        return NullOption;
    }

    return result;
}

String* Duration::typeName(ExecutionState& state, Type t)
{
    switch (t) {
#define DEFINE_GETTER(name, Name, index) \
    case Type::Name:                     \
        return state.context()->staticStrings().lazy##Name().string();
        FOR_EACH_DURATION(DEFINE_GETTER)
#undef DEFINE_GETTER
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return String::emptyString();
}

BigIntData Duration::totalNanoseconds(Duration::Type unit) const
{
    BigIntData resultNs;

    constexpr int64_t nanoMultiplier = 1000000000ULL;
    constexpr int64_t milliMultiplier = 1000000ULL;
    constexpr int64_t microMultiplier = 1000ULL;

    if (unit <= Duration::Type::Days) {
        BigIntData s(days());
        s = s.multiply(86400);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Hours) {
        BigIntData s(hours());
        s = s.multiply(3600);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Minutes) {
        BigIntData s(minutes());
        s = s.multiply(60);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Seconds) {
        BigIntData s(seconds());
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Milliseconds) {
        BigIntData s(milliseconds());
        s = s.multiply(milliMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Microseconds) {
        BigIntData s(microseconds());
        s = s.multiply(microMultiplier);
        resultNs = resultNs.addition(s);
    }
    if (unit <= Duration::Type::Nanoseconds) {
        BigIntData s(nanoseconds());
        resultNs = resultNs.addition(s);
    }

    return resultNs;
}

PartialDuration PartialDuration::toTemporalPartialDurationRecord(ExecutionState& state, const Value& temporalDurationLike)
{
    // If temporalDurationLike is not an Object, then
    if (!temporalDurationLike.isObject()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "toTemporalPartialDurationRecord needs Object");
    }

    // Let result be a new partial Duration Record with each field set to undefined.
    PartialDuration result;
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

} // namespace ISO8601
} // namespace Escargot
