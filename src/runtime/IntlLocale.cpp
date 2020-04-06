#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
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

#include "Escargot.h"
#include "Context.h"
#include "ExecutionState.h"
#include "Value.h"
#include "Intl.h"
#include "IntlLocale.h"

namespace Escargot {

struct BuildLocaleResult {
    Optional<String*> locale;
    String* language;
    Optional<String*> script;
    Optional<String*> region;
    Optional<String*> variant;

    BuildLocaleResult()
    {
        language = String::emptyString;
    }
};

static BuildLocaleResult buildLocale(ExecutionState& state, const std::string& u8Tag, Optional<String*> language, Optional<String*> script, Optional<String*> region)
{
    // We should not allow extlang here too(double-check needed)
    auto tags = split(u8Tag, '-');
    if (tags.size() >= 2 && tags[1].length() == 3 && isAllSpecialCharacters(tags[1], isASCIIAlpha)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Incorrect locale information provided");
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t resultLen = uloc_getLanguage(u8Tag.data(), nullptr, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Unexpected error is occured while parsing locale");
    }
    char* tagLanguage = (char*)alloca(sizeof(char) * resultLen + sizeof(char));
    status = U_ZERO_ERROR;
    uloc_getLanguage(u8Tag.data(), tagLanguage, sizeof(char) * resultLen + sizeof(char), &status);
    ASSERT(!U_FAILURE(status));

    Optional<char*> tagScript = (char*)alloca(12);
    resultLen = uloc_getScript(u8Tag.data(), tagScript.value(), 12, &status);
    if (!resultLen) {
        tagScript = nullptr;
    }
    status = U_ZERO_ERROR;

    Optional<char*> tagCountry;
    resultLen = uloc_getCountry(u8Tag.data(), nullptr, 0, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR && resultLen) {
        tagCountry = (char*)alloca(sizeof(char) * resultLen + sizeof(char));
        status = U_ZERO_ERROR;
        uloc_getCountry(u8Tag.data(), tagCountry.value(), sizeof(char) * resultLen + sizeof(char), &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Unexpected error is occured while parsing locale");
        }
    }
    status = U_ZERO_ERROR;

    Optional<char*> tagVariant;
    resultLen = uloc_getVariant(u8Tag.data(), nullptr, 0, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR && resultLen) {
        tagVariant = (char*)alloca(sizeof(char) * resultLen + sizeof(char));
        status = U_ZERO_ERROR;
        uloc_getVariant(u8Tag.data(), tagVariant.value(), sizeof(char) * resultLen + sizeof(char), &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Unexpected error is occured while parsing locale");
        }
    }
    status = U_ZERO_ERROR;

    BuildLocaleResult result;

    std::string localeStr;

    if (language) {
        localeStr = Intl::preferredLanguage(language.value()->toNonGCUTF8StringData());
        result.language = language.value();
    } else {
        localeStr = Intl::preferredLanguage(tagLanguage);
        result.language = String::fromUTF8(tagLanguage, strlen(tagLanguage));
    }

    if (script) {
        localeStr += "-";
        localeStr += script.value()->toNonGCUTF8StringData();
        result.script = script.value();
    } else if (tagScript) {
        localeStr += "-";
        localeStr += tagScript.value();
        result.script = String::fromUTF8(tagScript.value(), strlen(tagScript.value()));
    }

    if (region) {
        localeStr += "-";
        localeStr += region.value()->toNonGCUTF8StringData();
        result.region = region.value();
    } else if (tagCountry) {
        localeStr += "-";
        localeStr += tagCountry.value();
        result.region = String::fromUTF8(tagCountry.value(), strlen(tagCountry.value()));
    }

    if (tagVariant) {
        localeStr += "-";
        localeStr += tagVariant.value();
        result.variant = String::fromUTF8(tagVariant.value(), strlen(tagVariant.value()));
    }

    // We should not allow extlang here too(double-check needed)
    tags = split(localeStr, '-');
    if (tags.size() >= 2 && tags[1].length() == 3 && isAllSpecialCharacters(tags[1], isASCIIAlpha)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Incorrect locale information provided");
    }

    resultLen = uloc_getBaseName(localeStr.data(), nullptr, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        return BuildLocaleResult();
    }

    char* resultLocale = (char*)alloca(sizeof(char) * resultLen + sizeof(char));
    status = U_ZERO_ERROR;
    uloc_getBaseName(localeStr.data(), resultLocale, sizeof(char) * resultLen + sizeof(char), &status);
    ASSERT(!U_FAILURE(status));

    for (int32_t i = 0; i < resultLen; i++) {
        if (resultLocale[i] == '_') {
            resultLocale[i] = '-';
        }
    }

    result.locale = String::fromUTF8(resultLocale, resultLen);
    return result;
}

static BuildLocaleResult applyOptionsToTag(ExecutionState& state, String* tag, Optional<Object*> options)
{
    // If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError exception.
    if (!Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(state, tag)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "First argument of Intl.Locale should have valid tag");
    }

    auto u8Tag = tag->toNonGCUTF8StringData();

    String* languageString = String::fromASCII("language");
    // Let language be ? GetOption(options, "language", "string", undefined, undefined).
    Value language = options ? Intl::getOption(state, options.value(), languageString, state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If language is not undefined, then
    if (!language.isUndefined()) {
        // If language does not match the unicode_language_subtag production, throw a RangeError exception.
        auto u8Lang = language.asString()->toNonGCUTF8StringData();
        UErrorCode status = U_ZERO_ERROR;
        int32_t len = uloc_forLanguageTag(u8Lang.data(), nullptr, 0, nullptr, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR || u8Lang.length() != 2 || !isAllSpecialCharacters(u8Lang, isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "lanuage tag you give into Intl.Locale is invalid");
        }
    }

    String* scriptString = String::fromASCII("script");
    // Let script be ? GetOption(options, "script", "string", undefined, undefined).
    Value script = options ? Intl::getOption(state, options.value(), scriptString, state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If script is not undefined, then
    if (!script.isUndefined()) {
        // If script does not match the unicode_script_subtag, throw a RangeError exception.
        String* scriptString = script.asString();
        if (scriptString->length() != 4 || !scriptString->isAllSpecialCharacters(isASCIIAlpha) || !buildLocale(state, u8Tag, language.isUndefined() ? nullptr : language.asString(), scriptString, nullptr).locale) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "script you give into Intl.Locale is invalid");
        }
    }

    String* regionString = String::fromASCII("region");
    // Let region be ? GetOption(options, "region", "string", undefined, undefined).
    Value region = options ? Intl::getOption(state, options.value(), regionString, state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If region is not undefined, then
    if (!region.isUndefined()) {
        // If region does not match the unicode_region_subtag, throw a RangeError exception.
        String* regionString = region.asString();
        if ((!(regionString->length() == 2 && regionString->isAllSpecialCharacters(isASCIIAlpha)) && !(regionString->length() == 3 && regionString->isAllSpecialCharacters(isASCIIDigit)))
            || !buildLocale(state, u8Tag, language.isUndefined() ? nullptr : language.asString(), script.isUndefined() ? nullptr : script.asString(), region.asString()).locale) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "region you give into Intl.Locale is invalid");
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
    auto result = buildLocale(state, u8Tag, language.isUndefined() ? nullptr : language.asString(),
                              script.isUndefined() ? nullptr : script.asString(), region.isUndefined() ? nullptr : region.asString());
    ASSERT(result.locale);
    return result;
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
    : Object(state, proto)
{
    // Set tag to ? ApplyOptionsToTag(tag, options).
    BuildLocaleResult buildResult = applyOptionsToTag(state, tag, options);
    String* baseName = buildResult.locale.value();
    m_language = buildResult.language;

    // Let opt be a new Record.
    Optional<String*> ca;
    Optional<String*> co;
    Optional<String*> hc;
    Optional<String*> kf;
    Optional<String*> kn;
    Optional<String*> nu;

    if (tag->contains("-u-")) {
        String* e = tag->substring(tag->find("-u-"), tag->length());
        ca = Intl::unicodeExtensionValue(state, e, "ca");
        co = Intl::unicodeExtensionValue(state, e, "co");
        hc = Intl::unicodeExtensionValue(state, e, "hc");
        kf = Intl::unicodeExtensionValue(state, e, "kf");
        kn = Intl::unicodeExtensionValue(state, e, "kn");
        nu = Intl::unicodeExtensionValue(state, e, "nu");
    }

    // Let calendar be ? GetOption(options, "calendar", "string", undefined, undefined).
    String* calendarString = String::fromASCII("calendar");
    Value calendar = options ? Intl::getOption(state, options.value(), calendarString, state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If calendar is not undefined, then
    if (!calendar.isUndefined()) {
        // If calendar does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        // Overwrite existing, or insert new key-value to the locale string.
        // Set opt.[[ca]] to calendar.
        ca = calendar.asString();
    }

    // If calendar does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
    if (ca) {
        std::string value = ca->toNonGCUTF8StringData();
        if (!uloc_toLegacyType("calendar", value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "`calendar` option you input is not valid");
        }
    }

    // Let collation be ? GetOption(options, "collation", "string", undefined, undefined).
    Value collation = options ? Intl::getOption(state, options.value(), String::fromASCII("collation"), state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If collation is not undefined, then
    if (!collation.isUndefined()) {
        // If collation does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        // Set opt.[[co]] to collation.
        co = collation.asString();
    }

    // If collation does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
    if (co) {
        std::string value = co->toNonGCUTF8StringData();
        if (!uloc_toLegacyType("collation", value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "`collation` option you input is not valid");
        }
    }

    // Let hc be ? GetOption(options, "hourCycle", "string", « "h11", "h12", "h23", "h24" », undefined).
    ValueVector vv;
    vv.pushBack(String::fromASCII("h11"));
    vv.pushBack(String::fromASCII("h12"));
    vv.pushBack(String::fromASCII("h23"));
    vv.pushBack(String::fromASCII("h24"));
    Value hourCycle = options ? Intl::getOption(state, options.value(), String::fromASCII("hourCycle"), state.context()->staticStrings().string.string(), vv, Value()) : Value();
    // Set opt.[[hc]] to hc.
    if (!hourCycle.isUndefined()) {
        hc = hourCycle.asString();
    }

    // Let kf be ? GetOption(options, "caseFirst", "string", « "upper", "lower", "false" », undefined).
    vv.clear();
    vv.pushBack(String::fromASCII("upper"));
    vv.pushBack(String::fromASCII("lower"));
    vv.pushBack(String::fromASCII("false"));
    // Set opt.[[kf]] to kf.
    Value caseFirst = options ? Intl::getOption(state, options.value(), String::fromASCII("caseFirst"), state.context()->staticStrings().string.string(), vv, Value()) : Value();
    if (!caseFirst.isUndefined()) {
        kf = caseFirst.asString();
    }

    // Let kn be ? GetOption(options, "numeric", "boolean", undefined, undefined).
    Value numeric = options ? Intl::getOption(state, options.value(), String::fromASCII("numeric"), state.context()->staticStrings().boolean.string(), ValueVector(), Value()) : Value();
    // If kn is not undefined, set kn to ! ToString(kn).
    if (!numeric.isUndefined()) {
        kn = numeric.toString(state);
    }
    // Set opt.[[kn]] to kn.

    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem = options ? Intl::getOption(state, options.value(), String::fromASCII("numberingSystem"), state.context()->staticStrings().string.string(), ValueVector(), Value()) : Value();
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        nu = numberingSystem.asString();
    }
    // Set opt.[[nu]] to numberingSystem.

    // If numberingSystem does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.
    if (nu) {
        std::string value = nu->toNonGCUTF8StringData();
        if (!uloc_toLegacyType(uloc_toLegacyKey("nu"), value.data()) || !checkOptionValueIsAlphaNum38DashAlphaNum38(value)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "`collation` option you input is not valid");
        }
    }

    // Let r be ! ApplyUnicodeExtensionToTag(tag, opt, relevantExtensionKeys).
    m_baseName = baseName;
    m_calendar = ca;
    m_caseFirst = kf;
    m_collation = co;
    m_hourCycle = hc;
    m_numberingSystem = nu;
    m_numeric = kn;
}

static bool stringReplace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


String* IntlLocaleObject::locale()
{
    StringBuilder sb;

    sb.appendString(m_baseName);
    sb.appendString("-u");

    if (m_calendar) {
        sb.appendString("-ca-");
        sb.appendString(m_calendar.value());
    }

    if (m_caseFirst) {
        sb.appendString("-kf-");
        sb.appendString(m_caseFirst.value());
    }

    if (m_collation) {
        sb.appendString("-co-");
        sb.appendString(m_collation.value());
    }

    if (m_hourCycle) {
        sb.appendString("-hc-");
        sb.appendString(m_hourCycle.value());
    }

    if (m_numeric) {
        sb.appendString("-kn-");
        sb.appendString(m_numeric.value());
    }

    if (m_numberingSystem) {
        sb.appendString("-nu-");
        sb.appendString(m_numberingSystem.value());
    }

    String* testString = sb.finalize();
    std::string string = testString->toNonGCUTF8StringData();

    UErrorCode status = U_ZERO_ERROR;
    char* buffer = (char*)alloca(string.length() + 1);
    int32_t len = uloc_toLanguageTag(string.data(), buffer, string.length() + 1, FALSE, &status);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        buffer = (char*)alloca(string.length() * 2 + 1);
        uloc_toLanguageTag(string.data(), buffer, len + 1, FALSE, &status);
        ASSERT(U_SUCCESS(status));
        string = buffer;
    } else {
        string = buffer;
    }

    stringReplace(string, "kn-true", "kn");

    return String::fromUTF8(string.data(), string.length());
}

} // namespace Escargot
#endif
