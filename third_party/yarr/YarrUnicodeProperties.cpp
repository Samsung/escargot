/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WTFBridge.h"
#include "YarrUnicodeProperties.h"

#include "Yarr.h"
#include "YarrPattern.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Yarr {

#if defined(ENABLE_ICU)
#include "UnicodePatternTables.h"
#endif

// ---------------------------------------------------------------------------
// Runtime property cache – replaces build-time hash tables and pattern arrays.
//
// When unicodeMatchProperty() / unicodeMatchPropertyValue() validate a property
// name via ICU APIs, they store the result here.  The index returned to the
// caller is the offset into g_propertyCache.  Later,
// buildUnicodeCharacterClassFromICU() uses the cached entry to generate a
// [\p{...}] pattern string at runtime and feed it to uset_openPattern().
// ---------------------------------------------------------------------------

enum class PropertyKind : uint8_t {
    Binary,            // [\p{name}]
    GeneralCategory,   // [\p{name}]
    Script,            // [\p{Script=name}]
    ScriptExtension,   // [\p{Script_Extensions=name}]
    Sequence,          // [\p{name}]  – may contain strings
    Special,           // [\p{name}]  – validated via uset_openPattern fallback
};

struct UnicodePropertyCacheEntry {
    std::string name;
    PropertyKind kind;
    bool mayContainStrings;
};

static std::vector<UnicodePropertyCacheEntry>& propertyCache()
{
    static std::vector<UnicodePropertyCacheEntry> cache;
    return cache;
}

static std::unordered_map<std::string, unsigned>& propertyNameToIndex()
{
    static std::unordered_map<std::string, unsigned> map;
    return map;
}

