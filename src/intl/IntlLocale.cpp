/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/ArrayObject.h"
#include "Intl.h"
#include "IntlLocale.h"
#include "IntlDateTimeFormat.h"

namespace Escargot {

static Intl::CanonicalizedLangunageTag applyOptionsToTag(ExecutionState& state, String* tag, Optional<Object*> options)
{
    // we should not allow grandfathered tag like `i-default`(invalid lang), `no-bok`(have extlang)
    auto u8Tag = tag->toNonGCUTF8StringData();
    auto tagPart = split(u8Tag, '-');
    if ((tagPart.size() > 0 && tagPart[0].length() == 1) || (tagPart.size() > 1 && tagPart[1].length() == 3 && isAllSpecialCharacters(tagPart[1], isASCIIAlpha))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Incorrect locale information provided");
    }

    // If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError exception.
    auto parsedResult = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(u8Tag);
    if (!parsedResult.canonicalizedTag) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Incorrect locale information provided");
    }

    // We should not allow extlang here
    if (parsedResult.extLang.size()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Incorrect locale information provided");
    }

    String* languageString = state.context()->staticStrings().language.string();
    // Let language be ? GetOption(options, "language", "string", undefined, undefined).
    Value language = options ? Intl::getOption(state, options.value(), languageString, Intl::StringValue, nullptr, 0, Value()) : Value();
    // If language is not undefined, then
    if (!language.isUndefined()) {
        // If language does not match the unicode_language_subtag production, throw a RangeError exception.
        auto u8Lang = language.asString()->toNonGCUTF8StringData();
        UErrorCode status = U_ZERO_ERROR;
        int32_t len = uloc_forLanguageTag(u8Lang.data(), nullptr, 0, nullptr, &status);
        UNUSED_VARIABLE(len);
        if (status != U_BUFFER_OVERFLOW_ERROR || u8Lang.length() != 2 || !isAllSpecialCharacters(u8Lang, isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "lanuage tag you give into Intl.Locale is invalid");
        }
    }

    String* scriptString = state.context()->staticStrings().script.string();
    // Let script be ? GetOption(options, "script", "string", undefined, undefined).
    Value script = options ? Intl::getOption(state, options.value(), scriptString, Intl::StringValue, nullptr, 0, Value()) : Value();
    // If script is not undefined, then
    if (!script.isUndefined()) {
        // If script does not match the unicode_script_subtag, throw a RangeError exception.
        String* scriptString = script.asString();
        if (scriptString->length() != 4 || !scriptString->isAllSpecialCharacters(isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "script you give into Intl.Locale is invalid");
        }
    }

