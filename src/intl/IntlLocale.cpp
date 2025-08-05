/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

class LocaleIDBuilder {
public:
    bool initialize(const std::string&);
    std::string toCanonical();

    void overrideLanguageScriptRegionVariants(Optional<String*> language, Optional<String*> script,
                                              Optional<String*> region, Optional<String*> variants);
    bool setKeywordValue(const char* key, const std::string& value);

private:
    std::string m_buffer;
};


bool LocaleIDBuilder::initialize(const std::string& tag)
{
    if (!Intl::isStructurallyValidLanguageTag(tag)) {
        return false;
    }
    ASSERT(String::fromUTF8(tag.data(), tag.size())->is8Bit());
    m_buffer = Intl::localeIDBufferForLanguageTagWithNullTerminator(tag);
    return m_buffer.size();
}


std::string LocaleIDBuilder::toCanonical()
{
    ASSERT(m_buffer.size());

    auto buffer = Intl::canonicalizeLocaleID(m_buffer.data());
    if (!buffer)
        return {};

    return Intl::canonicalizeUnicodeExtensionsAfterICULocaleCanonicalization(std::move(buffer.value()));
}

// Because ICU's C API doesn't have set[Language|Script|Region|Variants] functions...
void LocaleIDBuilder::overrideLanguageScriptRegionVariants(Optional<String*> language, Optional<String*> script,
                                                           Optional<String*> region, Optional<String*> variants)
{
    unsigned length = m_buffer.length();
    const std::string& localeIDView = m_buffer;

    auto endOfLanguageScriptRegionVariant = localeIDView.find(ULOC_KEYWORD_SEPARATOR);
    if (endOfLanguageScriptRegionVariant == std::string::npos) {
        endOfLanguageScriptRegionVariant = length;
    }

    std::vector<std::string> subtags;

    for (auto subtag : split(localeIDView.substr(0, endOfLanguageScriptRegionVariant), '_')) {
        subtags.push_back(subtag);
    }

    if (language) {
        subtags[0] = language->toNonGCUTF8StringData();
    }

    bool hasScript = subtags.size() > 1 && subtags[1].length() == 4;
    if (script) {
        if (hasScript) {
            subtags[1] = script.value()->toNonGCUTF8StringData();
        } else {
            subtags.insert(subtags.begin() + 1, script.value()->toNonGCUTF8StringData());
            hasScript = true;
        }
    }

    if (region) {
        size_t index = hasScript ? 2 : 1;
        bool hasRegion = subtags.size() > index && subtags[index].length() < 4;
        if (hasRegion) {
            subtags[index] = region.value()->toNonGCUTF8StringData();
        } else {
            subtags.insert(index + subtags.begin(), region.value()->toNonGCUTF8StringData());
        }
    }

    if (variants) {
        while (subtags.size() > 1) {
            auto& lastSubtag = subtags.back();
            bool isVariant = (lastSubtag.length() >= 5 && lastSubtag.length() <= 8) || (lastSubtag.length() == 4 && isASCIIDigit(lastSubtag[0]));
            if (!isVariant)
                break;
            subtags.pop_back();
        }
        if (variants->length()) {
            for (auto variant : split(variants->toNonGCUTF8StringData(), '-')) {
                subtags.push_back(variant);
            }
        }
    }

    std::string buffer;
    bool hasAppended = false;
    for (auto subtag : subtags) {
        if (hasAppended) {
            buffer.push_back('_');
        } else {
            hasAppended = true;
        }

        buffer += subtag;
    }

    if (endOfLanguageScriptRegionVariant != length) {
        auto rest = localeIDView.substr(endOfLanguageScriptRegionVariant, length - endOfLanguageScriptRegionVariant);
        buffer += rest;
    }

    m_buffer.swap(buffer);
}

bool LocaleIDBuilder::setKeywordValue(const char* key, const std::string& value)
{
    ASSERT(m_buffer.size());

    UErrorCode status = U_ZERO_ERROR;
    auto length = uloc_setKeywordValue(key, value.data(), (char*)m_buffer.data(), m_buffer.size(), &status);
    // uloc_setKeywordValue does not set U_STRING_NOT_TERMINATED_WARNING.
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        m_buffer.resize(length);
        status = U_ZERO_ERROR;
        uloc_setKeywordValue(key, value.data(), (char*)m_buffer.data(), length + 1, &status);
    } else if (U_SUCCESS(status)) {
        m_buffer.resize(length);
    }
    return U_SUCCESS(status);
}

IntlLocaleObject::IntlLocaleObject(ExecutionState& state, String* tag, Object* options)
    : IntlLocaleObject(state, state.context()->globalObject()->intlLocalePrototype(), tag, options)
{
}

