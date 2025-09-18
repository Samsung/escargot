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
#include "runtime/TemporalPlainTimeObject.h"
#include "runtime/TemporalPlainDateObject.h"
#include "intl/Intl.h"

namespace Escargot {

void Calendar::lookupICUEra(ExecutionState& state, const std::function<bool(size_t idx, const std::string& icuEra)>& fn) const
{
    std::string s = "root/calendar/";
    s += toICUString();
    s += "/eras/abbreviated";
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle* t = nullptr;
    t = ures_findResource(s.data(), t, &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Internal ICU error");
    }

    size_t siz = ures_getSize(t);
    for (size_t i = 0; i < siz; i++) {
        int32_t len = 0;
        auto u16 = ures_getStringByIndex(t, i, &len, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Internal ICU error");
        }
        UTF16StringFromExternalMemory u16Str(u16, len);
        const UNormalizer2* normalizer = unorm2_getNFDInstance(&status);
        if (!normalizer || U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "normalization fails");
        }
        int32_t normalizedStringLength = unorm2_normalize(normalizer, u16, len, nullptr, 0, &status);

        if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
            // when normalize fails.
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "normalization fails");
        }
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(normalizedStringLength);
        status = U_ZERO_ERROR;
        unorm2_normalize(normalizer, u16, len, (UChar*)ret.data(), normalizedStringLength, &status);

        std::string icuString;
        for (int32_t i = 0; i < normalizedStringLength; i++) {
            if (ret[i] < 128) {
                icuString.push_back(tolower(ret[i]));
            }
            if (ret[i] == ' ') {
                break;
            }
        }

        if (fn(i, icuString)) {
            break;
        }
    }
    ures_close(t);
}

bool Calendar::isEraRelated() const
{
    switch (m_id) {
    case ID::Gregorian:
    case ID::Islamic:
    case ID::IslamicCivil:
    case ID::IslamicCivilLegacy:
    case ID::IslamicRGSA:
    case ID::IslamicTabular:
    case ID::IslamicUmmAlQura:
    case ID::ROC:
        return true;
    case ID::Japanese:
        return true;
    default:
        return false;
    }
}

bool Calendar::shouldUseICUExtendedYear() const
{
    if (id() == Calendar::ID::Dangi || id() == Calendar::ID::Japanese || id() == Calendar::ID::Chinese) {
        return true;
    }
    if (isEraRelated()) {
        return true;
    }
    return false;
}

Optional<Calendar> Calendar::fromString(ISO8601::CalendarID id)
{
    auto u = id;
    std::transform(u.begin(), u.end(), u.begin(), tolower);
    if (false) {}
#define DEFINE_FIELD(name, string, icuString, fullName) \
    else if (u == string || u == icuString)             \
    {                                                   \
        return Calendar(ID::name);                      \
    }
    CALENDAR_ID_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD

    return NullOption;
}

Optional<Calendar> Calendar::fromString(String* str)
{
    return fromString(str->toNonGCUTF8StringData());
}

String* Calendar::toString() const
{
    switch (m_id) {
#define DEFINE_FIELD(name, string, icuString, fullName) \
    case ID::name:                                      \
        return new ASCIIStringFromExternalMemory(fullName);
        CALENDAR_ID_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD
    default:
        ASSERT_NOT_REACHED();
    }
    return new ASCIIStringFromExternalMemory("iso8601");
}

std::string Calendar::toICUString() const
{
    switch (m_id) {
#define DEFINE_FIELD(name, string, icuString, fullName) \
    case ID::name:                                      \
        return icuString;
        CALENDAR_ID_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD
    default:
        ASSERT_NOT_REACHED();
    }
    return "iso8601";
}

UCalendar* Calendar::createICUCalendar(ExecutionState& state)
{
    std::string calName = "en@calendar=" + toICUString();
    UErrorCode status = U_ZERO_ERROR;
    auto icuCalendar = ucal_open(u"GMT", 3, calName.data(), UCalendarType::UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize ICU calendar");
    }
    ucal_setMillis(icuCalendar, 0, &status);
    return icuCalendar;
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

int32_t Temporal::computeTimeZoneOffset(ExecutionState& state, String* name, int64_t epoch)
{
    auto u16 = name->toUTF16StringData();
    UErrorCode status = U_ZERO_ERROR;
    const char* msg = "Failed to get timeZone offset from ICU";
    auto ucalendar = ucal_open(u16.data(), u16.length(), "en", UCAL_GREGORIAN, &status);
    if (!ucalendar) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        return 0;
    }

    ucal_setMillis(ucalendar, epoch, &status);
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
    return zoneOffset + dstOffset;
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

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
TemporalPlainTimeObject* Temporal::toTemporalTime(ExecutionState& state, Value item, Value options)
{
    ISO8601::PlainTime result;
    // If options is not present, set options to undefined.
    // If item is an Object, then
    if (item.isObject()) {
        // If item has an [[InitializedTemporalTime]] internal slot, then
        if (item.asObject()->isTemporalPlainTimeObject()) {
            // Let resolvedOptions be ? GetOptionsObject(options).
            auto resolvedOptions = Intl::getOptionsObject(state, options);
            // Perform ? GetTemporalOverflowOption(resolvedOptions).
            Temporal::getTemporalOverflowOption(state, resolvedOptions);
            // Return ! CreateTemporalTime(item.[[Time]]).
            return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(),
                                               item.asObject()->asTemporalPlainTimeObject()->plainTime());
        }
        // TODO If item has an [[InitializedTemporalZonedDateTime]] internal slot, then...
        // TODO If item has an [[InitializedTemporalDateTime]] internal slot, then...
        // Let result be ? ToTemporalTimeRecord(item).
        auto resultRecord = TemporalPlainTimeObject::toTemporalTimeRecord(state, item, NullOption);
        // Let resolvedOptions be ? GetOptionsObject(options).
        auto resolvedOptions = Intl::getOptionsObject(state, options);
        // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
        // Set result to ? RegulateTime(result.[[Hour]], result.[[Minute]], result.[[Second]], result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]], overflow).
        result = TemporalPlainTimeObject::regulateTime(state, resultRecord.hour().value(), resultRecord.minute().value(), resultRecord.second().value(), resultRecord.millisecond().value(),
                                                       resultRecord.microsecond().value(), resultRecord.nanosecond().value(), overflow);
    } else {
        // Else,
        // If item is not a String, throw a TypeError exception.
        if (!item.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ToTemporalTime needs Object or String");
        }
        // Let parseResult be ? ParseISODateTime(item, « TemporalTimeString »).
        auto parseDateTimeResult = ISO8601::parseCalendarDateTime(item.asString());
        auto parseTimeResult = ISO8601::parseTime(item.asString());
        // If ParseText(StringToCodePoints(item), AmbiguousTemporalTimeString) is a Parse Node, throw a RangeError exception.
        // Assert: parseResult.[[Time]] is not start-of-day.
        if (!parseTimeResult && (!parseDateTimeResult || !std::get<1>(parseDateTimeResult.value()))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalTime needs ISO Time string");
        }
        if ((parseDateTimeResult && std::get<2>(parseDateTimeResult.value()) && std::get<2>(parseDateTimeResult.value()).value().m_z) || (parseTimeResult && std::get<1>(parseTimeResult.value()) && std::get<1>(parseTimeResult.value()).value().m_z)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalTime needs ISO Time string without UTC designator");
        }
        // Set result to parseResult.[[Time]].
        if (parseDateTimeResult && std::get<1>(parseDateTimeResult.value())) {
            result = std::get<1>(parseDateTimeResult.value()).value();
        } else {
            result = std::get<0>(parseTimeResult.value());
        }
        // Let resolvedOptions be ? GetOptionsObject(options).
        auto resolvedOptions = Intl::getOptionsObject(state, options);
        // Perform ? GetTemporalOverflowOption(resolvedOptions).
        Temporal::getTemporalOverflowOption(state, resolvedOptions);
    }
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(), result);
}

