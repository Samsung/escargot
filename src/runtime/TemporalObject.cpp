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

#include "Escargot.h"
#include "TemporalObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/BigInt.h"
#include "runtime/DateObject.h"
#include "runtime/TemporalDurationObject.h"
#include "runtime/TemporalInstantObject.h"
#include "intl/Intl.h"

namespace Escargot {

//////////////////
// helper function
//////////////////
static Value getOption(ExecutionState& state, Object* options, Value property, bool isBool, Value* values, size_t valuesLength, const Value& defaultValue)
{
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    if (value.isUndefined()) {
        // TODO handle REQUIRED in default value
        return defaultValue;
    }
    if (isBool) {
        value = Value(value.toBoolean());
    } else {
        value = Value(value.toString(state));
    }

    if (valuesLength) {
        bool contains = false;
        for (size_t i = 0; i < valuesLength; i++) {
            if (values[i].equalsTo(state, value)) {
                contains = true;
            }
        }
        if (!contains) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
        }
    }

    return value;
}

static Object* getOptionsObject(ExecutionState& state, Value options)
{
    if (options.isUndefined()) {
        return new Object(state, Object::PrototypeIsNull);
    }
    if (options.isObject()) {
        return options.asObject();
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::TemporalError);
    return nullptr;
}

static bool getTemporalOverflowOptions(ExecutionState& state, Object* options)
{
    StaticStrings* strings = &state.context()->staticStrings();
    Value stringConstrain = Value(strings->lazyConstrain().string());

    Value values[2] = { stringConstrain, strings->reject.string() };
    Value stringValue = getOption(state, options, Value(strings->lazyOverflow().string()), false, values, 2, stringConstrain);
    if (stringValue.equalsTo(state, stringConstrain)) {
        // return CONSTRAIN
        return true;
    }

    // return REJECT
    return false;
}

static int64_t countModZerosInRange(int64_t mod, int64_t start, int64_t end)
{
    int64_t divEnd = end - 1;
    int negative = divEnd < 0;
    int64_t endPos = (divEnd + negative) / mod - negative;

    divEnd = start - 1;
    negative = divEnd < 0;
    int64_t startPos = (divEnd + negative) / mod - negative;

    return endPos - startPos;
}

static int countDaysOfYear(int year, int month, int day)
{
    if (month < 1 || month > 12) {
        return 0;
    }

    static const int seekTable[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int daysOfYear = seekTable[month - 1] + day - 1;

    if ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) && month >= 3) {
        daysOfYear++;
    }

    return daysOfYear;
}