    String* regionString = state.context()->staticStrings().region.string();
    // Let region be ? GetOption(options, "region", "string", undefined, undefined).
    Value region = options ? Intl::getOption(state, options.value(), regionString, Intl::StringValue, nullptr, 0, Value()) : Value();
    // If region is not undefined, then
    if (!region.isUndefined()) {
        // If region does not match the unicode_region_subtag, throw a RangeError exception.
        String* regionString = region.asString();
        if ((!(regionString->length() == 2 && regionString->isAllSpecialCharacters(isASCIIAlpha)) && !(regionString->length() == 3 && regionString->isAllSpecialCharacters(isASCIIDigit)))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "region you give into Intl.Locale is invalid");
        }
    }

    // Let tag to CanonicalizeUnicodeLocaleId(tag).
    // If language is not undefined,
    // Assert: tag matches the unicode_locale_id production.
    // Set tag to tag with the substring corresponding to the unicode_language_subtag production of the unicode_language_id replaced by the string language.
    // If script is not undefined, then
    // If tag does not contain a unicode_script_subtag production of the unicode_language_id, then
    // Set tag to the concatenation of the unicode_language_subtag production of the unicode_languge_id of tag, "-", script, and the rest of tag.
    // Else,
    // Set tag to tag with the substring corresponding to the unicode_script_subtag production of the unicode_language_id replaced by the string script.
    // If region is not undefined, then
    // If tag does not contain a unicode_region_subtag production of the unicode_language_id, then
    // Set tag to the concatenation of the unicode_language_subtag production of the unicode_language_id of tag, the substring corresponding to the "-" unicode_script_subtag production of the unicode_language_id if present, "-", region, and the rest of tag.
    // Else,
    // Set tag to tag with the substring corresponding to the unicode_region_subtag production of the unicode_language_id replaced by the string region.
    // Return CanonicalizeUnicodeLocaleId(tag).
    std::string localeStr;

    if (!language.isUndefined()) {
        localeStr = language.asString()->toNonGCUTF8StringData();
    } else {
        localeStr = parsedResult.language;
    }

    if (!script.isUndefined()) {
        localeStr += "-";
        localeStr += script.asString()->toNonGCUTF8StringData();
    } else if (parsedResult.script.length()) {
        localeStr += "-";
        localeStr += parsedResult.script;
    }

    if (!region.isUndefined()) {
        localeStr += "-";
        localeStr += region.asString()->toNonGCUTF8StringData();
    } else if (parsedResult.region.length()) {
        localeStr += "-";
        localeStr += parsedResult.region;
    }

    for (size_t i = 0; i < parsedResult.variant.size(); i++) {
        localeStr += "-";
        localeStr += parsedResult.variant[i];
    }

    auto newParsedResult = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(localeStr);
    if (!newParsedResult.canonicalizedTag) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Incorrect locale information provided");
    }

    // We should not allow extlang here
    if (newParsedResult.extLang.size()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "First argument of Intl.Locale should have valid tag");
    }

    newParsedResult.privateUse = parsedResult.privateUse;
    newParsedResult.extensions = parsedResult.extensions;
    newParsedResult.unicodeExtension = parsedResult.unicodeExtension;

    return newParsedResult;
}

static bool checkOptionValueIsAlphaNum38DashAlphaNum38(const std::string& value)
{
    if (value.length() == 0) {
        return false;
    }

    auto parts = split(value, '-');
    if (parts.size() > 2) {
        return false;
    }
    for (size_t i = 0; i < parts.size(); i++) {
        const auto& s = parts[i];
        if (s.length() < 3 || s.length() > 8 || !isAllSpecialCharacters(s, isASCIIAlphanumeric)) {
            return false;
        }
    }

    return true;
}

IntlLocaleObject::IntlLocaleObject(ExecutionState& state, String* tag, Optional<Object*> options)
    : IntlLocaleObject(state, state.context()->globalObject()->intlLocalePrototype(), tag, options)
{
}