TemporalPlainDateObject* Temporal::toTemporalDate(ExecutionState& state, Value item, Value options)
{
    // If options is not present, set options to undefined.
    // If item is an Object, then
    if (item.isObject()) {
        // If item has an [[InitializedTemporalDate]] internal slot, then
        if (item.asObject()->isTemporalPlainDateObject()) {
            // Let resolvedOptions be ? GetOptionsObject(options).
            auto resolvedOptions = Intl::getOptionsObject(state, options);
            // Perform ? GetTemporalOverflowOption(resolvedOptions).
            Temporal::getTemporalOverflowOption(state, resolvedOptions);
            // Return ! CreateTemporalDate(item.[[ISODate]], item.[[Calendar]]).
            return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                               item.asObject()->asTemporalPlainDateObject()->plainDate(), item.asObject()->asTemporalPlainDateObject()->calendarID());
        }
        // TODO If item has an [[InitializedTemporalZonedDateTime]] internal slot, then...
        // TODO If item has an [[InitializedTemporalDateTime]] internal slot, then...

        // Let calendar be ? GetTemporalCalendarIdentifierWithISODefault(item).
        auto calendar = Temporal::getTemporalCalendarIdentifierWithISODefault(state, item);
        // Let fields be ? PrepareCalendarFields(calendar, item, « year, month, month-code, day », «», «»).
        CalendarField f[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
        auto fields = prepareCalendarFields(state, calendar, item.asObject(), f, 4, nullptr, 0, nullptr, 0);
        // Let resolvedOptions be ? GetOptionsObject(options).
        auto resolvedOptions = Intl::getOptionsObject(state, options);
        // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
        // Let isoDate be ? CalendarDateFromFields(calendar, fields, overflow).
        auto isoDate = calendarDateFromFields(state, calendar, fields, overflow);
        // Return ! CreateTemporalDate(isoDate, calendar).
        return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                           isoDate, calendar);
    }

    // If item is not a String, throw a TypeError exception.
    if (!item.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ToTemporalDate needs Object or String");
    }
    // Let result be ? ParseISODateTime(item, « TemporalDateTimeString[~Zoned] »).
    ISO8601::DateTimeParseOption option;
    option.allowTimeZoneTimeWithoutTime = true;
    auto result = ISO8601::parseCalendarDateTime(item.asString(), option);
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalDate needs ISO Date string");
    }
    if (std::get<2>(result.value()) && std::get<2>(result.value()).value().m_z) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalDate needs ISO Time string without UTC designator");
    }
    if (!std::get<1>(result.value()) && std::get<2>(result.value()) && std::get<2>(result.value()).value().m_offset.hasValue() && !std::get<3>(result.value())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalDate needs ISO Date string");
    }
    if (!ISO8601::isDateTimeWithinLimits(std::get<0>(result.value()).year(), std::get<0>(result.value()).month(), std::get<0>(result.value()).day(), 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalDate needs ISO Date string in valid range");
    }

    // Let calendar be result.[[Calendar]].
    // If calendar is empty, set calendar to "iso8601".
    String* calendar = state.context()->staticStrings().lazyISO8601().string();
    if (std::get<3>(result.value())) {
        calendar = String::fromUTF8(std::get<3>(result.value()).value().data(), std::get<3>(result.value()).value().length());
    }
    // Set calendar to ? CanonicalizeCalendar(calendar).
    auto mayID = Calendar::fromString(calendar);
    if (!mayID) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid CalendarID");
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Perform ? GetTemporalOverflowOption(resolvedOptions).
    Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let isoDate be CreateISODateRecord(result.[[Year]], result.[[Month]], result.[[Day]]).
    // Return ? CreateTemporalDate(isoDate, calendar).
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                       std::get<0>(result.value()), mayID.value());
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
    ISO8601::DateTimeParseOption option;
    option.parseSubMinutePrecisionForTimeZone = false;
    Optional<int64_t> utcOffset = ISO8601::parseUTCOffset(temporalTimeZoneLikeString, option);
    if (utcOffset) {
        return TimeZone(utcOffset.value());
    }

    Optional<ISO8601::TimeZoneID> identifier = ISO8601::parseTimeZoneName(temporalTimeZoneLikeString);
    if (identifier) {
        return TimeZone(identifier.value());
    }

    auto complexTimeZone = ISO8601::parseCalendarDateTime(temporalTimeZoneLikeString, option);
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

Int128 Temporal::differenceTime(ISO8601::PlainTime time1, ISO8601::PlainTime time2)
{
    // Let hours be time2.[[Hour]] - time1.[[Hour]].
    // Let minutes be time2.[[Minute]] - time1.[[Minute]].
    // Let seconds be time2.[[Second]] - time1.[[Second]].
    // Let milliseconds be time2.[[Millisecond]] - time1.[[Millisecond]].
    // Let microseconds be time2.[[Microsecond]] - time1.[[Microsecond]].
    // Let nanoseconds be time2.[[Nanosecond]] - time1.[[Nanosecond]].
#define DEFINE_DIFF(name, capitalizedName) \
    int32_t name##s = time2.name() - time1.name();
    PLAIN_TIME_UNITS(DEFINE_DIFF)
#undef DEFINE_DIFF
    // Let timeDuration be TimeDurationFromComponents(hours, minutes, seconds, milliseconds, microseconds, nanoseconds).
    // Assert: abs(timeDuration) < nsPerDay.
    // Return timeDuration.
    return timeDurationFromComponents(hours, minutes, seconds, milliseconds, microseconds, nanoseconds);
}

Int128 Temporal::timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds)
{
    // Set minutes to minutes + hours × 60.
    // Set seconds to seconds + minutes × 60.
    // Set milliseconds to milliseconds + seconds × 1000.
    // Set microseconds to microseconds + milliseconds × 1000.
    // Set nanoseconds to nanoseconds + microseconds × 1000.
    // Assert: abs(nanoseconds) ≤ maxTimeDuration.
    // Return nanoseconds.
    return ISO8601::Duration({ 0.0, 0.0, 0.0, 0.0, hours, minutes, seconds, milliseconds, microseconds, nanoseconds }).totalNanoseconds(ISO8601::DateTimeUnit::Hour);
}

Calendar Temporal::getTemporalCalendarIdentifierWithISODefault(ExecutionState& state, Value item)
{
    // TODO [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]]
    // If item has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, then
    //   Return item.[[Calendar]].
    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainDateObject()) {
            return item.asObject()->asTemporalPlainDateObject()->calendarID();
        }
    }

    // Let calendarLike be ? Get(item, "calendar").
    Value calendarLike = Object::getV(state, item, state.context()->staticStrings().lazyCalendar());
    // If calendarLike is undefined, then
    if (calendarLike.isUndefined()) {
        // Return "iso8601".
        return Calendar();
    }
    // Return ? ToTemporalCalendarIdentifier(calendarLike).
    return toTemporalCalendarIdentifier(state, calendarLike);
}