static double makeDay(double year, double month, double date)
{
    if (!std::isfinite(year) || !std::isfinite(month) || !std::isfinite(date)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double ym = year + std::floor(month / TimeConstant::MonthsPerYear);
    if (!std::isfinite(ym)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mn = ((int)month % TimeConstant::MonthsPerYear);
    if ((ym > std::numeric_limits<int>::max()) || (ym < std::numeric_limits<int>::min())) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if ((mn + 1 > std::numeric_limits<int>::max()) || (mn + 1 < std::numeric_limits<int>::min())) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    int beginYear, endYear, leapSign;
    if (ym < 1970) {
        beginYear = static_cast<int>(ym);
        endYear = 1970;
        leapSign = -1;
    } else {
        beginYear = 1970;
        endYear = static_cast<int>(ym);
        leapSign = 1;
    }
    int64_t days = 365 * (static_cast<int64_t>(ym) - 1970);
    int64_t extraLeapDays = 0;
    extraLeapDays += countModZerosInRange(4, beginYear, endYear);
    extraLeapDays += countModZerosInRange(100, beginYear, endYear);
    extraLeapDays += countModZerosInRange(400, beginYear, endYear);
    int64_t yearsToDays = days + extraLeapDays * leapSign;

    int64_t daysOfYear = countDaysOfYear(static_cast<int>(ym), static_cast<int>(mn) + 1, 1);

    int64_t t = (yearsToDays + daysOfYear) * TimeConstant::MsPerDay;
    return (std::floor(t / TimeConstant::MsPerDay) + date - 1);
}

static double makeTime(double hour, double minute, double sec, double ms)
{
    if (!std::isfinite(hour) || !std::isfinite(minute) || !std::isfinite(sec) || !std::isfinite(ms)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return (hour * TimeConstant::MsPerHour + minute * TimeConstant::MsPerMinute + sec * TimeConstant::MsPerSecond + ms);
}

static double makeDate(double day, double time)
{
    if (!std::isfinite(day) || !std::isfinite(time)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double tv = day * TimeConstant::MsPerDay + time;
    if (!std::isfinite(tv)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return tv;
}

////////////////////////////
// Temporal helper functions
////////////////////////////
double Temporal::epochDayNumberForYear(const double year)
{
    return 365.0 * (year - 1970.0) + std::floor((year - 1969.0) / 4.0) - std::floor((year - 1901.0) / 100.0) + std::floor((year - 1601.0) / 400.0);
}

int Temporal::epochTimeToEpochYear(const double time)
{
    return DateObject::yearFromTime(time);
}

double Temporal::epochTimeForYear(const double year)
{
    return TimeConstant::MsPerDay * epochDayNumberForYear(year);
}

int Temporal::mathematicalInLeapYear(const double time)
{
    int daysInYear = mathematicalDaysInYear(epochTimeToEpochYear(time));

    if (daysInYear == 365) {
        return 0;
    } else {
        ASSERT(daysInYear == 366);
        return 1;
    }
}

int Temporal::mathematicalDaysInYear(const int year)
{
    if ((year % 4) != 0) {
        return 365;
    }

    if ((year % 100) != 0) {
        return 366;
    }

    if ((year % 400) != 0) {
        return 365;
    }

    return 366;
}

bool Temporal::isValidISODate(ExecutionState& state, const int year, const int month, const int day)
{
    if (month < 1 || month > 12) {
        return false;
    }

    int daysInMonth = Temporal::ISODaysInMonth(year, month);
    if (day < 1 || day > daysInMonth) {
        return false;
    }

    return true;
}

int Temporal::ISODaysInMonth(const int year, const int month)
{
    if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
        return 31;
    }

    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }

    ASSERT(month == 2);

    // Return 28 + MathematicalInLeapYear(EpochTimeForYear(year)).
    return 28 + mathematicalInLeapYear(epochTimeForYear(year));
}

int Temporal::ISODayOfWeek(const ISODate& date)
{
    // Let epochDays be ISODateToEpochDays(isoDate.[[Year]], isoDate.[[Month]] - 1, isoDate.[[Day]]).
    double epochDays = makeDay(date.year, date.month - 1, date.day);
    int dayOfWeek = static_cast<int>(std::floor((epochDays * TimeConstant::MsPerDay) / TimeConstant::MsPerDay) + 4) % 7;

    if (dayOfWeek == 0) {
        return 7;
    }

    return dayOfWeek;
}

int Temporal::ISODayOfYear(const ISODate& date)
{
    // Let epochDays be ISODateToEpochDays(isoDate.[[Year]], isoDate.[[Month]] - 1, isoDate.[[Day]]).
    double epochDays = makeDay(date.year, date.month - 1, date.day);
    double t = epochDays * TimeConstant::MsPerDay;

    return static_cast<int>(std::floor(t / TimeConstant::MsPerDay) - epochDayNumberForYear(DateObject::yearFromTime(t))) + 1;
}

YearWeek Temporal::ISOWeekOfYear(const ISODate& date)
{
    const int year = date.year;
    const int wednesday = 3;
    const int thursday = 4;
    const int friday = 5;
    const int saturday = 6;
    const int daysInWeek = 7;
    const int maxWeekNumber = 53;
    const int dayOfYear = ISODayOfYear(date);
    const int dayOfWeek = ISODayOfWeek(date);
    const int week = static_cast<int>(std::floor((double)(dayOfYear + daysInWeek - dayOfWeek + wednesday) / daysInWeek));

    if (week < 1) {
        ISODate jan1st(year, 1, 1);
        int dayOfJan1st = ISODayOfWeek(jan1st);
        if (dayOfJan1st == friday) {
            return YearWeek(maxWeekNumber, year - 1);
        }
        if ((dayOfJan1st == saturday) && (mathematicalInLeapYear(epochTimeForYear(year - 1)) == 1)) {
            return YearWeek(maxWeekNumber, year - 1);
        }
        return YearWeek(maxWeekNumber - 1, year - 1);
    }

    if (week == maxWeekNumber) {
        int daysInYear = mathematicalDaysInYear(year);
        int daysLaterInYear = daysInYear - dayOfYear;
        int daysAfterThursday = thursday - dayOfWeek;
        if (daysLaterInYear < daysAfterThursday) {
            return YearWeek(1, year + 1);
        }
    }

    return YearWeek(week, year);
}

bool Temporal::ISODateWithinLimits(ExecutionState& state, const ISODate& date)
{
    // NoonTimeRecord
    // { [[Days]]: 0, [[Hour]]: 12, [[Minute]]: 0, [[Second]]: 0, [[Millisecond]]: 0, [[Microsecond]]: 0, [[Nanosecond]]: 0  }
    TimeRecord record = TimeRecord::noonTimeRecord();
    ISODateTime dateTime = ISODateTime(date, record);

    return Temporal::ISODateTimeWithinLimits(state, dateTime);
}

bool Temporal::ISODateTimeWithinLimits(ExecutionState& state, const ISODateTime& dateTime)
{
    double date = makeDay(dateTime.date.year, dateTime.date.month - 1, dateTime.date.day);
    if (std::abs(date) > 100000001) {
        return false;
    }

    double time = makeTime(dateTime.time.hour, dateTime.time.minute, dateTime.time.second, dateTime.time.millisecond);
    double ms = makeDate(date, time);

    // Z(R(ms) × 10**6 + isoDateTime.[[Time]].[[Microsecond]] × 10**3 + isoDateTime.[[Time]].[[Nanosecond]]).
    int64_t ns = static_cast<int64_t>(ms) * 1000000 + dateTime.time.microsecond * 1000 + dateTime.time.nanosecond;

    if (ns <= Temporal::nsMinConstant()) {
        return false;
    }
    if (ns >= Temporal::nsMaxConstant()) {
        return false;
    }

    return true;
}

int64_t Temporal::nsPerDay()
{
    return 86400000000000;
}

Value Temporal::createTemporalDate(ExecutionState& state, const ISODate& isoDate, String* calendar, Optional<Object*> newTarget)
{
    if (!Temporal::ISODateWithinLimits(state, isoDate)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.hasValue() ? newTarget.value() : state.context()->globalObject()->temporalPlainDate(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainDatePrototype();
    });

    return new TemporalPlainDateObject(state, proto, isoDate, calendar);
}

Value Temporal::toTemporalDate(ExecutionState& state, Value item, Value options)
{
    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainDateObject()) {
            Object* resolvedOptions = getOptionsObject(state, options);
            getTemporalOverflowOptions(state, resolvedOptions);
            return Temporal::createTemporalDate(state, item.asObject()->asTemporalPlainDateObject()->date(), item.asObject()->asTemporalPlainDateObject()->calendar(), nullptr);
        }

        // TODO
        return Value();
    }

    if (!item.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::TemporalError);
    }

    // TODO ParseISODateTime
    return Value();
}

