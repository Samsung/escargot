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
#include "Context.h"
#include "ExecutionState.h"
#include "VMInstance.h"
#include "Value.h"
#include "Intl.h"
#include "IntlDateTimeFormat.h"
#include "DateObject.h"
#include "ArrayObject.h"

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
        // Ensure aliases used in language tag are allowed.
        if (calendar == std::string("gregorian")) {
            keyLocaleData.push_back(std::string("gregory"));
        } else if (calendar == std::string("islamic-civil")) {
            keyLocaleData.push_back(std::string("islamicc"));
        } else if (calendar == std::string("ethiopic-amete-alem")) {
            keyLocaleData.push_back(std::string("ethioaa"));
        } else {
            keyLocaleData.push_back(calendar);
        }
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

static String* canonicalizeTimeZoneName(String* timeZoneName)
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
    if (canonical->equals("Etc/UTC") || canonical->equals("Etc/GMT"))
        canonical = String::fromASCII("UTC");

    // 4. Return ianaTimeZone.
    return canonical;
}


static void toDateTimeOptionsTest(ExecutionState& state, Value options, const char* ch, bool& needDefaults)
{
    Value r = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII(ch))).value(state, options.asObject());
    if (!r.isUndefined()) {
        needDefaults = false;
    }
}

static String* defaultTimeZone(ExecutionState& state)
{
    // ensure timezone
    state.context()->vmInstance()->timezone();
    return String::fromUTF8(state.context()->vmInstance()->timezoneID().data(), state.context()->vmInstance()->timezoneID().length());
}

Object* IntlDateTimeFormat::create(ExecutionState& state, Context* realm, Value locales, Value options)
{
    Object* dateTimeFormat = new Object(state, realm->globalObject()->intlDateTimeFormatPrototype());
    initialize(state, dateTimeFormat, locales, options);
    return dateTimeFormat;
}

static std::string readHourCycleFromPattern(const UTF16StringDataNonGCStd& patternString)
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


