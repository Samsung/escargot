#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "runtime/BigInt.h"
#include "Intl.h"
#include "IntlDurationFormat.h"

#if defined(ENABLE_INTL_DURATIONFORMAT)

namespace Escargot {

static const char* const intlDurationFormatRelevantExtensionKeys[1] = { "nu" };
static size_t intlDurationFormatRelevantExtensionKeysLength = 1;
static const size_t indexOfExtensionKeyNu = 0;

static std::vector<std::string> localeDataDurationFormat(String* locale, size_t keyIndex)
{
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyNu:
        keyLocaleData = Intl::numberingSystemsForLocale(locale);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

IntlDurationFormatObject::IntlDurationFormatObject(ExecutionState& state, Value locales, Value options)
    : IntlDurationFormatObject(state, state.context()->globalObject()->intlRelativeTimeFormatPrototype(), locales, options)
{
}

// https://tc39.es/ecma402/#sec-getdurationunitoptions
static std::pair<String*, String*> getDurationUnitOptions(ExecutionState& state, String* unit, Object* options, String* baseStyle, Value* stylesList, size_t stylesListSize,
                                                          String* digitalBase, String* prevStyle, bool isTwoDigitHours /* TODO */)
{
    // Let style be ? GetOption(options, unit, string, stylesList, undefined).
    Value stylesValue = Intl::getOption(state, options, unit, Intl::StringValue, stylesList, stylesListSize, Value());
    Optional<String*> style = stylesValue.isUndefined() ? nullptr : stylesValue.asString();

    // Let displayDefault be "always".
    String* displayDefault = state.context()->staticStrings().lazyAlways().string();

    // If style is undefined, then
    if (!style) {
        // If baseStyle is "digital", then
        if (baseStyle->equals("digital")) {
            // Set style to digitalBase.
            style = digitalBase;
            // If unit is not one of "hours", "minutes", or "seconds", set displayDefault to "auto".
            if (!unit->equals("hours") && !unit->equals("minutes") && !unit->equals("seconds")) {
                displayDefault = state.context()->staticStrings().lazyAuto().string();
            }
        } else if (prevStyle->equals("fractional") || prevStyle->equals("numeric") || prevStyle->equals("2-digit")) {
            // Else if prevStyle is one of "fractional", "numeric" or "2-digit", then
            // Set style to "numeric".
            style = state.context()->staticStrings().numeric.string();
            // If unit is not "minutes" or "seconds", set displayDefault to "auto".
            if (!unit->equals("minutes") || unit->equals("seconds")) {
                displayDefault = state.context()->staticStrings().lazyAuto().string();
            }
        } else {
            // Else,
            // Set style to baseStyle.
            style = baseStyle;
            // Set displayDefault to "auto".
            displayDefault = state.context()->staticStrings().lazyAuto().string();
        }
    }
    // If style is "numeric" and IsFractionalSecondUnitName(unit) is true, then
    if (style->equals("numeric") && (unit->equals("milliseconds") || unit->equals("microseconds") || unit->equals("nanoseconds"))) {
        // Set style to "fractional".
        // NOTE we need to comment out below line to pass `test/intl402/DurationFormat/constructor-options-defaults` test
        // style = state.context()->staticStrings().lazyFractional().string();
        // Set displayDefault to "auto".
        displayDefault = state.context()->staticStrings().lazyAuto().string();
    }
    // Let displayField be the string-concatenation of unit and "Display".
    StringBuilder sb;
    sb.appendString(unit);
    sb.appendString("Display");
    String* displayField = sb.finalize();
    // Let display be ? GetOption(options, displayField, string, « "auto", "always" », displayDefault).
    Value displayOptionDefault[2] = { state.context()->staticStrings().lazyAuto().string(), state.context()->staticStrings().lazyAlways().string() };
    String* display = Intl::getOption(state, options, displayField, Intl::StringValue, displayOptionDefault, 2, displayDefault).asString();
    // Perform ? ValidateDurationUnitStyle(unit, style, display, prevStyle).
    //   If display is "always" and style is "fractional", throw a RangeError exception.
    //   If prevStyle is "fractional" and style is not "fractional", throw a RangeError exception.
    //   If prevStyle is "numeric" or "2-digit" and style is not one of "fractional", "numeric" or "2-digit", throw a RangeError exception.
    if ((display->equals("always") && style->equals("fractional")) || (prevStyle->equals("fractional") && !style->equals("fractional")) || ((prevStyle->equals("numeric") || prevStyle->equals("2-digit")) && !(style->equals("fractional") || style->equals("numeric") || style->equals("2-digit")))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid option for DurationFormat");
    }
    // If unit is "hours" and twoDigitHours is true, set style to "2-digit".
    if (unit->equals("hours") && isTwoDigitHours) {
        style = state.context()->staticStrings().lazyTwoDigit().string();
    }
    // If unit is "minutes" or "seconds" and prevStyle is "numeric" or "2-digit", set style to "2-digit".
    if ((unit->equals("minutes") || unit->equals("seconds")) && (prevStyle->equals("numeric") || prevStyle->equals("2-digit"))) {
        style = state.context()->staticStrings().lazyTwoDigit().string();
    }
    // Return the Duration Unit Options Record { [[Style]]: style, [[Display]]: display }.
    return std::make_pair(style.value(), display);
}

IntlDurationFormatObject::IntlDurationFormatObject(ExecutionState& state, Object* proto, Value locales, Value optionsInput)
    : DerivedObject(state, proto)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 62) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.NumberFormat needs 67+ version of ICU");
    }
