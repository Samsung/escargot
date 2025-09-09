#if defined(ENABLE_INTL)
/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 the V8 project authors. All rights reserved.
 * Copyright (C) 2010 Research In Motion Limited. All rights reserved.
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
#include "intl/Intl.h"

namespace Escargot {
namespace ISO8601 {

constexpr Int128 ExactTime::dayRangeSeconds;
constexpr Int128 ExactTime::nsPerMicrosecond;
constexpr Int128 ExactTime::nsPerMillisecond;
constexpr Int128 ExactTime::nsPerSecond;
constexpr Int128 ExactTime::nsPerMinute;
constexpr Int128 ExactTime::nsPerHour;
constexpr Int128 ExactTime::nsPerDay;
constexpr Int128 ExactTime::minValue;
constexpr Int128 ExactTime::maxValue;

constexpr Int128 InternalDuration::maxTimeDuration;

static constexpr int64_t nsPerHour = 1000LL * 1000 * 1000 * 60 * 60;
static constexpr int64_t nsPerMinute = 1000LL * 1000 * 1000 * 60;
static constexpr int64_t nsPerSecond = 1000LL * 1000 * 1000;
static constexpr int64_t nsPerMillisecond = 1000LL * 1000;
static constexpr int64_t nsPerMicrosecond = 1000LL;

static constexpr int32_t maxYear = 275760;
static constexpr int32_t minYear = -271821;

static constexpr uint8_t daysInMonths[2][12] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

inline bool isLeapYear(int year)
{
    if (year % 4 != 0)
        return false;
    if (year % 400 == 0)
        return true;
    if (year % 100 == 0)
        return false;
    return true;
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodaysinmonth
uint8_t daysInMonth(int32_t year, uint8_t month)
{
    return daysInMonths[isLeapYear(year)][month - 1];
}

uint8_t daysInMonth(uint8_t month)
{
    constexpr unsigned isLeapYear = 1;
    return daysInMonths[isLeapYear][month - 1];
}

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
static void handleFraction(Duration& duration, int factor, std::string fractionString, ISO8601::DateTimeUnit fractionType)
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
    if (fractionType == ISO8601::DateTimeUnit::Hour) {
        fraction *= 60;
        duration.setMinutes(fraction / divisor);
        fraction %= divisor;
        if (!fraction)
            return;
    }

    if (fractionType != ISO8601::DateTimeUnit::Second) {
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
    return std::stod(src);
}

DateTimeUnitCategory toDateTimeCategory(DateTimeUnit u)
{
    if (false) {}
#define DEFINE_TYPE(name, Name, names, Names, index, category) \
    else if (u == DateTimeUnit::Name)                          \
    {                                                          \
        return category;                                       \
    }
    PLAIN_DATETIME_UNITS(DEFINE_TYPE)
#undef DEFINE_TYPE

    ASSERT_NOT_REACHED();
    return DateTimeUnitCategory::Date;
}

DateTimeUnit toDateTimeUnit(String* unit)
{
    if (false) {}
#define DEFINE_TYPE(name, Name, names, Names, index, category) \
    else if (unit->equals(#name))                              \
    {                                                          \
        return DateTimeUnit::Name;                             \
    }
    PLAIN_DATETIME_UNITS(DEFINE_TYPE)
#undef DEFINE_TYPE

    ASSERT_NOT_REACHED();
    return DateTimeUnit::Year;
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
                handleFraction(result, factor, fractionalPart, ISO8601::DateTimeUnit::Hour);
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
                handleFraction(result, factor, fractionalPart, ISO8601::DateTimeUnit::Minute);
                timePartIndex = 3;
            } else {
                timePartIndex = 2;
            }
            break;
        case 'S':
            result.setSeconds(integer);
            if (fractionalPart.size()) {
                handleFraction(result, factor, fractionalPart, ISO8601::DateTimeUnit::Second);
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

static bool isValidDurationWork(double v, int& sign)
{
    // If 𝔽(v) is not finite, return false.
    if (!std::isfinite(v)) {
        return false;
    }
    // If v < 0, then
    if (v < 0) {
        // If sign > 0, return false.
        if (sign > 0) {
            return false;
        }
        // Set sign to -1.
        sign = -1;
    } else if (v > 0) {
        // Else if v > 0, then
        // If sign < 0, return false.
        if (sign < 0) {
            return false;
        }
        // Set sign to 1.
        sign = 1;
    }
    return true;
}


static BigIntData totalDateTimeNanoseconds(const Duration& record)
{
    BigIntData resultNs;
    constexpr int64_t nanoMultiplier = 1000000000ULL;
    constexpr int64_t milliMultiplier = 1000000ULL;
    constexpr int64_t microMultiplier = 1000ULL;

    {
        BigIntData s(record.days());
        s = s.multiply(86400);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.hours());
        s = s.multiply(3600);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.minutes());
        s = s.multiply(60);
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.seconds());
        s = s.multiply(nanoMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.milliseconds());
        s = s.multiply(milliMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.microseconds());
        s = s.multiply(microMultiplier);
        resultNs = resultNs.addition(s);
    }
    {
        BigIntData s(record.nanoseconds());
        resultNs = resultNs.addition(s);
    }

    return resultNs;
}


// https://tc39.es/ecma402/#sec-isvalidduration
bool Duration::isValid() const
{
    // Let sign be 0.
    int sign = 0;
    // For each value v of « years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds », do
    for (double v : m_data) {
        if (!isValidDurationWork(v, sign)) {
            return false;
        }
    }

    // If abs(years) ≥ 2**32, return false.
    if (std::abs(years()) >= (1ULL << 32)) {
        return false;
    }
    // If abs(months) ≥ 2**32, return false.
    if (std::abs(months()) >= (1ULL << 32)) {
        return false;
    }
    // If abs(weeks) ≥ 2**32, return false.
    if (std::abs(weeks()) >= (1ULL << 32)) {
        return false;
    }

    // Let normalizedSeconds be days × 86,400 + hours × 3600 + minutes × 60 + seconds + ℝ(𝔽(milliseconds)) × 10**-3 + ℝ(𝔽(microseconds)) × 10**-6 + ℝ(𝔽(nanoseconds)) × 10**-9.
    // NOTE: The above step cannot be implemented directly using floating-point arithmetic. Multiplying by 10**-3, 10**-6, and 10**-9 respectively may be imprecise when milliseconds, microseconds, or nanoseconds is an unsafe integer. This multiplication can be implemented in C++ with an implementation of std::remquo() with sufficient bits in the quotient. String manipulation will also give an exact result, since the multiplication is by a power of 10.
    BigIntData normalizedNanoSeconds = totalDateTimeNanoseconds(*this);
    // If abs(normalizedSeconds) ≥ 2**53, return false.
    BigIntData limit(int64_t(1ULL << 53));
    limit = limit.multiply(1000000000ULL);
    if (normalizedNanoSeconds.greaterThanEqual(limit)) {
        return false;
    }
    limit = limit.multiply(-1);
    if (normalizedNanoSeconds.lessThanEqual(limit)) {
        return false;
    }
    // Return true.
    return true;
}

String* Duration::typeName(ExecutionState& state, ISO8601::DateTimeUnit t)
{
    switch (t) {
#define DEFINE_GETTER(name, Name, names, Names, index, category) \
    case ISO8601::DateTimeUnit::Name:                            \
        return state.context()->staticStrings().lazy##Names().string();
        PLAIN_DATETIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return String::emptyString();
}

Int128 Duration::totalNanoseconds(ISO8601::DateTimeUnit unit) const
{
    ASSERT(unit != ISO8601::DateTimeUnit::Year);
    ASSERT(unit != ISO8601::DateTimeUnit::Month);
    ASSERT(unit != ISO8601::DateTimeUnit::Week);

    Int128 resultNs = 0;

    constexpr int64_t nanoMultiplier = 1000000000ULL;
    constexpr int64_t milliMultiplier = 1000000ULL;
    constexpr int64_t microMultiplier = 1000ULL;

    if (unit <= ISO8601::DateTimeUnit::Day) {
        Int128 s((int64_t)days());
        s *= 86400;
        s *= nanoMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Hour) {
        Int128 s((int64_t)hours());
        s *= 3600;
        s *= nanoMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Minute) {
        Int128 s((int64_t)minutes());
        s *= 60;
        s *= nanoMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Second) {
        Int128 s((int64_t)seconds());
        s *= nanoMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Millisecond) {
        Int128 s((int64_t)milliseconds());
        s *= milliMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Microsecond) {
        Int128 s((int64_t)microseconds());
        s *= microMultiplier;
        resultNs += s;
    }
    if (unit <= ISO8601::DateTimeUnit::Nanosecond) {
        Int128 s((int64_t)nanoseconds());
        resultNs += s;
    }

    return resultNs;
}

static bool canBeRFC9557Annotation(const ParserString& buffer)
{
    // https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime
    // Step 4(a)(ii)(2)(a):
    //  Let key be the source text matched by the AnnotationKey Parse Node contained within annotation
    //
    // https://tc39.es/proposal-temporal/#prod-Annotation
    // Annotation :::
    //     [ AnnotationCriticalFlag[opt] AnnotationKey = AnnotationValue ]
    //
    // AnnotationCriticalFlag :::
    //     !
    //
    // AnnotationKey :::
    //     AKeyLeadingChar
    //     AnnotationKey AKeyChar
    //
    // AKeyLeadingChar :::
    //     LowercaseAlpha
    //     _
    //
    // AKeyChar :::
    //     AKeyLeadingChar
    //     DecimalDigit
    //     -
    //
    // AnnotationValue :::
    //     AnnotationValueComponent
    //     AnnotationValueComponent - AnnotationValue
    //
    // AnnotationValueComponent :::
    //     Alpha AnnotationValueComponent[opt]
    //     DecimalDigit AnnotationValueComponent[opt]

    // This just checks for '[', followed by an optional '!' (critical flag),
    // followed by a valid key, followed by an '='.

    size_t length = buffer.lengthRemaining();
    // Because of `[`, `=`, `]`, `AnnotationKey`, and `AnnotationValue`,
    // the annotation must have length >= 5.
    if (length < 5)
        return false;
    if (*buffer != '[')
        return false;
    size_t index = 1;
    if (buffer[index] == '!')
        ++index;
    if (!isASCIILower(buffer[index]) && buffer[index] != '_')
        return false;
    ++index;
    while (index < length) {
        if (buffer[index] == '=')
            return true;
        if (isASCIILower(buffer[index]) || isASCIIDigit(buffer[index]) || buffer[index] == '-' || buffer[index] == '_')
            ++index;
        else
            return false;
    }
    return false;
}

static Optional<RFC9557Annotation> parseOneRFC9557Annotation(ParserString& buffer)
{
    // For BNF, see comment in canBeRFC9557Annotation()

    if (!canBeRFC9557Annotation(buffer))
        return NullOption;
    RFC9557Flag flag = buffer[1] == '!' ? RFC9557Flag::Critical : RFC9557Flag::None;
    // Skip '[' or '[!'
    buffer.advanceBy(flag == RFC9557Flag::Critical ? 2 : 1);

    // Parse the key
    unsigned keyLength = 0;
    while (buffer[keyLength] != '=')
        keyLength++;
    if (!keyLength)
        return NullOption;
    auto key(buffer.first(keyLength));
    buffer.advanceBy(keyLength);

    if (buffer.atEnd())
        return NullOption;

    // Consume the '='
    buffer.advance();

    unsigned nameLength = 0;
    {
        unsigned index = 0;
        for (; index < buffer.lengthRemaining(); ++index) {
            auto character = buffer[index];
            if (character == ']')
                break;
            if (!isASCIIAlpha(character) && !isASCIIDigit(character) && character != '-')
                return NullOption;
        }
        if (!index)
            return NullOption;
        nameLength = index;
    }

    // Check if the key is equal to "u-ca"
    if (key.size() != 4
        || key[0] != 'u' || key[1] != '-'
        || key[2] != 'c' || key[3] != 'a') {
        // Annotation is unknown
        // Consume the rest of the annotation
        buffer.advanceBy(nameLength);
        if (buffer.atEnd() || *buffer != ']') {
            // Parse error
            return NullOption;
        }
        // Consume the ']'
        buffer.advance();
        return RFC9557Annotation{ flag, RFC9557Key::Other, {} };
    }

    auto isValidComponent = [&](unsigned start, unsigned end) {
        unsigned componentLength = end - start;
        if (componentLength < minCalendarLength)
            return false;
        if (componentLength > maxCalendarLength)
            return false;
        return true;
    };

    unsigned currentNameComponentStartIndex = 0;
    bool isLeadingCharacterInNameComponent = true;
    for (unsigned index = 0; index < nameLength; ++index) {
        auto character = buffer[index];
        if (isLeadingCharacterInNameComponent) {
            if (!(isASCIIAlpha(character) || isASCIIDigit(character)))
                return NullOption;

            currentNameComponentStartIndex = index;
            isLeadingCharacterInNameComponent = false;
            continue;
        }

        if (character == '-') {
            if (!isValidComponent(currentNameComponentStartIndex, index))
                return NullOption;
            isLeadingCharacterInNameComponent = true;
            continue;
        }

        if (!(isASCIIAlpha(character) || isASCIIDigit(character)))
            return NullOption;
    }
    if (isLeadingCharacterInNameComponent)
        return NullOption;
    if (!isValidComponent(currentNameComponentStartIndex, nameLength))
        return NullOption;

    std::string result(buffer.consume(nameLength));

    if (buffer.atEnd())
        return NullOption;
    if (*buffer != ']')
        return NullOption;
    buffer.advance();
    return RFC9557Annotation{ flag, RFC9557Key::Calendar, std::move(result) };
}


static Optional<std::vector<CalendarID>> parseCalendar(ParserString& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Annotations
    //  Annotations :::
    //      Annotation Annotations[opt]

    if (!canBeRFC9557Annotation(buffer))
        return NullOption;

    std::vector<CalendarID> result;
    // https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime
    bool calendarWasCritical = false;
    while (canBeRFC9557Annotation(buffer)) {
        auto annotation = parseOneRFC9557Annotation(buffer);
        if (!annotation)
            return NullOption;
        if (annotation->m_key == RFC9557Key::Calendar) {
            result.push_back(annotation->m_value);
        }
        if (annotation->m_flag == RFC9557Flag::Critical) {
            // Check for unknown annotations with critical flag
            // step 4(a)(ii)(2)(d)(i)
            if (annotation->m_key != RFC9557Key::Calendar)
                return NullOption;
            // Check for multiple calendars and critical flag
            // step 4(a)(ii)(2)(c)(ii)
            if (result.size() == 1)
                calendarWasCritical = true;
            else
                return NullOption;
        }
        if (calendarWasCritical && result.size() > 1)
            return NullOption;
    }
    return result;
}

enum class Second60Mode { Accept,
                          Reject };
static Optional<PlainTime> parseTimeSpec(ParserString& buffer, Second60Mode second60Mode, bool parseSubMinutePrecision = true)
{
    // https://tc39.es/proposal-temporal/#prod-TimeSpec
    // TimeSpec :
    //     TimeHour
    //     TimeHour : TimeMinute
    //     TimeHour TimeMinute
    //     TimeHour : TimeMinute : TimeSecond TimeFraction[opt]
    //     TimeHour TimeMinute TimeSecond TimeFraction[opt]
    //
    //  TimeSecond can be 60. And if it is 60, we interpret it as 59.
    //  https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime

    if (buffer.lengthRemaining() < 2)
        return NullOption;

    ASSERT(buffer.lengthRemaining() >= 2);
    auto firstHourCharacter = *buffer;
    if (!(firstHourCharacter >= '0' && firstHourCharacter <= '2'))
        return NullOption;

    buffer.advance();
    auto secondHourCharacter = *buffer;
    if (!isASCIIDigit(secondHourCharacter))
        return NullOption;
    unsigned hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
    if (hour >= 24)
        return NullOption;
    buffer.advance();

    if (buffer.atEnd())
        return PlainTime(hour, 0, 0, 0, 0, 0);

    bool splitByColon = false;
    if (*buffer == ':') {
        splitByColon = true;
        buffer.advance();
    } else if (!(*buffer >= '0' && *buffer <= '5'))
        return PlainTime(hour, 0, 0, 0, 0, 0);

    if (buffer.lengthRemaining() < 2)
        return NullOption;
    auto firstMinuteCharacter = *buffer;
    if (!(firstMinuteCharacter >= '0' && firstMinuteCharacter <= '5'))
        return NullOption;

    buffer.advance();
    auto secondMinuteCharacter = *buffer;
    if (!isASCIIDigit(secondMinuteCharacter))
        return NullOption;
    unsigned minute = (secondMinuteCharacter - '0') + 10 * (firstMinuteCharacter - '0');
    ASSERT(minute < 60);
    buffer.advance();

    if (buffer.atEnd())
        return PlainTime(hour, minute, 0, 0, 0, 0);

    if (splitByColon) {
        if (*buffer == ':')
            buffer.advance();
        else
            return PlainTime(hour, minute, 0, 0, 0, 0);
    } else if (!(*buffer >= '0' && (second60Mode == Second60Mode::Accept ? (*buffer <= '6') : (*buffer <= '5'))))
        return PlainTime(hour, minute, 0, 0, 0, 0);

    if (!parseSubMinutePrecision)
        return NullOption;

    unsigned second = 0;
    if (buffer.lengthRemaining() < 2)
        return NullOption;
    auto firstSecondCharacter = *buffer;
    if (firstSecondCharacter >= '0' && firstSecondCharacter <= '5') {
        buffer.advance();
        auto secondSecondCharacter = *buffer;
        if (!isASCIIDigit(secondSecondCharacter))
            return NullOption;
        second = (secondSecondCharacter - '0') + 10 * (firstSecondCharacter - '0');
        ASSERT(second < 60);
        buffer.advance();
    } else if (second60Mode == Second60Mode::Accept && firstSecondCharacter == '6') {
        buffer.advance();
        auto secondSecondCharacter = *buffer;
        if (secondSecondCharacter != '0')
            return NullOption;
        second = 59;
        buffer.advance();
    } else
        return NullOption;

    if (buffer.atEnd())
        return PlainTime(hour, minute, second, 0, 0, 0);

    if (*buffer != '.' && *buffer != ',')
        return PlainTime(hour, minute, second, 0, 0, 0);
    buffer.advance();

    size_t digits = 0;
    size_t maxCount = std::min<size_t>(buffer.lengthRemaining(), 9);
    for (; digits < maxCount; ++digits) {
        if (!isASCIIDigit(buffer[digits]))
            break;
    }
    if (!digits)
        return NullOption;

    std::string padded("000000000");
    for (size_t i = 0; i < digits; ++i)
        padded[i] = buffer[i];
    buffer.advanceBy(digits);

    unsigned millisecond = parseDecimalInt32(padded.substr(0, 3));
    unsigned microsecond = parseDecimalInt32(padded.substr(3, 3));
    unsigned nanosecond = parseDecimalInt32(padded.substr(6, 3));

    return PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

static bool canBeTimeZone(const ParserString& buffer, char16_t character)
{
    switch (character) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z':
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-':
        return true;
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        // We should reject calendar extension case.
        // For BNF, see comment in canBeRFC9557Annotation()
        if (canBeRFC9557Annotation(buffer))
            return false;
        return true;
    }
    default:
        return false;
    }
}

Optional<int64_t> parseUTCOffset(ParserString& buffer, bool parseSubMinutePrecision)
{
    // UTCOffset[SubMinutePrecision] :
    //     ASCIISign Hour
    //     ASCIISign Hour TimeSeparator[+Extended] MinuteSecond
    //     ASCIISign Hour TimeSeparator[~Extended] MinuteSecond
    //     [+SubMinutePrecision] ASCIISign Hour TimeSeparator[+Extended] MinuteSecond TimeSeparator[+Extended] MinuteSecond TemporalDecimalFractionopt
    //     [+SubMinutePrecision] ASCIISign Hour TimeSeparator[~Extended] MinuteSecond TimeSeparator[~Extended] MinuteSecond TemporalDecimalFractionopt
    //
    //  This is the same to
    //     ASCIISign TimeSpec
    //
    //  Maximum and minimum values are ±23:59:59.999999999 = ±86399999999999ns, which can be represented by int64_t / double's integer part.

    // sign and hour.
    if (buffer.lengthRemaining() < 3)
        return NullOption;

    int64_t factor = 1;
    if (*buffer == '+')
        buffer.advance();
    else if (*buffer == '-') {
        factor = -1;
        buffer.advance();
    } else
        return NullOption;

    auto plainTime = parseTimeSpec(buffer, Second60Mode::Reject, parseSubMinutePrecision);
    if (!plainTime)
        return NullOption;

    int64_t hour = plainTime->hour();
    int64_t minute = plainTime->minute();
    int64_t second = plainTime->second();
    int64_t millisecond = plainTime->millisecond();
    int64_t microsecond = plainTime->microsecond();
    int64_t nanosecond = plainTime->nanosecond();

    return (nsPerHour * hour + nsPerMinute * minute + nsPerSecond * second + nsPerMillisecond * millisecond + nsPerMicrosecond * microsecond + nanosecond) * factor;
}

Optional<int64_t> parseUTCOffset(String* string, bool parseSubMinutePrecision)
{
    ParserString buffer(string);
    auto result = parseUTCOffset(buffer, parseSubMinutePrecision);
    if (!buffer.atEnd())
        return NullOption;
    return result;
}

static Optional<Variant<std::string, int64_t>> parseTimeZoneAnnotation(ParserString& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-TimeZoneAnnotation
    // TimeZoneAnnotation :
    //     [ AnnotationCriticalFlag_opt TimeZoneIdentifier ]
    // TimeZoneIdentifier :
    //     UTCOffset_[~SubMinutePrecision]
    //     TimeZoneIANAName

    if (buffer.lengthRemaining() < 3)
        return NullOption;

    if (*buffer != '[')
        return NullOption;
    buffer.advance();

    if (*buffer == '!')
        buffer.advance();

    switch (static_cast<char16_t>(*buffer)) {
    case '+':
    case '-': {
        auto offset = parseUTCOffset(buffer, false);
        if (!offset)
            return NullOption;
        if (buffer.atEnd())
            return NullOption;
        if (*buffer != ']')
            return NullOption;
        buffer.advance();
        return Variant<std::string, int64_t>::create<1>(offset.value());
    }
    case 'E': {
        // "Etc/GMT+20" and "]" => length is 11.
        if (buffer.lengthRemaining() >= 11) {
            if (buffer[0] == 'E' && buffer[1] == 't' && buffer[2] == 'c' && buffer[3] == '/' && buffer[4] == 'G' && buffer[5] == 'M' && buffer[6] == 'T') {
                auto signCharacter = buffer[7];
                // Not including minusSign since it is ASCIISign.
                if (signCharacter == '+' || signCharacter == '-') {
                    // Etc/GMT+01 is UTC-01:00. This sign is intentionally inverted.
                    // https://en.wikipedia.org/wiki/Tz_database#Area
                    int64_t factor = signCharacter == '+' ? -1 : 1;
                    int64_t hour = 0;
                    auto firstHourCharacter = buffer[8];
                    if (firstHourCharacter >= '0' && firstHourCharacter <= '2') {
                        auto secondHourCharacter = buffer[9];
                        if (isASCIIDigit(secondHourCharacter)) {
                            hour = (secondHourCharacter - '0') + 10 * (firstHourCharacter - '0');
                            if (hour < 24 && buffer[10] == ']') {
                                buffer.advanceBy(11);
                                return Variant<std::string, int64_t>::create<1>(nsPerHour * hour * factor);
                            }
                        }
                    }
                }
            }
        }
        [[fallthrough]];
    }
    default: {
        // TZLeadingChar :
        //     Alpha
        //     .
        //     _
        //
        // TZChar :
        //     Alpha
        //     .
        //     -
        //     _
        //
        // TimeZoneIANANameComponent :
        //     TZLeadingChar TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] TZChar[opt] but not one of . or ..
        //
        // TimeZoneIANAName :
        //     TimeZoneIANANameComponent
        //     TimeZoneIANAName / TimeZoneIANANameComponent

        unsigned nameLength = 0;
        {
            unsigned index = 0;
            for (; index < buffer.lengthRemaining(); ++index) {
                auto character = buffer[index];
                if (character == ']')
                    break;
                if (!isASCIIAlpha(character) && character != '.' && character != '_' && character != '-' && character != '/')
                    return NullOption;
            }
            if (!index)
                return NullOption;
            nameLength = index;
        }

        auto isValidComponent = [&](unsigned start, unsigned end) {
            unsigned componentLength = end - start;
            if (!componentLength)
                return false;
            if (componentLength > 14)
                return false;
            if (componentLength == 1 && buffer[start] == '.')
                return false;
            if (componentLength == 2 && buffer[start] == '.' && buffer[start + 1] == '.')
                return false;
            return true;
        };

        unsigned currentNameComponentStartIndex = 0;
        bool isLeadingCharacterInNameComponent = true;
        for (unsigned index = 0; index < nameLength; ++index) {
            auto character = buffer[index];
            if (isLeadingCharacterInNameComponent) {
                if (!(isASCIIAlpha(character) || character == '.' || character == '_'))
                    return NullOption;

                currentNameComponentStartIndex = index;
                isLeadingCharacterInNameComponent = false;
                continue;
            }

            if (character == '/') {
                if (!isValidComponent(currentNameComponentStartIndex, index))
                    return NullOption;
                isLeadingCharacterInNameComponent = true;
                continue;
            }

            if (!(isASCIIAlpha(character) || character == '.' || character == '_' || character == '-'))
                return NullOption;
        }
        if (isLeadingCharacterInNameComponent)
            return NullOption;
        if (!isValidComponent(currentNameComponentStartIndex, nameLength))
            return NullOption;

        std::string result(buffer.consume(nameLength));

        if (buffer.atEnd())
            return NullOption;
        if (*buffer != ']')
            return NullOption;
        buffer.advance();
        return Variant<std::string, int64_t>::create<0>(result);
    }
    }
}

static Optional<TimeZoneRecord> parseTimeZone(ParserString& buffer, bool parseSubMinutePrecisionForTimeZone)
{
    if (buffer.atEnd())
        return NullOption;
    switch (static_cast<char16_t>(*buffer)) {
    // UTCDesignator
    // https://tc39.es/proposal-temporal/#prod-UTCDesignator
    case 'z':
    case 'Z': {
        buffer.advance();
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneAnnotation(buffer);
            if (!timeZone)
                return NullOption;
            return TimeZoneRecord{ true, NullOption, std::move(timeZone.value()) };
        }
        return TimeZoneRecord{ true, NullOption, {} };
    }
    // TimeZoneUTCOffsetSign
    // https://tc39.es/proposal-temporal/#prod-TimeZoneUTCOffsetSign
    case '+':
    case '-': {
        auto offset = parseUTCOffset(buffer, parseSubMinutePrecisionForTimeZone);
        if (!offset)
            return NullOption;
        if (!buffer.atEnd() && *buffer == '[' && canBeTimeZone(buffer, *buffer)) {
            auto timeZone = parseTimeZoneAnnotation(buffer);
            if (!timeZone)
                return NullOption;
            return TimeZoneRecord{ false, offset.value(), std::move(timeZone.value()) };
        }
        return TimeZoneRecord{ false, offset.value(), {} };
    }
    // TimeZoneBracketedAnnotation
    // https://tc39.es/proposal-temporal/#prod-TimeZoneBracketedAnnotation
    case '[': {
        auto timeZone = parseTimeZoneAnnotation(buffer);
        if (!timeZone)
            return NullOption;
        return TimeZoneRecord{ false, NullOption, std::move(timeZone.value()) };
    }
    default:
        return NullOption;
    }
}

static Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>> parseTime(ParserString& buffer, bool parseSubMinutePrecisionForTimeZone = true)
{
    // https://tc39.es/proposal-temporal/#prod-Time
    // Time :
    //     TimeSpec TimeZone[opt]
    auto plainTime = parseTimeSpec(buffer, Second60Mode::Accept);
    if (!plainTime) {
        return NullOption;
    }
    if (buffer.atEnd())
        return Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainTime.value()), NullOption));
    if (canBeTimeZone(buffer, *buffer)) {
        auto timeZone = parseTimeZone(buffer, parseSubMinutePrecisionForTimeZone);
        if (!timeZone)
            return NullOption;
        return Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainTime.value()), std::move(timeZone)));
    }
    return Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainTime.value()), NullOption));
}