IntlLocaleObject::IntlLocaleObject(ExecutionState& state, Object* proto, String* tag, Optional<Object*> options)
    : DerivedObject(state, proto)
    , m_language(nullptr)
    , m_script(nullptr)
    , m_region(nullptr)
    , m_baseName(nullptr)
    , m_locale(nullptr)
{
    // Set tag to ? ApplyOptionsToTag(tag, options).
    Intl::CanonicalizedLangunageTag buildResult = applyOptionsToTag(state, tag, options);
    m_language = String::fromUTF8(buildResult.language.data(), buildResult.language.length());
    m_script = String::fromUTF8(buildResult.script.data(), buildResult.script.length());
    m_region = String::fromUTF8(buildResult.region.data(), buildResult.region.length());
    m_baseName = buildResult.canonicalizedTag.value();

    // Let opt be a new Record.
    Optional<String*> ca;
    Optional<String*> co;
    Optional<String*> hc;
    Optional<String*> nu;
    Optional<String*> kf;
    Optional<String*> kn;

    auto& parsedUnicodeExtension = buildResult.unicodeExtension;
    for (size_t i = 0; i < parsedUnicodeExtension.size(); i++) {
        if (!ca && parsedUnicodeExtension[i].first == "ca") {
            ca = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        } else if (!co && parsedUnicodeExtension[i].first == "co") {
            co = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        } else if (!hc && parsedUnicodeExtension[i].first == "hc") {
            hc = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        } else if (!kf && parsedUnicodeExtension[i].first == "kf") {
            kf = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        } else if (!kn && parsedUnicodeExtension[i].first == "kn") {
            kn = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        } else if (!nu && parsedUnicodeExtension[i].first == "nu") {
            nu = String::fromUTF8(parsedUnicodeExtension[i].second.data(), parsedUnicodeExtension[i].second.length());
        }
    }

    // Let calendar be ? GetOption(options, "calendar", "string", undefined, undefined).
    String* calendarString = state.context()->staticStrings().calendar.string();
    Value calendar = options ? Intl::getOption(state, options.value(), calendarString, Intl::StringValue, nullptr, 0, Value()) : Value();
    // If calendar is not undefined, then
    if (!calendar.isUndefined()) {
        // If calendar does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        // If calendar does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
        std::string value = calendar.asString()->toNonGCUTF8StringData();
        if (!uloc_toLegacyType("calendar", value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "`calendar` option you input is not valid");
        }
        // Set opt.[[ca]] to calendar.
        ca = calendar.asString();
    }

    // Let collation be ? GetOption(options, "collation", "string", undefined, undefined).
    Value collation = options ? Intl::getOption(state, options.value(), state.context()->staticStrings().collation.string(), Intl::StringValue, nullptr, 0, Value()) : Value();
    // If collation is not undefined, then
    if (!collation.isUndefined()) {
        // If collation does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        // If collation does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
        std::string value = collation.asString()->toNonGCUTF8StringData();
        if (!uloc_toLegacyType("collation", value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "`collation` option you input is not valid");
        }
        // Set opt.[[co]] to collation.
        co = collation.asString();
    }


    // Let hc be ? GetOption(options, "hourCycle", "string", « "h11", "h12", "h23", "h24" », undefined).
    Value hourCycleValues[4] = { state.context()->staticStrings().lazyH11().string(), state.context()->staticStrings().lazyH12().string(), state.context()->staticStrings().lazyH23().string(), state.context()->staticStrings().lazyH24().string() };
    Value hourCycle = options ? Intl::getOption(state, options.value(), state.context()->staticStrings().hourCycle.string(), Intl::StringValue, hourCycleValues, 4, Value()) : Value();
    // Set opt.[[hc]] to hc.
    if (!hourCycle.isUndefined()) {
        hc = hourCycle.asString();
    }

    // Let kf be ? GetOption(options, "caseFirst", "string", « "upper", "lower", "false" », undefined).
    Value caseFirstValues[3] = { state.context()->staticStrings().lazyUpper().string(), state.context()->staticStrings().lazyLower().string(), state.context()->staticStrings().stringFalse.string() };
    // Set opt.[[kf]] to kf.
    Value caseFirst = options ? Intl::getOption(state, options.value(), state.context()->staticStrings().caseFirst.string(), Intl::StringValue, caseFirstValues, 3, Value()) : Value();
    if (!caseFirst.isUndefined()) {
        kf = caseFirst.asString();
    }

    // Let kn be ? GetOption(options, "numeric", "boolean", undefined, undefined).
    Value numeric = options ? Intl::getOption(state, options.value(), state.context()->staticStrings().numeric.string(), Intl::BooleanValue, nullptr, 0, Value()) : Value();
    // If kn is not undefined, set kn to ! ToString(kn).
    if (!numeric.isUndefined()) {
        kn = numeric.toString(state);
    }
    // Set opt.[[kn]] to kn.

    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem = options ? Intl::getOption(state, options.value(), state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value()) : Value();
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        // If numberingSystem does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
        std::string value = numberingSystem.asString()->toNonGCUTF8StringData();
        if (!uloc_toLegacyType(uloc_toLegacyKey("nu"), value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "`collation` option you input is not valid");
        }
        // Set opt.[[nu]] to numberingSystem.
        nu = numberingSystem.asString();
    }

    // Let r be ! ApplyUnicodeExtensionToTag(tag, opt, relevantExtensionKeys).
    m_calendar = ca;
    m_caseFirst = kf;
    m_collation = co;
    m_hourCycle = hc;
    m_numberingSystem = nu;
    m_numeric = kn;

    std::vector<std::pair<std::string, std::string>> newUnicodeExtension;
    for (size_t i = 0; i < parsedUnicodeExtension.size(); i++) {
        if (parsedUnicodeExtension[i].first != "ca" && parsedUnicodeExtension[i].first != "co" && parsedUnicodeExtension[i].first != "hc" && parsedUnicodeExtension[i].first != "kf" && parsedUnicodeExtension[i].first != "kn" && parsedUnicodeExtension[i].first != "nu") {
            newUnicodeExtension.push_back(parsedUnicodeExtension[i]);
        }
    }

    if (m_calendar) {
        newUnicodeExtension.push_back(std::make_pair(std::string("ca"), m_calendar->asString()->toNonGCUTF8StringData()));
    }

    if (m_caseFirst) {
        newUnicodeExtension.push_back(std::make_pair(std::string("kf"), m_caseFirst->asString()->equals("true") ? std::string() : m_caseFirst->asString()->toNonGCUTF8StringData()));
    }

    if (m_collation) {
        newUnicodeExtension.push_back(std::make_pair(std::string("co"), m_collation->asString()->toNonGCUTF8StringData()));
    }

    if (m_hourCycle) {
        newUnicodeExtension.push_back(std::make_pair(std::string("hc"), m_hourCycle->asString()->toNonGCUTF8StringData()));
    }

    if (m_numberingSystem) {
        newUnicodeExtension.push_back(std::make_pair(std::string("nu"), m_numberingSystem->asString()->toNonGCUTF8StringData()));
    }

    if (m_numeric) {
        newUnicodeExtension.push_back(std::make_pair(std::string("kn"), m_numeric->asString()->equals("true") ? std::string() : m_numeric->asString()->toNonGCUTF8StringData()));
    }

    std::sort(newUnicodeExtension.begin(), newUnicodeExtension.end(),
              [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) -> bool {
                  return a.first < b.first;
              });

    StringBuilder localeBuilder;

    localeBuilder.appendString(m_baseName);

    size_t extensionIndex;
    for (extensionIndex = 0; extensionIndex < buildResult.extensions.size(); extensionIndex++) {
        if (buildResult.extensions[extensionIndex].key == 'u') {
            extensionIndex++;
            break;
        }

        localeBuilder.appendChar('-');
        localeBuilder.appendChar(buildResult.extensions[extensionIndex].key);
        localeBuilder.appendChar('-');
        localeBuilder.appendString(buildResult.extensions[extensionIndex].value.data(),
                                   buildResult.extensions[extensionIndex].value.length());
    }

    if (newUnicodeExtension.size()) {
        localeBuilder.appendChar('-');
        localeBuilder.appendChar('u');

        for (size_t i = 0; i < newUnicodeExtension.size(); i++) {
            if (newUnicodeExtension[i].first.length()) {
                localeBuilder.appendChar('-');
                localeBuilder.appendString(newUnicodeExtension[i].first.data(),
                                           newUnicodeExtension[i].first.length());
            }

            if (newUnicodeExtension[i].second.length()) {
                localeBuilder.appendChar('-');
                localeBuilder.appendString(newUnicodeExtension[i].second.data(),
                                           newUnicodeExtension[i].second.length());
            }
        }
    }

    for (; extensionIndex < buildResult.extensions.size(); extensionIndex++) {
        localeBuilder.appendChar('-');
        localeBuilder.appendChar(buildResult.extensions[extensionIndex].key);
        localeBuilder.appendChar('-');
        localeBuilder.appendString(buildResult.extensions[extensionIndex].value.data(),
                                   buildResult.extensions[extensionIndex].value.length());
    }

    if (buildResult.privateUse.length()) {
        localeBuilder.appendChar('-');
        localeBuilder.appendString(buildResult.privateUse.data(),
                                   buildResult.privateUse.length());
    }

    m_locale = localeBuilder.finalize();
}