void Temporal::formatSecondsStringFraction(StringBuilder& builder, Int128 fraction, Value precision)
{
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

Int128 Temporal::systemUTCEpochNanoseconds()
{
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();

    auto microSecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    unsigned long nano = std::chrono::duration_cast<std::chrono::nanoseconds>(d - std::chrono::duration_cast<std::chrono::microseconds>(d)).count();

    Int128 ret = Int128(microSecondsSinceEpoch);
    ret *= 1000;
    ret += nano;

    return ret;
}

bool Temporal::isValidEpochNanoseconds(Int128 epochNanoseconds)
{
    // If ℝ(epochNanoseconds) < nsMinInstant or ℝ(epochNanoseconds) > nsMaxInstant, then
    if (epochNanoseconds < Temporal::nsMinInstant() || epochNanoseconds > Temporal::nsMaxInstant()) {
        // Return false.
        return false;
    }

    // Return true.
    return true;
}

TemporalDurationObject* Temporal::toTemporalDuration(ExecutionState& state, const Value& item)
{
    // If item is an Object and item has an [[InitializedTemporalDuration]] internal slot, then
    if (item.isObject() && item.asObject()->isTemporalDurationObject()) {
        // Return ! CreateTemporalDuration(item.[[Years]], item.[[Months]], item.[[Weeks]], item.[[Days]], item.[[Hours]], item.[[Minutes]], item.[[Seconds]], item.[[Milliseconds]], item.[[Microseconds]], item.[[Nanoseconds]]).
        return new TemporalDurationObject(state, item.asPointerValue()->asTemporalDurationObject()->duration());
    }

    constexpr auto msg = "The value you gave for ToTemporalDuration is invalid";
    if (!item.isObject()) {
        // If item is not an Object, then
        // If item is not a String, throw a TypeError exception.
        if (!item.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
        }
        // Return ? ParseTemporalDurationString(item).
        auto duration = ISO8601::Duration::parseDurationString(item.asString());
        if (!duration) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        return new TemporalDurationObject(state, duration.value());
    }

    // Let result be a new Partial Duration Record with each field set to 0.
    // Let partial be ? ToTemporalPartialDurationRecord(item).
    auto partial = TemporalDurationObject::toTemporalPartialDurationRecord(state, item);
    // If partial.[[Years]] is not undefined, set result.[[Years]] to partial.[[Years]].
    // If partial.[[Months]] is not undefined, set result.[[Months]] to partial.[[Months]].
    // If partial.[[Weeks]] is not undefined, set result.[[Weeks]] to partial.[[Weeks]].
    // If partial.[[Days]] is not undefined, set result.[[Days]] to partial.[[Days]].
    // If partial.[[Hours]] is not undefined, set result.[[Hours]] to partial.[[Hours]].
    // If partial.[[Minutes]] is not undefined, set result.[[Minutes]] to partial.[[Minutes]].
    // If partial.[[Seconds]] is not undefined, set result.[[Seconds]] to partial.[[Seconds]].
    // If partial.[[Milliseconds]] is not undefined, set result.[[Milliseconds]] to partial.[[Milliseconds]].
    // If partial.[[Microseconds]] is not undefined, set result.[[Microseconds]] to partial.[[Microseconds]].
    // If partial.[[Nanoseconds]] is not undefined, set result.[[Nanoseconds]] to partial.[[Nanoseconds]].

    ISO8601::Duration duration;
    size_t idx = 0;
    for (const auto& s : partial) {
        if (s.hasValue()) {
            duration[idx] = s.value();
        }
        idx++;
    }

    if (!duration.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }

    // Return ? CreateTemporalDuration(result.[[Years]], result.[[Months]], result.[[Weeks]], result.[[Days]], result.[[Hours]], result.[[Minutes]], result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]], result.[[Nanoseconds]]).
    return new TemporalDurationObject(state, duration);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
TemporalInstantObject* Temporal::toTemporalInstant(ExecutionState& state, Value item)
{
    // If item is an Object, then
    if (item.isObject()) {
        // If item has an [[InitializedTemporalInstant]] or [[InitializedTemporalZonedDateTime]] internal slot, then
        // TODO [[InitializedTemporalZonedDateTime]]
        if (item.asObject()->isTemporalInstantObject()) {
            // Return ! CreateTemporalInstant(item.[[EpochNanoseconds]]).
            return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(),
                                             item.asObject()->asTemporalInstantObject()->epochNanoseconds());
        }
        // NOTE: This use of ToPrimitive allows Instant-like objects to be converted.
        // Set item to ? ToPrimitive(item, string).
        item = item.toPrimitive(state, Value::PrimitiveTypeHint::PreferString);
    }
    constexpr auto msg = "The value you gave for ToTemporalInstant is invalid";
    if (!item.isString()) {
        // If item is not a String, throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
    }

    // Let parsed be ? ParseISODateTime(item, « TemporalInstantString »).
    // Assert: Either parsed.[[TimeZone]].[[OffsetString]] is not empty or parsed.[[TimeZone]].[[Z]] is true, but not both.
    // If parsed.[[TimeZone]].[[Z]] is true, let offsetNanoseconds be 0; otherwise, let offsetNanoseconds be ! ParseDateTimeUTCOffset(parsed.[[TimeZone]].[[OffsetString]]).
    // If parsed.[[Time]] is start-of-day, let time be MidnightTimeRecord(); else let time be parsed.[[Time]].
    // Let balanced be BalanceISODateTime(parsed.[[Year]], parsed.[[Month]], parsed.[[Day]], time.[[Hour]], time.[[Minute]], time.[[Second]], time.[[Millisecond]], time.[[Microsecond]], time.[[Nanosecond]] - offsetNanoseconds).
    // Perform ? CheckISODaysRange(balanced.[[ISODate]]).
    // Let epochNanoseconds be GetUTCEpochNanoseconds(balanced).
    // If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    auto parsed = ISO8601::parseISODateTimeWithInstantFormat(item.asString());
    if (!parsed || !parsed.value().isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    // Return ! CreateTemporalInstant(epochNanoseconds).
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), parsed.value().epochNanoseconds());
}