Optional<std::tuple<PlainTime, Optional<TimeZoneRecord>>> parseTime(String* input)
{
    ParserString buffer(input);
    if (toupper(*buffer) == 'T') {
        buffer.advance();
    }
    auto result = parseTime(buffer, true);

    Optional<CalendarID> calendarOptional;
    if (!buffer.atEnd() && canBeRFC9557Annotation(buffer)) {
        auto calendars = parseCalendar(buffer);
        if (!calendars)
            return NullOption;
        if (calendars.value().size() > 0)
            calendarOptional = std::move(calendars.value()[0]);
    }

    if (buffer.atEnd()) {
        return result;
    }

    return NullOption;
}

static Optional<PlainDate> parseDate(ParserString& buffer)
{
    // https://tc39.es/proposal-temporal/#prod-Date
    // Date :
    //     DateYear - DateMonth - DateDay
    //     DateYear DateMonth DateDay
    //
    // DateYear :
    //     DateFourDigitYear
    //     DateExtendedYear
    //
    // DateFourDigitYear :
    //     Digit Digit Digit Digit
    //
    // DateExtendedYear :
    //     Sign Digit Digit Digit Digit Digit Digit
    //
    // DateMonth :
    //     0 NonzeroDigit
    //     10
    //     11
    //     12
    //
    // DateDay :
    //     0 NonzeroDigit
    //     1 Digit
    //     2 Digit
    //     30
    //     31

    if (buffer.atEnd())
        return NullOption;

    bool sixDigitsYear = false;
    int yearFactor = 1;
    if (*buffer == '+') {
        buffer.advance();
        sixDigitsYear = true;
    } else if (*buffer == '-') {
        yearFactor = -1;
        buffer.advance();
        sixDigitsYear = true;
    } else if (!isASCIIDigit(*buffer))
        return NullOption;

    int32_t year = 0;
    if (sixDigitsYear) {
        if (buffer.lengthRemaining() < 6)
            return NullOption;
        for (unsigned index = 0; index < 6; ++index) {
            if (!isASCIIDigit(buffer[index]))
                return NullOption;
        }
        year = parseDecimalInt32(buffer.first(6)) * yearFactor;
        if (!year && yearFactor < 0)
            return NullOption;
        buffer.advanceBy(6);
    } else {
        if (buffer.lengthRemaining() < 4)
            return NullOption;
        for (unsigned index = 0; index < 4; ++index) {
            if (!isASCIIDigit(buffer[index]))
                return NullOption;
        }
        year = parseDecimalInt32(buffer.first(4));
        buffer.advanceBy(4);
    }

    if (buffer.atEnd())
        return NullOption;

    bool splitByHyphen = false;
    if (*buffer == '-') {
        splitByHyphen = true;
        buffer.advance();
        if (buffer.lengthRemaining() < 5)
            return NullOption;
    } else {
        if (buffer.lengthRemaining() < 4)
            return NullOption;
    }
    // We ensured that buffer has enough length for month and day. We do not need to check length.

    unsigned month = 0;
    auto firstMonthCharacter = *buffer;
    if (firstMonthCharacter == '0' || firstMonthCharacter == '1') {
        buffer.advance();
        auto secondMonthCharacter = *buffer;
        if (!isASCIIDigit(secondMonthCharacter))
            return NullOption;
        month = (secondMonthCharacter - '0') + 10 * (firstMonthCharacter - '0');
        if (!month || month > 12)
            return NullOption;
        buffer.advance();
    } else
        return NullOption;

    if (splitByHyphen) {
        if (*buffer == '-')
            buffer.advance();
        else
            return NullOption;
    }

    unsigned day = 0;
    auto firstDayCharacter = *buffer;
    if (firstDayCharacter >= '0' && firstDayCharacter <= '3') {
        buffer.advance();
        auto secondDayCharacter = *buffer;
        if (!isASCIIDigit(secondDayCharacter))
            return NullOption;
        day = (secondDayCharacter - '0') + 10 * (firstDayCharacter - '0');
        if (!day || day > daysInMonth(year, month))
            return NullOption;
        buffer.advance();
    } else
        return NullOption;

    return PlainDate(year, month, day);
}

static Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>> parseDateTime(ParserString& buffer, bool parseSubMinutePrecisionForTimeZone)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // DateTime :
    //     Date TimeSpecSeparator[opt] TimeZone[opt]
    //
    // TimeSpecSeparator :
    //     DateTimeSeparator TimeSpec
    auto plainDate = parseDate(buffer);
    if (!plainDate)
        return NullOption;
    if (buffer.atEnd())
        return Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainDate.value()), NullOption, NullOption));

    if (*buffer == ' ' || *buffer == 'T' || *buffer == 't') {
        buffer.advance();
        auto plainTimeAndTimeZone = parseTime(buffer, parseSubMinutePrecisionForTimeZone);
        if (!plainTimeAndTimeZone)
            return NullOption;
        auto plainTime = std::get<0>(plainTimeAndTimeZone.value());
        auto timeZone = std::get<1>(plainTimeAndTimeZone.value());
        return Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainDate.value()), std::move(plainTime), std::move(timeZone)));
    }

    if (canBeTimeZone(buffer, *buffer))
        return NullOption;

    return Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>>(std::make_tuple(std::move(plainDate.value()), NullOption, NullOption));
}

Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>>> parseDateTime(String* input)
{
    ParserString ps(input);
    auto result = parseDateTime(ps, true);
    if (ps.atEnd()) {
        return result;
    }
    return NullOption;
}

static Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>, Optional<CalendarID>>> parseCalendarDateTime(ParserString& buffer, bool parseSubMinutePrecisionForTimeZone)
{
    // https://tc39.es/proposal-temporal/#prod-DateTime
    // CalendarDateTime :
    //     DateTime CalendarName[opt]
    //
    auto dateTime = parseDateTime(buffer, parseSubMinutePrecisionForTimeZone);
    if (!dateTime) {
        return NullOption;
    }

    auto plainDate = std::get<0>(dateTime.value());
    auto plainTimeOptional = std::get<1>(dateTime.value());
    auto timeZoneOptional = std::get<2>(dateTime.value());

    Optional<CalendarID> calendarOptional;
    if (!buffer.atEnd() && canBeRFC9557Annotation(buffer)) {
        auto calendars = parseCalendar(buffer);
        if (!calendars)
            return NullOption;
        if (calendars.value().size() > 0)
            calendarOptional = std::move(calendars.value()[0]);
    }

    return std::make_tuple(std::move(plainDate), std::move(plainTimeOptional), std::move(timeZoneOptional), std::move(calendarOptional));
}


Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>, Optional<CalendarID>>> parseCalendarDateTime(String* input, bool parseSubMinutePrecisionForTimeZone)
{
    ParserString buffer(input);
    auto result = parseCalendarDateTime(buffer, parseSubMinutePrecisionForTimeZone);
    if (!buffer.atEnd()) {
        return NullOption;
    }
    return result;
}

