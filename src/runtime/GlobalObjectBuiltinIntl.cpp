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
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"
#include "ArrayObject.h"

namespace Escargot {

static std::string grandfatheredLangTag(String* locale)
{
    // grandfathered = irregular / regular
    std::unordered_map<std::string, std::string> tagMap;
    // Irregular.
    tagMap["en-gb-oed"] = "en-GB-oed";
    tagMap["i-ami"] = "ami";
    tagMap["i-bnn"] = "bnn";
    tagMap["i-default"] = "i-default";
    tagMap["i-enochian"] = "i-enochian";
    tagMap["i-hak"] = "hak";
    tagMap["i-klingon"] = "tlh";
    tagMap["i-lux"] = "lb";
    tagMap["i-mingo"] = "i-mingo";
    tagMap["i-navajo"] = "nv";
    tagMap["i-pwn"] = "pwn";
    tagMap["i-tao"] = "tao";
    tagMap["i-tay"] = "tay";
    tagMap["i-tsu"] = "tsu";
    tagMap["sgn-be-fr"] = "sfb";
    tagMap["sgn-be-nl"] = "vgt";
    tagMap["sgn-ch-de"] = "sgg";
    // Regular.
    tagMap["art-lojban"] = "jbo";
    tagMap["cel-gaulish"] = "cel-gaulish";
    tagMap["no-bok"] = "nb";
    tagMap["no-nyn"] = "nn";
    tagMap["zh-guoyu"] = "cmn";
    tagMap["zh-hakka"] = "hak";
    tagMap["zh-min"] = "zh-min";
    tagMap["zh-min-nan"] = "nan";
    tagMap["zh-xiang"] = "hsn";

    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());
    std::transform(stdUTF8.begin(), stdUTF8.end(), stdUTF8.begin(), ::tolower);
    auto iter = tagMap.find(stdUTF8);

    if (iter != tagMap.end()) {
        return iter->second;
    }
    return std::string();
}

static std::vector<std::string> split(const std::string& s, char seperator)
{
    std::vector<std::string> output;
    std::string::size_type prevPos = 0, pos = 0;

    while ((pos = s.find(seperator, pos)) != std::string::npos) {
        std::string substring(s.substr(prevPos, pos - prevPos));
        output.push_back(substring);
        prevPos = ++pos;
    }

    output.push_back(s.substr(prevPos, pos - prevPos));
    return output;
}

static bool isASCIIAlpha(char ch)
{
    return isalpha(ch);
}

static bool isASCIIDigit(char ch)
{
    return isdigit(ch);
}

static bool isASCIIAlphanumeric(char ch)
{
    return isASCIIAlpha(ch) || isASCIIDigit(ch);
}

static bool isAllSpecialCharacters(const std::string& s, bool (*fn)(char))
{
    bool isAllSpecial = true;
    for (size_t i = 0; i < s.size(); i++) {
        if (!fn(s[i])) {
            isAllSpecial = false;
            break;
        }
    }
    return isAllSpecial;
}

static String* icuLocaleToBCP47Tag(String* string)
{
    StringBuilder sb;
    for (size_t i = 0; i < string->length(); i++) {
        char16_t ch = string->charAt(i);
        if (ch == '_')
            ch = '-';
        sb.appendChar(ch);
    }
    return sb.finalize();
}

static const char* const intlCollatorRelevantExtensionKeys[2] = { "co", "kn" };
static const int intlCollatorRelevantExtensionKeysLength = 2;
static const size_t indexOfExtensionKeyCo = 0;
static const size_t indexOfExtensionKeyKn = 1;

typedef std::vector<std::string> (*LocaleDataImplFunction)(String* locale, size_t keyIndex);

static std::vector<std::string> sortLocaleData(String* locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo: {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.push_back(std::string());

        UErrorCode status = U_ZERO_ERROR;

        auto utf8 = locale->toUTF8StringData();
        UEnumeration* enumeration = ucol_getKeywordValuesForLocale("collation", utf8.data(), false, &status);
        if (U_SUCCESS(status)) {
            const char* collation;
            while ((collation = uenum_next(enumeration, nullptr, &status)) && U_SUCCESS(status)) {
                // 10.2.3 "The values "standard" and "search" must not be used as elements in any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co array."
                if (!strcmp(collation, "standard") || !strcmp(collation, "search"))
                    continue;

                // Map keyword values to BCP 47 equivalents.
                if (!strcmp(collation, "dictionary"))
                    collation = "dict";
                else if (!strcmp(collation, "gb2312han"))
                    collation = "gb2312";
                else if (!strcmp(collation, "phonebook"))
                    collation = "phonebk";
                else if (!strcmp(collation, "traditional"))
                    collation = "trad";

                keyLocaleData.push_back(std::string(collation));
            }
            uenum_close(enumeration);
        }
        break;
    }
    case indexOfExtensionKeyKn:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("true"));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

static std::vector<std::string> searchLocaleData(String* locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo:
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.push_back(std::string());
        break;
    case indexOfExtensionKeyKn:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("true"));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}


static std::string privateUseLangTag(const std::vector<std::string>& parts, size_t startIndex)
{
    size_t numParts = parts.size();
    size_t currentIndex = startIndex;

    // Check for privateuse.
    // privateuse = "x" 1*("-" (2*8alphanum))
    StringBuilder privateuse;
    while (currentIndex < numParts) {
        std::string singleton = parts[currentIndex];
        unsigned singletonLength = singleton.length();
        bool isValid = (singletonLength == 1 && (singleton == "x" || singleton == "X"));
        if (!isValid)
            break;

        if (currentIndex != startIndex)
            privateuse.appendChar('-');

        ++currentIndex;
        unsigned numExtParts = 0;
        privateuse.appendChar('x');
        while (currentIndex < numParts) {
            std::string extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 1 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            privateuse.appendChar('-');
            std::transform(extPart.begin(), extPart.end(), extPart.begin(), ::tolower);
            privateuse.appendString(String::fromASCII(extPart.c_str()));
        }

        // Requires at least one production.
        if (!numExtParts)
            return std::string();
    }

    // Leftovers makes it invalid.
    if (currentIndex < numParts)
        return std::string();

    String* e = privateuse.finalize();
    auto estd = e->toUTF8StringData();
    return std::string(estd.data(), estd.length());
}

static std::string canonicalLangTag(const std::vector<std::string>& parts)
{
    // Follows the grammar at https://www.rfc-editor.org/rfc/bcp/bcp47.txt
    // langtag = language ["-" script] ["-" region] *("-" variant) *("-" extension) ["-" privateuse]

    size_t numParts = parts.size();
    // Check for language.
    // language = 2*3ALPHA ["-" extlang] / 4ALPHA / 5*8ALPHA
    size_t currentIndex = 0;
    std::string language = parts[currentIndex];
    unsigned languageLength = language.length();
    bool canHaveExtlang = languageLength >= 2 && languageLength <= 3;
    bool isValidLanguage = languageLength >= 2 && languageLength <= 8 && isAllSpecialCharacters(language, isASCIIAlpha);
    if (!isValidLanguage)
        return std::string();

    ++currentIndex;
    StringBuilder canonical;

    std::transform(language.begin(), language.end(), language.begin(), ::tolower);
    canonical.appendString(String::fromASCII(language.c_str()));

    // Check for extlang.
    // extlang = 3ALPHA *2("-" 3ALPHA)
    if (canHaveExtlang) {
        for (unsigned times = 0; times < 3 && currentIndex < numParts; ++times) {
            std::string extlang = parts[currentIndex];
            unsigned extlangLength = extlang.length();
            if (extlangLength == 3 && isAllSpecialCharacters(extlang, isASCIIAlpha)) {
                ++currentIndex;
                canonical.appendString("-");
                std::transform(extlang.begin(), extlang.end(), extlang.begin(), ::tolower);
                canonical.appendString(String::fromASCII(extlang.c_str()));
            } else
                break;
        }
    }

    // Check for script.
    // script = 4ALPHA
    if (currentIndex < numParts) {
        std::string script = parts[currentIndex];
        unsigned scriptLength = script.length();
        if (scriptLength == 4 && isAllSpecialCharacters(script, isASCIIAlpha)) {
            ++currentIndex;
            canonical.appendString("-");
            std::transform(script.begin(), script.end(), script.begin(), ::tolower);
            canonical.appendChar((char)::toupper(script[0]));
            script = script.substr(1, 3);
            canonical.appendString(String::fromASCII(script.c_str()));
        }
    }

    // Check for region.
    // region = 2ALPHA / 3DIGIT
    if (currentIndex < numParts) {
        std::string region = parts[currentIndex];
        unsigned regionLength = region.length();
        bool isValidRegion = ((regionLength == 2 && isAllSpecialCharacters(region, isASCIIAlpha))
                              || (regionLength == 3 && isAllSpecialCharacters(region, isASCIIDigit)));
        if (isValidRegion) {
            ++currentIndex;
            canonical.appendChar('-');
            std::transform(region.begin(), region.end(), region.begin(), ::toupper);
            canonical.appendString(String::fromASCII(region.c_str()));
        }
    }

    // Check for variant.
    // variant = 5*8alphanum / (DIGIT 3alphanum)
    std::unordered_set<std::string> subtags;
    while (currentIndex < numParts) {
        std::string variant = parts[currentIndex];
        unsigned variantLength = variant.length();
        bool isValidVariant = ((variantLength >= 5 && variantLength <= 8 && isAllSpecialCharacters(variant, isASCIIAlphanumeric))
                               || (variantLength == 4 && isASCIIDigit(variant[0]) && isAllSpecialCharacters(variant.substr(1, 3), isASCIIAlphanumeric)));
        if (!isValidVariant)
            break;

        // Cannot include duplicate subtags (case insensitive).
        std::string lowerVariant = variant;
        std::transform(lowerVariant.begin(), lowerVariant.end(), lowerVariant.begin(), ::tolower);
        auto iter = subtags.find(lowerVariant);
        if (iter != subtags.end()) {
            return std::string();
        }
        subtags.insert(lowerVariant);

        ++currentIndex;

        // Reordering variant subtags is not required in the spec.
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(lowerVariant.c_str()));
    }

    // Check for extension.
    // extension = singleton 1*("-" (2*8alphanum))
    // singleton = alphanum except x or X
    subtags.clear();
    std::vector<std::string> extensions;
    while (currentIndex < numParts) {
        std::string possibleSingleton = parts[currentIndex];
        unsigned singletonLength = possibleSingleton.length();
        bool isValidSingleton = (singletonLength == 1 && possibleSingleton != "x" && possibleSingleton != "X" && isASCIIAlphanumeric(possibleSingleton[0]));
        if (!isValidSingleton)
            break;

        // Cannot include duplicate singleton (case insensitive).
        std::string singleton = possibleSingleton;
        std::transform(possibleSingleton.begin(), possibleSingleton.end(), possibleSingleton.begin(), ::tolower);

        auto iter = subtags.find(singleton);
        if (iter != subtags.end()) {
            return std::string();
        }
        subtags.insert(singleton);

        ++currentIndex;
        int numExtParts = 0;
        StringBuilder extension;
        extension.appendString(String::fromASCII(singleton.c_str()));
        while (currentIndex < numParts) {
            std::string extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 2 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            extension.appendChar('-');
            std::transform(extPart.begin(), extPart.end(), extPart.begin(), ::tolower);
            extension.appendString(String::fromASCII(extPart.c_str()));
        }

        // Requires at least one production.
        if (!numExtParts)
            return std::string();

        String* e = extension.finalize();
        auto estd = e->toUTF8StringData();
        extensions.push_back(std::string(estd.data()));
    }

    // Add extensions to canonical sorted by singleton.
    std::sort(
        extensions.begin(),
        extensions.end(),
        [](const std::string& a, const std::string& b) -> bool {
            return a[0] < b[0];
        });
    size_t numExtenstions = extensions.size();
    for (size_t i = 0; i < numExtenstions; ++i) {
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(extensions[i].c_str()));
    }

    // Check for privateuse.
    if (currentIndex < numParts) {
        std::string privateuse = privateUseLangTag(parts, currentIndex);
        if (privateuse.length() == 0)
            return std::string();
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(privateuse.c_str()));
    }

    String* e = canonical.finalize();
    auto estd = e->toUTF8StringData();
    return std::string(estd.data(), estd.length());
}