Calendar Temporal::toTemporalCalendarIdentifier(ExecutionState& state, Value temporalCalendarLike)
{
    // TODO [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]]
    // If temporalCalendarLike is an Object, then
    //   If temporalCalendarLike has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, then
    //     Return temporalCalendarLike.[[Calendar]].
    if (temporalCalendarLike.isObject()) {
        if (temporalCalendarLike.asObject()->isTemporalPlainDateObject()) {
            return temporalCalendarLike.asObject()->asTemporalPlainDateObject()->calendarID();
        }
    }

    // If temporalCalendarLike is not a String, throw a TypeError exception.
    if (!temporalCalendarLike.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ToTemporalCalendarIdentifier needs Temporal date-like Object or String");
    }
    // Let identifier be ? ParseTemporalCalendarString(temporalCalendarLike).
    // Return ? CanonicalizeCalendar(identifier).
    auto mayCalendar = Calendar::fromString(temporalCalendarLike.asString());
    auto parseResult = ISO8601::parseCalendarDateTime(temporalCalendarLike.asString());
    auto parseResultYearMonth = ISO8601::parseCalendarYearMonth(temporalCalendarLike.asString());
    auto parseResultMonthDay = ISO8601::parseCalendarMonthDay(temporalCalendarLike.asString());
    // TODO parse m-d, y-m from when implement YearMonth, MonthDay
    // test/built-ins/Temporal/PlainDate/from/argument-propertybag-calendar-iso-string.js
    if (!mayCalendar && !parseResult && !parseResultYearMonth && !parseResultMonthDay) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
    }

    if (mayCalendar) {
        return mayCalendar.value();
    }
    auto tryOnce = [](ExecutionState& state, Optional<ISO8601::CalendarID> cid) -> Optional<Calendar> {
        if (cid) {
            auto mayCalendar = Calendar::fromString(cid.value());
            if (!mayCalendar) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
            }
            return mayCalendar.value();
        }
        return NullOption;
    };
    if (parseResult) {
        auto cid = tryOnce(state, std::get<3>(parseResult.value()));
        if (cid) {
            return cid.value();
        }
    }
    if (parseResultYearMonth) {
        auto cid = tryOnce(state, std::get<1>(parseResultYearMonth.value()));
        if (cid) {
            return cid.value();
        }
    }
    if (parseResultMonthDay) {
        auto cid = tryOnce(state, std::get<1>(parseResultMonthDay.value()));
        if (cid) {
            return cid.value();
        }
    }
    return Calendar();
}

static AtomicString calendarFieldsRecordToString(ExecutionState& state, CalendarField f)
{
    if (false) {}
#define DEFINE_FIELD(name, Name, type)                        \
    else if (f == CalendarField::Name)                        \
    {                                                         \
        return state.context()->staticStrings().lazy##Name(); \
    }
    CALENDAR_FIELD_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD

    ASSERT_NOT_REACHED();
    return AtomicString();
}

static CalendarField stringToCalendarFieldsRecord(ExecutionState& state, AtomicString f)
{
    if (false) {}
#define DEFINE_FIELD(name, Name, type)                           \
    else if (f == state.context()->staticStrings().lazy##Name()) \
    {                                                            \
        return CalendarField::Name;                              \
    }
    CALENDAR_FIELD_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD

    ASSERT_NOT_REACHED();
    return CalendarField::Year;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsemonthcode
static MonthCode parseMonthCode(ExecutionState& state, Value input)
{
    constexpr auto msg = "Failed to parse month code";
    // Let monthCode be ? ToPrimitive(argument, string).
    Value monthCode = input.toPrimitive(state, Value::PrimitiveTypeHint::PreferString);
    // If monthCode is not a String, throw a TypeError exception.
    if (!monthCode.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, msg);
    }
    // If ParseText(StringToCodePoints(monthCode), MonthCode) is a List of errors, throw a RangeError exception.
    // Let isLeapMonth be false.
    // If the length of monthCode is 4, then
    //   Assert: The fourth code unit of monthCode is 0x004C (LATIN CAPITAL LETTER L).
    //   Set isLeapMonth to true.
    // Let monthCodeDigits be the substring of monthCode from 1 to 3.
    // Let monthNumber be ℝ(StringToNumber(monthCodeDigits)).
    ParserString buffer(monthCode.asString());

    if (buffer.atEnd() || *buffer != 'M') {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }
    buffer.advance();

    bool isLeapMonth = false;
    unsigned code = 0;
    if (!buffer.atEnd() && *buffer == '0') {
        buffer.advance();
        if (!buffer.atEnd() && isASCIIDigit(*buffer) && *buffer != '0') {
            code = *buffer - '0';
            buffer.advance();
        } else {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
    } else if (!buffer.atEnd() && isASCIIDigit(*buffer)) {
        code = (*buffer - '0') * 10;
        buffer.advance();
        if (!buffer.atEnd() && isASCIIDigit(*buffer)) {
            code += *buffer - '0';
            buffer.advance();
        } else {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
        }
    }

    // process 'L'
    if (!buffer.atEnd() && *buffer == 'L') {
        buffer.advance();
        isLeapMonth = true;
    }

    if (!buffer.atEnd()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }

    // If monthNumber is 0 and isLeapMonth is false, throw a RangeError exception.
    if (code == 0 && !isLeapMonth) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, msg);
    }

    // Return the Record { [[MonthNumber]]: monthNumber, [[IsLeapMonth]]: isLeapMonth }.
    MonthCode record;
    record.monthNumber = code;
    record.isLeapMonth = isLeapMonth;
    return record;
}

void CalendarFieldsRecord::setValue(ExecutionState& state, CalendarField f, Value value)
{
    if (f == CalendarField::Era) {
        era = value.toString(state);
    } else if (f == CalendarField::EraYear) {
        eraYear = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Year) {
        year = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Month) {
        month = value.toPostiveIntegerWithTruncation(state);
    } else if (f == CalendarField::MonthCode) {
        monthCode = parseMonthCode(state, value);
    } else if (f == CalendarField::Day) {
        day = value.toPostiveIntegerWithTruncation(state);
    } else if (f == CalendarField::Hour) {
        hour = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Minute) {
        minute = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Second) {
        second = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Millisecond) {
        millisecond = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Microsecond) {
        microsecond = value.toIntegerWithTruncation(state);
    } else if (f == CalendarField::Nanosecond) {
        nanosecond = value.toIntegerWithTruncation(state);
    } else {
        // TODO
        ASSERT_NOT_REACHED();
    }
}

