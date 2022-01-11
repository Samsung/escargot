#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (C) 2015 Andy VanWagoner (thetalecrafter@gmail.com)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
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
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "runtime/VMInstance.h"
#include "runtime/Value.h"
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"
#include "Intl.h"
#include "IntlDateTimeFormat.h"

namespace Escargot {

static const char* const intlDateTimeFormatRelevantExtensionKeys[3] = { "ca", "nu", "hc" };
static size_t intlDateTimeFormatRelevantExtensionKeysLength = 3;
static const size_t indexOfExtensionKeyCa = 0;
static const size_t indexOfExtensionKeyNu = 1;
static const size_t indexOfExtensionKeyHc = 2;
static const double minECMAScriptTime = -8.64E15;

std::vector<std::string> Intl::calendarsForLocale(String* locale)
{
    std::vector<std::string> keyLocaleData;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* calendars = ucal_getKeywordValuesForLocale("calendar", locale->toUTF8StringData().data(), false, &status);
    ASSERT(U_SUCCESS(status));

    status = U_ZERO_ERROR;
    int32_t nameLength;
    while (const char* availableName = uenum_next(calendars, &nameLength, &status)) {
        ASSERT(U_SUCCESS(status));
        status = U_ZERO_ERROR;
        std::string calendar = std::string(availableName, nameLength);
        keyLocaleData.push_back(Intl::convertICUCalendarKeywordToBCP47KeywordIfNeeds(calendar));
    }
    uenum_close(calendars);

    return keyLocaleData;
}

static std::vector<std::string> localeDataDateTimeFormat(String* locale, size_t keyIndex)
{
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCa:
        keyLocaleData = Intl::calendarsForLocale(locale);
        break;
    case indexOfExtensionKeyNu:
        keyLocaleData = Intl::numberingSystemsForLocale(locale);
        break;
    case indexOfExtensionKeyHc:
        keyLocaleData.push_back("h12");
        keyLocaleData.push_back("h23");
        keyLocaleData.push_back("h24");
        keyLocaleData.push_back("h11");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

static bool equalIgnoringASCIICase(String* timeZoneName, const UTF16StringDataNonGCStd& ianaTimeZoneView)
{
    if (timeZoneName->length() != ianaTimeZoneView.size()) {
        return false;
    }
    for (size_t i = 0; i < ianaTimeZoneView.size(); i++) {
        if (tolower(ianaTimeZoneView[i]) != tolower(timeZoneName->charAt(i))) {
            return false;
        }
    }
    return true;
}

static String* canonicalizeTimeZoneName(ExecutionState& state, String* timeZoneName)
{
    // 6.4.1 IsValidTimeZoneName (timeZone)
    // The abstract operation returns true if timeZone, converted to upper case as described in 6.1, is equal to one of the Zone or Link names of the IANA Time Zone Database, converted to upper case as described in 6.1. It returns false otherwise.
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* timeZones = ucal_openTimeZones(&status);
    ASSERT(U_SUCCESS(status));

    String* canonical = String::emptyString;
    do {
        status = U_ZERO_ERROR;
        int32_t ianaTimeZoneLength;
        // Time zone names are respresented as UChar[] in all related ICU apis.
        const UChar* ianaTimeZone = uenum_unext(timeZones, &ianaTimeZoneLength, &status);
        ASSERT(U_SUCCESS(status));

        // End of enumeration.
        if (!ianaTimeZone)
            break;

        UTF16StringDataNonGCStd ianaTimeZoneView((char16_t*)ianaTimeZone, ianaTimeZoneLength);
        if (!equalIgnoringASCIICase(timeZoneName, ianaTimeZoneView))
            continue;

        // Found a match, now canonicalize.
        // 6.4.2 CanonicalizeTimeZoneName (timeZone) (ECMA-402 2.0)
        // 1. Let ianaTimeZone be the Zone or Link name of the IANA Time Zone Database such that timeZone, converted to upper case as described in 6.1, is equal to ianaTimeZone, converted to upper case as described in 6.1.
        // 2. If ianaTimeZone is a Link name, then let ianaTimeZone be the corresponding Zone name as specified in the “backward” file of the IANA Time Zone Database.

        UTF16StringDataNonGCStd buffer;
        buffer.resize(ianaTimeZoneLength);
        UBool isSystemID = false;
        status = U_ZERO_ERROR;
        auto canonicalLength = ucal_getCanonicalTimeZoneID(ianaTimeZone, ianaTimeZoneLength, (UChar*)buffer.data(), ianaTimeZoneLength, &isSystemID, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            buffer.resize(canonicalLength);
            isSystemID = false;
            status = U_ZERO_ERROR;
            ucal_getCanonicalTimeZoneID(ianaTimeZone, ianaTimeZoneLength, (UChar*)buffer.data(), canonicalLength, &isSystemID, &status);
        }
        ASSERT(U_SUCCESS(status));
        canonical = new UTF16String(buffer.data(), canonicalLength);
    } while (canonical->length() == 0);
    uenum_close(timeZones);

    // 3. If ianaTimeZone is "Etc/UTC" or "Etc/GMT", then return "UTC".
    if (canonical->equals("Etc/UTC") || canonical->equals("Etc/GMT")) {
        canonical = state.context()->staticStrings().UTC.string();
    }

    // 4. Return ianaTimeZone.
    return canonical;
}


static void toDateTimeOptionsTest(ExecutionState& state, Value options, AtomicString name, bool& needDefaults)
{
    Value r = options.asObject()->get(state, name).value(state, options.asObject());
    if (!r.isUndefined()) {
        needDefaults = false;
    }
}

static String* defaultTimeZone(ExecutionState& state)
{
#if !defined(OS_WINDOWS_UWP)
    state.context()->vmInstance()->ensureTimezone();
#endif
    return String::fromUTF8(state.context()->vmInstance()->timezoneID().data(), state.context()->vmInstance()->timezoneID().length());
}

std::string IntlDateTimeFormatObject::readHourCycleFromPattern(const UTF16StringDataNonGCStd& patternString)
{
    bool inQuote = false;
    for (size_t i = 0; i < patternString.length(); i++) {
        auto ch = patternString[i];
        switch (ch) {
        case '\'':
            inQuote = !inQuote;
            break;
        case 'K':
            if (!inQuote) {
                return "h11";
            }
            break;
        case 'h':
            if (!inQuote) {
                return "h12";
            }
            break;
        case 'H':
            if (!inQuote) {
                return "h23";
            }
            break;
        case 'k':
            if (!inQuote) {
                return "h24";
            }
            break;
        }
    }
    return "";
}

UTF16StringDataNonGCStd updateHourCycleInPatternDueToHourCycle(const UTF16StringDataNonGCStd& pattern, String* hc)
{
    char16_t newHcChar;
    if (hc->equals("h11")) {
        newHcChar = 'K';
    } else if (hc->equals("h12")) {
        newHcChar = 'h';
    } else if (hc->equals("h23")) {
        newHcChar = 'H';
    } else {
        ASSERT(hc->equals("h24"));
        newHcChar = 'k';
    }
    bool inQuote = false;
    UTF16StringDataNonGCStd result;
    for (size_t i = 0; i < pattern.length(); i++) {
        auto ch = pattern[i];
        switch (ch) {
        case '\'':
            inQuote = !inQuote;
            result += ch;
            break;
        case 'K':
        case 'k':
        case 'H':
        case 'h':
            result += inQuote ? ch : newHcChar;
            break;
        default:
            result += ch;
            break;
        }
    }

    return result;
}


static String* icuFieldTypeToPartName(ExecutionState& state, int32_t fieldName)
{
    if (fieldName == -1) {
        return state.context()->staticStrings().lazyLiteral().string();
    }

    switch (static_cast<UDateFormatField>(fieldName)) {
    case UDAT_ERA_FIELD:
        return state.context()->staticStrings().lazyEra().string();
    case UDAT_YEAR_FIELD:
    case UDAT_YEAR_WOY_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
    case UDAT_YEAR_NAME_FIELD:
        return state.context()->staticStrings().lazyYear().string();
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        return state.context()->staticStrings().lazyMonth().string();
    case UDAT_DATE_FIELD:
    case UDAT_JULIAN_DAY_FIELD:
        return state.context()->staticStrings().lazyDay().string();
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
        return state.context()->staticStrings().lazyHour().string();
    case UDAT_MINUTE_FIELD:
        return state.context()->staticStrings().lazyMinute().string();
    case UDAT_SECOND_FIELD:
        return state.context()->staticStrings().lazySecond().string();
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
        return state.context()->staticStrings().lazyWeekday().string();
    case UDAT_AM_PM_FIELD:
    case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
        return state.context()->staticStrings().lazyDayPeriod().string();
    case UDAT_TIMEZONE_FIELD:
        return state.context()->staticStrings().lazyTimeZoneName().string();
    case UDAT_FRACTIONAL_SECOND_FIELD:
        return state.context()->staticStrings().lazyFractionalSecond().string();
    default:
        ASSERT_NOT_REACHED();
        return String::emptyString;
    }
}

IntlDateTimeFormatObject::IntlDateTimeFormatObject(ExecutionState& state, Value locales, Value options)
    : IntlDateTimeFormatObject(state, state.context()->globalObject()->intlDateTimeFormatPrototype(), locales, options)
{
}

IntlDateTimeFormatObject::IntlDateTimeFormatObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : Object(state, proto)
    , m_locale(String::emptyString)
    , m_calendar(String::emptyString)
    , m_numberingSystem(String::emptyString)
    , m_timeZone(String::emptyString)
{
    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // Let options be ? ToDateTimeOptions(options, "any", "date").
    options = toDateTimeOptions(state, options, state.context()->staticStrings().lazyAny().string(), state.context()->staticStrings().lazyDate().string());
    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    // Set opt.[[localeMatcher]] to matcher.
    Value matcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    Value matcher = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);

    // Let calendar be ? GetOption(options, "calendar", "string", undefined, undefined).
    Value calendar = Intl::getOption(state, options.asObject(), state.context()->staticStrings().calendar.string(), Intl::StringValue, nullptr, 0, Value());
    // If calendar is not undefined, then
    if (!calendar.isUndefined()) {
        // If calendar does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(calendar.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "The calendar value you gave is not valid");
        }
    }