static String* isStructurallyValidLanguageTagAndcanonicalizeLanguageTag(ExecutionState& state, String* locale)
{
    std::string grandfather = grandfatheredLangTag(locale);
    if (grandfather.length() != 0)
        return String::fromUTF8(grandfather.c_str(), grandfather.length());

    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());

    std::vector<std::string> parts = split(stdUTF8, '-');

    if (parts.size()) {
        std::string langtag = canonicalLangTag(parts);
        if (langtag.length() != 0)
            return String::fromASCII(langtag.c_str());

        std::string privateuse = privateUseLangTag(parts, 0);
        if (privateuse.length() != 0)
            return String::fromASCII(privateuse.c_str());
    }

    return nullptr;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.1
static ValueVector canonicalizeLocaleList(ExecutionState& state, Value locales)
{
    // If locales is undefined, then
    if (locales.isUndefined()) {
        // Return a new empty List.
        return ValueVector();
    }
    // Let seen be a new empty List.
    ValueVector seen;
    // If locales is a String value, then
    if (locales.isString()) {
        // Let locales be a new array created as if
        // by the expression new Array(locales) where Array is the standard built-in constructor with that name and locales is the value of locales.
        Value callArg[] = { locales };
        locales = state.context()->globalObject()->array()->newInstance(state, 1, callArg);
    }
    // Let O be ToObject(locales).
    Object* O = locales.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    Value lenValue = Value(O->length(state));
    // Let len be ToUint32(lenValue).
    uint32_t len = lenValue.toUint32(state);
    // Let k be 0.
    // Repeat, while k < len
    uint32_t k = 0;
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectGetResult pkResult = O->get(state, ObjectPropertyName(state, Value(k)));
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        // If kPresent is true, then
        if (pkResult.hasValue()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = pkResult.value(state, O);
            // If the type of kValue is not String or Object, then throw a TypeError exception.
            if (!kValue.isString() && !kValue.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Type of element of locales must be String or Object");
            }
            // Let tag be ToString(kValue).
            String* tag = kValue.toString(state);
            // If the result of calling the abstract operation IsStructurallyValidLanguageTag (defined in 6.2.2), passing tag as the argument,
            // is false, then throw a RangeError exception.
            // Let tag be the result of calling the abstract operation CanonicalizeLanguageTag (defined in 6.2.3), passing tag as the argument.
            // If tag is not an element of seen, then append tag as the last element of seen.
            tag = isStructurallyValidLanguageTagAndcanonicalizeLanguageTag(state, tag);
            if (!tag) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got Invalid locale");
            }
            bool has = false;
            for (size_t i = 0; i < seen.size(); i++) {
                if (seen[i].equalsTo(state, tag)) {
                    has = true;
                    break;
                }
            }
            if (!has) {
                seen.pushBack(tag);
            }
        }
        // Increase k by 1.
        k++;
    }

    // Return seen.
    return seen;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.9
static Value getOption(ExecutionState& state, Object* options, Value property, String* type, ValueVector values, const Value& fallback)
{
    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Assert: type is "boolean" or "string".
        // If type is "boolean", then let value be ToBoolean(value).
        if (type->equals("boolean")) {
            value = Value(value.toBoolean(state));
        }
        // If type is "string", then let value be ToString(value).
        if (type->equals("string")) {
            value = Value(value.toString(state));
        }
        // If values is not undefined, then
        if (values.size()) {
            // If values does not contain an element equal to value, then throw a RangeError exception.
            bool contains = false;
            for (size_t i = 0; i < values.size(); i++) {
                if (values[i].equalsTo(state, value)) {
                    contains = true;
                }
            }
            if (!contains) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid value");
            }
        }
        // Return value.
        return value;
    } else {
        // Else return fallback.
        return fallback;
    }
}