CalendarFieldsRecord Temporal::prepareCalendarFields(ExecutionState& state, Calendar calendar, Object* fields, CalendarField* calendarFieldNames, size_t calendarFieldNamesLength,
                                                     CalendarField* nonCalendarFieldNames, size_t nonCalendarFieldNamesLength, CalendarField* requiredFieldNames, size_t requiredFieldNamesLength)
{
    // Let fieldNames be the list-concatenation of calendarFieldNames and nonCalendarFieldNames.
    std::vector<CalendarField> fieldName;
    for (size_t i = 0; i < calendarFieldNamesLength; i++) {
        fieldName.push_back(calendarFieldNames[i]);
    }
    for (size_t i = 0; i < nonCalendarFieldNamesLength; i++) {
        fieldName.push_back(nonCalendarFieldNames[i]);
    }
    // Let extraFieldNames be CalendarExtraFields(calendar, calendarFieldNames).
    // Set fieldNames to the list-concatenation of fieldNames and extraFieldNames.
    if (calendar.isEraRelated()) {
        fieldName.push_back(CalendarField::Era);
        fieldName.push_back(CalendarField::EraYear);
    }

    // Assert: fieldNames contains no duplicate elements.
    // Let any be false.
    bool any = false;
    // Let result be a Calendar Fields Record with all fields equal to unset.
    CalendarFieldsRecord result;
    std::vector<AtomicString> sortedPropertyNames;
    for (auto n : fieldName) {
        sortedPropertyNames.push_back(calendarFieldsRecordToString(state, n));
    }

    std::sort(sortedPropertyNames.begin(), sortedPropertyNames.end(), [](AtomicString lhs, AtomicString rhs) {
        return *lhs.string() < *rhs.string();
    });

    // For each property name property of sortedPropertyNames, do
    for (auto property : sortedPropertyNames) {
        // Let key be the value in the Enumeration Key column of Table 19 corresponding to the row whose Property Key value is property.
        // Let value be ? Get(fields, property).
        Value value = fields->get(state, ObjectPropertyName(property)).value(state, fields);
        // If value is not undefined, then
        if (!value.isUndefined()) {
            // Step i...ix
            result.setValue(state, stringToCalendarFieldsRecord(state, property), value);
            any = true;
        } else if (requiredFieldNamesLength && requiredFieldNamesLength != SIZE_MAX) {
            // Else if requiredFieldNames is a List, then
            // If requiredFieldNames contains key, then
            for (size_t i = 0; i < requiredFieldNamesLength; i++) {
                // Throw a TypeError exception.
                if (requiredFieldNames[i] == stringToCalendarFieldsRecord(state, property)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
                }
            }
            // TODO Set result's field whose name is given in the Field Name column of the same row to the corresponding Default value of the same row.
        }
    }

    // If requiredFieldNames is partial and any is false, then
    if (requiredFieldNamesLength == SIZE_MAX && !any) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
    }

    // Return result.
    return result;
}

UCalendar* Temporal::calendarDateFromFields(ExecutionState& state, Calendar calendar, CalendarFieldsRecord fields, TemporalOverflowOption overflow)
{
    // CalendarResolveFields steps
    if (calendar.isISO8601() || !calendar.isEraRelated()) {
        if (!fields.year || !fields.day) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
        }
    } else {
        if (!fields.day) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
        }
        if (!(fields.era && fields.eraYear) && !fields.year) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
        }
        if ((fields.era && !fields.eraYear) || (!fields.era && fields.eraYear)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
        }
    }

    if (!fields.monthCode && !fields.month) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Missing required field");
    }
    if (fields.monthCode && (fields.monthCode.value().isLeapMonth || fields.monthCode.value().monthNumber > 12)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Wrong month code");
    }
    if (fields.month && fields.monthCode && fields.month.value() != fields.monthCode.value().monthNumber) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Wrong month code or month");
    }
    if (fields.monthCode) {
        fields.month = fields.monthCode.value().monthNumber;
    }

    auto icuCalendar = calendar.createICUCalendar(state);

    if (calendar.isISO8601()) {
        // CalendarDateToISO steps
        if (overflow == TemporalOverflowOption::Constrain) {
            fields.month = std::min<unsigned>(fields.month.value(), 12);
            fields.day = std::min<unsigned>(fields.day.value(), ISO8601::daysInMonth(fields.year.value(), fields.month.value()));
        }
        auto plainDate = ISO8601::toPlainDate(ISO8601::Duration({ static_cast<double>(fields.year.value()), static_cast<double>(fields.month.value()), 0.0, static_cast<double>(fields.day.value()), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }));
        if (!plainDate || !ISO8601::isDateTimeWithinLimits(plainDate.value().year(), plainDate.value().month(), plainDate.value().day(), 12, 0, 0, 0, 0, 0)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date");
        }

        ucal_set(icuCalendar, UCAL_YEAR, fields.year.value());
        ucal_set(icuCalendar, UCAL_MONTH, fields.month.value() - 1);
        ucal_set(icuCalendar, UCAL_DAY_OF_MONTH, fields.day.value());

    } else if (calendar.isEraRelated() && (fields.era && fields.eraYear)) {
        Optional<int32_t> eraIdx;

        auto fieldEraValue = fields.era.value()->toNonGCUTF8StringData();
        calendar.lookupICUEra(state, [&](size_t idx, const std::string& s) -> bool {
            if (s == fieldEraValue) {
                eraIdx = idx;
                return true;
            }
            return false;
        });

        if (!eraIdx) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid era value");
        }

        ucal_set(icuCalendar, UCAL_ERA, eraIdx.value());
        ucal_set(icuCalendar, UCAL_YEAR, fields.eraYear.value());
        ucal_set(icuCalendar, UCAL_MONTH, fields.month.value() - 1);
        ucal_set(icuCalendar, UCAL_DAY_OF_MONTH, fields.day.value());

        if (fields.year) {
            UErrorCode status = U_ZERO_ERROR;
            auto epochTime = ucal_getMillis(icuCalendar, &status);
            DateObject::DateTimeInfo timeInfo;
            DateObject::computeTimeInfoFromEpoch(epochTime, timeInfo);
            if (timeInfo.year != fields.year.value()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "'year' and computed 'year' calendar fields are inconsistent");
            }
        }
    } else {
        if (calendar.shouldUseICUExtendedYear()) {
            ucal_set(icuCalendar, UCAL_EXTENDED_YEAR, fields.year.value());
        } else {
            ucal_set(icuCalendar, UCAL_YEAR, fields.year.value());
        }
        ucal_set(icuCalendar, UCAL_MONTH, fields.month.value() - 1);
        ucal_set(icuCalendar, UCAL_DAY_OF_MONTH, fields.day.value());
    }

    return icuCalendar;
}

CalendarFieldsRecord Temporal::calendarMergeFields(ExecutionState& state, Calendar calendar, const CalendarFieldsRecord& fields, const CalendarFieldsRecord& additionalFields)
{
    CalendarFieldsRecord merged;

    CalendarFieldsRecord copiedAdditionalFields = additionalFields;

    if (copiedAdditionalFields.month && !copiedAdditionalFields.monthCode) {
        MonthCode mc;
        mc.monthNumber = copiedAdditionalFields.month.value();
        copiedAdditionalFields.monthCode = mc;
    } else if (!copiedAdditionalFields.month && copiedAdditionalFields.monthCode) {
        copiedAdditionalFields.month = copiedAdditionalFields.monthCode.value().monthNumber;
    }

#define COPY_FIELD(name, Name, type)               \
    if (fields.name) {                             \
        merged.name = fields.name;                 \
    }                                              \
    if (copiedAdditionalFields.name) {             \
        merged.name = copiedAdditionalFields.name; \
    }
    CALENDAR_FIELD_RECORDS(COPY_FIELD)
#undef COPY_FIELD

    if (copiedAdditionalFields.era || copiedAdditionalFields.eraYear) {
        merged.year = NullOption;
    }

    return merged;
}