Optional<unsigned> Temporal::getTemporalFractionalSecondDigitsOption(ExecutionState& state, Optional<Object*> resolvedOptions)
{
    constexpr auto msg = "The value you gave for GetTemporalFractionalSecondDigitsOption is invalid";
    // Let digitsValue be ? Get(options, "fractionalSecondDigits").
    if (!resolvedOptions) {
        return NullOption;
    }
    Value digitsValue = resolvedOptions->get(state, state.context()->staticStrings().lazyFractionalSecondDigits()).value(state, resolvedOptions.value());
    // If digitsValue is undefined, return auto.
    if (digitsValue.isUndefined()) {
        return NullOption;
    }
    // If digitsValue is not a Number, then
    if (!digitsValue.isNumber()) {
        // If ? ToString(digitsValue) is not "auto", throw a RangeError exception.
        String* dv = digitsValue.toString(state);
        if (!dv->equals("auto")) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
        // Return auto.
        return NullOption;
    }
    double dv = digitsValue.asNumber();
    // If digitsValue is NaN, +∞𝔽, or -∞𝔽, throw a RangeError exception.
    if (std::isnan(dv) || std::isinf(dv)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    // Let digitCount be floor(ℝ(digitsValue)).
    double digitCount = floor(dv);
    // If digitCount < 0 or digitCount > 9, throw a RangeError exception.
    if (digitCount < 0 || digitCount > 9) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    // Return digitCount.
    return digitCount;
}

static ISO8601::RoundingMode toRoundingMode(String* roundingMode)
{
    if (roundingMode->equals("ceil")) {
        return ISO8601::RoundingMode::Ceil;
    } else if (roundingMode->equals("floor")) {
        return ISO8601::RoundingMode::Floor;
    } else if (roundingMode->equals("expand")) {
        return ISO8601::RoundingMode::Expand;
    } else if (roundingMode->equals("trunc")) {
        return ISO8601::RoundingMode::Trunc;
    } else if (roundingMode->equals("halfCeil")) {
        return ISO8601::RoundingMode::HalfCeil;
    } else if (roundingMode->equals("halfFloor")) {
        return ISO8601::RoundingMode::HalfFloor;
    } else if (roundingMode->equals("halfExpand")) {
        return ISO8601::RoundingMode::HalfExpand;
    } else if (roundingMode->equals("halfTrunc")) {
        return ISO8601::RoundingMode::HalfTrunc;
    }
    ASSERT(roundingMode->equals("halfEven"));
    return ISO8601::RoundingMode::HalfEven;
}

ISO8601::RoundingMode Temporal::getRoundingModeOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* fallback)
{
    // Let allowedStrings be the List of Strings from the "String Identifier" column of Table 27.
    // Let stringFallback be the value from the "String Identifier" column of the row with fallback in its "Rounding Mode" column.
    // Let stringValue be ? GetOption(options, "roundingMode", string, allowedStrings, stringFallback).
    // Return the value from the "Rounding Mode" column of the row with stringValue in its "String Identifier" column.
    if (!resolvedOptions) {
        return toRoundingMode(fallback);
    }

    Value values[] = {
        state.context()->staticStrings().ceil.string(),
        state.context()->staticStrings().floor.string(),
        state.context()->staticStrings().lazyExpand().string(),
        state.context()->staticStrings().trunc.string(),
        state.context()->staticStrings().lazyHalfCeil().string(),
        state.context()->staticStrings().lazyHalfFloor().string(),
        state.context()->staticStrings().lazyHalfExpand().string(),
        state.context()->staticStrings().lazyHalfTrunc().string(),
        state.context()->staticStrings().lazyHalfEven().string()
    };

    String* roundingMode = Intl::getOption(state, resolvedOptions.value(), state.context()->staticStrings().lazyRoundingMode().string(), Intl::OptionValueType::StringValue, values, 9, fallback).asString();
    return toRoundingMode(roundingMode);
}