static String* removeUnicodeLocaleExtension(ExecutionState& state, String* locale)
{
    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());
    std::vector<std::string> parts = split(stdUTF8, '-');

    StringBuilder builder;
    size_t partsSize = parts.size();
    if (partsSize > 0)
        builder.appendString(String::fromUTF8(parts[0].data(), parts[0].size()));
    for (size_t p = 1; p < partsSize; ++p) {
        if (parts[p] == "u") {
            // Skip the u- and anything that follows until another singleton.
            // While the next part is part of the unicode extension, skip it.
            while (p + 1 < partsSize && parts[p + 1].length() > 1)
                ++p;
        } else {
            builder.appendChar('-');
            builder.appendString(String::fromUTF8(parts[p].data(), parts[p].size()));
        }
    }
    return builder.finalize();
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.2
static String* bestAvailableLocale(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, Value locale)
{
    // Let candidate be locale.
    String* candidate = locale.toString(state);

    // Repeat
    while (candidate->length()) {
        // If availableLocales contains an element equal to candidate, then return candidate.
        bool contains = false;
        for (size_t i = 0; i < availableLocales.size(); i++) {
            if (availableLocales[i]->equals(candidate)) {
                contains = true;
                break;
            }
        }

        if (contains)
            return candidate;

        // Let pos be the character index of the last occurrence of "-" (U+002D) within candidate. If that character does not occur, return undefined.
        size_t pos = candidate->rfind(String::fromASCII("-"), candidate->length() - 1);
        if (pos == SIZE_MAX)
            return String::emptyString;

        // If pos ≥ 2 and the character "-" occurs at index pos-2 of candidate, then decrease pos by 2.
        if (pos >= 2 && candidate->charAt(pos - 2) == '-')
            pos -= 2;

        // d. Let candidate be the substring of candidate from position 0, inclusive, to position pos, exclusive.
        candidate = candidate->subString(0, pos);
    }

    return String::emptyString;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.6
static ValueVector lookupSupportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
{
    // Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();
    // Let subset be a new empty List.
    ValueVector subset;
    // Let k be 0.
    size_t k = 0;
    // Repeat while k < len
    while (k < len) {
        // Let locale be the element of requestedLocales at 0-origined list position k.
        Value locale = requestedLocales[k];
        // Let noExtensionsLocale be the String value that is locale with all Unicode locale extension sequences removed.
        String* noExtensionsLocale = removeUnicodeLocaleExtension(state, locale.toString(state));
        // Let availableLocale be the result of calling the BestAvailableLocale abstract operation (defined in 9.2.2) with arguments availableLocales and noExtensionsLocale.
        String* availableLocale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
        // If availableLocale is not undefined, then append locale to the end of subset.
        if (availableLocale->length()) {
            subset.pushBack(locale);
        }
        // Increment k by 1.
        k++;
    }

    return subset;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.7
static ValueVector bestfitSupportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
{
    // TODO
    return lookupSupportedLocales(state, availableLocales, requestedLocales);
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.8
static Value supportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options)
{
    Value matcher;
    // If options is not undefined, then
    if (!options.isUndefined()) {
        // Let options be ToObject(options).
        options = options.toObject(state);
        // Let matcher be the result of calling the [[Get]] internal method of options with argument "localeMatcher".
        matcher = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII("localeMatcher"))).value(state, options.asObject());
        // If matcher is not undefined, then
        if (!matcher.isUndefined()) {
            // Let matcher be ToString(matcher).
            matcher = matcher.toString(state);
            // If matcher is not "lookup" or "best fit", then throw a RangeError exception.
            if (matcher.asString()->equals("lookup") || matcher.asString()->equals("best fit")) {
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid value on options.localeMatcher");
            }
        }
    }

    // If matcher is undefined or "best fit", then
    ValueVector subset;
    if (matcher.isUndefined() || matcher.equalsTo(state, String::fromASCII("best fit"))) {
        // Let subset be the result of calling the BestFitSupportedLocales abstract operation (defined in 9.2.7) with arguments availableLocales and requestedLocales.
        subset = bestfitSupportedLocales(state, availableLocales, requestedLocales);
    } else {
        // Let subset be the result of calling the LookupSupportedLocales abstract operation (defined in 9.2.6) with arguments availableLocales and requestedLocales.
        subset = lookupSupportedLocales(state, availableLocales, requestedLocales);
    }

    // For each named own property name P of subset,
    ArrayObject* result = new ArrayObject(state);
    for (size_t i = 0; i < subset.size(); i++) {
        String* P = subset[i].toString(state);
        // Let desc be the result of calling the [[GetOwnProperty]] internal method of subset with P.
        // Set desc.[[Writable]] to false.
        // Set desc.[[Configurable]] to false.
        // Call the [[DefineOwnProperty]] internal method of subset with P, desc, and true as arguments.
        result->defineOwnProperty(state, ObjectPropertyName(state, Value(i)), ObjectPropertyDescriptor(P, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    }

    return result;
}

static String* defaultLocale(ExecutionState& state)
{
    const char* locale = uloc_getDefault();
    String* localeString = String::fromUTF8(locale, strlen(locale));
    return icuLocaleToBCP47Tag(localeString);
}

struct IntlMatcherResult {
    IntlMatcherResult()
    {
        extension = locale = String::emptyString;
        extensionIndex = SIZE_MAX;
    }

    String* locale;
    String* extension;
    size_t extensionIndex;
};

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.3
static IntlMatcherResult lookupMatcher(ExecutionState& state, const Vector<String*, gc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
{
    // Let i be 0.
    size_t i = 0;
    // Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();
    // Let availableLocale be undefined.
    String* availableLocale = String::emptyString;

    String* locale = String::emptyString;
    String* noExtensionsLocale = String::emptyString;

    // Repeat while i < len and availableLocale is undefined:
    while (i < len && availableLocale->length() == 0) {
        // Let locale be the element of requestedLocales at 0-origined list position i.
        locale = requestedLocales[i].toString(state);
        // Let noExtensionsLocale be the String value that is locale with all Unicode locale extension sequences removed.
        noExtensionsLocale = removeUnicodeLocaleExtension(state, locale);
        // Let availableLocale be the result of calling the BestAvailableLocale abstract operation (defined in 9.2.2) with arguments availableLocales and noExtensionsLocale.
        availableLocale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
        // Increase i by 1.
        i++;
    }

    // Let result be a new Record.
    IntlMatcherResult result;

    // If availableLocale is not undefined, then
    if (availableLocale->length()) {
        // Set result.[[locale]] to availableLocale.
        result.locale = availableLocale;
        // If locale and noExtensionsLocale are not the same String value, then
        if (!locale->equals(noExtensionsLocale)) {
            // Let extension be the String value consisting of the first substring of locale that is a Unicode locale extension sequence.
            // Let extensionIndex be the character position of the initial "-" of the first Unicode locale extension sequence within locale.
            // Set result.[[extension]] to extension.
            // Set result.[[extensionIndex]] to extensionIndex.

            size_t extensionIndex = locale->find(String::fromASCII("-u-"));
            RELEASE_ASSERT(extensionIndex != SIZE_MAX);

            size_t extensionLength = locale->length() - extensionIndex;
            size_t end = extensionIndex + 3;
            while (end < locale->length()) {
                end = locale->find(String::fromASCII("-"), end);
                if (end == SIZE_MAX)
                    break;
                if (end + 2 < locale->length() && locale->charAt(end + 2) == '-') {
                    extensionLength = end - extensionIndex;
                    break;
                }
                end++;
            }

            result.extension = locale->subString(extensionIndex, extensionLength);
            result.extensionIndex = extensionIndex;
        }
    } else {
        // Set result.[[locale]] to the value returned by the DefaultLocale abstract operation (defined in 6.2.4).
        result.locale = defaultLocale(state);
    }

    return result;
}

static IntlMatcherResult bestFitMatcher(ExecutionState& state, const Vector<String*, gc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
{
    // TODO
    return lookupMatcher(state, availableLocales, requestedLocales);
}


// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.5
static StringMap* resolveLocale(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, StringMap* options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData)
{
    // Let matcher be the value of options.[[localeMatcher]].
    auto iter = options->find(String::fromASCII("localeMatcher"));
    Value matcher;
    if (iter != options->end()) {
        matcher = iter->second;
    }

    IntlMatcherResult r;
    // If matcher is "lookup", then
    if (matcher.equalsTo(state, String::fromASCII("lookup"))) {
        // Let r be the result of calling the LookupMatcher abstract operation (defined in 9.2.3) with arguments availableLocales and requestedLocales.
        r = lookupMatcher(state, availableLocales, requestedLocales);
    } else {
        // Else
        // Let r be the result of calling the BestFitMatcher abstract operation (defined in 9.2.4) with arguments availableLocales and requestedLocales.
        r = bestFitMatcher(state, availableLocales, requestedLocales);
    }

    // Let foundLocale be the value of r.[[locale]].
    String* foundLocale = r.locale;

    std::vector<std::string> extensionSubtags;
    if (r.extension->length()) {
        // Let extension be the value of r.[[extension]].
        String* extension = r.extension;
        // Let extensionIndex be the value of r.[[extensionIndex]].
        size_t extensionIndex = r.extensionIndex;
        // Let split be the standard built-in function object defined in ES5, 15.5.4.14.
        // Let extensionSubtags be the result of calling the [[Call]] internal method of split with extension as the this value and an argument list containing the single item "-".
        // Let extensionSubtagsLength be the result of calling the [[Get]] internal method of extensionSubtags with argument "length".
        auto utf8 = extension->toUTF8StringData();
        std::string stdUTF8(utf8.data(), utf8.length());
        extensionSubtags = split(stdUTF8, '-');
    }

    // Let result be a new Record.
    StringMap* result = new StringMap;

    // Set result.[[dataLocale]] to foundLocale.
    result->insert(std::make_pair(String::fromASCII("dataLocale"), foundLocale));

    // Let supportedExtension be "-u".
    String* supportedExtension = String::fromASCII("-u");

    // Let i be 0.
    size_t i = 0;
    // Let len be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument "length".
    size_t len = relevantExtensionKeyCount;

    // Repeat while i < len:
    while (i < len) {
        // Let key be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument ToString(i).
        const char* key = relevantExtensionKeys[i];
        // Let foundLocaleData be the result of calling the [[Get]] internal method of localeData with the argument foundLocale.
        // Let keyLocaleData be the result of calling the [[Get]] internal method of foundLocaleData with the argument key.
        // Let value be the result of calling the [[Get]] internal method of keyLocaleData with argument "0".
        std::vector<std::string> keyLocaleData = localeData(foundLocale, i);

        // Let supportedExtensionAddition be "".
        String* supportedExtensionAddition = String::emptyString;

        std::string value = keyLocaleData[0];
        // If extensionSubtags is not undefined, then
        if (extensionSubtags.size()) {
            // Let keyPos be Call(%ArrayProto_indexOf%, extensionSubtags, «key») .
            size_t keyPos = SIZE_MAX;
            for (size_t i = 0; i < extensionSubtags.size(); i++) {
                if (extensionSubtags[i] == key) {
                    keyPos = i;
                    break;
                }
            }
            // If keyPos != -1, then
            if (keyPos != SIZE_MAX) {
                // If keyPos + 1 < extensionSubtagsLength and the length of the result of Get(extensionSubtags, ToString(keyPos +1)) is greater than 2, then
                if (keyPos + 1 < extensionSubtags.size() && extensionSubtags[keyPos + 1].length() > 2) {
                    const std::string& requestedValue = extensionSubtags[keyPos + 1];

                    bool contains = false;
                    for (size_t k = 0; k < keyLocaleData.size(); k++) {
                        if (keyLocaleData[i] == requestedValue) {
                            contains = true;
                        }
                    }
                    if (contains) {
                        value = requestedValue;
                        std::string s = std::string("-") + key + std::string("-") + value;
                        supportedExtensionAddition = String::fromUTF8(s.data(), s.length());
                    }
                } else if (std::find(keyLocaleData.begin(), keyLocaleData.end(), std::string("true")) != keyLocaleData.end()) {
                    // Else, if the result of Call(%StringProto_includes%, keyLocaleData, «"true"») is true, then
                    value = "true";
                }
            }
        }

        // If options has a field [[<key>]], then
        if (options->find(String::fromASCII(key)) != options->end()) {
            // Let optionsValue be the value of options.[[<key>]].
            std::string optionsValue = std::string(options->at(String::fromASCII(key))->toUTF8StringData().data());

            // If the result of calling the [[Call]] internal method of indexOf with keyLocaleData as the this value
            // and an argument list containing the single item optionsValue is not -1, then
            if (std::find(keyLocaleData.begin(), keyLocaleData.end(), optionsValue) != keyLocaleData.end()) {
                // If optionsValue is not equal to value, then
                if (optionsValue != value) {
                    // Let value be optionsValue.
                    // Let supportedExtensionAddition be "".
                    value = optionsValue;
                    supportedExtensionAddition = String::emptyString;
                }
            }
        }
        // Set result.[[<key>]] to value.
        result->insert(std::make_pair(String::fromASCII(key), String::fromUTF8(value.data(), value.length())));

        // Append supportedExtensionAddition to supportedExtension.
        StringBuilder sb;
        sb.appendString(supportedExtension);
        sb.appendString(supportedExtensionAddition);
        supportedExtension = sb.finalize();

        // Increase i by 1.
        i++;
    }

    // If the length of supportedExtension is greater than 2, then
    if (supportedExtension->length() > 2) {
        // Let preExtension be the substring of foundLocale from position 0, inclusive, to position extensionIndex, exclusive.
        String* preExtension = foundLocale->subString(0, r.extensionIndex);
        // Let postExtension be the substring of foundLocale from position extensionIndex to the end of the string.
        String* postExtension = foundLocale->subString(r.extensionIndex, foundLocale->length());
        // Let foundLocale be the concatenation of preExtension, supportedExtension, and postExtension.
        StringBuilder sb;
        sb.appendString(preExtension);
        sb.appendString(supportedExtension);
        sb.appendString(postExtension);
        foundLocale = sb.finalize();
    }
    // Set result.[[locale]] to foundLocale.
    result->insert(std::make_pair(String::fromASCII("locale"), foundLocale));
    // Return result.
    return result;
}

struct CollatorResolvedOptions {
    String* locale;
    String* sensitivity;
    String* usage;
    String* collation;
    bool numeric;
    bool ignorePunctuation;
    bool caseFirst;
};

static CollatorResolvedOptions collatorResolvedOptions(ExecutionState& state, Object* internalSlot)
{
    CollatorResolvedOptions opt;
    opt.locale = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("locale"))).value(state, internalSlot).toString(state);
    opt.sensitivity = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("sensitivity"))).value(state, internalSlot).toString(state);
    opt.usage = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("usage"))).value(state, internalSlot).toString(state);
    opt.collation = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("collation"))).value(state, internalSlot).toString(state);
    opt.numeric = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("numeric"))).value(state, internalSlot).toBoolean(state);
    opt.ignorePunctuation = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation"))).value(state, internalSlot).toBoolean(state);
    opt.caseFirst = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("caseFirst"))).value(state, internalSlot).toBoolean(state);
    return opt;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.1.1