    // Set opt.[[ca]] to calendar.
    if (!calendar.isUndefined()) {
        opt.insert(std::make_pair("ca", calendar.toString(state)));
    }

    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem = Intl::getOption(state, options.asObject(), state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value());
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(numberingSystem.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "The numberingSystem value you gave is not valid");
        }
    }
    // Set opt.[[nu]] to numberingSystem.
    if (!numberingSystem.isUndefined()) {
        opt.insert(std::make_pair("nu", numberingSystem.toString(state)));
    }

    // Let hour12 be ? GetOption(options, "hour12", "boolean", undefined, undefined).
    Value hour12 = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyHour12().string(), Intl::BooleanValue, nullptr, 0, Value());

    // Let hourCycle be ? GetOption(options, "hourCycle", "string", « "h11", "h12", "h23", "h24" », undefined).
    Value hourCycleValue[4] = { state.context()->staticStrings().lazyH11().string(), state.context()->staticStrings().lazyH12().string(), state.context()->staticStrings().lazyH23().string(), state.context()->staticStrings().lazyH24().string() };
    Value hourCycle = Intl::getOption(state, options.asObject(), state.context()->staticStrings().hourCycle.string(), Intl::StringValue, hourCycleValue, 4, Value());
    // If hour12 is not undefined, then
    if (!hour12.isUndefined()) {
        // Let hourCycle be null.
        hourCycle = Value(Value::Null);
    }
    // Set opt.[[hc]] to hourCycle.
    if (!hourCycle.isUndefinedOrNull()) {
        opt.insert(std::make_pair("hc", hourCycle.toString(state)));
    }

    // Let localeData be %DateTimeFormat%.[[LocaleData]].
    const auto& availableLocales = state.context()->vmInstance()->intlDateTimeFormatAvailableLocales();
    // Let r be ResolveLocale(%DateTimeFormat%.[[AvailableLocales]], requestedLocales, opt, %DateTimeFormat%.[[RelevantExtensionKeys]], localeData).
    StringMap r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, intlDateTimeFormatRelevantExtensionKeys, intlDateTimeFormatRelevantExtensionKeysLength, localeDataDateTimeFormat);

    // The resolved locale doesn't include a hc Unicode extension value if the hour12 or hourCycle option is also present.
    if (!hour12.isUndefined() || !hourCycle.isUndefinedOrNull()) {
        auto iter = r.find("locale");
        String* locale = iter->second;
        iter->second = Intl::canonicalizeLanguageTag(locale->toNonGCUTF8StringData(), "hc").canonicalizedTag.value();
    }

    // Set dateTimeFormat.[f[Locale]] to r.[[locale]].
    m_locale = r.at("locale");
    // Let calendar be r.[[ca]].
    calendar = r.at("ca");
    // Set dateTimeFormat.[[Calendar]] to calendar.
    m_calendar = calendar.asString();
    // Set dateTimeFormat.[[HourCycle]] to r.[[hc]].
    m_hourCycle = r.at("hc");
    // Set dateTimeFormat.[[NumberingSystem]] to r.[[nu]].
    m_numberingSystem = r.at("nu");

    // Let dataLocale be r.[[dataLocale]].
    Value dataLocale = r.at("dataLocale");
    std::string dataLocaleWithExtensions = dataLocale.toString(state)->toNonGCUTF8StringData() + "-u-ca-" + m_calendar->toNonGCUTF8StringData() + "-nu-" + m_numberingSystem->toNonGCUTF8StringData();

    // Let timeZone be ? Get(options, "timeZone").
    Value timeZone = options.asObject()->get(state, state.context()->staticStrings().lazyTimeZone()).value(state, options.asObject());
    // If timeZone is undefined, then
    if (timeZone.isUndefined()) {
        // Let timeZone be DefaultTimeZone().
        timeZone = defaultTimeZone(state);
    } else {
        // Else,
        // Let timeZone be ? ToString(timeZone).
        // If the result of IsValidTimeZoneName(timeZone) is false, then Throw a RangeError exception.
        // Let timeZone be CanonicalizeTimeZoneName(timeZone).
        String* tzString = timeZone.toString(state);
        timeZone = canonicalizeTimeZoneName(state, tzString);
        tzString = timeZone.toString(state);
        if (tzString->length() == 0) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid timezone value");
        }
    }

    // Set dateTimeFormat.[[TimeZone]] to timeZone.
    if (!timeZone.isUndefined()) {
        m_timeZone = timeZone.asString();
    }

    // Let opt be a new Record.
    opt.clear();

    // Table 4: Components of date and time formats
    // [[Weekday]]     "weekday"   "narrow", "short", "long"
    // [[Era]]     "era"   "narrow", "short", "long"
    // [[Year]]    "year"  "2-digit", "numeric"
    // [[Month]]   "month"     "2-digit", "numeric", "narrow", "short", "long"
    // [[Day]]     "day"   "2-digit", "numeric"
    // [[DayPeriod]]   "dayPeriod"     "narrow", "short", "long"
    // [[Hour]]    "hour"  "2-digit", "numeric"
    // [[Minute]]  "minute"    "2-digit", "numeric"
    // [[Second]]  "second"    "2-digit", "numeric"
    // [[FractionalSecondDigits]]  "fractionalSecondDigits"    1𝔽, 2𝔽, 3𝔽
    // [[TimeZoneName]]    "timeZoneName"  "short", "long"

    std::function<void(String * prop, Value * values, size_t valuesSize)> doTable4 = [&](String* prop, Value* values, size_t valuesSize) {
        Value value = Intl::getOption(state, options.asObject(), prop, Intl::StringValue, values, valuesSize, Value());
        // Set opt.[[<prop>]] to value.
        if (!value.isUndefined()) {
            opt.insert(std::make_pair(prop->toNonGCUTF8StringData(), value.toString(state)));
        }
    };
    StringBuilder skeletonBuilder;

    // For each row of Table 4, except the header row, in table order, do
    // Let prop be the name given in the Property column of the row.
    // If prop is "fractionalSecondDigits", then
    //   Let value be ? GetNumberOption(options, "fractionalSecondDigits", 1, 3, undefined).
    // Else,
    //   Let value be ? GetOption(options, prop, "string", « the strings given in the Values column of the row », undefined).
    // Set opt.[[<prop>]] to value.

    Value narrowShortLongValues[3] = { state.context()->staticStrings().lazyNarrow().string(), state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyLong().string() };

    doTable4(state.context()->staticStrings().lazyWeekday().string(), narrowShortLongValues, 3);

    String* ret = opt.at("weekday");

    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("EEEEE");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("EEE");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("EEEE");
    }

    doTable4(state.context()->staticStrings().lazyEra().string(), narrowShortLongValues, 3);

    ret = opt.at("era");
    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("GGGGG");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("GGG");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("GGGG");
    }

    Value twoDightNumericValues[2] = { state.context()->staticStrings().lazyTwoDigit().string(), state.context()->staticStrings().numeric.string() };
    doTable4(state.context()->staticStrings().lazyYear().string(), twoDightNumericValues, 2);

    ret = opt.at("year");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("yy");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("y");
    }

    Value allValues[5] = { state.context()->staticStrings().lazyTwoDigit().string(), state.context()->staticStrings().numeric.string(), state.context()->staticStrings().lazyNarrow().string(), state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyLong().string() };
    doTable4(state.context()->staticStrings().lazyMonth().string(), allValues, 5);

    ret = opt.at("month");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("MM");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("M");
    } else if (ret->equals("narrow")) {
        skeletonBuilder.appendString("MMMMM");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("MMM");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("MMMM");
    }

    doTable4(state.context()->staticStrings().lazyDay().string(), twoDightNumericValues, 2);

    ret = opt.at("day");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("dd");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("d");
    }

    doTable4(state.context()->staticStrings().lazyDayPeriod().string(), narrowShortLongValues, 3);
    String* dayPeriodString = opt.at("dayPeriod");

    doTable4(state.context()->staticStrings().lazyHour().string(), twoDightNumericValues, 2);

    String* hour = ret = opt.at("hour");
    Value hr12Value = hour12;
    bool isHour12Undefined = hr12Value.isUndefined();
    bool hr12 = hr12Value.toBoolean(state);

    if (isHour12Undefined) {
        if (ret->equals("2-digit"))
            skeletonBuilder.appendString("jj");
        else if (ret->equals("numeric"))
            skeletonBuilder.appendChar('j');
    } else if (hr12) {
        if (ret->equals("2-digit"))
            skeletonBuilder.appendString("hh");
        else if (ret->equals("numeric"))
            skeletonBuilder.appendChar('h');
    } else {
        if (ret->equals("2-digit"))
            skeletonBuilder.appendString("HH");
        else if (ret->equals("numeric"))
            skeletonBuilder.appendChar('H');
    }

    // dayPeriod must be set after setting hour.
    // https://unicode-org.atlassian.net/browse/ICU-20731
    if (dayPeriodString->equals("narrow")) {
        skeletonBuilder.appendString("BBBBB");
    } else if (dayPeriodString->equals("short")) {
        skeletonBuilder.appendString("B");
    } else if (dayPeriodString->equals("long")) {
        skeletonBuilder.appendString("BBBB");
    }

    doTable4(state.context()->staticStrings().lazyMinute().string(), twoDightNumericValues, 2);

    ret = opt.at("minute");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("mm");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("m");
    }

    doTable4(state.context()->staticStrings().lazySecond().string(), twoDightNumericValues, 2);

    ret = opt.at("second");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("ss");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("s");
    }

    Value fractionalSecondDigits = Intl::getNumberOption(state, options.asObject(), state.context()->staticStrings().lazyFractionalSecondDigits().string(), 1, 3, Value());
    if (!fractionalSecondDigits.isUndefined()) {
        for (double i = 0; i < fractionalSecondDigits.asNumber(); i++) {
            skeletonBuilder.appendString("S");
        }
        opt.insert(std::make_pair("fractionalSecondDigits", fractionalSecondDigits.toString(state)));
    }

    Value shortLongValues[2] = { state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyLong().string() };
    doTable4(state.context()->staticStrings().lazyTimeZoneName().string(), shortLongValues, 2);

    ret = opt.at("timeZoneName");
    if (ret->equals("short")) {
        skeletonBuilder.appendString("z");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("zzzz");
    }

    // Let dataLocaleData be localeData.[[<dataLocale>]].

    matcherValues[0] = state.context()->staticStrings().lazyBasic().string();
    matcher = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyFormatMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);

    // Let dateStyle be ? GetOption(options, "dateStyle", "string", « "full", "long", "medium", "short" », undefined).
    // Set dateTimeFormat.[[DateStyle]] to dateStyle.
    Value dateTimeStyleValues[4] = { state.context()->staticStrings().lazyFull().string(), state.context()->staticStrings().lazyLong().string(),
                                     state.context()->staticStrings().lazyMedium().string(), state.context()->staticStrings().lazyShort().string() };
    Value dateStyle = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyDateStyle().string(), Intl::StringValue, dateTimeStyleValues, 4, Value());
    m_dateStyle = dateStyle;
    // Let timeStyle be ? GetOption(options, "timeStyle", "string", « "full", "long", "medium", "short" », undefined).
    Value timeStyle = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyTimeStyle().string(), Intl::StringValue, dateTimeStyleValues, 4, Value());
    // Set dateTimeFormat.[[TimeStyle]] to timeStyle.
    m_timeStyle = timeStyle;

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringData skeleton;
    UTF16StringDataNonGCStd patternBuffer;

    LocalResourcePointer<UDateTimePatternGenerator> generator(udatpg_open(dataLocale.toString(state)->toUTF8StringData().data(), &status),
                                                              [](UDateTimePatternGenerator* d) { udatpg_close(d); });
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    // If dateStyle is not undefined or timeStyle is not undefined, then
    if (!dateStyle.isUndefined() || !timeStyle.isUndefined()) {
        // For each row in Table 4, except the header row, do
        // Let prop be the name given in the Property column of the row.
        // Let p be opt.[[<prop>]].
        // If p is not undefined, then Throw a TypeError exception.
        auto iter = opt.begin();
        while (iter != opt.end()) {
            if (iter->second->length()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "dateStyle and timeStyle may not be used with other DateTimeFormat options");
            }
            iter++;
        }
        // Let styles be dataLocaleData.[[styles]].[[<calendar>]].
        // Let bestFormat be DateTimeStyleFormat(dateStyle, timeStyle, styles).

        auto convertFormatStyle = [](Value s) -> UDateFormatStyle {
            if (s.isString()) {
                String* style = s.asString();
                if (style->equals("full")) {
                    return UDAT_FULL;
                } else if (style->equals("long")) {
                    return UDAT_LONG;
                } else if (style->equals("medium")) {
                    return UDAT_MEDIUM;
                } else if (style->equals("short")) {
                    return UDAT_SHORT;
                }
            }
            return UDAT_NONE;
        };

        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UDateFormat> dateFormatFromStyle(
            udat_open(convertFormatStyle(m_timeStyle), convertFormatStyle(m_dateStyle), dataLocaleWithExtensions.data(), m_timeZone->toUTF16StringData().data(), m_timeZone->length(), nullptr, -1, &status),
            [](UDateFormat* fmt) { udat_close(fmt); });

        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
            return;
        }

        auto toPatternResult = INTL_ICU_STRING_BUFFER_OPERATION(udat_toPattern, dateFormatFromStyle.get(), false);
        patternBuffer = std::move(toPatternResult.second);
        if (U_FAILURE(toPatternResult.first)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
            return;
        }

        // It is possible that timeStyle includes dayPeriod, which is sensitive to hour-cycle.
        // If dayPeriod is included, just replacing hour based on hourCycle / hour12 produces strange results.
        // Let's consider about the example. The formatted result looks like "02:12:47 PM Coordinated Universal Time"
        // If we simply replace 02 to 14, this becomes "14:12:47 PM Coordinated Universal Time", this looks strange since "PM" is unnecessary!
        //
        // If the generated pattern's hour12 does not match against the option's one, we retrieve skeleton from the pattern, enforcing hour-cycle,
        // and re-generating the best pattern from the modified skeleton. ICU will look into the generated skeleton, and pick the best format for the request.
        // We do not care about h11 vs. h12 and h23 vs. h24 difference here since this will be later adjusted by replaceHourCycleInPattern.
        //
        // test262/test/intl402/DateTimeFormat/prototype/format/timedatestyle-en.js includes the test for this behavior.
        if (!Value(m_timeStyle).isUndefined() && (!hourCycle.isUndefinedOrNull() || !hour12.isUndefined())) {
            bool isHour12 = hourCycle.isString() && (hourCycle.asString()->equals("h11") || hourCycle.asString()->equals("h12"));
            bool specifiedHour12 = false;
            // If hour12 is specified, we prefer it and ignore hourCycle.
            if (!hour12.isUndefined()) {
                specifiedHour12 = hour12.asBoolean();
            } else {
                specifiedHour12 = isHour12;
            }
            std::string extractedHourCycle = readHourCycleFromPattern(patternBuffer);
            if (extractedHourCycle.size() && (extractedHourCycle == "h11" || extractedHourCycle == "h12") != specifiedHour12) {
                auto getSkeletonResult = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getSkeleton, nullptr, (UChar*)patternBuffer.data(), patternBuffer.size());
                if (U_FAILURE(getSkeletonResult.first)) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
                    return;
                }

                UTF16StringDataNonGCStd skeleton = std::move(getSkeletonResult.second);
                for (size_t i = 0; i < skeleton.size(); i++) {
                    auto ch = skeleton[i];
                    if (ch == 'h' || ch == 'H' || ch == 'j') {
                        if (specifiedHour12) {
                            skeleton[i] = 'H';
                        } else {
                            skeleton[i] = 'h';
                        }
                    }
                }

                auto getBestPatternWithOptionsResult = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getBestPatternWithOptions, generator.get(), (UChar*)skeleton.data(), skeleton.length(), UDATPG_MATCH_HOUR_FIELD_LENGTH);
                if (U_FAILURE(getBestPatternWithOptionsResult.first)) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
                    return;
                }
                patternBuffer = std::move(getBestPatternWithOptionsResult.second);
            }
        }

    } else {
        // Else,
        // Let formats be dataLocaleData.[[formats]].[[<calendar>]].
        // If matcher is "basic", then
        // Let bestFormat be BasicFormatMatcher(opt, formats).
        // Else,
        // Let bestFormat be BestFitFormatMatcher(opt, formats).
        skeleton = skeletonBuilder.finalize()->toUTF16StringData();
        auto getBestPatternWithOptionsResult = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getBestPatternWithOptions, generator.get(), (UChar*)skeleton.data(), skeleton.length(), UDATPG_MATCH_HOUR_FIELD_LENGTH);
        if (U_FAILURE(getBestPatternWithOptionsResult.first)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
            return;
        }
        patternBuffer = std::move(getBestPatternWithOptionsResult.second);
    }

    // If dateTimeFormat.[[Hour]] is not undefined, then
    bool hasHourOption = hour->length();
    if (hasHourOption) {
        // Let hcDefault be dataLocaleData.[[hourCycle]].
        UTF16StringDataNonGCStd patternBuffer;
        auto getBestPatternResult = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getBestPattern, generator.get(), u"jjmm", 4);
        ASSERT(U_SUCCESS(getBestPatternResult.first));
        patternBuffer = std::move(getBestPatternResult.second);
        auto hcDefault = readHourCycleFromPattern(patternBuffer);
        // Let hc be dateTimeFormat.[[HourCycle]].
        auto hc = m_hourCycle;
        // If hc is null, then
        if (Value(hc).isUndefined()) {
            // Set hc to hcDefault.
            hc = String::fromUTF8(hcDefault.data(), hcDefault.size());
        }
        // If hour12 is not undefined, then
        if (!hour12.isUndefined()) {
            // If hour12 is true, then
            if (hour12.asBoolean()) {
                // If hcDefault is "h11" or "h23", then
                if (hcDefault == "h11" || hcDefault == "h23") {
                    // Set hc to "h11".
                    hc = state.context()->staticStrings().lazyH11().string();
                } else {
                    // Set hc to "h12".
                    hc = state.context()->staticStrings().lazyH12().string();
                }
            } else {
                // Assert: hour12 is false.
                // If hcDefault is "h11" or "h23", then
                if (hcDefault == "h11" || hcDefault == "h23") {
                    // Set hc to "h23".
                    hc = state.context()->staticStrings().lazyH23().string();
                } else {
                    // Else,
                    // Set hc to "h24".
                    hc = state.context()->staticStrings().lazyH24().string();
                }
            }
        }

        // Let dateTimeFormat.[[HourCycle]] to hc.
        // If dateTimeformat.[[HourCycle]] is "h11" or "h12", then
        //   Let pattern be bestFormat.[[pattern12]].
        //   Let rangePatterns be bestFormat.[[rangePatterns12]].
        // Else,
        //   Let pattern be bestFormat.[[pattern]].
        //   Let rangePatterns be bestFormat.[[rangePatterns]].
        m_hourCycle = hc;
    } else {
        // Set dateTimeFormat.[[HourCycle]] to undefined.
        // Let pattern be bestFormat.[[pattern]].
        // Let rangePatterns be bestFormat.[[rangePatterns]].
        m_hourCycle = Value();
    }

    // Set dateTimeFormat.[[Pattern]] to pattern.
    // Set dateTimeFormat.[[RangePatterns]] to rangePatterns.
    // Return dateTimeFormat.

    // After generating pattern from skeleton, we need to change h11 vs. h12 and h23 vs. h24 if hourCycle is specified.
    if (!Value(m_hourCycle).isUndefined()) {
        patternBuffer = updateHourCycleInPatternDueToHourCycle(patternBuffer, Value(m_hourCycle).asString());
    }

    // For each row in Table 4, except the header row, in table order, do
    // Let prop be the name given in the Property column of the row.
    // If bestFormat has a field [[<prop>]], then
    // Let p be bestFormat.[[<prop>]].
    // Set dateTimeFormat's internal slot whose name is the Internal Slot column of the row to p.
    bool inQuote = false;
    unsigned length = patternBuffer.length();
    for (unsigned i = 0; i < length; ++i) {
        char16_t currentCharacter = patternBuffer[i];

        if (currentCharacter == '\'') {
            inQuote = !inQuote;
        }

        if (!isASCIIAlpha(currentCharacter))
            continue;

        unsigned count = 1;
        while (i + 1 < length && patternBuffer[i + 1] == currentCharacter) {
            ++count;
            ++i;
        }

        if (hasHourOption && !inQuote) {
            if (currentCharacter == 'h' || currentCharacter == 'K') {
                m_hour12 = Value(true);
            } else if (currentCharacter == 'H' || currentCharacter == 'k') {
                m_hour12 = Value(false);
            }
        }

        switch (currentCharacter) {
        case 'G':
            if (count <= 3) {
                m_era = state.context()->staticStrings().lazyShort().string();
            } else if (count == 4) {
                m_era = state.context()->staticStrings().lazyLong().string();
            } else if (count == 5) {
                m_era = state.context()->staticStrings().lazyNarrow().string();
            }
            break;
        case 'y':
            if (count == 1) {
                m_year = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_year = state.context()->staticStrings().lazyTwoDigit().string();
            }
            break;
        case 'M':
        case 'L':
            if (count == 1) {
                m_month = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_month = state.context()->staticStrings().lazyTwoDigit().string();
            } else if (count == 3) {
                m_month = state.context()->staticStrings().lazyShort().string();
            } else if (count == 4) {
                m_month = state.context()->staticStrings().lazyLong().string();
            } else if (count == 5) {
                m_month = state.context()->staticStrings().lazyNarrow().string();
            }
            break;
        case 'E':
        case 'e':
        case 'c':
            if (count <= 3) {
                m_weekday = state.context()->staticStrings().lazyShort().string();
            } else if (count == 4) {
                m_weekday = state.context()->staticStrings().lazyLong().string();
            } else if (count == 5) {
                m_weekday = state.context()->staticStrings().lazyNarrow().string();
            }
            break;
        case 'd':
            if (count == 1) {
                m_day = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_day = state.context()->staticStrings().lazyTwoDigit().string();
            }
            break;
        case 'a':
        case 'b':
        case 'B':
            if (count <= 3) {
                m_dayPeriod = state.context()->staticStrings().lazyShort().string();
            } else if (count == 4) {
                m_dayPeriod = state.context()->staticStrings().lazyLong().string();
            } else if (count == 5) {
                m_dayPeriod = state.context()->staticStrings().lazyNarrow().string();
            }
            break;
        case 'h':
        case 'H':
        case 'k':
        case 'K':
            if (count == 1) {
                m_hour = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_hour = state.context()->staticStrings().lazyTwoDigit().string();
            }
            break;
        case 'm':
            if (count == 1) {
                m_minute = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_minute = state.context()->staticStrings().lazyTwoDigit().string();
            }
            break;
        case 's':
            if (count == 1) {
                m_second = state.context()->staticStrings().numeric.string();
            } else if (count == 2) {
                m_second = state.context()->staticStrings().lazyTwoDigit().string();
            }
            break;
        case 'z':
        case 'v':
        case 'V':
            if (count == 1) {
                m_timeZoneName = state.context()->staticStrings().lazyShort().string();
            } else if (count == 4) {
                m_timeZoneName = state.context()->staticStrings().lazyLong().string();
            }
            break;
        case 'S':
            m_fractionalSecondDigits = Value(count);
            break;
        }
    }

    status = U_ZERO_ERROR;
    UTF16StringData timeZoneView = m_timeZone->toUTF16StringData();
    m_icuDateFormat = udat_open(UDAT_PATTERN, UDAT_PATTERN, dataLocaleWithExtensions.data(), (UChar*)timeZoneView.data(), timeZoneView.length(), (UChar*)patternBuffer.data(), patternBuffer.length(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    addFinalizer([](Object* obj, void* data) {
        IntlDateTimeFormatObject* self = (IntlDateTimeFormatObject*)obj;
        udat_close(self->m_icuDateFormat);
    },
                 nullptr);

    status = U_ZERO_ERROR;
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(m_icuDateFormat));
    std::string type(ucal_getType(cal, &status));
    if (status == U_ZERO_ERROR && std::string("gregorian") == type) {
        ucal_setGregorianChange(cal, minECMAScriptTime, &status);
    }
}