// Convert WTF::String to std::string (property names are always ASCII).
static std::string wtfStringToStd(const WTF::String& s)
{
    if (s.isNull())
        return "";
    if (s.is8Bit()) {
        return std::string(reinterpret_cast<const char*>(s.characters8()), s.length());
    }
    std::string result;
    for (size_t i = 0; i < s.length(); i++) {
        char16_t c = s[i];
        if (c < 0x80)
            result += static_cast<char>(c);
        else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

// Build a [\p{...}] pattern string from a cache entry.
static std::string buildPatternString(const UnicodePropertyCacheEntry& entry)
{
    switch (entry.kind) {
    case PropertyKind::Binary:
    case PropertyKind::GeneralCategory:
    case PropertyKind::Sequence:
    case PropertyKind::Special:
        return "[\\p{" + entry.name + "}]";
    case PropertyKind::Script:
        return "[\\p{Script=" + entry.name + "}]";
    case PropertyKind::ScriptExtension:
        return "[\\p{Script_Extensions=" + entry.name + "}]";
    }
    return "";
}

#if defined(ENABLE_ICU)
// A property (value) may have more names than the short and the long one; the
// additional Unicode aliases are exposed through nameChoice values above
// U_LONG_PROPERTY_NAME (e.g. "digit" for General_Category=Nd, "Qaac" for
// Script=Coptic).  Unicode never defines more than a handful, so stop well
// before ICU starts returning NULL forever.
static const int32_t maxPropertyNameChoice = 8;

// Verify that a property name exactly matches one of the canonical ICU names.
// This prevents ICU's loose matching from accepting names like "lowercase"
// when the spec requires "Lowercase".
static bool exactPropertyNameMatch(UProperty prop, const std::string& name)
{
    for (int32_t choice = 0; choice < maxPropertyNameChoice; choice++) {
        const char* candidate = u_getPropertyName(prop, static_cast<UPropertyNameChoice>(choice));
        if (candidate && name == candidate)
            return true;
    }
    return false;
}

// Verify that a property value name exactly matches one of the canonical ICU names.
static bool exactPropertyValueNameMatch(UProperty prop, int32_t value, const std::string& name)
{
    for (int32_t choice = 0; choice < maxPropertyNameChoice; choice++) {
        const char* candidate = u_getPropertyValueName(prop, value, static_cast<UPropertyNameChoice>(choice));
        if (candidate && name == candidate)
            return true;
    }
    return false;
}

// General_Category values include the group aliases (L, Letter, P, Punctuation, ...)
// that u_getPropertyValueEnum does not recognise for UCHAR_GENERAL_CATEGORY.
// UCHAR_GENERAL_CATEGORY_MASK does recognise them, and covers the single
// categories as well, so use it for both.
static bool isGeneralCategoryName(const std::string& name)
{
    int32_t value = u_getPropertyValueEnum(UCHAR_GENERAL_CATEGORY_MASK, name.c_str());
    return value >= 0 && exactPropertyValueNameMatch(UCHAR_GENERAL_CATEGORY_MASK, value, name);
}
#endif

// Get or create a cache entry.  Returns the index, or -1 if validation fails.
static int getOrCreatePropertyIndex(const std::string& cacheKey,
                                    const std::string& name,
                                    PropertyKind kind,
                                    bool mayContainStrings)
{
    auto it = propertyNameToIndex().find(cacheKey);
    if (it != propertyNameToIndex().end())
        return static_cast<int>(it->second);

    unsigned index = propertyCache().size();
    propertyCache().push_back({name, kind, mayContainStrings});
    propertyNameToIndex()[cacheKey] = index;
    return static_cast<int>(index);
}

std::optional<BuiltInCharacterClassID> unicodeMatchPropertyValue(WTF::String unicodePropertyName, WTF::String unicodePropertyValue)
{
#if defined(ENABLE_ICU)
    std::string propName = wtfStringToStd(unicodePropertyName);
    std::string propValue = wtfStringToStd(unicodePropertyValue);

    PropertyKind kind;
    std::string cacheKey;

    if (propName == "Script" || propName == "sc") {
        kind = PropertyKind::Script;
        cacheKey = "sc:" + propValue;
    } else if (propName == "Script_Extensions" || propName == "scx") {
        kind = PropertyKind::ScriptExtension;
        cacheKey = "scx:" + propValue;
    } else if (propName == "General_Category" || propName == "gc") {
        kind = PropertyKind::GeneralCategory;
        cacheKey = "gc:" + propValue;
    } else {
        return std::nullopt;
    }

    // Already cached?
    auto it = propertyNameToIndex().find(cacheKey);
    if (it != propertyNameToIndex().end())
        return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + it->second));

    // Validate with exact name matching to prevent ICU's loose matching from
    // accepting spellings the spec rejects (e.g. "uppercaseletter").
    if (kind == PropertyKind::GeneralCategory) {
        if (!isGeneralCategoryName(propValue))
            return std::nullopt;
    } else {
        int32_t value = u_getPropertyValueEnum(UCHAR_SCRIPT, propValue.c_str());
        if (value < 0 || !exactPropertyValueNameMatch(UCHAR_SCRIPT, value, propValue))
            return std::nullopt;
    }

    int idx = getOrCreatePropertyIndex(cacheKey, propValue, kind, false);
    return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + idx));
#else
    (void)unicodePropertyName;
    (void)unicodePropertyValue;
#endif
    return std::nullopt;
}