static void initializeCollator(ExecutionState& state, Object* collator, Value locales, Value options)
{
    // If collator has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (collator->hasInternalSlot() && collator->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, PropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);

    // If options is undefined, then
    // Let options be the result of creating a new object as
    // if by the expression new Object() where Object is the standard built-in constructor with that name.
    if (options.isUndefined()) {
        options = new Object(state);
    } else {
        // Let options be ToObject(options).
        options = options.toObject(state);
    }
    // Let u be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "usage", "string", a List containing the two String values "sort" and "search", and "sort".
    ValueVector values;
    values.pushBack(String::fromASCII("sort"));
    values.pushBack(String::fromASCII("search"));
    Value u = getOption(state, options.asObject(), String::fromASCII("usage"), String::fromASCII("string"), values, String::fromASCII("sort"));

    // Set the [[usage]] internal property of collator to u.
    collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("usage")), u, collator->internalSlot());

    // Let Collator be the standard built-in object that is the initial value of Intl.Collator.
    FunctionObject* Collator = state.context()->globalObject()->intlCollator();

    // If u is "sort", then let localeData be the value of the [[sortLocaleData]] internal property of Collator;
    LocaleDataImplFunction localeData;
    if (u.equalsTo(state, String::fromASCII("sort"))) {
        localeData = sortLocaleData;
    } else {
        // else let localeData be the value of the [[searchLocaleData]] internal property of Collator.
        localeData = searchLocaleData;
    }

    // Let opt be a new Record.
    StringMap* opt = new StringMap();

    // Let matcher be the result of calling the GetOption abstract operation with arguments
    // options, "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit"
    values.clear();
    values.pushBack(String::fromASCII("lookup"));
    values.pushBack(String::fromASCII("best fit"));
    Value matcher = getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), values, String::fromASCII("best fit"));
    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Table 1 – Collator options settable through both extension keys and options properties
    // Key Property    Type            Values
    // kn  numeric    "boolean"
    // kf  caseFirst  "string"   "upper", "lower", "false"
    std::function<void(String*, Value, String*, ValueVector)> doTable1 = [&](String* keyColumn, Value propertyColumn, String* typeColumn, ValueVector values) {
        // Let key be the name given in the Key column of the row.
        Value key = keyColumn;

        // Let value be the result of calling the GetOption abstract operation, passing as arguments options, the name given in the Property column of the row,
        // the string given in the Type column of the row,
        // a List containing the Strings given in the Values column of the row or undefined if no strings are given, and undefined.
        Value value = getOption(state, options.asObject(), propertyColumn, typeColumn, values, Value());
        // If the string given in the Type column of the row is "boolean" and value is not undefined, then
        // Let value be ToString(value).
        if (typeColumn->equals("boolean") && !value.isUndefined()) {
            value = value.toString(state);
        }
        // Set opt.[[<key>]] to value.
        opt->insert(std::make_pair(keyColumn, value.toString(state)));
    };

    doTable1(String::fromASCII("kn"), String::fromASCII("numeric"), String::fromASCII("boolean"), ValueVector());
    ValueVector tempV;
    tempV.pushBack(String::fromASCII("upper"));
    tempV.pushBack(String::fromASCII("lower"));
    tempV.pushBack(String::fromASCII("false"));
    doTable1(String::fromASCII("kf"), String::fromASCII("caseFirst"), String::fromASCII("string"), tempV);

    // Let relevantExtensionKeys be the value of the [[relevantExtensionKeys]] internal property of Collator.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of Collator, requestedLocales, opt, relevantExtensionKeys, and localeData.
    StringMap* r = resolveLocale(state, state.context()->globalObject()->intlCollatorAvailableLocales(), requestedLocales, opt, intlCollatorRelevantExtensionKeys, intlCollatorRelevantExtensionKeysLength, localeData);

    // Set the [[locale]] internal property of collator to the value of r.[[locale]].
    String* localeInternalString = String::fromASCII("locale");
    collator->internalSlot()->set(state, ObjectPropertyName(state, localeInternalString), r->at(localeInternalString), collator->internalSlot());

    // Let i be 0.
    size_t i = 0;
    // Let len be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument "length".
    size_t len = intlCollatorRelevantExtensionKeysLength;
    // Repeat while i < len:
    while (i < len) {
        // Let key be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument ToString(i).
        const char* key = intlCollatorRelevantExtensionKeys[i];
        // If key is "co", then
        const char* property;
        Value value;
        if (strcmp(key, "co") == 0) {
            // Let property be "collation".
            property = "collation";
            // Let value be the value of r.[[co]].
            auto iter = r->find(String::fromASCII("co"));
            // If value is null, then let value be "default".
            if (r->end() == iter) {
                value = String::fromASCII("default");
            } else {
                value = iter->second;
            }
        } else if (strcmp(key, "kn") == 0) {
            // Table 1 – Collator options settable through both extension keys and options properties
            // Key Property    Type            Values
            // kn  numeric    "boolean"
            // kf  caseFirst  "string"   "upper", "lower", "false"

            // Else use the row of Table 1 that contains the value of key in the Key column:
            // Let property be the name given in the Property column of the row.
            property = "numeric";
            // Let value be the value of r.[[<key>]].
            value = r->at(String::fromASCII("kn"));
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
            value = Value(value.equalsTo(state, String::fromASCII("true")));

        } else if (strcmp(key, "kf") == 0) {
            // Table 1 – Collator options settable through both extension keys and options properties
            // Key Property    Type            Values
            // kn  numeric    "boolean"
            // kf  caseFirst  "string"   "upper", "lower", "false"

            // Else use the row of Table 1 that contains the value of key in the Key column:
            // Let property be the name given in the Property column of the row.
            property = "caseFirst";
            // Let value be the value of r.[[<key>]].
            value = r->at(String::fromASCII("string"));
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
        // Set the [[<property>]] internal property of collator to value.
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII(property)), value, collator->internalSlot());
        // Increase i by 1.
        i++;
    }
    // Let s be the result of calling the GetOption abstract operation with arguments
    // options, "sensitivity", "string", a List containing the four String values "base", "accent", "case", and "variant", and undefined.
    tempV.clear();
    tempV.pushBack(String::fromASCII("base"));
    tempV.pushBack(String::fromASCII("accent"));
    tempV.pushBack(String::fromASCII("case"));
    tempV.pushBack(String::fromASCII("variant"));
    Value s = getOption(state, options.asObject(), Value(String::fromASCII("sensitivity")), String::fromASCII("string"), tempV, Value());
    String* sensitivityString = s.toString(state);
    // If s is undefined, then
    // If u is "sort", then let s be "variant".
    // Else
    // Let dataLocale be the value of r.[[dataLocale]].
    // Let dataLocaleData be the result of calling the [[Get]] internal operation of localeData with argument dataLocale.
    // Let s be the result of calling the [[Get]] internal operation of dataLocaleData with argument "sensitivity".
    if (sensitivityString->equals("base"))
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("base"), collator->internalSlot());
    else if (sensitivityString->equals("accent"))
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("accent"), collator->internalSlot());
    else if (sensitivityString->equals("case"))
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("case"), collator->internalSlot());
    else
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("variant"), collator->internalSlot());


    // Let ip be the result of calling the GetOption abstract operation with arguments options, "ignorePunctuation", "boolean", undefined, and false.
    Value ip = getOption(state, options.asObject(), String::fromASCII("ignorePunctuation"), String::fromASCII("boolean"), ValueVector(), Value(false));
    // Set the [[ignorePunctuation]] internal property of collator to ip.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation")), ObjectPropertyDescriptor(ip));
    // Set the [[boundCompare]] internal property of collator to undefined.
    // Set the [[initializedCollator]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")), ObjectPropertyDescriptor(Value(true)));
    // Return collator.

    {
        Object* internalSlot = collator->internalSlot();
        CollatorResolvedOptions opt = collatorResolvedOptions(state, internalSlot);
        UErrorCode status = U_ZERO_ERROR;
        String* locale = opt.locale;
        UCollator* collator = ucol_open(locale->toUTF8StringData().data(), &status);
        if (U_FAILURE(status)) {
            return;
        }

        UColAttributeValue strength = UCOL_PRIMARY;
        UColAttributeValue caseLevel = UCOL_OFF;
        String* sensitivity = opt.sensitivity;
        if (sensitivity->equals("base")) {
        } else if (sensitivity->equals("accent")) {
            strength = UCOL_SECONDARY;
        } else if (sensitivity->equals("case")) {
            caseLevel = UCOL_ON;
        } else if (sensitivity->equals("variant")) {
            strength = UCOL_TERTIARY;
        } else {
            ASSERT_NOT_REACHED();
        }

        ucol_setAttribute(collator, UCOL_STRENGTH, strength, &status);
        ucol_setAttribute(collator, UCOL_CASE_LEVEL, caseLevel, &status);

        bool numeric = opt.numeric;
        ucol_setAttribute(collator, UCOL_NUMERIC_COLLATION, numeric ? UCOL_ON : UCOL_OFF, &status);

        // FIXME: Setting UCOL_ALTERNATE_HANDLING to UCOL_SHIFTED causes punctuation and whitespace to be
        // ignored. There is currently no way to ignore only punctuation.
        bool ignorePunctuation = opt.ignorePunctuation;
        ucol_setAttribute(collator, UCOL_ALTERNATE_HANDLING, ignorePunctuation ? UCOL_SHIFTED : UCOL_DEFAULT, &status);

        // "The method is required to return 0 when comparing Strings that are considered canonically
        // equivalent by the Unicode standard."
        ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
        if (U_FAILURE(status)) {
            ucol_close(collator);
            return;
        }

        internalSlot->setExtraData(collator);

        GC_REGISTER_FINALIZER_NO_ORDER(internalSlot, [](void* obj,
                                                        void*) {
            Object* self = (Object*)obj;
            ucol_close((UCollator*)self->extraData());
        },
                                       nullptr, nullptr, nullptr);
    }
}