Value IntlLocaleObject::calendars(ExecutionState& state)
{
    ValueVector resultVector;
    if (m_calendar) {
        resultVector.pushBack(m_calendar.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UEnumeration> calendars(
            ucal_getKeywordValuesForLocale("calendar", m_locale->toNonGCUTF8StringData().data(), true, &status),
            [](UEnumeration* fmt) { uenum_close(fmt); });

        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }

        const char* buffer;
        int32_t bufferLength = 0;
        while ((buffer = uenum_next(calendars.get(), &bufferLength, &status)) && U_SUCCESS(status)) {
            std::string calendar(buffer, bufferLength);
            calendar = Intl::convertICUCalendarKeywordToBCP47KeywordIfNeeds(calendar);
            resultVector.pushBack(String::fromUTF8(calendar.data(), calendar.size()));
        }

        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }
    }

    return Object::createArrayFromList(state, resultVector);
}

Value IntlLocaleObject::collations(ExecutionState& state)
{
    ValueVector resultVector;
    if (m_collation) {
        resultVector.pushBack(m_collation.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UEnumeration> collations(
            ucol_getKeywordValuesForLocale("collation", m_locale->toNonGCUTF8StringData().data(), true, &status),
            [](UEnumeration* f) { uenum_close(f); });

        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }

        const char* buffer;
        int32_t bufferLength = 0;
        while ((buffer = uenum_next(collations.get(), &bufferLength, &status)) && U_SUCCESS(status)) {
            // https://tc39.es/proposal-intl-locale-info/#sec-collations-of-locale
            // Let list be a List of 1 or more unique collation identifiers,
            // which must be lower case String values conforming to the type sequence from UTS 35 Unicode Locale Identifier,
            // section 3.2, sorted in descending preference of those in common use for string comparison in locale.
            // The values "standard" and "search" must be excluded from list.
            std::string collation(buffer, bufferLength);
            if (collation == "standard" || collation == "search") {
                continue;
            }
            collation = Intl::convertICUCollationKeywordToBCP47KeywordIfNeeds(collation);
            resultVector.pushBack(String::fromUTF8(collation.data(), collation.size()));
        }

        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }
    }

    return Object::createArrayFromList(state, resultVector);
}