Optional<TemporalUnit> Temporal::getTemporalUnitValuedOption(ExecutionState& state, Optional<Object*> resolvedOptions, String* key, Optional<Value> defaultValue)
{
    // Let allowedStrings be a List containing all values in the "Singular property name" and "Plural property name" columns of Table 21, except the header row.
    // Append "auto" to allowedStrings.
    // 3. NOTE: For each singular Temporal unit name that is contained within allowedStrings, the corresponding plural name is also contained within it.
    Value allowedStrings[] = {
#define DEFINE_STRING(name, Name, names, Names, index, category) \
    state.context()->staticStrings().lazy##Name().string(),      \
        state.context()->staticStrings().lazy##Names().string(),
        PLAIN_DATETIME_UNITS(DEFINE_STRING)
            state.context()
                ->staticStrings()
                .lazyAuto()
                .string()
    };
#undef DEFINE_STRING
    // If default is unset, then
    if (!defaultValue) {
        // Let defaultValue be undefined.
        defaultValue = Value();
    } else {
        // Else,
        // Let defaultValue be default.
    }

    // Let value be ? GetOption(options, key, string, allowedStrings, defaultValue).
    Value value;
    if (resolvedOptions) {
        value = Intl::getOption(state, resolvedOptions.value(), key, Intl::StringValue, allowedStrings, sizeof(allowedStrings) / sizeof(Value), defaultValue.value());
        if (value.isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Required options value is missing");
        }
    } else {
        if (defaultValue && defaultValue.value().isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Required options value is missing");
        }
        value = defaultValue.value();
    }
    // If value is undefined, return unset.
    if (value.isUndefined()) {
        return NullOption;
    }
    String* stringValue = value.asString();
    // If value is "auto", return auto.
    if (stringValue->equals("auto")) {
        return TemporalUnit::Auto;
    }
    // Return the value in the "Value" column of Table 21 corresponding to the row with value in its "Singular property name" or "Plural property name" column.
    if (false) {}
#define DEFINE_COMPARE(name, Name, names, Names, index, category) \
    else if (stringValue->equals(#name))                          \
    {                                                             \
        return TemporalUnit::Name;                                \
    }                                                             \
    else if (stringValue->equals(#names))                         \
    {                                                             \
        return TemporalUnit::Name;                                \
    }
    PLAIN_DATETIME_UNITS(DEFINE_COMPARE)
#undef DEFINE_COMPARE

    ASSERT_NOT_REACHED();
    return TemporalUnit::Auto;
}

void Temporal::validateTemporalUnitValue(ExecutionState& state, Optional<TemporalUnit> value, ISO8601::DateTimeUnitCategory unitGroup, Optional<TemporalUnit*> extraValues, size_t extraValueSize)
{
    // If value is unset, return unused.
    if (!value) {
        return;
    }
    // If extraValues is present and extraValues contains value, return unused.
    if (extraValues && value) {
        for (size_t i = 0; i < extraValueSize; i++) {
            if (extraValues.value()[i] == value.value()) {
                return;
            }
        }
    }
    ISO8601::DateTimeUnitCategory categoryValue = ISO8601::DateTimeUnitCategory::DateTime;
    const auto* msg = "Invalid temporal unit value";
    // Let category be the value in the “Category” column of the row of Table 21 whose “Value” column contains value. If there is no such row, throw a RangeError exception.
    if (false) {}
#define DEFINE_COMPARE(name, Name, names, Names, index, category)          \
    else if (toDateTimeUnit(value.value()) == ISO8601::DateTimeUnit::Name) \
    {                                                                      \
        categoryValue = category;                                          \
    }
    PLAIN_DATETIME_UNITS(DEFINE_COMPARE)
#undef DEFINE_COMPARE
    else
    {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }

    // If category is date and unitGroup is date or datetime, return unused.
    if (categoryValue == ISO8601::DateTimeUnitCategory::Date && (unitGroup == ISO8601::DateTimeUnitCategory::Date || unitGroup == ISO8601::DateTimeUnitCategory::DateTime)) {
        return;
    }
    // If category is time and unitGroup is time or datetime, return unused.
    if (categoryValue == ISO8601::DateTimeUnitCategory::Time && (unitGroup == ISO8601::DateTimeUnitCategory::Time || unitGroup == ISO8601::DateTimeUnitCategory::DateTime)) {
        return;
    }
    // Throw a RangeError exception.
    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
}

TimeZone Temporal::toTemporalTimezoneIdentifier(ExecutionState& state, const Value& temporalTimeZoneLike)
{
    // TODO If temporalTimeZoneLike is an Object, then
    // TODO   If temporalTimeZoneLike has an [[InitializedTemporalZonedDateTime]] internal slot, then
    // TODO     Return temporalTimeZoneLike.[[TimeZone]].

    // If temporalTimeZoneLike is not a String, throw a TypeError exception.
    if (!temporalTimeZoneLike.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "temporalTimeZoneLike is not String");
    }

    String* temporalTimeZoneLikeString = temporalTimeZoneLike.asString();
    // Let parseResult be ? ParseTemporalTimeZoneString(temporalTimeZoneLike).
    // Let offsetMinutes be parseResult.[[OffsetMinutes]].
    // If offsetMinutes is not empty, return FormatOffsetTimeZoneIdentifier(offsetMinutes).
    // Let name be parseResult.[[Name]].
    // Let timeZoneIdentifierRecord be GetAvailableNamedTimeZoneIdentifier(name).
    // If timeZoneIdentifierRecord is empty, throw a RangeError exception.
    // Return timeZoneIdentifierRecord.[[Identifier]].
    Optional<int64_t> utcOffset = ISO8601::parseUTCOffset(temporalTimeZoneLikeString, false);
    if (utcOffset) {
        return TimeZone(utcOffset.value());
    }

    Optional<ISO8601::TimeZoneID> identifier = ISO8601::parseTimeZoneName(temporalTimeZoneLikeString);
    if (identifier) {
        return TimeZone(identifier.value());
    }

    // Optional<std::tuple<PlainDate, Optional<PlainTime>, Optional<TimeZoneRecord>, Optional<CalendarID>>>
    auto complexTimeZone = ISO8601::parseCalendarDateTime(temporalTimeZoneLikeString, false);
    if (complexTimeZone && std::get<2>(complexTimeZone.value())) {
        ISO8601::TimeZoneRecord record = std::get<2>(complexTimeZone.value()).value();
        if (record.m_z) {
            return TimeZone(int64_t(0));
        } else if (record.m_nameOrOffset && record.m_nameOrOffset.id().value() == 0) {
            const std::string& s = record.m_nameOrOffset.get<0>();
            return TimeZone(String::fromUTF8(s.data(), s.length()));
        } else if (record.m_nameOrOffset && record.m_nameOrOffset.id().value() == 1) {
            return TimeZone(record.m_nameOrOffset.get<1>());
        } else if (record.m_offset) {
            return TimeZone(record.m_offset.value());
        }
    }

    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid timeZone string");
    return TimeZone(String::emptyString());
}

Temporal::StringPrecisionRecord Temporal::toSecondsStringPrecisionRecord(ExecutionState& state, Optional<ISO8601::DateTimeUnit> smallestUnit, Optional<unsigned> fractionalDigitCount)
{
    if (smallestUnit) {
        // If smallestUnit is minute, then
        if (smallestUnit.value() == ISO8601::DateTimeUnit::Minute) {
            // Return the Record { [[Precision]]: minute, [[Unit]]: minute, [[Increment]]: 1  }.
            return { state.context()->staticStrings().lazyMinute().string(), ISO8601::DateTimeUnit::Minute, 1 };
        } else if (smallestUnit.value() == ISO8601::DateTimeUnit::Second) {
            // If smallestUnit is second, then
            // Return the Record { [[Precision]]: 0, [[Unit]]: second, [[Increment]]: 1  }.
            return { Value(0), ISO8601::DateTimeUnit::Second, 1 };
        } else if (smallestUnit.value() == ISO8601::DateTimeUnit::Millisecond) {
            // If smallestUnit is millisecond, then
            // Return the Record { [[Precision]]: 3, [[Unit]]: millisecond, [[Increment]]: 1  }.
            return { Value(3), ISO8601::DateTimeUnit::Millisecond, 1 };
        } else if (smallestUnit.value() == ISO8601::DateTimeUnit::Microsecond) {
            // If smallestUnit is microsecond, then
            // Return the Record { [[Precision]]: 6, [[Unit]]: microsecond, [[Increment]]: 1  }.
            return { Value(6), ISO8601::DateTimeUnit::Microsecond, 1 };
        } else if (smallestUnit.value() == ISO8601::DateTimeUnit::Nanosecond) {
            // If smallestUnit is nanosecond, then
            // Return the Record { [[Precision]]: 9, [[Unit]]: nanosecond, [[Increment]]: 1  }.
            return { Value(9), ISO8601::DateTimeUnit::Nanosecond, 1 };
        }
    }
    // Assert: smallestUnit is unset.
    ASSERT(!smallestUnit.hasValue());
    // If fractionalDigitCount is auto, then
    if (!fractionalDigitCount.hasValue()) {
        // Return the Record { [[Precision]]: auto, [[Unit]]: nanosecond, [[Increment]]: 1  }.
        return { state.context()->staticStrings().lazyAuto().string(), ISO8601::DateTimeUnit::Nanosecond, 1 };
    }

    auto pow10Unsigned = [](unsigned n) -> unsigned {
        unsigned result = 1;
        for (unsigned i = 0; i < n; ++i)
            result *= 10;
        return result;
    };

    // If fractionalDigitCount = 0, then
    if (fractionalDigitCount && fractionalDigitCount.value() == 0) {
        // Return the Record { [[Precision]]: 0, [[Unit]]: second, [[Increment]]: 1  }.
        return { Value(0), ISO8601::DateTimeUnit::Second, 1 };
    } else if (fractionalDigitCount && fractionalDigitCount.value() >= 1 && fractionalDigitCount.value() <= 3) {
        // If fractionalDigitCount is in the inclusive interval from 1 to 3, then
        // Return the Record { [[Precision]]: fractionalDigitCount, [[Unit]]: millisecond, [[Increment]]: 10**(3 - fractionalDigitCount)  }
        return { Value(fractionalDigitCount.value()), ISO8601::DateTimeUnit::Millisecond, pow10Unsigned(3 - fractionalDigitCount.value()) };
    } else if (fractionalDigitCount && fractionalDigitCount.value() >= 4 && fractionalDigitCount.value() <= 6) {
        // If fractionalDigitCount is in the inclusive interval from 4 to 6, then
        // Return the Record { [[Precision]]: fractionalDigitCount, [[Unit]]: microsecond, [[Increment]]: 10**(6 - fractionalDigitCount)  }.
        return { Value(fractionalDigitCount.value()), ISO8601::DateTimeUnit::Microsecond, pow10Unsigned(6 - fractionalDigitCount.value()) };
    }
    // Assert: fractionalDigitCount is in the inclusive interval from 7 to 9.
    ASSERT(fractionalDigitCount && fractionalDigitCount.value() >= 7 && fractionalDigitCount.value() <= 9);
    // Return the Record { [[Precision]]: fractionalDigitCount, [[Unit]]: nanosecond, [[Increment]]: 10**(9 - fractionalDigitCount)  }.
    return { Value(fractionalDigitCount.value()), ISO8601::DateTimeUnit::Nanosecond, pow10Unsigned(9 - fractionalDigitCount.value()) };
}

void Temporal::validateTemporalRoundingIncrement(ExecutionState& state, unsigned increment, Int128 dividend, bool inclusive)
{
    // If inclusive is true, then
    Int128 maximum;
    if (inclusive) {
        // Let maximum be dividend.
        maximum = dividend;
    } else {
        // Else,
        // Assert: dividend > 1.
        // Let maximum be dividend - 1.
        maximum = dividend - 1;
    }

    // If increment > maximum, throw a RangeError exception.
    if (increment > maximum) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid increment value");
    }
    // If dividend modulo increment ≠ 0, then
    if (dividend % increment) {
        // Throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid dividend value");
    }
    // Return unused.
}

unsigned Temporal::getRoundingIncrementOption(ExecutionState& state, Optional<Object*> options)
{
    if (!options) {
        return 1;
    }
    // Let value be ? Get(options, "roundingIncrement").
    Value value = options.value()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyRoundingIncrement())).value(state, options.value());
    // If value is undefined, return 1𝔽.
    if (value.isUndefined()) {
        return 1;
    }
    // Let integerIncrement be ? ToIntegerWithTruncation(value).
    auto integerIncrement = value.toIntegerWithTruncation(state);
    // If integerIncrement < 1 or integerIncrement > 10**9, throw a RangeError exception.
    if (integerIncrement < 1 || integerIncrement > 1000000000) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid roundingIncrement value");
    }
    // Return integerIncrement.
    return integerIncrement;
}