std::optional<BuiltInCharacterClassID> unicodeMatchProperty(WTF::String unicodePropertyValue, CompileMode compileMode)
{
#if defined(ENABLE_ICU)
    std::string name = wtfStringToStd(unicodePropertyValue);

    // Already cached?
    auto it = propertyNameToIndex().find(name);
    if (it != propertyNameToIndex().end())
        return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + it->second));

    // 1. Sequence property (UnicodeSets mode only) — must be checked before
    //    binary property, because ICU 74+ registers Basic_Emoji etc. as binary
    //    properties, but ECMAScript treats them as properties of strings
    //    (mayContainStrings = true).
    if (compileMode == CompileMode::UnicodeSets && isSequencePropertyName(unicodePropertyValue)) {
        int idx = getOrCreatePropertyIndex(name, name, PropertyKind::Sequence, true);
        return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + idx));
    }
    // In non-UnicodeSets mode, sequence property names must be rejected.
    if (compileMode != CompileMode::UnicodeSets && isSequencePropertyName(unicodePropertyValue))
        return std::nullopt;

    // 2. Binary property. ICU knows more binary properties than ECMAScript allows
    //    (Case_Sensitive, NFC_Inert, ID_Compat_Math_Start, ...) and gains new ones
    //    with every Unicode release, so the set of *names* has to come from the
    //    spec rather than from ICU. Everything else — alias resolution and the
    //    code point data — is still ICU's job.
    //    To add a property, add its canonical long name from table
    //    "Binary Unicode property aliases and their canonical property names"
    //    (sec-runtime-semantics-unicodematchproperty-p) here.
    static const std::unordered_set<std::string> binaryPropertyNames = {
        "Alphabetic", "ASCII_Hex_Digit", "Bidi_Control", "Bidi_Mirrored", "Cased",
        "Case_Ignorable", "Changes_When_Casefolded", "Changes_When_Casemapped",
        "Changes_When_Lowercased", "Changes_When_NFKC_Casefolded",
        "Changes_When_Titlecased", "Changes_When_Uppercased", "Dash",
        "Default_Ignorable_Code_Point", "Deprecated", "Diacritic", "Emoji",
        "Emoji_Component", "Emoji_Modifier", "Emoji_Modifier_Base",
        "Emoji_Presentation", "Extended_Pictographic", "Extender", "Grapheme_Base",
        "Grapheme_Extend", "Hex_Digit", "ID_Continue", "Ideographic",
        "IDS_Binary_Operator", "ID_Start", "IDS_Trinary_Operator", "Join_Control",
        "Logical_Order_Exception", "Lowercase", "Math", "Noncharacter_Code_Point",
        "Pattern_Syntax", "Pattern_White_Space", "Quotation_Mark", "Radical",
        "Regional_Indicator", "Sentence_Terminal", "Soft_Dotted",
        "Terminal_Punctuation", "Unified_Ideograph", "Uppercase",
        "Variation_Selector", "White_Space", "XID_Continue", "XID_Start",
    };
    // The canonical long name is always accepted, even if the running ICU is too
    // old to know the property: it is valid syntax, and the character class simply
    // ends up empty. Any other spelling has to be an alias ICU recognises.
    bool isBinaryProperty = binaryPropertyNames.count(name);
    if (!isBinaryProperty) {
        UProperty prop = u_getPropertyEnum(name.c_str());
        // Binary properties occupy [0, UCHAR_INT_START); do not use
        // UCHAR_BINARY_LIMIT, which is fixed at build time and lags the ICU that
        // is actually loaded at runtime.
        if (prop >= 0 && prop < UCHAR_INT_START && exactPropertyNameMatch(prop, name)) {
            const char* longName = u_getPropertyName(prop, U_LONG_PROPERTY_NAME);
            isBinaryProperty = longName && binaryPropertyNames.count(longName);
        }
    }
    if (isBinaryProperty) {
        int idx = getOrCreatePropertyIndex(name, name, PropertyKind::Binary, false);
        return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + idx));
    }

    // 3. General Category (including group aliases like L, N, P)
    if (isGeneralCategoryName(name)) {
        int idx = getOrCreatePropertyIndex(name, name, PropertyKind::GeneralCategory, false);
        return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + idx));
    }

    // 4. Fallback: validate with uset_openPattern for special properties
    //    (e.g. "Any", "ASCII", "Assigned") that ICU APIs don't recognise.
    //    Skip names containing '=' to avoid accepting "Script=Latin" style input.
    //    Use exact name matching to prevent loose matching (e.g. "ANY" should
    //    not be accepted as "Any").
    if (name.find('=') == std::string::npos) {
        static const std::unordered_set<std::string> specialPropertyNames = {
            "Any", "ASCII", "Assigned"
        };
        if (specialPropertyNames.count(name)) {
            int idx = getOrCreatePropertyIndex(name, name, PropertyKind::Special, false);
            return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + idx));
        }
    }
#else
    (void)unicodePropertyValue;
    (void)compileMode;
#endif
    return std::nullopt;
}