static Value builtinIntlCollatorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression) {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.3.1
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        initializeCollator(state, thisValue.asObject(), locales, options);
        return thisValue;
    } else {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.2
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.Collator(locales, options),
        // where Intl.Collator is the standard built-in constructor defined in 10.1.3.
        if (thisValue.isUndefined() || (thisValue.isObject() && thisValue.asObject() == state.context()->globalObject()->intl())) {
            Value callArgv[] = { locales, options };
            return builtinIntlCollatorConstructor(state, new Object(state), 2, callArgv, true);
        } else {
            // Let obj be the result of calling ToObject passing the this value as the argument.
            Object* obj = thisValue.toObject(state);
            // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
            if (!obj->isExtensible()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.Collator function must be extensible");
            }
            initializeCollator(state, obj, locales, options);
            return obj;
        }
    }
    return Value();
}

static Value builtinIntlCollatorCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    FunctionObject* callee = state.executionContext()->resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = callee->internalSlot();

    String* a = argv[0].toString(state);
    String* b = argv[1].toString(state);

    void* collator = nullptr;
    collator = internalSlot->extraData();
    UCollator* ucol = (UCollator*)collator;

    auto utf16A = a->toUTF16StringData();
    auto utf16B = b->toUTF16StringData();

    UCharIterator iterA;
    UCharIterator iterB;

    uiter_setString(&iterA, (const UChar*)utf16A.data(), utf16A.length());
    uiter_setString(&iterB, (const UChar*)utf16B.data(), utf16B.length());

    UErrorCode status = U_ZERO_ERROR;
    auto result = ucol_strcollIter(ucol, &iterA, &iterB, &status);
    // TODO check icu error
    return Value(result);
}

static Value builtinIntlCollatorCompareGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* compareFunctionString = String::fromASCII("compareFunction");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, compareFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new FunctionObject(state, NativeFunctionInfo(strings->compare, builtinIntlCollatorCompare, 2, nullptr, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, compareFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static Value builtinIntlCollatorResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    auto r = collatorResolvedOptions(state, internalSlot);
    Object* result = new Object(state);
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("caseFirst")), ObjectPropertyDescriptor(Value(r.caseFirst), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("collation")), ObjectPropertyDescriptor(r.collation, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation")), ObjectPropertyDescriptor(Value(r.ignorePunctuation), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("locale")), ObjectPropertyDescriptor(r.locale, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("numeric")), ObjectPropertyDescriptor(Value(r.numeric), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), ObjectPropertyDescriptor(r.sensitivity, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("usage")), ObjectPropertyDescriptor(r.usage, ObjectPropertyDescriptor::AllPresent));
    return result;
}

static Value builtinIntlCollatorSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.Collator.
    const auto& availableLocales = state.context()->globalObject()->intlCollatorAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return supportedLocales(state, availableLocales, requestedLocales, options);
}

void toDateTimeOptionsTest(ExecutionState& state, Value options, const char* ch, bool& needDefaults)
{
    if (!needDefaults) {
        return;
    }
    Value r = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII(ch))).value(state, options.asObject());
    if (!r.isUndefined()) {
        needDefaults = false;
    }
}

static Value toDateTimeOptions(ExecutionState& state, Value options, Value required, Value defaults)
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
    options = state.context()->globalObject()->objectCreate()->call(state, Value(), 1, &options);

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
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("year")), ObjectPropertyDescriptor(
                                                                                                               v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("month")), ObjectPropertyDescriptor(
                                                                                                                v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("day")), ObjectPropertyDescriptor(
                                                                                                              v, ObjectPropertyDescriptor::AllPresent));
    }
    // If needDefaults is true and defaults is either "time" or "all", then
    if (needDefaults && (defaults.equalsTo(state, String::fromASCII("time")) || defaults.equalsTo(state, String::fromASCII("all")))) {
        // For each of the property names "hour", "minute", "second":
        // Call the [[DefineOwnProperty]] internal method of options with the property name, Property Descriptor {[[Value]]: "numeric", [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        String* v = String::fromASCII("numeric");
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("hour")), ObjectPropertyDescriptor(
                                                                                                               v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("minute")), ObjectPropertyDescriptor(
                                                                                                                 v, ObjectPropertyDescriptor::AllPresent));
        options.asObject()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("second")), ObjectPropertyDescriptor(
                                                                                                                 v, ObjectPropertyDescriptor::AllPresent));
    }

    return options;
}

static const char* const intlDateTimeFormatRelevantExtensionKeys[2] = { "ca", "nu" };
static size_t intlDateTimeFormatRelevantExtensionKeysLength = 2;
static const size_t indexOfExtensionKeyCa = 0;
static const size_t indexOfExtensionKeyNu = 1;

static std::vector<std::string> numberingSystemsForLocale(String* locale)
{
    static std::vector<std::string> availableNumberingSystems;

    UErrorCode status = U_ZERO_ERROR;
    if (availableNumberingSystems.size() == 0) {
        UEnumeration* numberingSystemNames = unumsys_openAvailableNames(&status);
        ASSERT(U_SUCCESS(status));

        int32_t resultLength;
        // Numbering system names are always ASCII, so use char[].
        while (const char* result = uenum_next(numberingSystemNames, &resultLength, &status)) {
            ASSERT(U_SUCCESS(status));
            availableNumberingSystems.push_back(std::string(result, resultLength));
        }
        uenum_close(numberingSystemNames);
    }

    status = U_ZERO_ERROR;
    UNumberingSystem* defaultSystem = unumsys_open(locale->toUTF8StringData().data(), &status);
    ASSERT(U_SUCCESS(status));
    std::string defaultSystemName(unumsys_getName(defaultSystem));
    unumsys_close(defaultSystem);

    std::vector<std::string> numberingSystems;
    numberingSystems.push_back(defaultSystemName);
    numberingSystems.insert(numberingSystems.end(), availableNumberingSystems.begin(), availableNumberingSystems.end());
    return numberingSystems;
}

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
            keyLocaleData.push_back(calendar);
            // Ensure aliases used in language tag are allowed.
            if (calendar == std::string("gregorian"))
                keyLocaleData.push_back(std::string("gregory"));
            else if (calendar == std::string("islamic-civil"))
                keyLocaleData.push_back(std::string("islamicc"));
            else if (calendar == std::string("ethiopic-amete-alem"))
                keyLocaleData.push_back(std::string("ethioaa"));
        }
        uenum_close(calendars);
        break;
    }
    case indexOfExtensionKeyNu:
        keyLocaleData = numberingSystemsForLocale(locale);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