static double daysFrom1970ToYear(int year)
{
    // The Gregorian Calendar rules for leap years:
    // Every fourth year is a leap year. 2004, 2008, and 2012 are leap years.
    // However, every hundredth year is not a leap year. 1900 and 2100 are not leap years.
    // Every four hundred years, there's a leap year after all. 2000 and 2400 are leap years.

    static constexpr int leapDaysBefore1971By4Rule = 1970 / 4;
    static constexpr int excludedLeapDaysBefore1971By100Rule = 1970 / 100;
    static constexpr int leapDaysBefore1971By400Rule = 1970 / 400;

    const double yearMinusOne = static_cast<double>(year) - 1;
    const double yearsToAddBy4Rule = floor(yearMinusOne / 4.0) - leapDaysBefore1971By4Rule;
    const double yearsToExcludeBy100Rule = floor(yearMinusOne / 100.0) - excludedLeapDaysBefore1971By100Rule;
    const double yearsToAddBy400Rule = floor(yearMinusOne / 400.0) - leapDaysBefore1971By400Rule;

    return 365.0 * (year - 1970.0) + yearsToAddBy4Rule - yearsToExcludeBy100Rule + yearsToAddBy400Rule;
}

static int dayInYear(int year, int month, int day)
{
    const std::array<std::array<int, 12>, 2> firstDayOfMonth{
        std::array<int, 12>{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
        std::array<int, 12>{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
    };
    return firstDayOfMonth[isLeapYear(year)][month] + day - 1;
}

static double dateToDaysFrom1970(int year, int month, int day)
{
    year += month / 12;

    month %= 12;
    if (month < 0) {
        month += 12;
        --year;
    }

    double yearday = floor(daysFrom1970ToYear(year));
    ASSERT((year >= 1970 && yearday >= 0) || (year < 1970 && yearday < 0));
    return yearday + dayInYear(year, month, day);
}

ExactTime ExactTime::fromISOPartsAndOffset(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond, int64_t offset)
{
    ASSERT(month >= 1 && month <= 12);
    ASSERT(day >= 1 && day <= 31);
    ASSERT(hour <= 23);
    ASSERT(minute <= 59);
    ASSERT(second <= 59);
    ASSERT(millisecond <= 999);
    ASSERT(microsecond <= 999);
    ASSERT(nanosecond <= 999);

    Int128 dateDays = static_cast<int64_t>(dateToDaysFrom1970(year, month - 1, day));
    Int128 utcNanoseconds = dateDays * nsPerDay + hour * nsPerHour + minute * nsPerMinute + second * nsPerSecond + millisecond * nsPerMillisecond + microsecond * nsPerMicrosecond + nanosecond;
    return ExactTime{ utcNanoseconds - offset };
}

Int128 lengthInNanoseconds(DateTimeUnit unit)
{
    if (unit == DateTimeUnit::Nanosecond) {
        return 1;
    } else if (unit == DateTimeUnit::Microsecond) {
        return 1000;
    } else if (unit == DateTimeUnit::Millisecond) {
        return 1000 * 1000;
    } else if (unit == DateTimeUnit::Second) {
        return Int128(1000 * 1000) * Int128(1000);
    } else if (unit == DateTimeUnit::Minute) {
        return Int128(1000 * 1000) * Int128(1000) * Int128(60);
    } else if (unit == DateTimeUnit::Hour) {
        return Int128(1000 * 1000) * Int128(1000) * Int128(60) * Int128(60);
    } else {
        ASSERT(unit == DateTimeUnit::Day);
        return Int128(1000 * 1000) * Int128(1000) * Int128(60) * Int128(60) * Int128(24);
    }
}

double roundNumberToIncrement(double x, double increment, RoundingMode roundingMode)
{
    auto quotient = x / increment;
    auto truncatedQuotient = std::trunc(quotient);
    if (truncatedQuotient == quotient)
        return truncatedQuotient * increment;

    auto isNegative = quotient < 0;
    auto expandedQuotient = isNegative ? truncatedQuotient - 1 : truncatedQuotient + 1;

    if (roundingMode >= RoundingMode::HalfCeil) {
        auto unsignedFractionalPart = std::abs(quotient - truncatedQuotient);
        if (unsignedFractionalPart < 0.5)
            return truncatedQuotient * increment;
        if (unsignedFractionalPart > 0.5)
            return expandedQuotient * increment;
    }

    if (roundingMode == RoundingMode::Ceil) {
        return (isNegative ? truncatedQuotient : expandedQuotient) * increment;
    } else if (roundingMode == RoundingMode::Floor) {
        return (isNegative ? expandedQuotient : truncatedQuotient) * increment;
    } else if (roundingMode == RoundingMode::Expand) {
        return expandedQuotient * increment;
    } else if (roundingMode == RoundingMode::Trunc) {
        return truncatedQuotient * increment;
    } else if (roundingMode == RoundingMode::HalfCeil) {
        return (isNegative ? truncatedQuotient : expandedQuotient) * increment;
    } else if (roundingMode == RoundingMode::HalfFloor) {
        return (isNegative ? expandedQuotient : truncatedQuotient) * increment;
    } else if (roundingMode == RoundingMode::HalfExpand) {
        return expandedQuotient * increment;
    } else if (roundingMode == RoundingMode::HalfTrunc) {
        return truncatedQuotient * increment;
    } else if (roundingMode == RoundingMode::HalfEven) {
        return (!std::fmod(truncatedQuotient, 2) ? truncatedQuotient : expandedQuotient) * increment;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

Int128 roundNumberToIncrement(Int128 x, Int128 increment, RoundingMode roundingMode)
{
    // This follows the polyfill code rather than the spec, in order to work around
    // being unable to apply floating-point division in x / increment.
    // See https://github.com/tc39/proposal-temporal/blob/main/polyfill/lib/ecmascript.mjs#L4043
    Int128 quotient = x / increment;
    Int128 remainder = x % increment;
    bool isNegative = x < 0;
    Int128 r1 = std::abs(quotient);
    Int128 r2 = r1 + 1;
    Int128 even = r1 % 2;
    auto unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, isNegative);
    Int128 rounded = 0;
    if (std::abs(x) == r1 * increment)
        rounded = r1;
    else if (unsignedRoundingMode == UnsignedRoundingMode::Zero)
        rounded = r1;
    else if (unsignedRoundingMode == UnsignedRoundingMode::Infinity)
        rounded = r2;
    else if (std::abs(remainder * 2) < increment)
        rounded = r1;
    else if (std::abs(remainder * 2) > increment)
        rounded = r2;
    else if (unsignedRoundingMode == UnsignedRoundingMode::HalfZero)
        rounded = r1;
    else if (unsignedRoundingMode == UnsignedRoundingMode::HalfInfinity)
        rounded = r2;
    else
        rounded = !even ? r1 : r2;
    if (isNegative)
        rounded = -rounded;
    return rounded * increment;
}

Int128 roundNumberToIncrementAsIfPositive(Int128 x, Int128 increment, RoundingMode roundingMode)
{
    // The following code follows the polyfill rather than the spec, because we don't have float128.
    // ApplyUnsignedRoundingMode is inlined here to mirror the polyfill's implementation of it,
    // which has a different type than in the spec.
    // See https://github.com/tc39/proposal-temporal/blob/main/polyfill/lib/ecmascript.mjs#L4056
    Int128 quotient = x / increment;
    Int128 remainder = x % increment;
    auto unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, false);
    auto r1 = quotient;
    auto r2 = quotient + 1;
    if (x < 0) {
        r1 = quotient - 1;
        r2 = quotient;
    }
    auto doubleRemainder = std::abs(remainder * 2);
    auto even = r1 % 2;
    if (quotient * increment == x)
        return x;
    if (unsignedRoundingMode == UnsignedRoundingMode::Zero)
        return r1 * increment;
    if (unsignedRoundingMode == UnsignedRoundingMode::Infinity)
        return r2 * increment;
    if (doubleRemainder < increment)
        return r1 * increment;
    if (doubleRemainder > increment)
        return r2 * increment;
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfZero)
        return r1 * increment;
    if (unsignedRoundingMode == UnsignedRoundingMode::HalfInfinity)
        return r2 * increment;
    return !even ? r1 * increment : r2 * increment;
}

UnsignedRoundingMode getUnsignedRoundingMode(RoundingMode roundingMode, bool isNegative)
{
    if (roundingMode == RoundingMode::Ceil) {
        return isNegative ? UnsignedRoundingMode::Zero : UnsignedRoundingMode::Infinity;
    } else if (roundingMode == RoundingMode::Floor) {
        return isNegative ? UnsignedRoundingMode::Infinity : UnsignedRoundingMode::Zero;
    } else if (roundingMode == RoundingMode::Expand) {
        return UnsignedRoundingMode::Infinity;
    } else if (roundingMode == RoundingMode::Trunc) {
        return UnsignedRoundingMode::Zero;
    } else if (roundingMode == RoundingMode::HalfCeil) {
        return isNegative ? UnsignedRoundingMode::HalfZero : UnsignedRoundingMode::HalfInfinity;
    } else if (roundingMode == RoundingMode::HalfFloor) {
        return isNegative ? UnsignedRoundingMode::HalfInfinity : UnsignedRoundingMode::HalfZero;
    } else if (roundingMode == RoundingMode::HalfExpand) {
        return UnsignedRoundingMode::HalfInfinity;
    } else if (roundingMode == RoundingMode::HalfTrunc) {
        return UnsignedRoundingMode::HalfZero;
    }
    return UnsignedRoundingMode::HalfEven;
}

static Int128 roundTemporalInstant(Int128 ns, unsigned increment, DateTimeUnit unit, RoundingMode roundingMode)
{
    auto unitLength = lengthInNanoseconds(unit);
    auto incrementNs = increment * unitLength;
    return roundNumberToIncrementAsIfPositive(ns, incrementNs, roundingMode);
}

ExactTime ExactTime::round(ExecutionState& state, unsigned increment, DateTimeUnit unit, RoundingMode roundingMode)
{
    auto roundedNs = roundTemporalInstant(m_epochNanoseconds, increment, unit, roundingMode);
    return ExactTime{ roundedNs };
}

// https://tc39.es/proposal-temporal/#sec-temporal-datedurationsign
static int32_t dateDurationSign(const Duration& d)
{
    if (d.years() > 0)
        return 1;
    if (d.years() < 0)
        return -1;
    if (d.months() > 0)
        return 1;
    if (d.months() < 0)
        return -1;
    if (d.weeks() > 0)
        return 1;
    if (d.weeks() < 0)
        return -1;
    if (d.days() > 0)
        return 1;
    if (d.days() < 0)
        return -1;
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-temporal-internaldurationsign
int32_t ISO8601::InternalDuration::sign() const
{
    int32_t sign = dateDurationSign(m_dateDuration);
    if (sign)
        return sign;
    return timeDurationSign();
}

// https://tc39.es/proposal-temporal/#sec-temporal-combinedateandtimeduration
InternalDuration InternalDuration::combineDateAndTimeDuration(Duration dateDuration, Int128 timeDuration)
{
    int32_t dateSign = dateDurationSign(dateDuration);
    int32_t timeSign = timeDuration < 0 ? -1 : timeDuration > 0 ? 1
                                                                : 0;
    return InternalDuration{ std::move(dateDuration), timeDuration };
}

Optional<ExactTime> parseISODateTimeWithInstantFormat(String* input)
{
    // https://tc39.es/proposal-temporal/#prod-TemporalInstantString
    // TemporalInstantString :
    //     Date TimeZoneOffsetRequired
    //     Date DateTimeSeparator TimeSpec TimeZoneOffsetRequired

    // https://tc39.es/proposal-temporal/#prod-TimeZoneOffsetRequired
    // TimeZoneOffsetRequired :
    //     TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt

    ParserString buffer(input);
    auto datetime = parseCalendarDateTime(buffer, true);
    if (!datetime)
        return NullOption;
    auto plainDate = std::get<0>(datetime.value());
    auto plainTimeOptional = std::get<1>(datetime.value());
    auto timeZoneOptional = std::get<2>(datetime.value());
    auto calendarOptional = std::get<3>(datetime.value());
    if (!timeZoneOptional || (!timeZoneOptional->m_z && !timeZoneOptional->m_offset))
        return NullOption;
    if (!buffer.atEnd())
        return NullOption;

    PlainTime plainTime;
    if (plainTimeOptional) {
        plainTime = plainTimeOptional.value();
    }

    int64_t offset = timeZoneOptional->m_z ? 0 : timeZoneOptional->m_offset.value();
    return ExactTime::fromISOPartsAndOffset(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond(), offset);
}

Optional<TimeZoneID> parseTimeZoneName(String* string)
{
    std::string lowerString = string->toNonGCUTF8StringData();
    std::transform(lowerString.begin(), lowerString.end(), lowerString.begin(), tolower);

    Optional<TimeZoneID> ret;
    Intl::availableTimeZones([&](const char* buf, size_t len) {
        if (len == lowerString.size()) {
            char* newBuf = reinterpret_cast<char*>(alloca(len));
            std::transform(buf, buf + len, newBuf, tolower);
            if (memcmp(lowerString.data(), newBuf, len) == 0) {
                ret = String::fromUTF8(buf, len);
            }
        }
    });
    return ret;
}

Int128 resolveNanosecondsValueByUnit(DateTimeUnit unit)
{
    Int128 maximum = 0;
    constexpr int64_t hoursPerDay = 24;
    constexpr int64_t minutesPerHour = 60;
    constexpr int64_t secondsPerMinute = 60;
    constexpr int64_t msPerDay = hoursPerDay * minutesPerHour * secondsPerMinute * 1000;

    if (unit == DateTimeUnit::Hour) {
        maximum = static_cast<Int128>(hoursPerDay);
    } else if (unit == DateTimeUnit::Minute) {
        maximum = static_cast<Int128>(minutesPerHour * hoursPerDay);
    } else if (unit == DateTimeUnit::Second) {
        maximum = static_cast<Int128>(secondsPerMinute * minutesPerHour * hoursPerDay);
    } else if (unit == DateTimeUnit::Millisecond) {
        maximum = static_cast<Int128>(msPerDay);
    } else if (unit == DateTimeUnit::Microsecond) {
        maximum = static_cast<Int128>(msPerDay * 1000);
    } else if (unit == DateTimeUnit::Nanosecond) {
        maximum = ISO8601::ExactTime::nsPerDay;
    }

    return maximum;
}

} // namespace ISO8601
} // namespace Escargot

#endif