Temporal::DifferenceSettingsRecord Temporal::getDifferenceSettings(ExecutionState& state, bool isSinceOperation, Optional<Object*> options, ISO8601::DateTimeUnitCategory unitGroup,
                                                                   Optional<TemporalUnit*> disallowedUnits, size_t disallowedUnitsLength, TemporalUnit fallbackSmallestUnit, TemporalUnit smallestLargestDefaultUnit)
{
    // NOTE: The following steps read options and perform independent validation in alphabetical order.
    // Let largestUnit be ? GetTemporalUnitValuedOption(options, "largestUnit", unset).
    Optional<TemporalUnit> largestUnit = Temporal::getTemporalUnitValuedOption(state, options, state.context()->staticStrings().lazyLargestUnit().string(), NullOption);
    // Let roundingIncrement be ? GetRoundingIncrementOption(options).
    auto roundingIncrement = Temporal::getRoundingIncrementOption(state, options);
    // Let roundingMode be ? GetRoundingModeOption(options, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, options, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(options, "smallestUnit", unset).
    Optional<TemporalUnit> smallestUnit = Temporal::getTemporalUnitValuedOption(state, options, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Perform ? ValidateTemporalUnitValue(largestUnit, unitGroup, « auto »).
    TemporalUnit extraValues[1] = { TemporalUnit::Auto };
    Temporal::validateTemporalUnitValue(state, largestUnit, unitGroup, extraValues, 1);
    // If largestUnit is unset, then
    if (!largestUnit) {
        // Set largestUnit to auto.
        largestUnit = TemporalUnit::Auto;
    }

    // If disallowedUnits contains largestUnit, throw a RangeError exception.
    if (disallowedUnits) {
        for (size_t i = 0; i < disallowedUnitsLength; i++) {
            if (disallowedUnits.value()[i] == largestUnit.value()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid largestUnit value");
            }
        }
    }
    // If operation is since, then
    if (isSinceOperation) {
        // Set roundingMode to NegateRoundingMode(roundingMode).
        roundingMode = Temporal::negateRoundingMode(state, roundingMode);
    }

    // Perform ? ValidateTemporalUnitValue(smallestUnit, unitGroup).
    Temporal::validateTemporalUnitValue(state, smallestUnit, unitGroup, nullptr, 0);
    // If smallestUnit is unset, then
    if (!smallestUnit) {
        // Set smallestUnit to fallbackSmallestUnit.
        smallestUnit = fallbackSmallestUnit;
    }

    // If disallowedUnits contains smallestUnit, throw a RangeError exception.
    if (disallowedUnits) {
        for (size_t i = 0; i < disallowedUnitsLength; i++) {
            if (disallowedUnits.value()[i] == smallestUnit.value()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid largestUnit value");
            }
        }
    }
    // Let defaultLargestUnit be LargerOfTwoTemporalUnits(smallestLargestDefaultUnit, smallestUnit).
    auto defaultLargestUnit = Temporal::largerOfTwoTemporalUnits(toDateTimeUnit(smallestLargestDefaultUnit), toDateTimeUnit(smallestUnit.value()));
    // If largestUnit is auto, set largestUnit to defaultLargestUnit.
    if (largestUnit == TemporalUnit::Auto) {
        largestUnit = static_cast<TemporalUnit>(defaultLargestUnit);
    }
    // If LargerOfTwoTemporalUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
    if (!(Temporal::largerOfTwoTemporalUnits(toDateTimeUnit(largestUnit.value()), toDateTimeUnit(smallestUnit.value())) == toDateTimeUnit(largestUnit.value()))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid largestUnit or smallestUnit value");
    }
    // Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
    auto maximum = Temporal::maximumTemporalDurationRoundingIncrement(toDateTimeUnit(smallestUnit.value()));
    // If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
    if (maximum) {
        Temporal::validateTemporalRoundingIncrement(state, roundingIncrement, maximum.value(), false);
    }
    // Return the Record { [[SmallestUnit]]: smallestUnit, [[LargestUnit]]: largestUnit, [[RoundingMode]]: roundingMode, [[RoundingIncrement]]: roundingIncrement,  }.
    return { toDateTimeUnit(smallestUnit.value()), toDateTimeUnit(largestUnit.value()), roundingMode, roundingIncrement };
}

ISO8601::RoundingMode Temporal::negateRoundingMode(ExecutionState& state, ISO8601::RoundingMode roundingMode)
{
    // If roundingMode is ceil, return floor.
    if (roundingMode == ISO8601::RoundingMode::Ceil) {
        return ISO8601::RoundingMode::Floor;
    }
    // If roundingMode is floor, return ceil.
    if (roundingMode == ISO8601::RoundingMode::Floor) {
        return ISO8601::RoundingMode::Ceil;
    }
    // If roundingMode is half-ceil, return half-floor.
    if (roundingMode == ISO8601::RoundingMode::HalfCeil) {
        return ISO8601::RoundingMode::HalfFloor;
    }
    // If roundingMode is half-floor, return half-ceil.
    if (roundingMode == ISO8601::RoundingMode::HalfFloor) {
        return ISO8601::RoundingMode::HalfCeil;
    }
    // Return roundingMode
    return roundingMode;
}

ISO8601::DateTimeUnit Temporal::largerOfTwoTemporalUnits(ISO8601::DateTimeUnit u1, ISO8601::DateTimeUnit u2)
{
    // For each row of Table 21, except the header row, in table order, do
    //   Let unit be the value in the "Value" column of the row.
    //   If u1 is unit, return unit.
    //   If u2 is unit, return unit.
    if (false) {}
#define DEFINE_COMPARE(name, Name, names, Names, index, category) \
    else if (u1 == ISO8601::DateTimeUnit::Name)                   \
    {                                                             \
        return u1;                                                \
    }                                                             \
    else if (u2 == ISO8601::DateTimeUnit::Name)                   \
    {                                                             \
        return u2;                                                \
    }
    PLAIN_DATETIME_UNITS(DEFINE_COMPARE)
#undef DEFINE_COMPARE
    else
    {
        ASSERT_NOT_REACHED();
        return ISO8601::DateTimeUnit::Year;
    }
}

Optional<unsigned> Temporal::maximumTemporalDurationRoundingIncrement(ISO8601::DateTimeUnit unit)
{
    // Return the value from the "Maximum duration rounding increment" column of the row of Table 21 in which unit is in the "Value" column.
    if (unit == ISO8601::DateTimeUnit::Hour) {
        return 24;
    } else if (unit == ISO8601::DateTimeUnit::Minute) {
        return 60;
    } else if (unit == ISO8601::DateTimeUnit::Second) {
        return 60;
    } else if (unit == ISO8601::DateTimeUnit::Millisecond) {
        return 1000;
    } else if (unit == ISO8601::DateTimeUnit::Microsecond) {
        return 1000;
    } else if (unit == ISO8601::DateTimeUnit::Nanosecond) {
        return 1000;
    }
    return NullOption;
}

Int128 Temporal::timeDurationFromEpochNanosecondsDifference(Int128 one, Int128 two)
{
    // Let result be ℝ(one) - ℝ(two).
    auto result = one - two;
    // Assert: abs(result) ≤ maxTimeDuration.
    ASSERT(std::abs(result) < ISO8601::InternalDuration::maxTimeDuration);
    // Return result.
    return result;
}

bool Temporal::isCalendarUnit(ISO8601::DateTimeUnit unit)
{
    if (unit == ISO8601::DateTimeUnit::Year || unit == ISO8601::DateTimeUnit::Month || unit == ISO8601::DateTimeUnit::Week) {
        return true;
    }
    return false;
}

bool Temporal::isValidTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond)
{
    if (hour < 0 || hour > 23) {
        return false;
    }
    if (minute < 0 || minute > 59) {
        return false;
    }
    if (second < 0 || second > 59) {
        return false;
    }
    if (millisecond < 0 || millisecond > 999) {
        return false;
    }
    if (microsecond < 0 || microsecond > 999) {
        return false;
    }
    if (nanosecond < 0 || nanosecond > 999) {
        return false;
    }
    return true;
}