bool equalIgnoringASCIICase(String* timeZoneName, const UTF16StringDataNonGCStd& ianaTimeZoneView)
{
    if (timeZoneName->length() != ianaTimeZoneView.size()) {
        return false;
    }
    for (size_t i = 0; i < ianaTimeZoneView.size(); i++) {
        if (::tolower(ianaTimeZoneView[i]) != ::tolower(timeZoneName->charAt(i))) {
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

static void initializeDateTimeFormat(ExecutionState& state, Object* dateTimeFormat, Value locales, Value options)
{
    // If dateTimeFormat has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (dateTimeFormat->hasInternalSlot() && dateTimeFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, PropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of dateTimeFormat to true.
    dateTimeFormat->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);

    // Let options be the result of calling the ToDateTimeOptions abstract operation (defined below) with arguments options, "any", and "date".
    options = toDateTimeOptions(state, options, String::fromASCII("any"), String::fromASCII("date"));

    // Let opt be a new Record.
    StringMap* opt = new StringMap;
    // Let matcher be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit".
    ValueVector tempV;
    tempV.pushBack(String::fromASCII("lookup"));
    tempV.pushBack(String::fromASCII("best fit"));
    Value matcher = getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), tempV, String::fromASCII("best fit"));

    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Let DateTimeFormat be the standard built-in object that is the initial value of Intl.DateTimeFormat.
    // Let localeData be the value of the [[localeData]] internal property of DateTimeFormat.
    const auto& availableLocales = state.context()->globalObject()->intlDateTimeFormatAvailableLocales();

    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the
    // [[availableLocales]] internal property of DateTimeFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of DateTimeFormat, and localeData.
    StringMap* r = resolveLocale(state, availableLocales, requestedLocales, opt, intlDateTimeFormatRelevantExtensionKeys, intlDateTimeFormatRelevantExtensionKeysLength, localeDataDateTimeFormat);

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
        tz = String::fromASCII("UTC");
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
        Value value = getOption(state, options.asObject(), prop, String::fromASCII("string"), values, Value());
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
    Value hr12Value = getOption(state, options.asObject(), String::fromASCII("hour12"), String::fromASCII("boolean"), ValueVector(), Value());
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
    matcher = getOption(state, options.asObject(), String::fromASCII("formatMatcher"), String::fromASCII("string"), tempV, String::fromASCII("best fit"));

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
    UDateFormat* icuDateFormat = udat_open(UDAT_IGNORE, UDAT_IGNORE, localeStringView.data(), (UChar*)timeZoneView.data(), timeZoneView.length(), (UChar*)patternBuffer.data(), patternBuffer.length(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DateTimeFormat");
        return;
    }

    dateTimeFormat->internalSlot()->setExtraData(icuDateFormat);

    GC_REGISTER_FINALIZER_NO_ORDER(dateTimeFormat->internalSlot(), [](void* obj,
                                                                      void*) {
        Object* self = (Object*)obj;
        udat_close((UDateFormat*)self->extraData());
    },
                                   nullptr, nullptr, nullptr);

    // Set dateTimeFormat.[[boundFormat]] to undefined.
    // Set dateTimeFormat.[[initializedDateTimeFormat]] to true.
    dateTimeFormat->internalSlot()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")), ObjectPropertyDescriptor(Value(true)));

    // Return dateTimeFormat.
    return;
}

static Value builtinIntlDateTimeFormatConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!isNewExpression) {
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        Value locales, options;
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.DateTimeFormat(locales, options), where Intl.DateTimeFormat is the standard built-in constructor defined in 12.1.3.
        if (thisValue.isUndefined() || thisValue.equalsTo(state, state.context()->globalObject()->intl())) {
            thisValue = new Object(state);
        }
        // Let obj be the result of calling ToObject passing the this value as the argument.
        Object* obj = thisValue.toObject(state);
        // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
        if (!obj->isExtensible()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.DateTimeFormat function must be extensible");
        }
        // Call the InitializeDateTimeFormat abstract operation (defined in 12.1.1.1) with arguments obj, locales, and options.
        initializeDateTimeFormat(state, obj, locales, options);
        // Return obj.
        return obj;
    } else {
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        Value locales, options;
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        initializeDateTimeFormat(state, thisValue.asObject(), locales, options);
        return thisValue.asObject();
    }
}

static Value builtinIntlDateTimeFormatFormat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    FunctionObject* callee = state.executionContext()->resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = callee->internalSlot();
    UDateFormat* udat = (UDateFormat*)internalSlot->extraData();

    double value = argv[0].toNumber(state);
    // 1. If x is not a finite Number, then throw a RangeError exception.
    if (!std::isfinite(value)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "date value is not finite in DateTimeFormat format()");
    }

    // Delegate remaining steps to ICU.
    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd result;
    result.resize(32);
    auto resultLength = udat_format(udat, value, (UChar*)result.data(), result.size(), nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        result.resize(resultLength);
        udat_format(udat, value, (UChar*)result.data(), resultLength, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format date value");
    }

    return Value(new UTF16String(result.data(), result.length()));
}

static Value builtinIntlDateTimeFormatFormatGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* formatFunctionString = String::fromASCII("format");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, formatFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new FunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlDateTimeFormatFormat, 1, nullptr, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, formatFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static void setFormatOpt(ExecutionState& state, Object* internalSlot, Object* result, const char* pName)
{
    String* prop = String::fromASCII(pName);

    ObjectGetResult r;
    r = internalSlot->get(state, ObjectPropertyName(state, prop));

    if (r.hasValue()) {
        result->defineOwnProperty(state, ObjectPropertyName(state, prop), ObjectPropertyDescriptor(Value(r.value(state, internalSlot)), ObjectPropertyDescriptor::AllPresent));
    }
}

static Value builtinIntlDateTimeFormatResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }
    Object* internalSlot = thisValue.asObject()->internalSlot();

    Object* result = new Object(state);

    setFormatOpt(state, internalSlot, result, "locale");
    setFormatOpt(state, internalSlot, result, "calendar");
    setFormatOpt(state, internalSlot, result, "numberingSystem");
    setFormatOpt(state, internalSlot, result, "timeZone");
    setFormatOpt(state, internalSlot, result, "hour12");
    setFormatOpt(state, internalSlot, result, "era");
    setFormatOpt(state, internalSlot, result, "year");
    setFormatOpt(state, internalSlot, result, "month");
    setFormatOpt(state, internalSlot, result, "weekday");
    setFormatOpt(state, internalSlot, result, "day");
    setFormatOpt(state, internalSlot, result, "hour");
    setFormatOpt(state, internalSlot, result, "minute");
    setFormatOpt(state, internalSlot, result, "second");
    setFormatOpt(state, internalSlot, result, "timeZoneName");

    return Value(result);
}

static Value builtinIntlDateTimeFormatSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.DateTimeFormat.
    const auto& availableLocales = state.context()->globalObject()->intlDateTimeFormatAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return supportedLocales(state, availableLocales, requestedLocales, options);
}

static const char* const intlNumberFormatRelevantExtensionKeys[1] = { "nu" };
static const int intlNumberFormatRelevantExtensionKeysLength = 1;

static std::vector<std::string> localeDataNumberFormat(String* locale, size_t keyIndex)
{
    return numberingSystemsForLocale(locale);
}

static size_t currencyDigits(const char* currency)
{
    // 11.1.1 The abstract operation CurrencyDigits (currency)
    // "If the ISO 4217 currency and funds code list contains currency as an alphabetic code,
    // then return the minor unit value corresponding to the currency from the list; else return 2.
    std::pair<const char*, unsigned> currencyMinorUnits[] = {
        { "BHD", 3 },
        { "BIF", 0 },
        { "BYR", 0 },
        { "CLF", 4 },
        { "CLP", 0 },
        { "DJF", 0 },
        { "GNF", 0 },
        { "IQD", 3 },
        { "ISK", 0 },
        { "JOD", 3 },
        { "JPY", 0 },
        { "KMF", 0 },
        { "KRW", 0 },
        { "KWD", 3 },
        { "LYD", 3 },
        { "OMR", 3 },
        { "PYG", 0 },
        { "RWF", 0 },
        { "TND", 3 },
        { "UGX", 0 },
        { "UYI", 0 },
        { "VND", 0 },
        { "VUV", 0 },
        { "XAF", 0 },
        { "XOF", 0 },
        { "XPF", 0 }
    };

    size_t len = sizeof(currencyMinorUnits) / sizeof(std::pair<const char*, unsigned>);
    for (size_t i = 0; i < len; i++) {
        if (strcmp(currencyMinorUnits[i].first, currency) == 0) {
            return currencyMinorUnits[i].second;
        }
    }
    return 2;
}