Value IntlLocaleObject::hourCycles(ExecutionState& state)
{
    ValueVector resultVector;
    if (m_hourCycle) {
        resultVector.pushBack(m_hourCycle.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UDateTimePatternGenerator> generator(udatpg_open(m_locale->toNonGCUTF8StringData().data(), &status),
                                                                  [](UDateTimePatternGenerator* d) { udatpg_close(d); });
        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }

        UChar skeleton[] = { 'j', 0 };
        auto getBestPatternWithOptionsResult = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getBestPatternWithOptions, generator.get(), skeleton, 1, UDATPG_MATCH_HOUR_FIELD_LENGTH);
        if (U_FAILURE(getBestPatternWithOptionsResult.first)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }

        auto hc = IntlDateTimeFormatObject::readHourCycleFromPattern(getBestPatternWithOptionsResult.second);
        if (hc.size()) {
            resultVector.pushBack(String::fromUTF8(hc.data(), hc.size()));
        }
    }

    return Object::createArrayFromList(state, resultVector);
}

Value IntlLocaleObject::numberingSystems(ExecutionState& state)
{
    ValueVector resultVector;
    if (m_numberingSystem) {
        resultVector.pushBack(m_numberingSystem.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UNumberingSystem> numberingSystem(
            unumsys_open(m_locale->toNonGCUTF8StringData().data(), &status),
            [](UNumberingSystem* p) { unumsys_close(p); });

        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }

        const char* u8Name = unumsys_getName(numberingSystem.get());
        resultVector.pushBack(String::fromUTF8(u8Name, strlen(u8Name)));
    }

    return Object::createArrayFromList(state, resultVector);
}