TemporalOverflowOption Temporal::getTemporalOverflowOption(ExecutionState& state, Optional<Object*> options)
{
    // Let stringValue be ? GetOption(options, "overflow", string, « "constrain", "reject" », "constrain").
    String* stringValue = state.context()->staticStrings().lazyConstrain().string();
    if (options) {
        Value values[2] = { state.context()->staticStrings().lazyConstrain().string(),
                            state.context()->staticStrings().lazyReject().string() };
        stringValue = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyOverflow().string(), Intl::StringValue,
                                      values, 2, state.context()->staticStrings().lazyConstrain().string())
                          .asString();
    }
    // If stringValue is "constrain", return constrain.
    if (stringValue->equals("constrain")) {
        return TemporalOverflowOption::Constrain;
    }
    // Return reject.
    return TemporalOverflowOption::Reject;
}

bool Temporal::isPartialTemporalObject(ExecutionState& state, Value value)
{
    // If value is not an Object, return false.
    if (!value.isObject()) {
        return false;
    }
    // TODO If value has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalTime]],
    // [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, return false.
    if (value.asObject()->isTemporalPlainTimeObject()) {
        return false;
    }
    // Let calendarProperty be ? Get(value, "calendar").
    Value calendarProperty = value.asObject()->get(state, state.context()->staticStrings().lazyCalendar()).value(state, value);
    // If calendarProperty is not undefined, return false.
    if (!calendarProperty.isUndefined()) {
        return false;
    }
    // Let timeZoneProperty be ? Get(value, "timeZone").
    Value timeZoneProperty = value.asObject()->get(state, state.context()->staticStrings().lazyTimeZone()).value(state, value);
    // If timeZoneProperty is not undefined, return false.
    if (!timeZoneProperty.isUndefined()) {
        return false;
    }
    // Return true.
    return true;
}