TemporalShowCalendarNameOption Temporal::getTemporalShowCalendarNameOption(ExecutionState& state, Optional<Object*> options)
{
    if (!options) {
        return TemporalShowCalendarNameOption::Auto;
    }
    Value values[4] = { state.context()->staticStrings().lazyAuto().string(), state.context()->staticStrings().lazyAlways().string(),
                        state.context()->staticStrings().lazyNever().string(), state.context()->staticStrings().lazyCritical().string() };
    // Let stringValue be ? GetOption(options, "calendarName", string, « "auto", "always", "never", "critical" », "auto").
    auto stringValue = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyCalendarName().string(), Intl::StringValue,
                                       values, 4, state.context()->staticStrings().lazyAuto().string())
                           .asString();
    // If stringValue is "always", return always.
    if (stringValue->equals("always")) {
        return TemporalShowCalendarNameOption::Always;
    }
    // If stringValue is "never", return never.
    if (stringValue->equals("never")) {
        return TemporalShowCalendarNameOption::Never;
    }
    // If stringValue is "critical", return critical.
    if (stringValue->equals("critical")) {
        return TemporalShowCalendarNameOption::Critical;
    }
    // Return auto.
    return TemporalShowCalendarNameOption::Auto;
}

inline int daysInYear(int year)
{
    return 365 + ISO8601::isLeapYear(year);
}

static bool balanceISODate(double& year, double& month, double& day)
{
    ASSERT(month >= 1 && month <= 12);
    if (!ISO8601::isYearWithinLimits(year))
        return false;

    double daysFrom1970 = day + ISO8601::dateToDaysFrom1970(static_cast<int>(year), static_cast<int>(month - 1), 1) - 1;

    double balancedYear = std::floor(daysFrom1970 / 365.2425) + 1970;
    if (!ISO8601::isYearWithinLimits(balancedYear))
        return false;

    double daysUntilYear = ISO8601::daysFrom1970ToYear(static_cast<int>(balancedYear));
    if (daysUntilYear > daysFrom1970) {
        balancedYear--;
        daysUntilYear -= daysInYear(static_cast<int>(balancedYear));
    } else {
        double daysUntilFollowingYear = daysUntilYear + daysInYear(static_cast<int>(balancedYear));
        if (daysUntilFollowingYear <= daysFrom1970) {
            daysUntilYear = daysUntilFollowingYear;
            balancedYear++;
        }
    }

    ASSERT(daysFrom1970 - daysUntilYear >= 0);
    auto dayInYear = static_cast<unsigned>(daysFrom1970 - daysUntilYear + 1);

    unsigned daysUntilMonth = 0;
    unsigned balancedMonth = 1;
    for (; balancedMonth < 12; balancedMonth++) {
        auto monthDays = balancedMonth != 2 ? ISO8601::daysInMonth(balancedMonth) : ISO8601::daysInMonth(static_cast<int>(balancedYear), balancedMonth);
        if (daysUntilMonth + monthDays >= dayInYear)
            break;
        daysUntilMonth += monthDays;
    }

    year = balancedYear;
    month = balancedMonth;
    day = dayInYear - daysUntilMonth;
    return true;
}

static ISO8601::PlainDate balanceISODate(ExecutionState& state, double year, double month, double day)
{
    if (!balanceISODate(year, month, day)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
    }
    return ISO8601::PlainDate(year, month, day);
}

#define CHECK_ICU_CALENDAR()                                                                                  \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