void IntlDateTimeFormat::initialize(ExecutionState& state, Object* dateTimeFormat, Value locales, Value options)
{
    // If dateTimeFormat has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (dateTimeFormat->hasInternalSlot() && dateTimeFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, ObjectStructurePropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of dateTimeFormat to true.
    dateTimeFormat->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    // Let options be the result of calling the ToDateTimeOptions abstract operation (defined below) with arguments options, "any", and "date".
    options = toDateTimeOptions(state, options, String::fromASCII("any"), String::fromASCII("date"));

    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit".
    Value matcherValues[2] = { String::fromASCII("lookup"), String::fromASCII("best fit") };
    Value matcher = Intl::getOption(state, options.asObject(), String::fromASCII("localeMatcher"), Intl::StringValue, matcherValues, 2, matcherValues[1]);

    // Set opt.[[localeMatcher]] to matcher.
    opt.insert(std::make_pair("localeMatcher", matcher.toString(state)));

    // Let calendar be ? GetOption(options, "calendar", "string", undefined, undefined).
    Value calendar = Intl::getOption(state, options.asObject(), String::fromASCII("calendar"), Intl::StringValue, nullptr, 0, Value());
    // If calendar is not undefined, then
    if (!calendar.isUndefined()) {
        // If calendar does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminal(calendar.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "The calendar value you gave is not valid");
        }
    }
    // Set opt.[[ca]] to calendar.
    if (!calendar.isUndefined()) {
        opt.insert(std::make_pair("ca", calendar.toString(state)));
    }
    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem = Intl::getOption(state, options.asObject(), String::fromASCII("numberingSystem"), Intl::StringValue, nullptr, 0, Value());
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        std::string s = numberingSystem.asString()->toNonGCUTF8StringData();
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminal(numberingSystem.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "The numberingSystem value you gave is not valid");
        }
    }
    // Set opt.[[nu]] to numberingSystem.
    if (!numberingSystem.isUndefined()) {
        opt.insert(std::make_pair("nu", numberingSystem.toString(state)));
    }

    // Let hour12 be ? GetOption(options, "hour12", "boolean", undefined, undefined).
    Value hour12 = Intl::getOption(state, options.asObject(), String::fromASCII("hour12"), Intl::BooleanValue, nullptr, 0, Value());

    // Let hourCycle be ? GetOption(options, "hourCycle", "string", « "h11", "h12", "h23", "h24" », undefined).
    Value hourCycleValue[4] = { String::fromASCII("h11"), String::fromASCII("h12"), String::fromASCII("h23"), String::fromASCII("h24") };
    Value hourCycle = Intl::getOption(state, options.asObject(), String::fromASCII("hourCycle"), Intl::StringValue, hourCycleValue, 4, Value());
    // If hour12 is not undefined, then
    if (!hour12.isUndefined()) {
        // Let hourCycle be null.
        hourCycle = Value(Value::Null);
    }
    // Set opt.[[hc]] to hourCycle.
    if (!hourCycle.isUndefinedOrNull()) {
        opt.insert(std::make_pair("hc", hourCycle.toString(state)));
    }

    // Let DateTimeFormat be the standard built-in object that is the initial value of Intl.DateTimeFormat.
    // Let localeData be the value of the [[localeData]] internal property of DateTimeFormat.
    const auto& availableLocales = state.context()->globalObject()->intlDateTimeFormatAvailableLocales();

    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the
    // [[availableLocales]] internal property of DateTimeFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of DateTimeFormat, and localeData.
    StringMap r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, intlDateTimeFormatRelevantExtensionKeys, intlDateTimeFormatRelevantExtensionKeysLength, localeDataDateTimeFormat);

    // The resolved locale doesn't include a hc Unicode extension value if the hour12 or hourCycle option is also present.
    if (!hour12.isUndefined() || !hourCycle.isUndefinedOrNull()) {
        auto iter = r.find("locale");
        String* locale = iter->second;
        iter->second = Intl::canonicalizeLanguageTag(locale->toNonGCUTF8StringData(), "hc").canonicalizedTag.value();
    }

    // Set the [[locale]] internal property of dateTimeFormat to the value of r.[[locale]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("locale")), r.at("locale"), dateTimeFormat->internalSlot());
    // Set the [[calendar]] internal property of dateTimeFormat to the value of r.[[ca]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("calendar")), r.at("ca"), dateTimeFormat->internalSlot());
    // Set dateTimeFormat.[[hourCycle]] to r.[[hc]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hourCycle")), r.at("hc"), dateTimeFormat->internalSlot());
    // Set the [[numberingSystem]] internal property of dateTimeFormat to the value of r.[[nu]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("numberingSystem")), r.at("nu"), dateTimeFormat->internalSlot());

    // Let dataLocale be the value of r.[[dataLocale]].
    Value dataLocale = r.at("dataLocale");

    // Always use ICU date format generator, rather than our own pattern list and matcher.
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UDateTimePatternGenerator> generator(udatpg_open(dataLocale.toString(state)->toUTF8StringData().data(), &status),
                                                              [](UDateTimePatternGenerator* d) { udatpg_close(d); });
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    // Let tz be the result of calling the [[Get]] internal method of options with argument "timeZone".
    Value tz = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII("timeZone"))).value(state, options.asObject());

    // If tz is not undefined, then
    if (!tz.isUndefined()) {
        // Let tz be ToString(tz).
        String* tzString = tz.toString(state);
        // Convert tz to upper case as described in 6.1.
        // NOTE:   If an implementation accepts additional time zone values, as permitted under certain conditions by the Conformance clause,
        // different casing rules apply., If tz is not "UTC", then throw a RangeError exception.
        tz = canonicalizeTimeZoneName(tzString);
        tzString = tz.toString(state);
        if (tzString->length() == 0) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid timezone value");
        }
    } else {
        tz = defaultTimeZone(state);
    }
    // Set the [[timeZone]] internal property of dateTimeFormat to tz.
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("timeZone")), tz, dateTimeFormat->internalSlot());

    // Let opt be a new Record.
    opt.clear();

    // Table 3 – Components of date and time formats
    // Property    Values
    // weekday "narrow", "short", "long"
    // era "narrow", "short", "long"
    // year    "2-digit", "numeric"
    // month   "2-digit", "numeric", "narrow", "short", "long"
    // day "2-digit", "numeric"
    // hour    "2-digit", "numeric"
    // minute  "2-digit", "numeric"
    // second  "2-digit", "numeric"
    // timeZoneName    "short", "long"

    // For each row of Table 3, except the header row, do:
    std::function<void(String * prop, Value * values, size_t valuesSize)> doTable3 = [&](String* prop, Value* values, size_t valuesSize) {
        // Let prop be the name given in the Property column of the row.
        // Let value be the result of calling the GetOption abstract operation, passing as argument options, the name given in the Property column of the row,
        // "string", a List containing the strings given in the Values column of the row, and undefined.
        Value value = Intl::getOption(state, options.asObject(), prop, Intl::StringValue, values, valuesSize, Value());
        // Set opt.[[<prop>]] to value.
        if (!value.isUndefined()) {
            opt.insert(std::make_pair(prop->toNonGCUTF8StringData(), value.toString(state)));
        }
    };

    StringBuilder skeletonBuilder;

    Value narrowShortLongValues[3] = { String::fromASCII("narrow"), String::fromASCII("short"), String::fromASCII("long") };

    doTable3(String::fromASCII("weekday"), narrowShortLongValues, 3);

    String* ret = opt.at("weekday");

    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("EEEEE");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("EEE");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("EEEE");
    }

    doTable3(String::fromASCII("era"), narrowShortLongValues, 3);

    ret = opt.at("era");
    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("GGGGG");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("GGG");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("GGGG");
    }

    Value twoDightNumericValues[2] = { String::fromASCII("2-digit"), String::fromASCII("numeric") };
    doTable3(String::fromASCII("year"), twoDightNumericValues, 2);

    ret = opt.at("year");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("yy");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("y");
    }

    Value allValues[5] = { String::fromASCII("2-digit"), String::fromASCII("numeric"), String::fromASCII("narrow"), String::fromASCII("short"), String::fromASCII("long") };
    doTable3(String::fromASCII("month"), allValues, 5);

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

    doTable3(String::fromASCII("day"), twoDightNumericValues, 2);

    ret = opt.at("day");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("dd");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("d");
    }

    doTable3(String::fromASCII("hour"), twoDightNumericValues, 2);

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

    doTable3(String::fromASCII("minute"), twoDightNumericValues, 2);

    ret = opt.at("minute");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("mm");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("m");
    }

    doTable3(String::fromASCII("second"), twoDightNumericValues, 2);

    ret = opt.at("second");
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("ss");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("s");
    }

    Value shortLongValues[2] = { String::fromASCII("short"), String::fromASCII("long") };
    doTable3(String::fromASCII("timeZoneName"), shortLongValues, 2);

    ret = opt.at("timeZoneName");
    if (ret->equals("short")) {
        skeletonBuilder.appendString("z");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("zzzz");
    }

    // Let dataLocaleData be the result of calling the [[Get]] internal method of localeData with argument dataLocale.
    // Let formats be the result of calling the [[Get]] internal method of dataLocaleData with argument "formats".
    // Let matcher be the result of calling the GetOption abstract operation with arguments options,
    // "formatMatcher", "string", a List containing the two String values "basic" and "best fit", and "best fit".
    Value formatMatcherValues[2] = { String::fromASCII("basic"), String::fromASCII("best fit") };
    matcher = Intl::getOption(state, options.asObject(), String::fromASCII("formatMatcher"), Intl::StringValue, formatMatcherValues, 2, formatMatcherValues[1]);

    // Always use ICU date format generator, rather than our own pattern list and matcher.
    // Covers steps 28-36.
    String* skeleton = skeletonBuilder.finalize();
    UTF16StringData skeletonUTF16String = skeleton->toUTF16StringData();

    // If dateTimeFormat.[[Hour]] is not undefined, then
    bool hasHourOption = hour->length();
    if (hasHourOption) {
        // Let hcDefault be dataLocaleData.[[hourCycle]].
        UTF16StringDataNonGCStd patternBuffer;
        patternBuffer.resize(32);
        status = U_ZERO_ERROR;
        auto patternLength = udatpg_getBestPattern(generator.get(), u"jjmm", 4, (UChar*)patternBuffer.data(), patternBuffer.length(), &status);
        patternBuffer.resize(patternLength);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            patternBuffer.resize(patternLength);
            udatpg_getBestPattern(generator.get(), u"jjmm", 4, (UChar*)patternBuffer.data(), patternLength, &status);
        }
        ASSERT(U_SUCCESS(status));
        auto hcDefault = readHourCycleFromPattern(patternBuffer);
        // Let hc be dateTimeFormat.[[HourCycle]].
        auto hc = dateTimeFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("hourCycle"))).value(state, dateTimeFormat->internalSlot()).asString();
        // If hc is null, then
        if (!hc->length()) {
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
                    hc = String::fromASCII("h11");
                } else {
                    // Set hc to "h12".
                    hc = String::fromASCII("h12");
                }
            } else {
                // Assert: hour12 is false.
                // If hcDefault is "h11" or "h23", then
                if (hcDefault == "h11" || hcDefault == "h23") {
                    // Set hc to "h23".
                    hc = String::fromASCII("h23");
                } else {
                    // Else,
                    // Set hc to "h24".
                    hc = String::fromASCII("h24");
                }
            }
        }

        // Let dateTimeFormat.[[HourCycle]] to hc.
        dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hourCycle")), Value(hc), dateTimeFormat->internalSlot());
    } else {
        // Set dateTimeFormat.[[HourCycle]] to undefined.
        dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hourCycle")), Value(), dateTimeFormat->internalSlot());
        // Let pattern be bestFormat.[[pattern]].
    }

    UTF16StringDataNonGCStd patternBuffer;
    patternBuffer.resize(32);
    status = U_ZERO_ERROR;
    auto patternLength = udatpg_getBestPatternWithOptions(generator.get(), (UChar*)skeletonUTF16String.data(), skeletonUTF16String.length(), UDATPG_MATCH_HOUR_FIELD_LENGTH, (UChar*)patternBuffer.data(), patternBuffer.size(), &status);
    patternBuffer.resize(patternLength);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        patternBuffer.resize(patternLength);
        udatpg_getBestPatternWithOptions(generator.get(), (UChar*)skeletonUTF16String.data(), skeletonUTF16String.length(), UDATPG_MATCH_HOUR_FIELD_LENGTH, (UChar*)patternBuffer.data(), patternLength, &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    Value hc = dateTimeFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("hourCycle"))).value(state, dateTimeFormat->internalSlot());
    if (!hc.isUndefined() && hc.asString()->length()) {
        patternBuffer = updateHourCycleInPatternDueToHourCycle(patternBuffer, hc.asString());
    }

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
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour12")), Value(true), dateTimeFormat->internalSlot());
            } else if (currentCharacter == 'H' || currentCharacter == 'k') {
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour12")), Value(false), dateTimeFormat->internalSlot());
            }
        }

        switch (currentCharacter) {
        case 'G':
            if (count <= 3)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("era")), Value(String::fromASCII("short")), dateTimeFormat->internalSlot());
            else if (count == 4)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("era")), Value(String::fromASCII("long")), dateTimeFormat->internalSlot());
            else if (count == 5)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("era")), Value(String::fromASCII("narrow")), dateTimeFormat->internalSlot());
            break;
        case 'y':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("year")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("year")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            break;
        case 'M':
        case 'L':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("month")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("month")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            else if (count == 3)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("month")), Value(String::fromASCII("short")), dateTimeFormat->internalSlot());
            else if (count == 4)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("month")), Value(String::fromASCII("long")), dateTimeFormat->internalSlot());
            else if (count == 5)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("month")), Value(String::fromASCII("narrow")), dateTimeFormat->internalSlot());
            break;
        case 'E':
        case 'e':
        case 'c':
            if (count <= 3)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("weekday")), Value(String::fromASCII("short")), dateTimeFormat->internalSlot());
            else if (count == 4)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("weekday")), Value(String::fromASCII("long")), dateTimeFormat->internalSlot());
            else if (count == 5)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("weekday")), Value(String::fromASCII("narrow")), dateTimeFormat->internalSlot());
            break;
        case 'd':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("day")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("day")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            break;
        case 'h':
        case 'H':
        case 'k':
        case 'K':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            break;
        case 'm':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minute")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minute")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            break;
        case 's':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("second")), Value(String::fromASCII("numeric")), dateTimeFormat->internalSlot());
            else if (count == 2)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("second")), Value(String::fromASCII("2-digit")), dateTimeFormat->internalSlot());
            break;
        case 'z':
        case 'v':
        case 'V':
            if (count == 1)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("timeZoneName")), Value(String::fromASCII("short")), dateTimeFormat->internalSlot());
            else if (count == 4)
                dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("timeZoneName")), Value(String::fromASCII("long")), dateTimeFormat->internalSlot());
            break;
        }
    }

    status = U_ZERO_ERROR;
    UTF16StringData timeZoneView = dateTimeFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("timeZone"))).value(state, dateTimeFormat->internalSlot()).toString(state)->toUTF16StringData();
    UTF8StringData localeStringView = r.at("locale")->toUTF8StringData();
    UDateFormat* icuDateFormat = udat_open(UDAT_PATTERN, UDAT_PATTERN, localeStringView.data(), (UChar*)timeZoneView.data(), timeZoneView.length(), (UChar*)patternBuffer.data(), patternBuffer.length(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    dateTimeFormat->internalSlot()->setExtraData(icuDateFormat);

    GC_REGISTER_FINALIZER_NO_ORDER(dateTimeFormat->internalSlot(), [](void* obj, void*) {
        Object* self = (Object*)obj;
        udat_close((UDateFormat*)self->extraData());
    },
                                   nullptr, nullptr, nullptr);

    // Set dateTimeFormat.[[boundFormat]] to undefined.
    // Set dateTimeFormat.[[initializedDateTimeFormat]] to true.
    dateTimeFormat->internalSlot()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")), ObjectPropertyDescriptor(Value(true)));

    status = U_ZERO_ERROR;
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(icuDateFormat));
    std::string type(ucal_getType(cal, &status));
    if (status == U_ZERO_ERROR && std::string("gregorian") == type) {
        ucal_setGregorianChange(cal, minECMAScriptTime, &status);
    }
    // Return dateTimeFormat.
    return;
}