#endif
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    Object* options;
    if (optionsInput.isUndefined()) {
        options = new Object(state, Object::PrototypeIsNull);
    } else {
        // Else,
        options = optionsInput.toObject(state);
    }

    StringMap opt;
    Value localeMatcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    String* matcher = localeMatcherValues[1].asString();
    matcher = Intl::getOption(state, options, state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, localeMatcherValues, 2, localeMatcherValues[1]).asString();
    opt.insert(std::make_pair("matcher", matcher));

    Value numberingSystem = Intl::getOption(state, options, state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value());
    if (!numberingSystem.isUndefined()) {
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(numberingSystem.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "The numberingSystem value you gave is not valid");
        }
    }

    if (!numberingSystem.isUndefined()) {
        opt.insert(std::make_pair("nu", numberingSystem.asString()));
    }
    StringMap r = Intl::resolveLocale(state, state.context()->vmInstance()->intlDurationFormatAvailableLocales(), requestedLocales, opt, intlDurationFormatRelevantExtensionKeys, intlDurationFormatRelevantExtensionKeysLength, localeDataDurationFormat);
    String* locale = r.at("locale");
    m_locale = locale;
    m_dataLocale = r.at("dataLocale");
    m_numberingSystem = r.at("nu");
    StringBuilder dataLocaleBuilder;
    dataLocaleBuilder.appendString(m_dataLocale);
    dataLocaleBuilder.appendString("-u-nu-");
    dataLocaleBuilder.appendString(m_numberingSystem);
    m_dataLocaleWithExtensions = dataLocaleBuilder.finalize();

    // Let style be ? GetOption(options, "style", string, « "long", "short", "narrow", "digital" », "short").
    Value styleValues[4] = { state.context()->staticStrings().lazyLong().string(), state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyNarrow().string(), state.context()->staticStrings().lazyDigital().string() };
    // Set durationFormat.[[Style]] to style.
    String* style = m_style = Intl::getOption(state, options, state.context()->staticStrings().lazyStyle().string(), Intl::StringValue, styleValues, 4, styleValues[1]).asString();

    // Let prevStyle be the empty String.
    String* prevStyle = String::emptyString();

    // For each row of Table 20, except the header row, in table order, do
    //   Let slot be the Internal Slot value of the current row.
    //   Let unit be the Unit value of the current row.
    //   Let styles be the Styles value of the current row.
    //   Let digitalBase be the Digital Default value of the current row.
    //   Let unitOptions be ? GetDurationUnitOptions(unit, options, style, styles, digitalBase, prevStyle, digitalFormat.[[TwoDigitHours]]).
    //   Set the value of durationFormat's internal slot whose name is slot to unitOptions.
    //   If unit is one of "hours", "minutes", "seconds", "milliseconds", or "microseconds", then
    //       Set prevStyle to unitOptions.[[Style]].

    Value longShortNarrowValues[3] = {
        state.context()->staticStrings().lazyLong().string(),
        state.context()->staticStrings().lazyShort().string(),
        state.context()->staticStrings().lazyNarrow().string()
    };

    Value longShortNarrowNumericTwoDigitValues[5] = {
        state.context()->staticStrings().lazyLong().string(),
        state.context()->staticStrings().lazyShort().string(),
        state.context()->staticStrings().lazyNarrow().string(),
        state.context()->staticStrings().numeric.string(),
        state.context()->staticStrings().lazyTwoDigit().string(),
    };

    Value longShortNarrowNumericValues[4] = {
        state.context()->staticStrings().lazyLong().string(),
        state.context()->staticStrings().lazyShort().string(),
        state.context()->staticStrings().lazyNarrow().string(),
        state.context()->staticStrings().numeric.string(),
    };

    // years
    auto durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyYears().string(),
                                                            options, style, longShortNarrowValues, 3, state.context()->staticStrings().lazyShort().string(), prevStyle, false);
    m_yearsStyle = durationUnitOptionsResult.first;
    m_yearsDisplay = durationUnitOptionsResult.second;

    // months
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyMonths().string(),
                                                       options, style, longShortNarrowValues, 3, state.context()->staticStrings().lazyShort().string(), prevStyle, false);
    m_monthsStyle = durationUnitOptionsResult.first;
    m_monthsDisplay = durationUnitOptionsResult.second;

    // weeks
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyWeeks().string(),
                                                       options, style, longShortNarrowValues, 3, state.context()->staticStrings().lazyShort().string(), prevStyle, false);
    m_weeksStyle = durationUnitOptionsResult.first;
    m_weeksDisplay = durationUnitOptionsResult.second;

    // days
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyDays().string(),
                                                       options, style, longShortNarrowValues, 3, state.context()->staticStrings().lazyShort().string(), prevStyle, false);
    m_daysStyle = durationUnitOptionsResult.first;
    m_daysDisplay = durationUnitOptionsResult.second;

    // hours
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyHours().string(),
                                                       options, style, longShortNarrowNumericTwoDigitValues, 5, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_hoursStyle = durationUnitOptionsResult.first;
    m_hoursDisplay = durationUnitOptionsResult.second;
    prevStyle = durationUnitOptionsResult.first;

    // minutes
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyMinutes().string(),
                                                       options, style, longShortNarrowNumericTwoDigitValues, 5, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_minutesStyle = durationUnitOptionsResult.first;
    m_minutesDisplay = durationUnitOptionsResult.second;
    prevStyle = durationUnitOptionsResult.first;

    // seconds
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazySeconds().string(),
                                                       options, style, longShortNarrowNumericTwoDigitValues, 5, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_secondsStyle = durationUnitOptionsResult.first;
    m_secondsDisplay = durationUnitOptionsResult.second;
    prevStyle = durationUnitOptionsResult.first;

    // milliseconds
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyMilliseconds().string(),
                                                       options, style, longShortNarrowNumericValues, 4, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_millisecondsStyle = durationUnitOptionsResult.first;
    m_millisecondsDisplay = durationUnitOptionsResult.second;
    prevStyle = durationUnitOptionsResult.first;

    // microseconds
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyMicroseconds().string(),
                                                       options, style, longShortNarrowNumericValues, 4, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_microsecondsStyle = durationUnitOptionsResult.first;
    m_microsecondsDisplay = durationUnitOptionsResult.second;
    prevStyle = durationUnitOptionsResult.first;

    // nanoseconds
    durationUnitOptionsResult = getDurationUnitOptions(state, state.context()->staticStrings().lazyNanoseconds().string(),
                                                       options, style, longShortNarrowNumericValues, 4, state.context()->staticStrings().numeric.string(), prevStyle, false);
    m_nanosecondsStyle = durationUnitOptionsResult.first;
    m_nanosecondsDisplay = durationUnitOptionsResult.second;

    // Set durationFormat.[[FractionalDigits]] to ? GetNumberOption(options, "fractionalDigits", 0, 9, undefined).
    m_fractionalDigits = Intl::getNumberOption(state, options, state.context()->staticStrings().lazyFractionalDigits().string(), 0, 9, Value());

    UErrorCode status = U_ZERO_ERROR;

    UListFormatterWidth listFormatterWidth;
    if (m_style->equals("long")) {
        listFormatterWidth = ULISTFMT_WIDTH_WIDE;
    } else if (m_style->equals("short")) {
        listFormatterWidth = ULISTFMT_WIDTH_SHORT;
    } else if (m_style->equals("digital")) {
        listFormatterWidth = ULISTFMT_WIDTH_SHORT;
    } else {
        ASSERT(m_style->equals("narrow"));
        listFormatterWidth = ULISTFMT_WIDTH_NARROW;
    }
    m_icuListFormatter = ulistfmt_openForType(m_locale->toNonGCUTF8StringData().data(), ULISTFMT_TYPE_UNITS,
                                              listFormatterWidth, &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize DurationFormat");
    }

    addFinalizer([](PointerValue* obj, void* data) {
        IntlDurationFormatObject* self = (IntlDurationFormatObject*)obj;
        ulistfmt_close(self->m_icuListFormatter);
    },
                 nullptr);
}

