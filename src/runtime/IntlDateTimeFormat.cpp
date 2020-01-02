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
 *  version 2 of the License, or (at your option) any later version.
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

namespace Escargot {

static const char* const intlDateTimeFormatRelevantExtensionKeys[2] = { "ca", "nu" };
static size_t intlDateTimeFormatRelevantExtensionKeysLength = 2;
static const size_t indexOfExtensionKeyCa = 0;
static const size_t indexOfExtensionKeyNu = 1;
static const double minECMAScriptTime = -8.64E15;

static std::vector<std::string> localeDataDateTimeFormat(String* locale, size_t keyIndex)
{
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCa: {
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
        break;
    }
    case indexOfExtensionKeyNu:
        keyLocaleData = Intl::numberingSystemsForLocale(locale);
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
    if (!needDefaults) {
        return;
    }
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

Object* IntlDateTimeFormat::create(ExecutionState& state, Value locales, Value options)
{
    Object* dateTimeFormat = new Object(state);
    initialize(state, dateTimeFormat, locales, options);
    return dateTimeFormat;
}

void IntlDateTimeFormat::initialize(ExecutionState& state, Object* dateTimeFormat, Value locales, Value options)
{
    dateTimeFormat->setPrototype(state, state.context()->globalObject()->intlDateTimeFormat()->getFunctionPrototype(state));
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
    StringMap* opt = new StringMap;
    // Let matcher be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit".
    ValueVector tempV;
    tempV.pushBack(String::fromASCII("lookup"));
    tempV.pushBack(String::fromASCII("best fit"));
    Value matcher = Intl::getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), tempV, String::fromASCII("best fit"));

    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Let DateTimeFormat be the standard built-in object that is the initial value of Intl.DateTimeFormat.
    // Let localeData be the value of the [[localeData]] internal property of DateTimeFormat.
    const auto& availableLocales = state.context()->globalObject()->intlDateTimeFormatAvailableLocales();

    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the
    // [[availableLocales]] internal property of DateTimeFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of DateTimeFormat, and localeData.
    StringMap* r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, intlDateTimeFormatRelevantExtensionKeys, intlDateTimeFormatRelevantExtensionKeysLength, localeDataDateTimeFormat);

    // Set the [[locale]] internal property of dateTimeFormat to the value of r.[[locale]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("locale")), r->at(String::fromASCII("locale")), dateTimeFormat->internalSlot());
    // Set the [[calendar]] internal property of dateTimeFormat to the value of r.[[ca]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("calendar")), r->at(String::fromASCII("ca")), dateTimeFormat->internalSlot());
    // Set the [[numberingSystem]] internal property of dateTimeFormat to the value of r.[[nu]].
    dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("numberingSystem")), r->at(String::fromASCII("nu")), dateTimeFormat->internalSlot());

    // Let dataLocale be the value of r.[[dataLocale]].
    Value dataLocale = r->at(String::fromASCII("dataLocale"));

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
    opt = new StringMap();

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
    std::function<void(String*, const ValueVector)> doTable3 = [&](String* prop, const ValueVector& values) {
        // Let prop be the name given in the Property column of the row.
        // Let value be the result of calling the GetOption abstract operation, passing as argument options, the name given in the Property column of the row,
        // "string", a List containing the strings given in the Values column of the row, and undefined.
        Value value = Intl::getOption(state, options.asObject(), prop, String::fromASCII("string"), values, Value());
        // Set opt.[[<prop>]] to value.
        opt->insert(std::make_pair(prop, value.toString(state)));
    };

    StringBuilder skeletonBuilder;

    tempV.clear();
    tempV.pushBack(String::fromASCII("narrow"));
    tempV.pushBack(String::fromASCII("short"));
    tempV.pushBack(String::fromASCII("long"));
    doTable3(String::fromASCII("weekday"), tempV);

    String* ret = opt->at(String::fromASCII("weekday"));

    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("EEEEE");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("EEE");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("EEEE");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("narrow"));
    tempV.pushBack(String::fromASCII("short"));
    tempV.pushBack(String::fromASCII("long"));
    doTable3(String::fromASCII("era"), tempV);

    ret = opt->at(String::fromASCII("era"));
    if (ret->equals("narrow")) {
        skeletonBuilder.appendString("GGGGG");
    } else if (ret->equals("short")) {
        skeletonBuilder.appendString("GGG");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("GGGG");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    doTable3(String::fromASCII("year"), tempV);

    ret = opt->at(String::fromASCII("year"));
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("yy");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("y");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    tempV.pushBack(String::fromASCII("narrow"));
    tempV.pushBack(String::fromASCII("short"));
    tempV.pushBack(String::fromASCII("long"));
    doTable3(String::fromASCII("month"), tempV);

    ret = opt->at(String::fromASCII("month"));
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

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    doTable3(String::fromASCII("day"), tempV);

    ret = opt->at(String::fromASCII("day"));
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("dd");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("d");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    doTable3(String::fromASCII("hour"), tempV);

    ret = opt->at(String::fromASCII("hour"));

    // Let hr12 be the result of calling the GetOption abstract operation with arguments options, "hour12", "boolean", undefined, and undefined.
    Value hr12Value = Intl::getOption(state, options.asObject(), String::fromASCII("hour12"), String::fromASCII("boolean"), ValueVector(), Value());
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

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    doTable3(String::fromASCII("minute"), tempV);

    ret = opt->at(String::fromASCII("minute"));
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("mm");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("m");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("2-digit"));
    tempV.pushBack(String::fromASCII("numeric"));
    doTable3(String::fromASCII("second"), tempV);

    ret = opt->at(String::fromASCII("second"));
    if (ret->equals("2-digit")) {
        skeletonBuilder.appendString("ss");
    } else if (ret->equals("numeric")) {
        skeletonBuilder.appendString("s");
    }

    tempV.clear();
    tempV.pushBack(String::fromASCII("short"));
    tempV.pushBack(String::fromASCII("long"));
    doTable3(String::fromASCII("timeZoneName"), tempV);

    ret = opt->at(String::fromASCII("second"));
    if (ret->equals("short")) {
        skeletonBuilder.appendString("z");
    } else if (ret->equals("long")) {
        skeletonBuilder.appendString("zzzz");
    }

    // Let dataLocaleData be the result of calling the [[Get]] internal method of localeData with argument dataLocale.
    // Let formats be the result of calling the [[Get]] internal method of dataLocaleData with argument "formats".
    // Let matcher be the result of calling the GetOption abstract operation with arguments options,
    // "formatMatcher", "string", a List containing the two String values "basic" and "best fit", and "best fit".
    tempV.clear();
    tempV.pushBack(String::fromASCII("basic"));
    tempV.pushBack(String::fromASCII("best fit"));
    matcher = Intl::getOption(state, options.asObject(), String::fromASCII("formatMatcher"), String::fromASCII("string"), tempV, String::fromASCII("best fit"));

    // Always use ICU date format generator, rather than our own pattern list and matcher.
    // Covers steps 28-36.
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* generator = udatpg_open(dataLocale.toString(state)->toUTF8StringData().data(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    String* skeleton = skeletonBuilder.finalize();
    UTF16StringData skeletonUTF16String = skeleton->toUTF16StringData();
    UTF16StringDataNonGCStd patternBuffer;
    patternBuffer.resize(32);
    status = U_ZERO_ERROR;
    auto patternLength = udatpg_getBestPattern(generator, (UChar*)skeletonUTF16String.data(), skeletonUTF16String.length(), (UChar*)patternBuffer.data(), patternBuffer.size(), &status);
    patternBuffer.resize(patternLength);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        patternBuffer.resize(patternLength);
        udatpg_getBestPattern(generator, (UChar*)skeletonUTF16String.data(), skeletonUTF16String.length(), (UChar*)patternBuffer.data(), patternLength, &status);
    }
    udatpg_close(generator);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    unsigned length = patternBuffer.length();
    for (unsigned i = 0; i < length; ++i) {
        char16_t currentCharacter = patternBuffer[i];
        if (!isASCIIAlpha(currentCharacter))
            continue;

        unsigned count = 1;
        while (i + 1 < length && patternBuffer[i + 1] == currentCharacter) {
            ++count;
            ++i;
        }

        if (currentCharacter == 'h' || currentCharacter == 'K')
            dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour12")), Value(true), dateTimeFormat->internalSlot());
        else if (currentCharacter == 'H' || currentCharacter == 'k')
            dateTimeFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("hour12")), Value(false), dateTimeFormat->internalSlot());

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
    UTF8StringData localeStringView = r->at(String::fromASCII("locale"))->toUTF8StringData();
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
    // 1. If x is not a finite Number, then throw a RangeError exception.
    if (!std::isfinite(x)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is not finite in DateTimeFormat format()");
    }

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
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("year")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("month")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("day")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }
    // If needDefaults is true and defaults is either "time" or "all", then
    if (needDefaults && (defaults.equalsTo(state, String::fromASCII("time")) || defaults.equalsTo(state, String::fromASCII("all")))) {
        // For each of the property names "hour", "minute", "second":
        // Call the [[DefineOwnProperty]] internal method of options with the property name, Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = String::fromASCII("numeric");
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("hour")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("minute")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("second")), ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }

    return options;
}

} // namespace Escargot
#endif