Value IntlLocaleObject::textInfo(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    ULayoutType layout = uloc_getCharacterOrientation(m_locale->toNonGCUTF8StringData().data(), &status);
    if (!U_SUCCESS(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
        return Value();
    }

    Value layoutValue;
    if (layout == ULOC_LAYOUT_LTR) {
        layoutValue = String::fromASCII("ltr");
    } else if (layout == ULOC_LAYOUT_RTL) {
        layoutValue = String::fromASCII("rtl");
    } else if (layout == ULOC_LAYOUT_TTB) {
        layoutValue = String::fromASCII("ttb");
    } else {
        ASSERT(layout == ULOC_LAYOUT_BTT);
        layoutValue = String::fromASCII("btt");
    }

    Object* result = new Object(state);
    result->set(state, ObjectPropertyName(state, String::fromASCII("direction")), layoutValue, result);
    return result;
}

Value IntlLocaleObject::weekInfo(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> calendar(ucal_open(nullptr, 0, m_locale->toNonGCUTF8StringData().data(), UCAL_DEFAULT, &status),
                                             [](UCalendar* d) { ucal_close(d); });
    if (!U_SUCCESS(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
        return Value();
    }

    int32_t firstDayOfWeek = ucal_getAttribute(calendar.get(), UCAL_FIRST_DAY_OF_WEEK);
    int32_t minimalDays = ucal_getAttribute(calendar.get(), UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);

    auto canonicalizeDayOfWeekType = [](UCalendarWeekdayType type) {
        switch (type) {
        // UCAL_WEEKEND_ONSET is a day that starts as a weekday and transitions to the weekend. It means this is WeekDay.
        case UCAL_WEEKEND_ONSET:
        case UCAL_WEEKDAY:
            return UCAL_WEEKDAY;
        // UCAL_WEEKEND_CEASE is a day that starts as the weekend and transitions to a weekday. It means this is WeekEnd.
        case UCAL_WEEKEND_CEASE:
        case UCAL_WEEKEND:
            return UCAL_WEEKEND;
        default:
            return UCAL_WEEKEND;
        }
    };

    UCalendarWeekdayType previous = canonicalizeDayOfWeekType(ucal_getDayOfWeekType(calendar.get(), UCAL_SATURDAY, &status));
    if (!U_SUCCESS(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
        return Value();
    }

    int32_t weekendStart = 0;
    int32_t weekendEnd = 0;
    for (int32_t day = UCAL_SUNDAY; day <= UCAL_SATURDAY; ++day) {
        UCalendarWeekdayType type = canonicalizeDayOfWeekType(ucal_getDayOfWeekType(calendar.get(), static_cast<UCalendarDaysOfWeek>(day), &status));
        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }
        if (previous != type) {
            switch (type) {
            case UCAL_WEEKDAY: // WeekEnd => WeekDay
                if (day == UCAL_SUNDAY)
                    weekendEnd = UCAL_SATURDAY;
                else
                    weekendEnd = day - 1;
                break;
            case UCAL_WEEKEND: // WeekDay => WeekEnd
                weekendStart = day;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        previous = type;
    }

    auto convertUCalendarDaysOfWeekToMondayBasedDay = [](int32_t day) -> int32_t {
        // Convert from
        //     Sunday => 1
        //     Saturday => 7
        // to
        //     Monday => 1
        //     Sunday => 7
        if (day == UCAL_SUNDAY)
            return 7;
        return day - 1;
    };

    Object* result = new Object(state);
    result->set(state, ObjectPropertyName(state, String::fromASCII("firstDay")), Value(convertUCalendarDaysOfWeekToMondayBasedDay(firstDayOfWeek)), result);
    result->set(state, ObjectPropertyName(state, String::fromASCII("weekendStart")), Value(convertUCalendarDaysOfWeekToMondayBasedDay(weekendStart)), result);
    result->set(state, ObjectPropertyName(state, String::fromASCII("weekendEnd")), Value(convertUCalendarDaysOfWeekToMondayBasedDay(weekendEnd)), result);
    result->set(state, ObjectPropertyName(state, String::fromASCII("minimalDays")), Value(minimalDays), result);
    return result;
}

Value IntlLocaleObject::timeZones(ExecutionState& state)
{
    if (!m_region->length()) {
        return Value();
    }

    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UEnumeration> enumeration(ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, m_region->toNonGCUTF8StringData().data(), nullptr, &status),
                                                   [](UEnumeration* d) { uenum_close(d); });
    if (!U_SUCCESS(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
        return Value();
    }

    ValueVector resultVector;
    int32_t length;
    const char* collation;
    while ((collation = uenum_next(enumeration.get(), &length, &status)) && U_SUCCESS(status)) {
        resultVector.pushBack(String::fromUTF8(collation, length));
    }

    if (!U_SUCCESS(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
        return Value();
    }

    return Object::createArrayFromList(state, resultVector);
}

} // namespace Escargot
#endif