UTF16StringDataNonGCStd IntlDateTimeFormatObject::format(ExecutionState& state, double x)
{
    // If x is not a finite Number, then throw a RangeError exception.
    // If x is NaN, throw a RangeError exception
    // If abs(time) > 8.64 × 10^15, return NaN.
    if (std::isinf(x) || std::isnan(x) || !IS_IN_TIME_RANGE(x)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is valid in DateTimeFormat format()");
    }

    x = Value(x).toInteger(state);

    // Delegate remaining steps to ICU.
    auto formatResult = INTL_ICU_STRING_BUFFER_OPERATION_COMPLEX(udat_format, nullptr, m_icuDateFormat, x);
    if (U_FAILURE(formatResult.first)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format date value");
    }

    return formatResult.second;
}

ArrayObject* IntlDateTimeFormatObject::formatToParts(ExecutionState& state, double x)
{
    // If x is not a finite Number, then throw a RangeError exception.
    // If x is NaN, throw a RangeError exception
    // If abs(time) > 8.64 × 10^15, return NaN.
    if (std::isinf(x) || std::isnan(x) || !IS_IN_TIME_RANGE(x)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is not valid in DateTimeFormat formatToParts()");
    }

    x = Value(x).toInteger(state);

    ArrayObject* result = new ArrayObject(state);

    UErrorCode status = U_ZERO_ERROR;

    UFieldPositionIterator* fpositer;
    fpositer = ufieldpositer_open(&status);
    ASSERT(U_SUCCESS(status));

    auto formatResult = INTL_ICU_STRING_BUFFER_OPERATION_COMPLEX(udat_formatForFields, fpositer, m_icuDateFormat, x);
    UTF16StringDataNonGCStd resultString = std::move(formatResult.second);

    struct FieldItem {
        int32_t start;
        int32_t end;
        int32_t type;
    };

    std::vector<FieldItem> fields;
    while (true) {
        int32_t start;
        int32_t end;
        int32_t type = ufieldpositer_next(fpositer, &start, &end);
        if (type < 0) {
            break;
        }

        FieldItem item;
        item.start = start;
        item.end = end;
        item.type = type;

        fields.push_back(item);
    }

    ufieldpositer_close(fpositer);

    // add literal field if needs
    std::vector<FieldItem> literalFields;
    for (size_t i = 0; i < resultString.length(); i++) {
        bool found = false;
        for (size_t j = 0; j < fields.size(); j++) {
            if ((size_t)fields[j].start <= i && i < (size_t)fields[j].end) {
                found = true;
                break;
            }
        }
        if (!found) {
            size_t end = i + 1;
            while (end != resultString.length()) {
                bool found = false;
                for (size_t j = 0; j < fields.size(); j++) {
                    if ((size_t)fields[j].start <= end && end < (size_t)fields[j].end) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    break;
                }
                end++;
            }

            FieldItem newItem;
            newItem.type = -1;
            newItem.start = i;
            newItem.end = end;
            i = end - 1;
            literalFields.push_back(newItem);
        }
    }
    fields.insert(fields.end(), literalFields.begin(), literalFields.end());

    std::sort(fields.begin(), fields.end(),
              [](const FieldItem& a, const FieldItem& b) -> bool {
                  return a.start < b.start;
              });

    AtomicString typeAtom(state, "type", 4);
    AtomicString valueAtom = state.context()->staticStrings().value;

    for (size_t i = 0; i < fields.size(); i++) {
        const FieldItem& item = fields[i];

        Object* o = new Object(state);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(typeAtom), ObjectPropertyDescriptor(icuFieldTypeToPartName(state, item.type), ObjectPropertyDescriptor::AllPresent));
        auto sub = resultString.substr(item.start, item.end - item.start);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(valueAtom), ObjectPropertyDescriptor(new UTF16String(sub.data(), sub.length()), ObjectPropertyDescriptor::AllPresent));

        result->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, i), ObjectPropertyDescriptor(o, ObjectPropertyDescriptor::AllPresent));
    }

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format date value");
    }

    return result;
}