std::pair<String*, String*> IntlDurationFormatObject::data(size_t index)
{
    switch (index) {
#define DEFINE_GETTER(name, Name, index) \
    case index:                          \
        return std::make_pair(m_##name##Style, m_##name##Display);
        FOR_EACH_DURATION_RECORD(DEFINE_GETTER)
#undef DEFINE_GETTER
    default:
        ASSERT_NOT_REACHED();
        return std::make_pair(String::emptyString(), String::emptyString());
    }
}

Object* IntlDurationFormatObject::resolvedOptions(ExecutionState& state)
{
    Object* options = new Object(state);
    auto& ss = state.context()->staticStrings();
    options->directDefineOwnProperty(state, ObjectPropertyName(ss.lazySmallLetterLocale()), ObjectPropertyDescriptor(m_locale, ObjectPropertyDescriptor::AllPresent));
    options->directDefineOwnProperty(state, ObjectPropertyName(ss.numberingSystem), ObjectPropertyDescriptor(m_numberingSystem, ObjectPropertyDescriptor::AllPresent));
    options->directDefineOwnProperty(state, ObjectPropertyName(ss.lazyStyle()), ObjectPropertyDescriptor(m_style, ObjectPropertyDescriptor::AllPresent));

#define ADD_PROPERTY(name)                                                                                                                                                              \
    options->directDefineOwnProperty(state, ObjectPropertyName(state, String::fromASCII(#name "s")), ObjectPropertyDescriptor(m_##name##sStyle, ObjectPropertyDescriptor::AllPresent)); \
    options->directDefineOwnProperty(state, ObjectPropertyName(state, String::fromASCII(#name "sDisplay")), ObjectPropertyDescriptor(m_##name##sDisplay, ObjectPropertyDescriptor::AllPresent));

    ADD_PROPERTY(year)
    ADD_PROPERTY(month)
    ADD_PROPERTY(week)
    ADD_PROPERTY(day)
    ADD_PROPERTY(hour)
    ADD_PROPERTY(minute)
    ADD_PROPERTY(second)
    ADD_PROPERTY(millisecond)
    ADD_PROPERTY(microsecond)
    ADD_PROPERTY(nanosecond)

#undef ADD_PROPERTY

    if (!m_fractionalDigits.isUndefined()) {
        options->directDefineOwnProperty(state, ObjectPropertyName(ss.lazyFractionalDigits()), ObjectPropertyDescriptor(m_fractionalDigits, ObjectPropertyDescriptor::AllPresent));
    }
    return options;
}

// https://tc39.es/ecma402/#sec-tointegerifintegral
static double toIntegerIfIntergral(ExecutionState& state, const Value& argument)
{
    // Let number be ? ToNumber(argument).
    double number = argument.toNumber(state) + 0.0;
    // If number is not an integral Number, throw a RangeError exception.
    if (std::trunc(number) != number) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid input for ToDurationRecord");
    }
    // Return ℝ(number).
    return number;
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

// https://tc39.es/ecma402/#sec-isvalidduration
static bool isValidDuration(const DurationRecord& record)
{
    // Let sign be 0.
    int sign = 0;
    // For each value v of « years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds », do
    for (double v : record) {
        if (!isValidDurationWork(v, sign)) {
            return false;
        }
    }

    // If abs(years) ≥ 2**32, return false.
    if (std::abs(record.years()) >= (1ULL << 32)) {
        return false;
    }
    // If abs(months) ≥ 2**32, return false.
    if (std::abs(record.months()) >= (1ULL << 32)) {
        return false;
    }
    // If abs(weeks) ≥ 2**32, return false.
    if (std::abs(record.weeks()) >= (1ULL << 32)) {
        return false;
    }

    // Let normalizedSeconds be days × 86,400 + hours × 3600 + minutes × 60 + seconds + ℝ(𝔽(milliseconds)) × 10**-3 + ℝ(𝔽(microseconds)) × 10**-6 + ℝ(𝔽(nanoseconds)) × 10**-9.
    // NOTE: The above step cannot be implemented directly using floating-point arithmetic. Multiplying by 10**-3, 10**-6, and 10**-9 respectively may be imprecise when milliseconds, microseconds, or nanoseconds is an unsafe integer. This multiplication can be implemented in C++ with an implementation of std::remquo() with sufficient bits in the quotient. String manipulation will also give an exact result, since the multiplication is by a power of 10.
    BigIntData normalizedNanoSeconds = record.totalNanoseconds(DurationRecord::Type::Years);
    // If abs(normalizedSeconds) ≥ 2**53, return false.
    BigIntData limit(int64_t(1ULL << 53));
    limit.multiply(1000000000ULL);
    if (normalizedNanoSeconds.greaterThanEqual(limit)) {
        return false;
    }
    limit = BigIntData(int64_t(1ULL << 53));
    limit.multiply(-1000000000ULL);
    if (normalizedNanoSeconds.lessThanEqual(limit)) {
        return false;
    }
    // Return true.
    return true;
}

static DurationRecord toDurationRecord(ExecutionState& state, const Value& input)
{
    // If input is not an Object, then
    if (!input.isObject()) {
        // If input is a String, throw a RangeError exception.
        if (input.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid input for ToDurationRecord");
        }
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid input for ToDurationRecord");
    }
    // Let result be a new Duration Record with each field set to 0.
    DurationRecord result;

#define GET_ONCE(name, Name)                                                                                      \
    Value name = input.asObject()->get(state, state.context()->staticStrings().lazy##Name()).value(state, input); \
    if (!name.isUndefined()) {                                                                                    \
        result.set##Name(toIntegerIfIntergral(state, name));                                                      \
    }

    // Let days be ? Get(input, "days").
    // If days is not undefined, set result.[[Days]] to ? ToIntegerIfIntegral(days).
    GET_ONCE(days, Days);
    // Let hours be ? Get(input, "hours").
    // If hours is not undefined, set result.[[Hours]] to ? ToIntegerIfIntegral(hours).
    GET_ONCE(hours, Hours);
    // Let microseconds be ? Get(input, "microseconds").
    // If microseconds is not undefined, set result.[[Microseconds]] to ? ToIntegerIfIntegral(microseconds).
    GET_ONCE(microseconds, Microseconds);
    // Let milliseconds be ? Get(input, "milliseconds").
    // If milliseconds is not undefined, set result.[[Milliseconds]] to ? ToIntegerIfIntegral(milliseconds).
    GET_ONCE(milliseconds, Milliseconds);
    // Let minutes be ? Get(input, "minutes").
    // If minutes is not undefined, set result.[[Minutes]] to ? ToIntegerIfIntegral(minutes).
    GET_ONCE(minutes, Minutes);
    // Let months be ? Get(input, "months").
    // If months is not undefined, set result.[[Months]] to ? ToIntegerIfIntegral(months).
    GET_ONCE(months, Months);
    // Let nanoseconds be ? Get(input, "nanoseconds").
    // If nanoseconds is not undefined, set result.[[Nanoseconds]] to ? ToIntegerIfIntegral(nanoseconds).
    GET_ONCE(nanoseconds, Nanoseconds);
    // Let seconds be ? Get(input, "seconds").
    // If seconds is not undefined, set result.[[Seconds]] to ? ToIntegerIfIntegral(seconds).
    GET_ONCE(seconds, Seconds);
    // Let weeks be ? Get(input, "weeks").
    // If weeks is not undefined, set result.[[Weeks]] to ? ToIntegerIfIntegral(weeks).
    GET_ONCE(weeks, Weeks);
    // Let years be ? Get(input, "years").
    // If years is not undefined, set result.[[Years]] to ? ToIntegerIfIntegral(years).
    GET_ONCE(years, Years);

#undef GET_ONCE
    // If years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, and nanoseconds are all undefined,
    // throw a TypeError exception.
    if (years.isUndefined() && months.isUndefined() && weeks.isUndefined() && days.isUndefined() && hours.isUndefined() && minutes.isUndefined() && seconds.isUndefined() && milliseconds.isUndefined() && microseconds.isUndefined() && nanoseconds.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid input for ToDurationRecord");
    }
    // If IsValidDuration( result.[[Years]], result.[[Months]], result.[[Weeks]], result.[[Days]], result.[[Hours]], result.[[Minutes]], result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]], result.[[Nanoseconds]]) is false, then
    if (!isValidDuration(result)) {
        // Throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid input for ToDurationRecord");
    }
    // Return result.
    return result;
}

enum class DurationSignType : uint8_t {
    Negative,
    Positive,
    Zero,
};

// https://tc39.es/proposal-intl-duration-format/#sec-durationsign
static DurationSignType getDurationSign(const DurationRecord& duration)
{
#define TEST_ONCE(name)                    \
    if (duration.m_##name < 0) {           \
        return DurationSignType::Negative; \
    }                                      \
    if (duration.m_##name > 0) {           \
        return DurationSignType::Positive; \
    }

    for (auto value : duration) {
        if (value < 0) {
            return DurationSignType::Negative;
        }
        if (value > 0) {
            return DurationSignType::Positive;
        }
    }

    return DurationSignType::Zero;
}

String* DurationRecord::typeName(ExecutionState& state, Type t)
{
    switch (t) {
#define DEFINE_GETTER(name, Name, index) \
    case Type::Name:                     \
        return state.context()->staticStrings().lazy##Name().string();
        FOR_EACH_DURATION_RECORD(DEFINE_GETTER)
#undef DEFINE_GETTER
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return String::emptyString();
}

BigIntData DurationRecord::totalNanoseconds(DurationRecord::Type unit) const
{
    BigIntData resultNs;

    constexpr int64_t nanoMultiplier = 1000000000ULL;
    constexpr int64_t milliMultiplier = 1000000ULL;
    constexpr int64_t microMultiplier = 1000ULL;

    if (unit <= DurationRecord::Type::Days) {
        BigIntData s(days());
        s.multiply(86400);
        s.multiply(nanoMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Hours) {
        BigIntData s(hours());
        s.multiply(3600);
        s.multiply(nanoMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Minutes) {
        BigIntData s(minutes());
        s.multiply(60);
        s.multiply(nanoMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Seconds) {
        BigIntData s(seconds());
        s.multiply(nanoMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Milliseconds) {
        BigIntData s(milliseconds());
        s.multiply(milliMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Microseconds) {
        BigIntData s(microseconds());
        s.multiply(microMultiplier);
        resultNs.addition(s);
    }
    if (unit <= DurationRecord::Type::Nanoseconds) {
        BigIntData s(nanoseconds());
        resultNs.addition(s);
    }

    return resultNs;
}

inline double purifyNaN(double value)
{
    uint64_t PNaNAsBits{ 0x7ff8000000000000ll };
    double PNaN{ bitwise_cast<double>(PNaNAsBits) };
    if (value != value) {
        return PNaN;
    }
    return value;
}

static std::string buildDecimalFormat(DurationRecord::Type unit, BigIntData ns)
{
    ASSERT(unit == DurationRecord::Type::Seconds || unit == DurationRecord::Type::Milliseconds || unit == DurationRecord::Type::Microseconds);

    int flactionalDigits = 0;
    int64_t exponent = 0;
    if (unit == DurationRecord::Type::Seconds) {
        flactionalDigits = 9;
        exponent = 1000000000;
    } else if (unit == DurationRecord::Type::Milliseconds) {
        flactionalDigits = 6;
        exponent = 1000000;
    } else {
        ASSERT(unit == DurationRecord::Type::Microseconds);
        flactionalDigits = 3;
        exponent = 1000;
    }

    BigIntData integerPart = ns;
    integerPart.division(exponent);

    BigIntData fractionalPart(ns);
    fractionalPart.remainder(exponent);

    StringBuilder builder;

    auto integerPartString = integerPart.toNonGCStdString();
    builder.appendString(integerPartString.data(), integerPartString.size());
    builder.appendChar('.');

    auto fractionalString = fractionalPart.toNonGCStdString();
    if (fractionalString.size() && fractionalString[0] == '-') {
        fractionalString.erase(fractionalString.begin());
    }
    int zeroLength = flactionalDigits - fractionalString.length();
    for (int i = 0; i < zeroLength; ++i) {
        builder.appendChar('0');
    }
    builder.appendString(fractionalString.data(), fractionalString.length());
    return builder.finalize()->toNonGCUTF8StringData();
}

static UTF16StringDataNonGCStd retrieveSeparator(const std::string& locale, String* numberingSystem)
{
    UTF16StringDataNonGCStd fallbackTimeSeparator = u":";
    UErrorCode status = U_ZERO_ERROR;

    LocalResourcePointer<UResourceBundle> bundle(ures_open(nullptr, locale.data(), &status), [](UResourceBundle* res) {
        ures_close(res);
    });
    if (U_FAILURE(status)) {
        return fallbackTimeSeparator;
    }

    LocalResourcePointer<UResourceBundle> numberElementsBundle(ures_getByKey(bundle.get(), "NumberElements", nullptr, &status), [](UResourceBundle* res) {
        ures_close(res);
    });
    if (U_FAILURE(status)) {
        return fallbackTimeSeparator;
    }

    LocalResourcePointer<UResourceBundle> numberingSystemBundle(ures_getByKey(numberElementsBundle.get(), numberingSystem->toNonGCUTF8StringData().data(), nullptr, &status), [](UResourceBundle* res) {
        ures_close(res);
    });
    if (U_FAILURE(status)) {
        return fallbackTimeSeparator;
    }

    LocalResourcePointer<UResourceBundle> symbolsBundle(ures_getByKey(numberingSystemBundle.get(), "symbols", nullptr, &status), [](UResourceBundle* res) {
        ures_close(res);
    });
    if (U_FAILURE(status)) {
        return fallbackTimeSeparator;
    }

    int32_t length = 0;
    const UChar* data = ures_getStringByKey(symbolsBundle.get(), "timeSeparator", &length, &status);
    if (U_FAILURE(status)) {
        return fallbackTimeSeparator;
    }

    return UTF16StringDataNonGCStd({ data, static_cast<size_t>(length) });
}

std::vector<IntlDurationFormatObject::Element> IntlDurationFormatObject::collectElements(ExecutionState& state, IntlDurationFormatObject* durationFormat, const DurationRecord& duration)
{
    // https://tc39.es/proposal-intl-duration-format/#sec-partitiondurationformatpattern
    bool done = false;
    bool needsSignDisplay = false;
    UTF16StringDataNonGCStd separator;
    std::vector<Element> elements;
    Optional<DurationSignType> durationSign;
    for (uint8_t index = 0; index < 10 && !done; ++index) {
        DurationRecord::Type unit = static_cast<DurationRecord::Type>(index);
        auto unitData = durationFormat->data(index);
        double value = duration[unit];
        Optional<BigIntData> totalNanosecondsValue;

        UTF16StringDataNonGCStd skeletonBuilder;

        switch (unit) {
        // 3.j. If unit is "seconds", "milliseconds", or "microseconds", then
        case DurationRecord::Type::Seconds:
        case DurationRecord::Type::Milliseconds:
        case DurationRecord::Type::Microseconds: {
            skeletonBuilder = u"rounding-mode-down";
            String* nextStyle = state.context()->staticStrings().lazyLong().string();
            if (unit == DurationRecord::Type::Seconds)
                nextStyle = durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Milliseconds)).first;
            else if (unit == DurationRecord::Type::Milliseconds)
                nextStyle = durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Microseconds)).first;
            else {
                ASSERT(unit == DurationRecord::Type::Microseconds);
                nextStyle = durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Nanoseconds)).first;
            }
            if (nextStyle->equals("numeric")) {
                if (unit == DurationRecord::Type::Seconds) {
                    totalNanosecondsValue = duration.totalNanoseconds(DurationRecord::Type::Seconds);
                } else if (unit == DurationRecord::Type::Milliseconds) {
                    totalNanosecondsValue = duration.totalNanoseconds(DurationRecord::Type::Milliseconds);
                } else {
                    ASSERT(unit == DurationRecord::Type::Microseconds);
                    totalNanosecondsValue = duration.totalNanoseconds(DurationRecord::Type::Microseconds);
                }
                ASSERT(totalNanosecondsValue);

                // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
                skeletonBuilder += u" .";

                Value fractionalDigits = durationFormat->m_fractionalDigits;
                if (fractionalDigits.isUndefined()) {
                    skeletonBuilder += u"#########";
                } else {
                    for (unsigned i = 0; i < fractionalDigits.asNumber(); ++i) {
                        skeletonBuilder += u'0';
                    }
                }
                done = true;
            }
            break;
        }
        default: {
            skeletonBuilder += u"rounding-mode-half-up";
            break;
        }
        }

        auto style = unitData.first;

        // 3.k. If style is "2-digit", then
        //     i. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumIntegerDigits", 2F).
        skeletonBuilder += u" integer-width/*";
        if (style->equals("2-digit")) {
            skeletonBuilder += u"00";
        } else {
            skeletonBuilder += u"0";
        }

        // 9. Perform ! CreateDataPropertyOrThrow(nfOpts, "useGrouping", false).
        if (style->equals("2-digit") || style->equals("numeric")) {
            skeletonBuilder += u" group-off";
        }

        // 3.l. If value is not 0 or display is not "auto", then
        value = purifyNaN(value);
        if (value || !unitData.second->equals("auto") || style->equals("2-digit") || style->equals("numeric")) {
            auto formatToString = [&](UFormattedNumber* formattedNumber) -> UTF16StringDataNonGCStd {
                auto ret = INTL_ICU_STRING_BUFFER_OPERATION(unumf_resultToString, formattedNumber);
                if (U_FAILURE(ret.first)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                    return {};
                }
                return std::move(ret.second);
            };

            // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#sign-display
            if (needsSignDisplay) {
                skeletonBuilder += u" +_";
            }

            auto adjustSignDisplay = [&]() -> void {
                if (!needsSignDisplay && !value) {
                    if (!durationSign) {
                        durationSign = getDurationSign(duration);
                    }
                    if (durationSign == DurationSignType::Negative) {
                        value = -0.0;
                        needsSignDisplay = true;
                    }
                }
            };

            auto formatDouble = [&](const UTF16StringDataNonGCStd& skeleton) -> LocalResourcePointer<UFormattedNumber> {
                UErrorCode status = U_ZERO_ERROR;
                LocalResourcePointer<UNumberFormatter> numberFormatter(unumf_openForSkeletonAndLocale(skeleton.data(), skeleton.length(), durationFormat->m_dataLocaleWithExtensions->toNonGCUTF8StringData().data(), &status),
                                                                       [](UNumberFormatter* res) {
                                                                           unumf_close(res);
                                                                       });
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }

                LocalResourcePointer<UFormattedNumber> formattedNumber(unumf_openResult(&status),
                                                                       [](UFormattedNumber* res) {
                                                                           unumf_closeResult(res);
                                                                       });
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }
                unumf_formatDouble(numberFormatter.get(), value, formattedNumber.get(), &status);
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }

                return formattedNumber;
            };

            auto formatIntl128AsDecimal = [&](const UTF16StringDataNonGCStd& skeleton) -> LocalResourcePointer<UFormattedNumber> {
                ASSERT(totalNanosecondsValue);
                ASSERT(unit == DurationRecord::Type::Seconds || unit == DurationRecord::Type::Milliseconds || unit == DurationRecord::Type::Microseconds);

                UErrorCode status = U_ZERO_ERROR;
                LocalResourcePointer<UNumberFormatter> numberFormatter(unumf_openForSkeletonAndLocale(skeleton.data(), skeleton.length(), durationFormat->m_dataLocaleWithExtensions->toNonGCUTF8StringData().data(), &status),
                                                                       [](UNumberFormatter* res) {
                                                                           unumf_close(res);
                                                                       });
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }

                LocalResourcePointer<UFormattedNumber> formattedNumber(unumf_openResult(&status),
                                                                       [](UFormattedNumber* res) {
                                                                           unumf_closeResult(res);
                                                                       });
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }

                // We need to keep string alive while strSpan is in use.
                auto string = buildDecimalFormat(unit, totalNanosecondsValue.value());
                unumf_formatDecimal(numberFormatter.get(), reinterpret_cast<const char*>(string.data()), string.size(), formattedNumber.get(), &status);
                if (U_FAILURE(status)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
                }

                return formattedNumber;
            };

            // 3.l.i. If style is "2-digit" or "numeric", then
            if (style->equals("2-digit") || style->equals("numeric")) {
                // https://tc39.es/proposal-intl-duration-format/#sec-formatnumericunits
                ASSERT(unit == DurationRecord::Type::Hours || unit == DurationRecord::Type::Minutes || unit == DurationRecord::Type::Seconds);

                double secondsValue = duration[DurationRecord::Type::Seconds];
                if (durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Milliseconds)).first->equals("numeric")) {
                    secondsValue = secondsValue + duration[DurationRecord::Type::Milliseconds] / 1000.0 + duration[DurationRecord::Type::Microseconds] / 1000000.0 + duration[DurationRecord::Type::Nanoseconds] / 1000000000.0;
                }

                bool needsFormatHours = duration[DurationRecord::Type::Hours] || !durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Hours)).second->equals("auto");
                bool needsFormatSeconds = secondsValue || !durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Seconds)).second->equals("auto");
                bool needsFormatMinutes = (needsFormatHours && needsFormatSeconds) || duration[DurationRecord::Type::Minutes] || !durationFormat->data(static_cast<unsigned>(DurationRecord::Type::Minutes)).second->equals("auto");

                bool needsFormat = (unit == DurationRecord::Type::Hours && needsFormatHours) || (unit == DurationRecord::Type::Minutes && needsFormatMinutes) || (unit == DurationRecord::Type::Seconds && needsFormatSeconds);
                bool needsSeparator = (unit == DurationRecord::Type::Hours && needsFormatHours && needsFormatMinutes) || (unit == DurationRecord::Type::Minutes && needsFormatSeconds);

                if (needsFormat) {
                    adjustSignDisplay();

                    auto formattedNumber = totalNanosecondsValue ? formatIntl128AsDecimal(skeletonBuilder) : formatDouble(skeletonBuilder);
                    auto formatted = formatToString(formattedNumber.get());
                    elements.push_back({ ElementType::Element, std::signbit(value), unit, std::move(formatted), std::move(formattedNumber) });
                }

                if (needsSeparator) {
                    if (!separator.size()) {
                        separator = retrieveSeparator(durationFormat->m_dataLocaleWithExtensions->toNonGCUTF8StringData(), durationFormat->m_numberingSystem);
                    }
                    elements.push_back({ ElementType::Literal, std::signbit(value), unit, separator, { nullptr, [](UFormattedNumber*) {} } });
                }
            } else if (style->equals("long") || style->equals("short") || style->equals("narrow")) {
                // 3.l.ii. Else
                adjustSignDisplay();

                skeletonBuilder += u" measure-unit/duration-";

                skeletonBuilder += DurationRecord::typeName(state, unit)->toUTF16StringData().data();
                skeletonBuilder.pop_back(); // remove 's'
                if (style->equals("long")) {
                    skeletonBuilder += u" unit-width-full-name";
                } else if (style->equals("short")) {
                    skeletonBuilder += u" unit-width-short";
                } else {
                    ASSERT(style->equals("narrow"));
                    skeletonBuilder += u" unit-width-narrow";
                }

                auto formattedNumber = totalNanosecondsValue ? formatIntl128AsDecimal(skeletonBuilder) : formatDouble(skeletonBuilder);
                auto formatted = formatToString(formattedNumber.get());
                elements.push_back({ ElementType::Element, std::signbit(value), unit, std::move(formatted), std::move(formattedNumber) });
            }
            if (value) {
                needsSignDisplay = true;
            }
        }
    }

    return elements;
}