UTF16StringDataNonGCStd IntlDateTimeFormat::format(ExecutionState& state, Object* dateTimeFormat, double x)
{
    // If x is not a finite Number, then throw a RangeError exception.
    // If x is NaN, throw a RangeError exception
    // If abs(time) > 8.64 × 10^15, return NaN.
    if (std::isinf(x) || std::isnan(x) || !IS_IN_TIME_RANGE(x)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is valid in DateTimeFormat format()");
    }

    x = Value(x).toInteger(state);

    UDateFormat* udat = (UDateFormat*)dateTimeFormat->internalSlot()->extraData();

    // Delegate remaining steps to ICU.
    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd result;
    result.resize(32);
    auto resultLength = udat_format(udat, x, (UChar*)result.data(), result.size(), nullptr, &status);
    result.resize(resultLength);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        udat_format(udat, x, (UChar*)result.data(), resultLength, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format date value");
    }

    return result;
}


static String* icuFieldTypeToPartName(int32_t fieldName)
{
    if (fieldName == -1) {
        return String::fromASCII("literal");
    }

    switch ((UDateFormatField)fieldName) {
    case UDAT_ERA_FIELD:
        return String::fromASCII("era");
    case UDAT_YEAR_FIELD:
    case UDAT_YEAR_WOY_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
    case UDAT_YEAR_NAME_FIELD:
        return String::fromASCII("year");
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
        return String::fromASCII("month");
    case UDAT_DATE_FIELD:
    case UDAT_JULIAN_DAY_FIELD:
        return String::fromASCII("day");
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
        return String::fromASCII("hour");
    case UDAT_MINUTE_FIELD:
        return String::fromASCII("minute");
    case UDAT_SECOND_FIELD:
        return String::fromASCII("second");
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
        return String::fromASCII("weekday");
    case UDAT_AM_PM_FIELD:
        return String::fromASCII("dayPeriod");
    case UDAT_TIMEZONE_FIELD:
        return String::fromASCII("timeZoneName");
    default:
        ASSERT_NOT_REACHED();
        return String::emptyString;
    }
}