UCalendar* Temporal::calendarDateAdd(ExecutionState& state, Calendar calendar, ISO8601::PlainDate isoDate, UCalendar* icuDate, const ISO8601::Duration& duration, TemporalOverflowOption overflow)
{
    UErrorCode status = U_ZERO_ERROR;
    auto newCal = ucal_clone(icuDate, &status);
    CHECK_ICU_CALENDAR()
    if (calendar.isISO8601()) {
        int32_t oldY = isoDate.year();
        int32_t oldM = isoDate.month();
        int32_t oldD = isoDate.day();

        double year = oldY + duration.years();
        double month = oldM + duration.months();
        if (month < 1 || month > 12) {
            year += std::floor((month - 1) / 12);
            month = nonNegativeModulo((month - 1), 12) + 1;
        }

        double daysInMonth = ISO8601::daysInMonth(year, month);
        double day = oldD;
        if (overflow == TemporalOverflowOption::Constrain)
            day = std::min<double>(day, daysInMonth);
        else if (day > daysInMonth) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
        }

        day += duration.days() + 7 * duration.weeks();
        if (day < 1 || day > daysInMonth) {
            if (!balanceISODate(year, month, day)) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
            }
        }

        ucal_set(newCal, UCAL_YEAR, year);
        ucal_set(newCal, UCAL_MONTH, month - 1);
        ucal_set(newCal, UCAL_DAY_OF_MONTH, day);

        if (!ISO8601::isDateTimeWithinLimits(year, month, day, 12, 0, 0, 0, 0, 0)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
        }

        return newCal;
    } else {
        int32_t o, check;

        o = ucal_get(newCal, UCAL_YEAR, &status);
        CHECK_ICU_CALENDAR()
        ucal_set(newCal, UCAL_YEAR, o + duration.years());
        check = ucal_get(newCal, UCAL_YEAR, &status);
        CHECK_ICU_CALENDAR()
        if (check != (o + duration.years())) {
            if (overflow == TemporalOverflowOption::Reject) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
            }
        }

        o = ucal_get(newCal, UCAL_MONTH, &status);
        CHECK_ICU_CALENDAR()
        ucal_set(newCal, UCAL_MONTH, o + duration.months() - 1);
        check = ucal_get(newCal, UCAL_MONTH, &status);
        CHECK_ICU_CALENDAR()
        if (check != (o + duration.months() - 1)) {
            if (overflow == TemporalOverflowOption::Reject) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
            }
        }

        o = ucal_get(newCal, UCAL_DAY_OF_MONTH, &status);
        CHECK_ICU_CALENDAR()
        ucal_set(newCal, UCAL_DAY_OF_MONTH, o + duration.days());
        check = ucal_get(newCal, UCAL_DAY_OF_MONTH, &status);
        CHECK_ICU_CALENDAR()
        if (check != (o + duration.days())) {
            if (overflow == TemporalOverflowOption::Reject) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
            }
        }

        if (duration.weeks()) {
            auto epochTime = ucal_getMillis(newCal, &status);
            int32_t w = ucal_get(newCal, UCAL_WEEK_OF_YEAR, &status);
            CHECK_ICU_CALENDAR()
            w += duration.weeks();
            ucal_set(newCal, UCAL_WEEK_OF_YEAR, w);
        }

        auto epoch = ucal_getMillis(newCal, &status);
        CHECK_ICU_CALENDAR()
        if (!ISO8601::isoDateTimeWithinLimits(Int128(epoch))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
        }
    }
    return newCal;
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodatesurpasses
static bool isoDateSurpasses(int32_t sign, double y1, double m1, double d1, const ISO8601::PlainDate& isoDate2)
{
    if (y1 != isoDate2.year()) {
        if (sign * (y1 - isoDate2.year()) > 0)
            return true;
    } else if (m1 != isoDate2.month()) {
        if (sign * (m1 - isoDate2.month()) > 0)
            return true;
    } else if (d1 != isoDate2.day()) {
        if (sign * (d1 - isoDate2.day()) > 0)
            return true;
    }
    return false;
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisoyearmonth
ISO8601::PlainYearMonth Temporal::balanceISOYearMonth(double year, double month)
{
    year += std::floor((month - 1) / 12);
    // ECMA modulo operator always results in same sign as y in x mod y
    month = std::fmod(month - 1, 12) + 1;
    if (month < 1)
        month += 12;
    return ISO8601::PlainYearMonth(year, month);
}

// https://tc39.es/proposal-temporal/#sec-temporal-regulateisodate
Optional<ISO8601::PlainDate> Temporal::regulateISODate(double year, double month, double day, TemporalOverflowOption overflow)
{
    if (overflow == TemporalOverflowOption::Constrain) {
        if (month < 1)
            month = 1;
        if (month > 12)
            month = 12;
        auto daysInMonth = ISO8601::daysInMonth(year, month);
        if (day < 1)
            day = 1;
        if (day > daysInMonth)
            day = daysInMonth;
    } else if (!ISO8601::isDateTimeWithinLimits(year, month, day, 12, 0, 0, 0, 0, 0))
        return NullOption;
    return ISO8601::PlainDate(year, month, day);
}

static inline double makeDay(double year, double month, double date)
{
    double additionalYears = std::floor(month / 12);
    double ym = year + additionalYears;
    if (!std::isfinite(ym))
        return std::numeric_limits<double>::quiet_NaN();
    double mm = month - additionalYears * 12;
    int32_t yearInt32 = ym;
    int32_t monthInt32 = mm;
    if (yearInt32 != ym || monthInt32 != mm)
        return std::numeric_limits<double>::quiet_NaN();
    double days = ISO8601::dateToDaysFrom1970(yearInt32, monthInt32, 1);
    return days + date - 1;
}

ISO8601::Duration Temporal::calendarDateUntil(Calendar calendar, ISO8601::PlainDate one, ISO8601::PlainDate two, TemporalUnit largestUnit)
{
    // TODO non-iso8601 calendar
    auto sign = -1 * one.compare(two);
    if (!sign)
        return {};

    double years = 0;
    double months = 0;

    if (largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month) {
        auto candidateYears = two.year() - one.year();
        if (candidateYears)
            candidateYears -= sign;
        while (!isoDateSurpasses(sign, one.year() + candidateYears, one.month(), one.day(), two)) {
            years = candidateYears;
            candidateYears += sign;
        }

        auto candidateMonths = sign;
        auto intermediate = balanceISOYearMonth(one.year() + years, one.month() + candidateMonths);
        while (!isoDateSurpasses(sign, intermediate.year(), intermediate.month(), one.day(), two)) {
            months = candidateMonths;
            candidateMonths += sign;
            intermediate = balanceISOYearMonth(intermediate.year(), intermediate.month() + sign);
        }

        if (largestUnit == TemporalUnit::Month) {
            months += years * 12;
            years = 0;
        }
    }

    auto intermediate = balanceISOYearMonth(one.year() + years, one.month() + months);
    auto constrained = regulateISODate(intermediate.year(), intermediate.month(), one.day(), TemporalOverflowOption::Constrain);

    double weeks = 0;
    double days = makeDay(two.year(), two.month() - 1, two.day()) - makeDay(constrained->year(), constrained->month() - 1, constrained->day());

    if (largestUnit == TemporalUnit::Week) {
        weeks = std::trunc(std::abs(days) / 7.0);
        days = std::trunc((double)(((Int128)std::trunc(days)) % 7));
        if (weeks)
            weeks *= sign; // Avoid -0
    }

    return { years, months, weeks, days, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
}

class NudgeResult {
public:
    ISO8601::InternalDuration m_duration;
    Int128 m_nudgedEpochNs;
    bool m_didExpandCalendarUnit;
    NudgeResult() {}
    NudgeResult(ISO8601::InternalDuration d, Int128 ns, bool expanded)
        : m_duration(d)
        , m_nudgedEpochNs(ns)
        , m_didExpandCalendarUnit(expanded)
    {
    }
};

class Nudged {
public:
    NudgeResult m_nudgeResult;
    double m_total;
    Nudged() {}
    Nudged(NudgeResult n, double t)
        : m_nudgeResult(n)
        , m_total(t)
    {
    }
};

static Int128 add24HourDaysToTimeDuration(ExecutionState& state, Int128 d, double days)
{
    Int128 daysInNanoseconds = ((Int128)days) * ISO8601::ExactTime::nsPerDay;
    Int128 result = d + daysInNanoseconds;
    if (std::abs(result) > ISO8601::InternalDuration::maxTimeDuration) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Total time in duration is out of range");
    }
    return result;
}

// There are two different versions of this method due to the lack
// of float128. The names are different (roundNumberToIncrementInt128() and
// roundNumberToIncrement()) to avoid confusion in the presence of
// implicit casts.
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrement
static Int128 roundNumberToIncrementInt128(Int128 x, Int128 increment, ISO8601::RoundingMode mode)
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
    auto unsignedRoundingMode = getUnsignedRoundingMode(mode, isNegative);
    Int128 rounded = 0;
    if (std::abs(x) == r1 * increment)
        rounded = r1;
    else if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::Zero)
        rounded = r1;
    else if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::Infinity)
        rounded = r2;
    else if (std::abs(remainder * 2) < increment)
        rounded = r1;
    else if (std::abs(remainder * 2) > increment)
        rounded = r2;
    else if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::HalfZero)
        rounded = r1;
    else if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::HalfInfinity)
        rounded = r2;
    else
        rounded = !even ? r1 : r2;
    if (isNegative)
        rounded = -rounded;
    return rounded * increment;
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

static double totalTimeDuration(ExecutionState& state, Int128 timeDuration, TemporalUnit unit)
{
    Int128 divisor = ISO8601::lengthInNanoseconds(toDateTimeUnit(unit));
    Int128 quotient = timeDuration / divisor;
    Int128 remainder = timeDuration % divisor;
    // Perform long division to calculate the fractional part of the quotient
    // remainder / n with more accuracy than 64-bit floating point division
    size_t precision = 50;
    size_t size = 0;
    StringBuilder decimalDigits;
    int32_t digit = 0;
    int32_t sign = timeDuration < 0 ? -1 : 1;
    while (remainder && size < precision) {
        remainder *= 10;
        digit = (int32_t)(remainder / divisor);
        remainder = remainder % divisor;
        appendInteger(decimalDigits, std::abs(digit));
        size++;
    }
    StringBuilder result;
    appendInteger(result, (double)std::abs(quotient));
    result.appendChar('.');
    result.appendString(decimalDigits.finalize());
    // NOTE: if result.toString() == 9007199254740992.999,
    // the result is rounded down to 9007199254740992.
    // This causes the test262 test
    // Temporal/Duration/prototype/total/precision-exact-mathematical-values-7.js
    // to fail when unit=milliseconds, smallerUnit=microseconds, integer=2**53, fraction=1999.
    return sign * Value(result.finalize()).toNumber(state);
}