Value IntlDateTimeFormatObject::toDateTimeOptions(ExecutionState& state, Value options, Value required, Value defaults)
{
    // If options is undefined, then let options be null, else let options be ToObject(options).
    if (options.isUndefined()) {
        options = Value(Value::Null);
    } else {
        options = options.toObject(state);
    }
    // Let options be OrdinaryObjectCreate(options).
    if (options.isObject()) {
        Value proto = options;
        options = new Object(state);
        options.asObject()->setPrototype(state, proto);
    } else {
        options = new Object(state, Object::PrototypeIsNull);
    }

    // Let needDefaults be true.
    bool needDefaults = true;
    // If required is "date" or "any", then
    if (required.equalsTo(state, state.context()->staticStrings().lazyDate().string()) || required.equalsTo(state, state.context()->staticStrings().lazyAny().string())) {
        // For each of the property names "weekday", "year", "month", "day":
        // If the result of calling the [[Get]] internal method of options with the property name is not undefined, then let needDefaults be false.
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyWeekday(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyYear(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyMonth(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyDay(), needDefaults);
    }
    // If required is "time" or "any", then
    if (required.equalsTo(state, state.context()->staticStrings().lazyTime().string()) || required.equalsTo(state, state.context()->staticStrings().lazyAny().string())) {
        // For each of the property names "dayPeriod", "hour", "minute", "second", "fractionalSecondDigits"
        // If the result of calling the [[Get]] internal method of options with the property name is not undefined, then let needDefaults be false.
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyDayPeriod(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyHour(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyMinute(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazySecond(), needDefaults);
        toDateTimeOptionsTest(state, options, state.context()->staticStrings().lazyFractionalSecondDigits(), needDefaults);
    }

    // Let dateStyle be ? Get(options, "dateStyle").
    Value dateStyle = options.asObject()->get(state, state.context()->staticStrings().lazyDateStyle()).value(state, options);
    // Let timeStyle be ? Get(options, "timeStyle").
    Value timeStyle = options.asObject()->get(state, state.context()->staticStrings().lazyTimeStyle()).value(state, options);
    // If dateStyle is not undefined or timeStyle is not undefined, let needDefaults be false.
    if (!dateStyle.isUndefined() || !timeStyle.isUndefined()) {
        needDefaults = false;
    }
    // If required is "date" and timeStyle is not undefined, then
    if (required.equalsTo(state, state.context()->staticStrings().lazyDate().string())) {
        if (!timeStyle.isUndefined()) {
            // Throw a TypeError exception.
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "The given option value is invalid");
        }
    }
    // If required is "time" and dateStyle is not undefined, then
    if (required.equalsTo(state, state.context()->staticStrings().lazyTime().string())) {
        if (!dateStyle.isUndefined()) {
            // Throw a TypeError exception.
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "The given option value is invalid");
        }
    }

    // If needDefaults is true and defaults is either "date" or "all", then
    if (needDefaults && (defaults.equalsTo(state, state.context()->staticStrings().lazyDate().string()) || defaults.equalsTo(state, state.context()->staticStrings().all.string()))) {
        // For each of the property names "year", "month", "day":
        // Call the [[DefineOwnProperty]] internal method of options with the property name,
        // Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = state.context()->staticStrings().numeric.string();
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazyYear()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazyMonth()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazyDay()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }
    // If needDefaults is true and defaults is either "time" or "all", then
    if (needDefaults && (defaults.equalsTo(state, state.context()->staticStrings().lazyTime().string()) || defaults.equalsTo(state, state.context()->staticStrings().all.string()))) {
        // For each of the property names "hour", "minute", "second":
        // Call the [[DefineOwnProperty]] internal method of options with the property name, Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = state.context()->staticStrings().numeric.string();
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazyHour()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazyMinute()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lazySecond()), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }

    return options;
}

} // namespace Escargot
#endif