IntlLocaleObject::IntlLocaleObject(ExecutionState& state, Object* proto, String* tag, Object* options)
    : DerivedObject(state, proto)
{
    auto u8Tag = tag->toNonGCUTF8StringData();
    LocaleIDBuilder localeID;
    if (!localeID.initialize(u8Tag)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "invalid language tag");
    }

    Value language = Intl::getOption(state, options, state.context()->staticStrings().language.string(), Intl::StringValue, nullptr, 0, Value());
    if (!language.isUndefined() && !Intl::isUnicodeLanguageSubtag(language.asString()->toNonGCUTF8StringData())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "invalid language tag");
    }

    Value script = Intl::getOption(state, options, state.context()->staticStrings().script.string(), Intl::StringValue, nullptr, 0, Value());
    if (!script.isUndefined() && !Intl::isUnicodeScriptSubtag(script.asString()->toNonGCUTF8StringData())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "script is not a well-formed script value");
    }

    Value region = Intl::getOption(state, options, state.context()->staticStrings().region.string(), Intl::StringValue, nullptr, 0, Value());
    if (!region.isUndefined() && !Intl::isUnicodeRegionSubtag(region.asString()->toNonGCUTF8StringData())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "region is not a well-formed region value");
    }

    Value variantsValue = Intl::getOption(state, options, state.context()->staticStrings().lazyVariants().string(), Intl::StringValue, nullptr, 0, Value());
    if (!variantsValue.isUndefined()) {
        const char* variantsErrorMessage = "variants is not a well-formed variants value";
        std::string variants = variantsValue.asString()->toNonGCUTF8StringData();
        if (variants.empty() || variants[0] == '-' || variants.back() == '-' || variants.find("--") != std::string::npos) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, variantsErrorMessage);
        }
        auto variantList = split(variants, '-');
        if (variantList.size() == 1) {
            if (!Intl::isUnicodeVariantSubtag(variantList[0])) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, variantsErrorMessage);
            }
        } else {
            std::unordered_set<std::string> seenVariants;
            for (const auto& variant : variantList) {
                if (!Intl::isUnicodeVariantSubtag(variant)) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, variantsErrorMessage);
                }
                std::string lowerVariant = variant;
                std::transform(lowerVariant.begin(), lowerVariant.end(), lowerVariant.begin(),
                               [](char c) { return std::tolower(c); });
                if (seenVariants.find(lowerVariant) != seenVariants.end()) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, variantsErrorMessage);
                }
                seenVariants.insert(lowerVariant);
            }
        }
    }

    if (!language.isUndefined() || !script.isUndefined() || !region.isUndefined() || !variantsValue.isUndefined()) {
        localeID.overrideLanguageScriptRegionVariants(language.isUndefined() ? nullptr : language.asString(),
                                                      script.isUndefined() ? nullptr : script.asString(),
                                                      region.isUndefined() ? nullptr : region.asString(),
                                                      variantsValue.isUndefined() ? nullptr : variantsValue.asString());
    }


    Value calendar = Intl::getOption(state, options, state.context()->staticStrings().calendar.string(), Intl::StringValue, nullptr, 0, Value());
    if (!calendar.isUndefined()) {
        std::string s = calendar.asString()->toNonGCUTF8StringData();
        if (!Intl::isUnicodeLocaleIdentifierType(s) || !localeID.setKeywordValue("calendar", s)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "calendar is not a well-formed calendar value");
        }
    }

    Value collation = Intl::getOption(state, options, state.context()->staticStrings().collation.string(), Intl::StringValue, nullptr, 0, Value());
    if (!collation.isUndefined()) {
        std::string s = collation.asString()->toNonGCUTF8StringData();
        if (!Intl::isUnicodeLocaleIdentifierType(s) || !localeID.setKeywordValue("collation", s)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "collation is not a well-formed collation value");
        }
    }

    Value firstDayOfWeek = Intl::getOption(state, options, state.context()->staticStrings().lazyFirstDayOfWeek().string(), Intl::StringValue, nullptr, 0, Value());
    if (!firstDayOfWeek.isUndefined()) {
        std::string s = firstDayOfWeek.asString()->toNonGCUTF8StringData();

        if (s.length() == 1) {
            if (s == "0") {
                s = "sun";
            } else if (s == "1") {
                s = "mon";
            } else if (s == "2") {
                s = "tue";
            } else if (s == "3") {
                s = "wed";
            } else if (s == "4") {
                s = "thu";
            } else if (s == "5") {
                s = "fri";
            } else if (s == "6") {
                s = "sat";
            } else if (s == "7") {
                s = "sun";
            }
        }

        if (!Intl::isUnicodeLocaleIdentifierType(s) || !localeID.setKeywordValue("fw", s)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "firstDayOfWeek is not a well-formed firstDayOfWeek value");
        }
    }

    Value hourCycleValues[4] = { state.context()->staticStrings().lazyH11().string(), state.context()->staticStrings().lazyH12().string(), state.context()->staticStrings().lazyH23().string(), state.context()->staticStrings().lazyH24().string() };
    Value hourCycle = options ? Intl::getOption(state, options, state.context()->staticStrings().hourCycle.string(), Intl::StringValue, hourCycleValues, 4, Value()) : Value();
    if (!hourCycle.isUndefined()) {
        localeID.setKeywordValue("hours", hourCycle.asString()->toNonGCUTF8StringData());
    }

    Value caseFirstValues[3] = { state.context()->staticStrings().lazyUpper().string(), state.context()->staticStrings().lazyLower().string(), state.context()->staticStrings().stringFalse.string() };
    Value caseFirst = options ? Intl::getOption(state, options, state.context()->staticStrings().caseFirst.string(), Intl::StringValue, caseFirstValues, 3, Value()) : Value();
    if (!caseFirst.isUndefined()) {
        localeID.setKeywordValue("colcasefirst", caseFirst.asString()->toNonGCUTF8StringData());
    }

    Value numeric = Intl::getOption(state, options, state.context()->staticStrings().numeric.string(), Intl::BooleanValue, nullptr, 0, Value());
    if (!numeric.isUndefined()) {
        localeID.setKeywordValue("colnumeric", numeric.toBoolean() ? "yes" : "no");
    }


    Value numberingSystem = Intl::getOption(state, options, state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value());
    if (!numberingSystem.isUndefined()) {
        std::string s = numberingSystem.asString()->toNonGCUTF8StringData();
        if (!Intl::isUnicodeLocaleIdentifierType(s) || !localeID.setKeywordValue("numbers", s)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "numberingSystem is not a well-formed numberingSystem value");
        }
    }

    std::string cLocale = localeID.toCanonical();
    m_localeID = String::fromUTF8(cLocale.data(), cLocale.length());
    if (!m_localeID->length()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize Locale");
    }
}