#if defined(ENABLE_ICU)
// Build a regex \p{...} character class by asking the runtime ICU for the
// property's code point set (via a uset pattern), instead of carrying hardcoded
// range tables in the binary. If the running ICU is older/limited and does not
// recognize the property pattern, an empty CharacterClass is returned: this
// degrades gracefully (the class simply matches nothing) and never crashes.
static std::unique_ptr<CharacterClass> buildUnicodeCharacterClassFromICU(unsigned unicodePropertyIndex)
{
    auto characterClass = makeUnique<CharacterClass>();

    if (unicodePropertyIndex >= propertyCache().size())
        return characterClass;

    const auto& entry = propertyCache()[unicodePropertyIndex];
    std::string patternStr = buildPatternString(entry);

    UChar patternBuffer[160];
    int32_t patternLength = 0;
    while (patternLength < 159 && static_cast<size_t>(patternLength) < patternStr.size()) {
        patternBuffer[patternLength] = static_cast<UChar>(static_cast<unsigned char>(patternStr[patternLength]));
        patternLength++;
    }
    patternBuffer[patternLength] = 0;

    UErrorCode status = U_ZERO_ERROR;
    USet* set = uset_openPattern(patternBuffer, patternLength, &status);
    if (U_FAILURE(status) || !set) {
        if (set)
            uset_close(set);
        return characterClass;
    }

    bool hasBMP = false;
    bool hasNonBMP = false;
    int32_t itemCount = uset_getItemCount(set);
    for (int32_t i = 0; i < itemCount; i++) {
        UChar32 start = 0;
        UChar32 end = 0;
        UChar strBuffer[256];
        UErrorCode itemStatus = U_ZERO_ERROR;
        int32_t strLength = uset_getItem(set, i, &start, &end, strBuffer, 256, &itemStatus);
        if (U_FAILURE(itemStatus))
            continue;

        if (strLength <= 0) {
            char32_t lo = static_cast<char32_t>(start);
            char32_t hi = static_cast<char32_t>(end);
            if (lo <= 0x7F) {
                char32_t asciiEnd = hi < 0x7F ? hi : 0x7F;
                if (lo == asciiEnd)
                    characterClass->m_matches.append(lo);
                else
                    characterClass->m_ranges.append(CharacterRange(lo, asciiEnd));
            }
            if (hi >= 0x80) {
                char32_t uStart = lo > 0x80 ? lo : 0x80;
                if (uStart == hi)
                    characterClass->m_matchesUnicode.append(hi);
                else
                    characterClass->m_rangesUnicode.append(CharacterRange(uStart, hi));
            }
            if (lo <= 0xFFFF)
                hasBMP = true;
            if (hi > 0xFFFF)
                hasNonBMP = true;
        } else {
            Vector<char32_t> codePoints;
            for (int32_t j = 0; j < strLength;) {
                UChar32 c = strBuffer[j++];
                if (c >= 0xD800 && c <= 0xDBFF && j < strLength) {
                    UChar32 low = strBuffer[j];
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        c = (c << 10) + low - 0x35FDC00;
                        j++;
                    }
                }
                codePoints.append(c);
            }
            characterClass->m_strings.append(std::move(codePoints));
        }
    }
    uset_close(set);

    if (hasBMP && hasNonBMP)
        characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    else if (hasNonBMP)
        characterClass->m_characterWidths = CharacterClassWidths::HasNonBMPChars;
    else if (hasBMP)
        characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;

    return characterClass;
}
#endif

std::unique_ptr<CharacterClass> createUnicodeCharacterClassFor(BuiltInCharacterClassID unicodeClassID)
{
    unsigned unicodePropertyIndex = static_cast<unsigned>(unicodeClassID) - static_cast<unsigned>(BuiltInCharacterClassID::BaseUnicodePropertyID);
#if defined(ENABLE_ICU)
    return buildUnicodeCharacterClassFromICU(unicodePropertyIndex);
#else
    (void)unicodePropertyIndex;
    return nullptr;
#endif
}

bool characterClassMayContainStrings(BuiltInCharacterClassID unicodeClassID)
{
    unsigned unicodePropertyIndex = static_cast<unsigned>(unicodeClassID) - static_cast<unsigned>(BuiltInCharacterClassID::BaseUnicodePropertyID);
#if defined(ENABLE_ICU)
    if (unicodePropertyIndex >= propertyCache().size())
        return false;
    return propertyCache()[unicodePropertyIndex].mayContainStrings;
#else
    (void)unicodePropertyIndex;
    return false;
#endif
}

} } // namespace JSC::Yarr

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