// http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.10
static double getNumberOption(ExecutionState& state, Object* options, String* property, double minimum, double maximum, double fallback)
{
    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Let value be ToNumber(value).
        double doubleValue = value.toNumber(state);
        // If value is NaN or less than minimum or greater than maximum, throw a RangeError exception.
        if (std::isnan(doubleValue) || doubleValue < minimum || maximum > doubleValue) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Got invalid number option value");
        }
        // Return floor(value).
        return floor(doubleValue);
    } else {
        // Else return fallback.
        return fallback;
    }
}

static void initializeNumberFormat(ExecutionState& state, Object* numberFormat, Value locales, Value options)
{
    // If dateTimeFormat has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (numberFormat->hasInternalSlot() && numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, PropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of dateTimeFormat to true.
    numberFormat->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);

    // If options is undefined, then
    if (options.isUndefined()) {
        // Let options be the result of creating a new object as if by the expression new Object() where Object is the standard built-in constructor with that name.
        options = new Object(state);
    } else {
        // Else
        // Let options be ToObject(options).
        options = options.toObject(state);
    }

    // Let opt be a new Record.
    StringMap* opt = new StringMap();
    // Let matcher be the result of calling the GetOption abstract operation (defined in 9.2.9) with the arguments options, "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit".
    // Set opt.[[localeMatcher]] to matcher.
    ValueVector values;
    values.pushBack(String::fromASCII("lookup"));
    values.pushBack(String::fromASCII("best fit"));
    Value matcher = getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), values, String::fromASCII("best fit"));
    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Let NumberFormat be the standard built-in object that is the initial value of Intl.NumberFormat.
    // Let localeData be the value of the [[localeData]] internal property of NumberFormat.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of NumberFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of NumberFormat, and localeData.
    StringMap* r = resolveLocale(state, state.context()->globalObject()->intlNumberFormatAvailableLocales(), requestedLocales, opt, intlNumberFormatRelevantExtensionKeys, intlNumberFormatRelevantExtensionKeysLength, localeDataNumberFormat);

    // Set the [[locale]] internal property of numberFormat to the value of r.[[locale]].
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("locale")), r->at(String::fromASCII("locale")), numberFormat->internalSlot());
    // Set the [[numberingSystem]] internal property of numberFormat to the value of r.[[nu]].
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("numberingSystem")), r->at(String::fromASCII("nu")), numberFormat->internalSlot());

    // Let dataLocale be the value of r.[[dataLocale]].
    // Let s be the result of calling the GetOption abstract operation with the arguments options, "style", "string",
    // a List containing the three String values "decimal", "percent", and "currency", and "decimal".
    values.clear();
    values.pushBack(String::fromASCII("decimal"));
    values.pushBack(String::fromASCII("percent"));
    values.pushBack(String::fromASCII("currency"));
    Value s = getOption(state, options.asObject(), String::fromASCII("style"), String::fromASCII("string"), values, String::fromASCII("decimal"));

    // Set the [[style]] internal property of numberFormat to s.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("style")), s, numberFormat->internalSlot());

    // Let c be the result of calling the GetOption abstract operation with the arguments options, "currency", "string", undefined, and undefined.
    Value c = getOption(state, options.asObject(), String::fromASCII("currency"), String::fromASCII("string"), ValueVector(), Value());
    // If c is not undefined and the result of calling the IsWellFormedCurrencyCode abstract operation (defined in 6.3.1) with argument c is false, then throw a RangeError exception.
    if (!c.isUndefined()) {
        String* currency = c.toString(state);
        UTF8StringDataNonGCStd cc = currency->toUTF8StringData().data();
        if (currency->length() != 3 || !isAllSpecialCharacters(cc, isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "currency is not a well-formed currency code");
        }
    }

    // If s is "currency" and c is undefined, throw a TypeError exception.
    size_t cDigits = 2;
    if (s.equalsTo(state, String::fromASCII("currency"))) {
        if (c.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "currency must be a string");
        }

        // If s is "currency", then
        // Let c be the result of converting c to upper case as specified in 6.1.
        String* cString = c.toString(state);
        std::string str = cString->toUTF8StringData().data();
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        // Set the [[currency]] internal property of numberFormat to c.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("currency")), String::fromASCII(str.data()), numberFormat->internalSlot());
        // Let cDigits be the result of calling the CurrencyDigits abstract operation (defined below) with argument c.
        cDigits = currencyDigits(str.data());
    }

    // Let cd be the result of calling the GetOption abstract operation with the arguments
    // options, "currencyDisplay", "string", a List containing the three String values "code", "symbol", and "name", and "symbol".
    values.clear();
    values.pushBack(String::fromASCII("code"));
    values.pushBack(String::fromASCII("symbol"));
    values.pushBack(String::fromASCII("name"));
    Value cd = getOption(state, options.asObject(), String::fromASCII("currencyDisplay"), String::fromASCII("string"), values, String::fromASCII("symbol"));
    // If s is "currency", then set the [[currencyDisplay]] internal property of numberFormat to cd.
    if (s.equalsTo(state, String::fromASCII("currency"))) {
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("currencyDisplay")), cd, numberFormat->internalSlot());
    }

    // Let mnid be the result of calling the GetNumberOption abstract operation (defined in 9.2.10) with arguments options, "minimumIntegerDigits", 1, 21, and 1.
    double mnid = getNumberOption(state, options.asObject(), String::fromASCII("minimumIntegerDigits"), 1, 21, 1);
    // Set the [[minimumIntegerDigits]] internal property of numberFormat to mnid.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minimumIntegerDigits")), Value(mnid), numberFormat->internalSlot());

    // If s is "currency", then let mnfdDefault be cDigits; else let mnfdDefault be 0.
    double mnfdDefault = 0;
    if (s.equalsTo(state, String::fromASCII("currency"))) {
        mnfdDefault = cDigits;
    }
    // Let mnfd be the result of calling the GetNumberOption abstract operation with arguments options, "minimumFractionDigits", 0, 20, and mnfdDefault.
    double mnfd = getNumberOption(state, options.asObject(), String::fromASCII("minimumFractionDigits"), 0, 20, mnfdDefault);

    // Set the [[minimumFractionDigits]] internal property of numberFormat to mnfd.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minimumFractionDigits")), Value(mnfd), numberFormat->internalSlot());

    // If s is "currency", then let mxfdDefault be max(mnfd, cDigits); else if s is "percent", then let mxfdDefault be max(mnfd, 0);
    // else let mxfdDefault be max(mnfd, 3).
    double mxfdDefault;
    if (s.equalsTo(state, String::fromASCII("currency"))) {
        mxfdDefault = std::max(mnfd, (double)cDigits);
    } else if (s.equalsTo(state, String::fromASCII("percent"))) {
        mxfdDefault = std::max(mnfd, 0.0);
    } else {
        mxfdDefault = std::max(mnfd, 3.0);
    }

    // Let mxfd be the result of calling the GetNumberOption abstract operation with arguments options, "maximumFractionDigits", mnfd, 20, and mxfdDefault.
    double mxfd = getNumberOption(state, options.asObject(), String::fromASCII("maximumFractionDigits"), mnfd, 20, mxfdDefault);

    // Set the [[maximumFractionDigits]] internal property of numberFormat to mxfd.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("maximumFractionDigits")), Value(mxfd), numberFormat->internalSlot());

    // Let mnsd be the result of calling the [[Get]] internal method of options with argument "minimumSignificantDigits".
    Value mnsd = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII("minimumSignificantDigits"))).value(state, options.asObject());
    // Let mxsd be the result of calling the [[Get]] internal method of options with argument "maximumSignificantDigits".
    Value mxsd = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII("maximumSignificantDigits"))).value(state, options.asObject());

    // If mnsd is not undefined or mxsd is not undefined, then:
    if (!mnsd.isUndefined() || !mxsd.isUndefined()) {
        // Let mnsd be the result of calling the GetNumberOption abstract operation with arguments options, "minimumSignificantDigits", 1, 21, and 1.
        mnsd = Value(getNumberOption(state, options.asObject(), String::fromASCII("minimumSignificantDigits"), 1, 21, 1));
        // Let mxsd be the result of calling the GetNumberOption abstract operation with arguments options, "maximumSignificantDigits", mnsd, 21, and 21.
        mxsd = Value(getNumberOption(state, options.asObject(), String::fromASCII("maximumSignificantDigits"), mnsd, 21, 21));
        // Set the [[minimumSignificantDigits]] internal property of numberFormat to mnsd,
        // and the [[maximumSignificantDigits]] internal property of numberFormat to mxsd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minimumSignificantDigits")), mnsd, numberFormat->internalSlot());
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("maximumSignificantDigits")), mxsd, numberFormat->internalSlot());
    }

    // Let g be the result of calling the GetOption abstract operation with the arguments options, "useGrouping", "boolean", undefined, and true.
    Value g = getOption(state, options.asObject(), String::fromASCII("useGrouping"), String::fromASCII("boolean"), ValueVector(), Value(true));
    // Set the [[useGrouping]] internal property of numberFormat to g.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("useGrouping")), g, numberFormat->internalSlot());

    // Let dataLocaleData be the result of calling the [[Get]] internal method of localeData with argument dataLocale.
    // Let dataLocaleData be the result of calling the [[Get]] internal method of localeData with argument dataLocale.
    // Let patterns be the result of calling the [[Get]] internal method of dataLocaleData with argument "patterns".
    // Assert: patterns is an object (see 11.2.3).
    // Let stylePatterns be the result of calling the [[Get]] internal method of patterns with argument s.
    // Set the [[positivePattern]] internal property of numberFormat to the result of calling the [[Get]] internal method of stylePatterns with the argument "positivePattern".
    // Set the [[negativePattern]] internal property of numberFormat to the result of calling the [[Get]] internal method of stylePatterns with the argument "negativePattern".
    // Set the [[boundFormat]] internal property of numberFormat to undefined.
    // Set the [[initializedNumberFormat]] internal property of numberFormat to true.
    numberFormat->internalSlot()->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")), ObjectPropertyDescriptor(Value(true)));
    // Return numberFormat


    UNumberFormatStyle style;

    String* styleOption = numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("style"))).value(state, numberFormat->internalSlot()).toString(state);

    if (styleOption->equals("decimal")) {
        style = UNUM_DECIMAL;
    } else if (styleOption->equals("percent")) {
        style = UNUM_PERCENT;
    } else {
        ASSERT(styleOption->equals("currency"));

        String* currencyDisplayOption = numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("currencyDisplay"))).value(state, numberFormat->internalSlot()).toString(state);
        if (currencyDisplayOption->equals("code")) {
            style = UNUM_CURRENCY_ISO;
        } else if (currencyDisplayOption->equals("symbol")) {
            style = UNUM_CURRENCY;
        } else if (currencyDisplayOption->equals("name")) {
            ASSERT(currencyDisplayOption->equals("name"));
            style = UNUM_CURRENCY_PLURAL;
        }
    }

    UErrorCode status = U_ZERO_ERROR;
    String* localeOption = numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("locale"))).value(state, numberFormat->internalSlot()).toString(state);
    UNumberFormat* unumberFormat = unum_open(style, nullptr, 0, localeOption->toUTF8StringData().data(), nullptr, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to init NumberFormat");
    }

    if (styleOption->equals("currency")) {
        String* currencyString = numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("currency"))).value(state, numberFormat->internalSlot()).toString(state);
        unum_setTextAttribute(unumberFormat, UNUM_CURRENCY_CODE, (UChar*)currencyString->toUTF16StringData().data(), 3, &status);
    }

    if (!numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("minimumSignificantDigits")))) {
        unum_setAttribute(unumberFormat, UNUM_MIN_INTEGER_DIGITS, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("minimumIntegerDigits"))).value(state, numberFormat->internalSlot()).toNumber(state));
        unum_setAttribute(unumberFormat, UNUM_MIN_FRACTION_DIGITS, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("minimumFractionDigits"))).value(state, numberFormat->internalSlot()).toNumber(state));
        unum_setAttribute(unumberFormat, UNUM_MAX_FRACTION_DIGITS, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("maximumFractionDigits"))).value(state, numberFormat->internalSlot()).toNumber(state));
    } else {
        unum_setAttribute(unumberFormat, UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(unumberFormat, UNUM_MIN_SIGNIFICANT_DIGITS, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("minimumSignificantDigits"))).value(state, numberFormat->internalSlot()).toNumber(state));
        unum_setAttribute(unumberFormat, UNUM_MAX_SIGNIFICANT_DIGITS, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("maximumSignificantDigits"))).value(state, numberFormat->internalSlot()).toNumber(state));
    }
    unum_setAttribute(unumberFormat, UNUM_GROUPING_USED, numberFormat->internalSlot()->get(state, ObjectPropertyName(state, String::fromASCII("useGrouping"))).value(state, numberFormat->internalSlot()).toBoolean(state));
    unum_setAttribute(unumberFormat, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to init NumberFormat");
    }

    numberFormat->internalSlot()->setExtraData(unumberFormat);

    GC_REGISTER_FINALIZER_NO_ORDER(numberFormat->internalSlot(), [](void* obj,
                                                                    void*) {
        Object* self = (Object*)obj;
        unum_close((UNumberFormat*)self->extraData());
    },
                                   nullptr, nullptr, nullptr);
}