String* IntlLocaleObject::locale() const
{
    auto langTag = Intl::languageTagForLocaleID(m_localeID->toNonGCUTF8StringData().data());
    if (langTag) {
        return String::fromUTF8(langTag.value().data(), langTag.value().size());
    } else {
        return String::emptyString();
    }
}

String* IntlLocaleObject::language() const
{
    auto result = INTL_ICU_STD_STRING_BUFFER_OPERATION(uloc_getLanguage, m_localeID->toNonGCUTF8StringData().data());
    if (result.second == "") {
        result.second = "und";
    }
    return String::fromUTF8(result.second.data(), result.second.length());
}

String* IntlLocaleObject::region() const
{
    auto result = INTL_ICU_STD_STRING_BUFFER_OPERATION(uloc_getCountry, m_localeID->toNonGCUTF8StringData().data());
    return String::fromUTF8(result.second.data(), result.second.length());
}

String* IntlLocaleObject::script() const
{
    auto result = INTL_ICU_STD_STRING_BUFFER_OPERATION(uloc_getScript, m_localeID->toNonGCUTF8StringData().data());
    return String::fromUTF8(result.second.data(), result.second.length());
}

String* IntlLocaleObject::variants() const
{
    std::string baseNameValue = baseName()->toNonGCUTF8StringData();
    std::vector<std::string> variantParts;
    if (baseNameValue.length()) {
        auto parts = split(baseNameValue, '-');
        for (size_t i = 1; i < parts.size(); i++) {
            const auto& part = parts[i];
            if (Intl::isUnicodeVariantSubtag(part)) {
                auto p = part;
                std::transform(p.begin(), p.end(), p.begin(),
                               [](char c) { return std::tolower(c); });
                variantParts.push_back(p);
            }
        }
    }
    if (variantParts.size() > 0) {
        std::string builder;
        bool first = true;
        for (const auto& variant : variantParts) {
            if (!first) {
                builder += "-";
            }
            builder.append(variant);
            first = false;
        }
        return String::fromUTF8(builder.data(), builder.length());
    } else {
        return String::emptyString();
    }
}

String* IntlLocaleObject::baseName() const
{
    std::string buffer;
    buffer.resize(32);
    UErrorCode status = U_ZERO_ERROR;
    auto bufferLength = uloc_getBaseName(m_localeID->toNonGCUTF8StringData().data(), (char*)buffer.data(), buffer.size(), &status);
    // uloc_setKeywordValue does not set U_STRING_NOT_TERMINATED_WARNING.
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        buffer.resize(bufferLength);
        status = U_ZERO_ERROR;
        uloc_getBaseName(m_localeID->toNonGCUTF8StringData().data(), (char*)buffer.data(), bufferLength, &status);
    } else if (U_SUCCESS(status)) {
        buffer.resize(bufferLength);
    }

    auto langTag = Intl::languageTagForLocaleID(buffer.data());
    if (langTag) {
        return String::fromUTF8(langTag.value().data(), langTag.value().size());
    } else {
        return String::emptyString();
    }
}