static ISO8601::Duration adjustDateDurationRecord(ExecutionState& state, const ISO8601::Duration& dateDuration, double days, Optional<double> weeks, Optional<double> months)
{
    auto result = ISO8601::Duration{ dateDuration.years(), months ? months.value() : dateDuration.months(), weeks ? weeks.value() : dateDuration.weeks(), days, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    if (!result.isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Duration properties must be valid and of consistent sign");
    }
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-nudgetodayortime
static NudgeResult nudgeToDayOrTime(ExecutionState& state, ISO8601::InternalDuration duration, Int128 destEpochNs, TemporalUnit largestUnit,
                                    double increment, TemporalUnit smallestUnit, ISO8601::RoundingMode roundingMode)
{
    Int128 timeDuration = add24HourDaysToTimeDuration(state, duration.time(), duration.dateDuration().days());
    Int128 unitLength = ISO8601::lengthInNanoseconds(toDateTimeUnit(smallestUnit));
    Int128 roundedTime = roundNumberToIncrementInt128(timeDuration,
                                                      unitLength * (Int128)std::trunc(increment), roundingMode);
    Int128 diffTime = roundedTime - timeDuration;
    double wholeDays = totalTimeDuration(state, timeDuration, TemporalUnit::Day);
    double roundedWholeDays = totalTimeDuration(state, roundedTime, TemporalUnit::Day);
    auto dayDelta = roundedWholeDays - wholeDays;
    auto dayDeltaSign = dayDelta < 0 ? -1 : dayDelta > 0 ? 1
                                                         : 0;
    bool didExpandDays = dayDeltaSign == (timeDuration < 0 ? -1 : timeDuration > 0 ? 1
                                                                                   : 0);
    auto nudgedEpochNs = diffTime + destEpochNs;
    auto days = 0;
    auto remainder = roundedTime;
    if (largestUnit <= TemporalUnit::Day) {
        days = roundedWholeDays;
        remainder = roundedTime + Temporal::timeDurationFromComponents(-roundedWholeDays * 24, 0, 0, 0, 0, 0);
    }
    auto dateDuration = adjustDateDurationRecord(state, duration.dateDuration(), days, NullOption, NullOption);
    auto resultDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, remainder);
    return NudgeResult(resultDuration, nudgedEpochNs, didExpandDays);
}

// https://tc39.es/proposal-temporal/#sec-temporal-addisodate
ISO8601::PlainDate Temporal::isoDateAdd(ExecutionState& state, const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflowOption overflow)
{
    double years = plainDate.year() + duration.years();
    double months = plainDate.month() + duration.months();
    double days = plainDate.day();
    ISO8601::PlainYearMonth intermediate = balanceISOYearMonth(years, months);
    Optional<ISO8601::PlainDate> intermediate1 = regulateISODate(intermediate.year(), intermediate.month(), days, overflow);
    if (!intermediate1) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date time is out of range of ECMAScript representation");
    }
    auto d = intermediate1.value().day() + duration.days() + (7 * duration.weeks());
    double yy = intermediate1.value().year(), mm = intermediate1.value().month(), dd = d;
    balanceISODate(yy, mm, dd);
    if (!ISO8601::isDateTimeWithinLimits(yy, mm, dd, 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date time is out of range of ECMAScript representation");
    }
    return ISO8601::PlainDate(yy, mm, dd);
}

Int128 Temporal::getEpochNanosecondsFor(ExecutionState& state, Optional<TimeZone> timeZone, ISO8601::PlainDateTime isoDateTime, TemporalDisambiguationOption disambiguation)
{
    const auto& date = isoDateTime.plainDate();
    const auto& time = isoDateTime.plainTime();
    auto epochNanoValue = ISO8601::ExactTime::fromISOPartsAndOffset(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(), time.nanosecond(), 0).epochNanoseconds();
    if (!timeZone) {
        return epochNanoValue;
    }
    if (timeZone.hasValue()) {
        if (timeZone.value().hasOffset()) {
            return epochNanoValue + timeZone.value().offset();
        }
    }
    // TODO https://tc39.es/proposal-temporal/#sec-temporal-disambiguatepossibleepochnanoseconds
    auto offset = computeTimeZoneOffset(state, timeZone.value().timeZoneName(), ISO8601::ExactTime(epochNanoValue).epochMilliseconds());
    return epochNanoValue + offset;
}

// https://tc39.es/proposal-temporal/#sec-applyunsignedroundingmode
double applyUnsignedRoundingMode(double x, double r1, double r2, ISO8601::UnsignedRoundingMode unsignedRoundingMode)
{
    if (x == r1)
        return r1;
    ASSERT(r1 < x && x < r2);
    if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::Zero)
        return r1;
    if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::Infinity)
        return r2;
    double d1 = x - r1;
    double d2 = r2 - x;
    if (d1 < d2)
        return r1;
    if (d2 < d1)
        return r2;
    ASSERT(d1 == d2);
    if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::HalfZero)
        return r1;
    if (unsignedRoundingMode == ISO8601::UnsignedRoundingMode::HalfInfinity)
        return r2;
    ASSERT(unsignedRoundingMode == ISO8601::UnsignedRoundingMode::HalfEven);
    auto cardinality = std::fmod(r1 / (r2 - r1), 2);
    return !cardinality ? r1 : r2;
}

// https://tc39.es/proposal-temporal/#sec-temporal-nudgetocalendarunit
static Nudged nudgeToCalendarUnit(ExecutionState& state, int32_t sign, const ISO8601::InternalDuration& duration, Int128 destEpochNs, ISO8601::PlainDateTime isoDateTime, Optional<TimeZone> timeZone, Calendar calendar, double increment, TemporalUnit unit, ISO8601::RoundingMode roundingMode)
{
    double r1 = 0;
    double r2 = 0;
    ISO8601::Duration startDuration;
    ISO8601::Duration endDuration;
    switch (unit) {
    case TemporalUnit::Year: {
        Int128 years = roundNumberToIncrementInt128((Int128)duration.dateDuration().years(),
                                                    (Int128)increment, ISO8601::RoundingMode::Trunc);
        r1 = (double)years;
        r2 = (double)years + increment * sign;
        startDuration = ISO8601::Duration{ r1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        endDuration = ISO8601::Duration{ r2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        break;
    }
    case TemporalUnit::Month: {
        Int128 months = roundNumberToIncrementInt128((Int128)duration.dateDuration().months(),
                                                     (Int128)increment, ISO8601::RoundingMode::Trunc);
        r1 = (double)months;
        r2 = (double)months + increment * sign;
        startDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, 0, r1);
        endDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, 0, r2);
        break;
    }
    case TemporalUnit::Week: {
        auto yearsMonths = adjustDateDurationRecord(state, duration.dateDuration(), 0, 0, NullOption);
        auto weeksStart = Temporal::isoDateAdd(state, isoDateTime.plainDate(), yearsMonths, TemporalOverflowOption::Constrain);
        auto weeksEnd = balanceISODate(state, weeksStart.year(), weeksStart.month(), weeksStart.day() + duration.dateDuration().days());
        auto untilResult = Temporal::calendarDateUntil(calendar, weeksStart, weeksEnd, TemporalUnit::Week);
        Int128 weeks = roundNumberToIncrementInt128((Int128)(duration.dateDuration().weeks() + untilResult.weeks()),
                                                    (Int128)increment, ISO8601::RoundingMode::Trunc);
        r1 = (double)weeks;
        r2 = (double)weeks + increment * sign;
        startDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, r1, NullOption);
        endDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, r2, NullOption);
        break;
    }
    default: {
        ASSERT(unit == TemporalUnit::Day);
        Int128 days = roundNumberToIncrementInt128((Int128)duration.dateDuration().days(),
                                                   (Int128)increment, ISO8601::RoundingMode::Trunc);
        r1 = (double)days;
        r2 = (double)days + increment * sign;
        startDuration = adjustDateDurationRecord(state, duration.dateDuration(), r1, NullOption, NullOption);
        endDuration = adjustDateDurationRecord(state, duration.dateDuration(), r2, NullOption, NullOption);
        break;
    }
    }
    ASSERT(sign != 1 || (r1 >= 0 && r1 < r2));
    ASSERT(sign != -1 || (r1 <= 0 && r1 > r2));
    auto start = Temporal::isoDateAdd(state, isoDateTime.plainDate(), startDuration, TemporalOverflowOption::Constrain);
    auto end = Temporal::isoDateAdd(state, isoDateTime.plainDate(), endDuration, TemporalOverflowOption::Constrain);
    auto startDateTime = ISO8601::PlainDateTime(start, isoDateTime.plainTime());
    auto endDateTime = ISO8601::PlainDateTime(end, isoDateTime.plainTime());
    Int128 startEpochNs = Temporal::getEpochNanosecondsFor(state, timeZone, startDateTime, TemporalDisambiguationOption::Compatible);
    Int128 endEpochNs = Temporal::getEpochNanosecondsFor(state, timeZone, endDateTime, TemporalDisambiguationOption::Compatible);
    ASSERT(sign != 1 || ((startEpochNs <= destEpochNs) && (destEpochNs <= endEpochNs)));
    ASSERT(sign == 1 || ((endEpochNs <= destEpochNs) && (destEpochNs <= startEpochNs)));
    ASSERT(startEpochNs != endEpochNs);
    // See 18. NOTE
    Int128 progressNumerator = destEpochNs - startEpochNs;
    Int128 progressDenominator = endEpochNs - startEpochNs;
    double total = r1 + (((double)progressNumerator) / ((double)progressDenominator)) * increment * sign;
    Int128 progress = progressNumerator / progressDenominator;
    ASSERT(0 <= progress && progress <= 1);
    auto isNegative = sign < 0;
    ISO8601::UnsignedRoundingMode unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, isNegative);
    double roundedUnit = std::abs(r2);
    if (progress != 1) {
        ASSERT(std::abs(r1) <= std::abs(total) && std::abs(total) < std::abs(r2));
        roundedUnit = applyUnsignedRoundingMode(
            std::abs(total), std::abs(r1), std::abs(r2), unsignedRoundingMode);
    }
    bool didExpandCalendarUnit = true;
    ISO8601::Duration resultDuration = endDuration;
    Int128 nudgedEpochNs = endEpochNs;
    if (roundedUnit != std::abs(r2)) {
        didExpandCalendarUnit = false;
        resultDuration = startDuration;
        nudgedEpochNs = startEpochNs;
    }
    ISO8601::InternalDuration resultDurationInternal = ISO8601::InternalDuration::combineDateAndTimeDuration(resultDuration, 0);
    auto nudgeResult = NudgeResult(resultDurationInternal, nudgedEpochNs, didExpandCalendarUnit);
    return Nudged(nudgeResult, total);
}