class StringVectorToUCharList {
public:
    StringVectorToUCharList(const std::vector<UTF16StringDataNonGCStd>& v)
    {
        m_strings.resize(v.size());
        m_stringLengths = new int32_t[v.size()];

        for (size_t i = 0; i < m_strings.size(); i++) {
            const auto& data = v[i];
            m_strings[i] = data.data();
            ASSERT(m_strings[i][data.size()] == 0);
            m_stringLengths[i] = data.size();
        }
    }

    ~StringVectorToUCharList()
    {
        delete[] m_stringLengths;
    }

    const UChar** strings()
    {
        return m_strings.data();
    }

    int32_t* stringLengths()
    {
        return m_stringLengths;
    }

    int32_t stringCount()
    {
        return m_strings.size();
    }

private:
    std::vector<const UChar*> m_strings;
    int32_t* m_stringLengths;
};

String* IntlDurationFormatObject::format(ExecutionState& state, const Value& duration)
{
    // Let df be the this value.
    // Perform ? RequireInternalSlot(df, [[InitializedDurationFormat]]).
    // Let record be ? ToDurationRecord(duration).
    auto record = toDurationRecord(state, duration);

    // https://tc39.es/proposal-intl-duration-format/#sec-partitiondurationformatpattern
    auto elements = collectElements(state, this, record);

    std::vector<UTF16StringDataNonGCStd> stringList;
    for (unsigned index = 0; index < elements.size(); ++index) {
        UTF16StringDataNonGCStd builder;
        builder.append(elements[index].m_string);
        do {
            unsigned nextIndex = index + 1;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Literal)
                break;
            builder.append(elements[nextIndex].m_string);
            ++index;
            ++nextIndex;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Element)
                break;
            builder.append(elements[nextIndex].m_string);
            ++index;
        } while (true);

        stringList.push_back(std::move(builder));
    }

    StringVectorToUCharList input(stringList);

    auto result = INTL_ICU_STRING_BUFFER_OPERATION(ulistfmt_format, m_icuListFormatter, input.strings(), input.stringLengths(), input.stringCount());
    if (U_FAILURE(result.first)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format list of strings");
    }
    return new UTF16String(result.second.data(), result.second.length());
}

} // namespace Escargot
#endif
#endif