static double nonNegativeModulo(double x, double y)
{
    double result = std::fmod(x, y);
    if (!result)
        return 0;
    if (result < 0)
        result += y;
    return result;
}

static double nonNegativeModulo(int64_t x, int64_t y)
{
    int64_t result = x % y;
    if (!result)
        return 0;
    if (result < 0)
        result += y;
    return result;
}

static int64_t int64Floor(int64_t x, int64_t y)
{
    if (x > 0) {
        return x / y;
    }
    if (x % y) {
        return x / y - 1;
    } else {
        return x / y;
    }
}

ISO8601::Duration Temporal::balanceTime(double hour, double minute, double second, double millisecond, double microsecond, double nanosecond)
{
    microsecond += std::floor(nanosecond / 1000);
    nanosecond = nonNegativeModulo(nanosecond, 1000);
    millisecond += std::floor(microsecond / 1000);
    microsecond = nonNegativeModulo(microsecond, 1000);
    second += std::floor(millisecond / 1000);
    millisecond = nonNegativeModulo(millisecond, 1000);
    minute += std::floor(second / 60);
    second = nonNegativeModulo(second, 60);
    hour += std::floor(minute / 60);
    minute = nonNegativeModulo(minute, 60);
    double days = std::floor(hour / 24);
    hour = nonNegativeModulo(hour, 24);
    return ISO8601::Duration({ 0.0, 0.0, 0.0, days, hour, minute, second, millisecond, microsecond, nanosecond });
}

ISO8601::Duration Temporal::balanceTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond)
{
    microsecond += int64Floor(nanosecond, 1000);
    nanosecond = nonNegativeModulo(nanosecond, 1000);
    millisecond += int64Floor(microsecond, 1000);
    microsecond = nonNegativeModulo(microsecond, 1000);
    second += int64Floor(millisecond, 1000);
    millisecond = nonNegativeModulo(millisecond, 1000);
    minute += int64Floor(second, 60);
    second = nonNegativeModulo(second, 60);
    hour += int64Floor(minute, 60);
    minute = nonNegativeModulo(minute, 60);
    int64_t days = int64Floor(hour, 24);
    hour = nonNegativeModulo(hour, 24);
    return ISO8601::Duration({ int64_t(0), int64_t(0), int64_t(0), days, hour, minute, second, millisecond, microsecond, nanosecond });
}

TemporalObject::TemporalObject(ExecutionState& state)
    : TemporalObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalObject::TemporalObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, const ISODate& date, String* calendar)
    : TemporalObject(state, proto)
    , m_calendar(calendar)
    , m_date(date)
{
}

void* TemporalPlainDateObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(TemporalPlainDateObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(TemporalPlainDateObject, m_calendar));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(TemporalPlainDateObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

} // namespace Escargot

#endif