static Optional<String*> keywordValue(String* localeID, const char* key, bool isBoolean = false)
{
    UErrorCode status = U_ZERO_ERROR;
    std::string cLocaleID = localeID->toNonGCUTF8StringData();
    auto result = INTL_ICU_STD_STRING_BUFFER_OPERATION(uloc_getKeywordValue, cLocaleID.data(), key);
    ASSERT(U_SUCCESS(result.first));
    if (isBoolean) {
        return String::fromUTF8(result.second.data(), result.second.length());
    }
    const char* value = uloc_toUnicodeLocaleType(key, result.second.data());
    if (!value) {
        return nullptr;
    }
    auto r = String::fromUTF8(value, strlen(value));
    if (r->equals("true")) {
        return String::emptyString();
    }
    return r;
}


Optional<String*> IntlLocaleObject::calendar() const
{
    return keywordValue(m_localeID, "calendar");
}

Optional<String*> IntlLocaleObject::caseFirst() const
{
    return keywordValue(m_localeID, "colcasefirst");
}

Optional<String*> IntlLocaleObject::collation() const
{
    return keywordValue(m_localeID, "collation");
}

Optional<String*> IntlLocaleObject::firstDayOfWeek() const
{
    return keywordValue(m_localeID, "fw");
}

Optional<String*> IntlLocaleObject::hourCycle() const
{
    return keywordValue(m_localeID, "hours");
}

Optional<String*> IntlLocaleObject::numeric() const
{
    return keywordValue(m_localeID, "colnumeric", true);
}

Optional<String*> IntlLocaleObject::numberingSystem() const
{
    return keywordValue(m_localeID, "numbers");
}

Value IntlLocaleObject::calendars(ExecutionState& state)
{
    ValueVector resultVector;
    auto calendar = this->calendar();
    if (calendar) {
        resultVector.pushBack(calendar.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UEnumeration> calendars(
            ucal_getKeywordValuesForLocale("calendar", locale()->toNonGCUTF8StringData().data(), true, &status),
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
    auto collation = this->collation();
    if (collation) {
        resultVector.pushBack(collation.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UEnumeration> collations(
            ucol_getKeywordValuesForLocale("collation", locale()->toNonGCUTF8StringData().data(), true, &status),
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
    auto hourCycle = this->hourCycle();
    if (hourCycle) {
        resultVector.pushBack(hourCycle.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UDateTimePatternGenerator> generator(udatpg_open(locale()->toNonGCUTF8StringData().data(), &status),
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
    auto numberingSystem = this->numberingSystem();
    if (numberingSystem) {
        resultVector.pushBack(numberingSystem.value());
    } else {
        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UNumberingSystem> numberingSystem(
            unumsys_open(locale()->toNonGCUTF8StringData().data(), &status),
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
    ULayoutType layout = uloc_getCharacterOrientation(locale()->toNonGCUTF8StringData().data(), &status);
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
    LocalResourcePointer<UCalendar> calendar(ucal_open(nullptr, 0, locale()->toNonGCUTF8StringData().data(), UCAL_DEFAULT, &status),
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

    ArrayObject* weekendArray = new ArrayObject(state);
    size_t index = 0;
    for (int32_t day = 1; day <= 7; ++day) {
        UCalendarWeekdayType type = canonicalizeDayOfWeekType(ucal_getDayOfWeekType(calendar.get(), static_cast<UCalendarDaysOfWeek>(convertUCalendarDaysOfWeekToMondayBasedDay(day)), &status));
        if (!U_SUCCESS(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid locale");
            return Value();
        }
        switch (type) {
        case UCAL_WEEKDAY:
            break;
        case UCAL_WEEKEND:
            weekendArray->setIndexedProperty(state, Value(index++), Value(day), weekendArray);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    Object* result = new Object(state);
    result->set(state, ObjectPropertyName(state, String::fromASCII("firstDay")), Value(convertUCalendarDaysOfWeekToMondayBasedDay(firstDayOfWeek)), result);
    result->set(state, ObjectPropertyName(state, String::fromASCII("weekend")), weekendArray, result);
    result->set(state, ObjectPropertyName(state, String::fromASCII("minimalDays")), Value(minimalDays), result);
    return result;
}

Value IntlLocaleObject::timeZones(ExecutionState& state)
{
    auto region = this->region();
    if (!region->length()) {
        return Value();
    }

    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UEnumeration> enumeration(ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, region->toNonGCUTF8StringData().data(), nullptr, &status),
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