ArrayObject* IntlDateTimeFormat::formatToParts(ExecutionState& state, Object* dateTimeFormat, double x)
{
    // If x is not a finite Number, then throw a RangeError exception.
    // If x is NaN, throw a RangeError exception
    // If abs(time) > 8.64 × 10^15, return NaN.
    if (std::isinf(x) || std::isnan(x) || !IS_IN_TIME_RANGE(x)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is not valid in DateTimeFormat formatToParts()");
    }

    x = Value(x).toInteger(state);

    ArrayObject* result = new ArrayObject(state);

    UDateFormat* udat = (UDateFormat*)dateTimeFormat->internalSlot()->extraData();

    UErrorCode status = U_ZERO_ERROR;

    UFieldPositionIterator* fpositer;
    fpositer = ufieldpositer_open(&status);
    ASSERT(U_SUCCESS(status));

    UTF16StringDataNonGCStd resultString;
    resultString.resize(32);
    auto resultLength = udat_formatForFields(udat, x, (UChar*)resultString.data(), resultString.size(), fpositer, &status);
    resultString.resize(resultLength);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        udat_formatForFields(udat, x, (UChar*)resultString.data(), resultLength, fpositer, &status);
    }

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
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(typeAtom), ObjectPropertyDescriptor(icuFieldTypeToPartName(item.type), ObjectPropertyDescriptor::AllPresent));
        auto sub = resultString.substr(item.start, item.end - item.start);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(valueAtom), ObjectPropertyDescriptor(new UTF16String(sub.data(), sub.length()), ObjectPropertyDescriptor::AllPresent));

        result->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, i), ObjectPropertyDescriptor(o, ObjectPropertyDescriptor::AllPresent));
    }

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format date value");
    }

    return result;
}