static Value builtinIntlNumberFormatFormat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    FunctionObject* callee = state.executionContext()->resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = callee->internalSlot();
    UNumberFormat* format = (UNumberFormat*)internalSlot->extraData();

    double number = argv[0].toNumber(state);
    // Map negative zero to positive zero.
    if (!number)
        number = 0.0;

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd buffer;
    buffer.resize(32);
    auto length = unum_formatDouble(format, number, (UChar*)buffer.data(), buffer.size(), nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        buffer.resize(length);
        status = U_ZERO_ERROR;
        unum_formatDouble(format, number, (UChar*)buffer.data(), length, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to format a number");
    }

    return new UTF16String(buffer.data(), buffer.length());
}

static Value builtinIntlNumberFormatFormatGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* formatFunctionString = String::fromASCII("format");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, formatFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new FunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlNumberFormatFormat, 1, nullptr, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, formatFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static Value builtinIntlNumberFormatConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression) {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.3.1
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        initializeNumberFormat(state, thisValue.asObject(), locales, options);
        return thisValue;
    } else {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.2
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.Collator(locales, options),
        // where Intl.Collator is the standard built-in constructor defined in 10.1.3.
        if (thisValue.isUndefined() || (thisValue.isObject() && thisValue.asObject() == state.context()->globalObject()->intl())) {
            Value callArgv[] = { locales, options };
            return builtinIntlCollatorConstructor(state, new Object(state), 2, callArgv, true);
        } else {
            // Let obj be the result of calling ToObject passing the this value as the argument.
            Object* obj = thisValue.toObject(state);
            // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
            if (!obj->isExtensible()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.NumberFormat function must be extensible");
            }
            initializeNumberFormat(state, obj, locales, options);
            return obj;
        }
    }
    return Value();
}

static Value builtinIntlNumberFormatResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    Object* result = new Object(state);

    setFormatOpt(state, internalSlot, result, "locale");
    setFormatOpt(state, internalSlot, result, "numberingSystem");
    setFormatOpt(state, internalSlot, result, "style");
    setFormatOpt(state, internalSlot, result, "currency");
    setFormatOpt(state, internalSlot, result, "currencyDisplay");
    setFormatOpt(state, internalSlot, result, "minimumIntegerDigits");
    setFormatOpt(state, internalSlot, result, "minimumFractionDigits");
    setFormatOpt(state, internalSlot, result, "maximumFractionDigits");
    setFormatOpt(state, internalSlot, result, "minimumSignificantDigits");
    setFormatOpt(state, internalSlot, result, "maximumSignificantDigits");
    setFormatOpt(state, internalSlot, result, "useGrouping");
    return result;
}

static Value builtinIntlNumberFormatSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.NumberFormat.
    const auto& availableLocales = state.context()->globalObject()->intlNumberFormatAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return supportedLocales(state, availableLocales, requestedLocales, options);
}

void GlobalObject::installIntl(ExecutionState& state)
{
    m_intl = new Object(state);
    m_intl->markThisObjectDontNeedStructureTransitionTable(state);

    const StaticStrings* strings = &state.context()->staticStrings();
    defineOwnProperty(state, ObjectPropertyName(strings->Intl),
                      ObjectPropertyDescriptor(m_intl, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlCollator = new FunctionObject(state, NativeFunctionInfo(strings->Collator, builtinIntlCollatorConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                            return new Object(state);
                                        }),
                                        FunctionObject::__ForBuiltin__);

    FunctionObject* compareFunction = new FunctionObject(state, NativeFunctionInfo(strings->compare, builtinIntlCollatorCompareGetter, 0, nullptr, NativeFunctionInfo::Strict));
    m_intlCollator->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().compare,
                                                                              ObjectPropertyDescriptor(JSGetterSetter(compareFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlCollator->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlCollatorResolvedOptions, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlCollator->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                      ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlCollatorSupportedLocalesOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlDateTimeFormat = new FunctionObject(state, NativeFunctionInfo(strings->DateTimeFormat, builtinIntlDateTimeFormatConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                                  return new Object(state);
                                              }),
                                              FunctionObject::__ForBuiltin__);

    FunctionObject* formatFunction = new FunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlDateTimeFormatFormatGetter, 0, nullptr, NativeFunctionInfo::Strict));
    m_intlDateTimeFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().format,
                                                                                    ObjectPropertyDescriptor(JSGetterSetter(formatFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlDateTimeFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                                    ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlDateTimeFormatResolvedOptions, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlDateTimeFormat->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                            ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlDateTimeFormatSupportedLocalesOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlNumberFormat = new FunctionObject(state, NativeFunctionInfo(strings->NumberFormat, builtinIntlNumberFormatConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                                return new Object(state);
                                            }),
                                            FunctionObject::__ForBuiltin__);

    formatFunction = new FunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlNumberFormatFormatGetter, 0, nullptr, NativeFunctionInfo::Strict));
    m_intlNumberFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().format,
                                                                                  ObjectPropertyDescriptor(JSGetterSetter(formatFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlNumberFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                                  ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlNumberFormatResolvedOptions, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlNumberFormat->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlNumberFormatSupportedLocalesOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));


    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->Collator),
                              ObjectPropertyDescriptor(m_intlCollator, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->DateTimeFormat),
                              ObjectPropertyDescriptor(m_intlDateTimeFormat, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->NumberFormat),
                              ObjectPropertyDescriptor(m_intlNumberFormat, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}

#endif // ENABLE_ICU