static int32_t unitIndexInTable(TemporalUnit unit)
{
    switch (unit) {
    case TemporalUnit::Year:
        return 0;
    case TemporalUnit::Month:
        return 1;
    case TemporalUnit::Week:
        return 2;
    case TemporalUnit::Day:
        return 3;
    case TemporalUnit::Hour:
        return 4;
    case TemporalUnit::Minute:
        return 5;
    case TemporalUnit::Second:
        return 6;
    case TemporalUnit::Millisecond:
        return 7;
    case TemporalUnit::Microsecond:
        return 8;
    case TemporalUnit::Nanosecond:
        return 9;
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

static TemporalUnit unitInTable(int32_t i)
{
    switch (i) {
    case 0:
        return TemporalUnit::Year;
    case 1:
        return TemporalUnit::Month;
    case 2:
        return TemporalUnit::Week;
    case 3:
        return TemporalUnit::Day;
    case 4:
        return TemporalUnit::Hour;
    case 5:
        return TemporalUnit::Minute;
    case 6:
        return TemporalUnit::Second;
    case 7:
        return TemporalUnit::Millisecond;
    case 8:
        return TemporalUnit::Microsecond;
    case 9:
        return TemporalUnit::Nanosecond;
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal-bubblerelativeduration
static ISO8601::InternalDuration bubbleRelativeDuration(ExecutionState& state, int32_t sign, ISO8601::InternalDuration duration, Int128 nudgedEpochNs, ISO8601::PlainDateTime isoDateTime, Optional<TimeZone> timeZone, TemporalUnit largestUnit, TemporalUnit smallestUnit)
{
    if (smallestUnit == largestUnit)
        return duration;
    auto largestUnitIndex = unitIndexInTable(largestUnit);
    auto smallestUnitIndex = unitIndexInTable(smallestUnit);
    auto unitIndex = smallestUnitIndex - 1;
    bool done = false;
    ISO8601::Duration endDuration;
    while (unitIndex >= largestUnitIndex && !done) {
        auto unit = unitInTable(unitIndex);
        if (unit != TemporalUnit::Week || largestUnit == TemporalUnit::Week) {
            if (unit == TemporalUnit::Year) {
                auto years = duration.dateDuration().years() + sign;
                endDuration = ISO8601::Duration{ years, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
            } else if (unit == TemporalUnit::Month) {
                auto months = duration.dateDuration().months() + sign;
                endDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, 0, months);
            } else {
                ASSERT(unit == TemporalUnit::Week);
                auto weeks = duration.dateDuration().weeks() + sign;
                endDuration = adjustDateDurationRecord(state, duration.dateDuration(), 0, weeks, NullOption);
            }
            auto end = Temporal::isoDateAdd(state, isoDateTime.plainDate(), endDuration, TemporalOverflowOption::Constrain);
            auto endDateTime = ISO8601::PlainDateTime(end, isoDateTime.plainTime());
            auto endEpochNs = Temporal::getEpochNanosecondsFor(state, timeZone, endDateTime, TemporalDisambiguationOption::Compatible);
            auto beyondEnd = nudgedEpochNs - endEpochNs;
            auto beyondEndSign = beyondEnd < 0 ? -1 : beyondEnd > 0 ? 1
                                                                    : 0;
            if (beyondEndSign != -sign) {
                duration = ISO8601::InternalDuration::combineDateAndTimeDuration(endDuration, 0);
            } else
                done = true;
        }
        unitIndex--;
    }
    return duration;
}

ISO8601::InternalDuration Temporal::roundRelativeDuration(ExecutionState& state, ISO8601::InternalDuration duration, Int128 destEpochNs, ISO8601::PlainDateTime isoDateTime, Optional<TimeZone> timeZone,
                                                          Calendar calendar, TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit, ISO8601::RoundingMode roundingMode)
{
    bool irregularLengthUnit = smallestUnit <= TemporalUnit::Week;
    int32_t sign = duration.sign() < 0 ? -1 : 1;
    NudgeResult nudgeResult;
    if (irregularLengthUnit) {
        Nudged record = nudgeToCalendarUnit(state, sign, duration, destEpochNs, isoDateTime, timeZone, calendar, increment, smallestUnit, roundingMode);
        nudgeResult = record.m_nudgeResult;
    } else {
        // 7
        nudgeResult = nudgeToDayOrTime(state, duration, destEpochNs, largestUnit, increment, smallestUnit, roundingMode);
    }
    // 8.
    duration = nudgeResult.m_duration;
    // 9.
    if (nudgeResult.m_didExpandCalendarUnit && smallestUnit != TemporalUnit::Week) {
        auto startUnit = smallestUnit <= TemporalUnit::Day ? smallestUnit : TemporalUnit::Day;
        duration = bubbleRelativeDuration(state, sign, duration, nudgeResult.m_nudgedEpochNs, isoDateTime, timeZone, largestUnit, startUnit);
    }
    return duration;
}

} // namespace Escargot

#endif