Value IntlDateTimeFormat::toDateTimeOptions(ExecutionState& state, Value options, Value required, Value defaults)
{
    // If options is undefined, then let options be null, else let options be ToObject(options).
    if (options.isUndefined()) {
        options = Value(Value::Null);
    } else {
        options = options.toObject(state);
    }
    // Let create be the standard built-in function object defined in ES5, 15.2.3.5.
    // Let options be the result of calling the [[Call]] internal method of create with undefined as the this value
    // and an argument list containing the single item options.
    options = Object::call(state, state.context()->globalObject()->objectCreate(), Value(), 1, &options);

    // Let needDefaults be true.
    bool needDefaults = true;
    // If required is "date" or "any", then
    if (required.equalsTo(state, String::fromASCII("date")) || required.equalsTo(state, String::fromASCII("any"))) {
        // For each of the property names "weekday", "year", "month", "day":
        // If the result of calling the [[Get]] internal method of options with the property name is not undefined, then let needDefaults be false.
        toDateTimeOptionsTest(state, options, "weekday", needDefaults);
        toDateTimeOptionsTest(state, options, "year", needDefaults);
        toDateTimeOptionsTest(state, options, "month", needDefaults);
        toDateTimeOptionsTest(state, options, "day", needDefaults);
    }
    // If required is "time" or "any", then
    if (required.equalsTo(state, String::fromASCII("time")) || required.equalsTo(state, String::fromASCII("any"))) {
        // For each of the property names "hour", "minute", "second":
        // If the result of calling the [[Get]] internal method of options with the property name is not undefined, then let needDefaults be false.
        toDateTimeOptionsTest(state, options, "hour", needDefaults);
        toDateTimeOptionsTest(state, options, "minute", needDefaults);
        toDateTimeOptionsTest(state, options, "second", needDefaults);
    }

    // If needDefaults is true and defaults is either "date" or "all", then
    if (needDefaults && (defaults.equalsTo(state, String::fromASCII("date")) || defaults.equalsTo(state, String::fromASCII("all")))) {
        // For each of the property names "year", "month", "day":
        // Call the [[DefineOwnProperty]] internal method of options with the property name,
        // Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = String::fromASCII("numeric");
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("year")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("month")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("day")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }
    // If needDefaults is true and defaults is either "time" or "all", then
    if (needDefaults && (defaults.equalsTo(state, String::fromASCII("time")) || defaults.equalsTo(state, String::fromASCII("all")))) {
        // For each of the property names "hour", "minute", "second":
        // Call the [[DefineOwnProperty]] internal method of options with the property name, Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = String::fromASCII("numeric");
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("hour")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("minute")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("second")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }

    return options;
}

} // namespace Escargot
#endif
