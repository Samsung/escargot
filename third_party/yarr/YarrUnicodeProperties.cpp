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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Yarr {

struct HashIndex {
    int16_t value;
    int16_t next;
};

struct HashValue {
    const char* key;
    int index;
};

struct HashTable {
    int numberOfValues;
    int indexMask;
    const HashValue* values;
    const HashIndex* index;

    ALWAYS_INLINE int entry(WTF::String& key) const
    {
        int indexEntry = key.hash() & indexMask;
        int valueIndex = index[indexEntry].value;

        if (valueIndex == -1)
            return -1;

        while (true) {
            const char* keyStr = values[valueIndex].key;

            // assume max length is 1024
            ASSERT(strlen(keyStr) < 1024);
            Escargot::Latin1StringFromExternalMemory str((const unsigned char*)keyStr, strnlen(keyStr, 1024));
            if (key.equals(&str)) {
                return values[valueIndex].index;
            }

            indexEntry = index[indexEntry].next;
            if (indexEntry == -1)
                return -1;
            valueIndex = index[indexEntry].value;
            ASSERT(valueIndex != -1);
        };
    }
};

#if defined(ENABLE_ICU)
#include "UnicodePatternTables.h"
#endif

std::optional<BuiltInCharacterClassID> unicodeMatchPropertyValue(WTF::String unicodePropertyName, WTF::String unicodePropertyValue)
{
    int propertyIndex = -1;
#if defined(ENABLE_ICU)
    if (unicodePropertyName == "Script" || unicodePropertyName == "sc")
        propertyIndex = scriptHashTable.entry(unicodePropertyValue);
    else if (unicodePropertyName == "Script_Extensions" || unicodePropertyName == "scx")
        propertyIndex = scriptExtensionHashTable.entry(unicodePropertyValue);
    else if (unicodePropertyName == "General_Category" || unicodePropertyName == "gc")
        propertyIndex = generalCategoryHashTable.entry(unicodePropertyValue);
#endif
    if (propertyIndex == -1)
        return std::nullopt;

    return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + propertyIndex));
}

std::optional<BuiltInCharacterClassID> unicodeMatchProperty(WTF::String unicodePropertyValue, CompileMode compileMode)
{
    int propertyIndex = -1;
#if defined(ENABLE_ICU)
    propertyIndex = binaryPropertyHashTable.entry(unicodePropertyValue);
    if (propertyIndex == -1)
        propertyIndex = generalCategoryHashTable.entry(unicodePropertyValue);
    if (propertyIndex == -1 && compileMode == CompileMode::UnicodeSets)
        propertyIndex = sequencePropertyHashTable.entry(unicodePropertyValue);
#endif
    if (propertyIndex == -1)
        return std::nullopt;

    return std::optional<BuiltInCharacterClassID>(static_cast<BuiltInCharacterClassID>(static_cast<int>(BuiltInCharacterClassID::BaseUnicodePropertyID) + propertyIndex));
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

    if (unicodePropertyIndex >= (sizeof(unicodePropertyPatterns) / sizeof(unicodePropertyPatterns[0]))) {
        return characterClass;
    }

    const char* pattern = unicodePropertyPatterns[unicodePropertyIndex];
    UChar patternBuffer[160];
    int32_t patternLength = 0;
    while (pattern[patternLength] && patternLength < 159) {
        patternBuffer[patternLength] = static_cast<UChar>(static_cast<unsigned char>(pattern[patternLength]));
        patternLength++;
    }
    patternBuffer[patternLength] = 0;

    UErrorCode status = U_ZERO_ERROR;
    USet* set = uset_openPattern(patternBuffer, patternLength, &status);
    if (U_FAILURE(status) || !set) {
        if (set) {
            uset_close(set);
        }
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
        if (U_FAILURE(itemStatus)) {
            continue;
        }

        if (strLength <= 0) {
            // Code point range [start, end]; split at the BMP boundary.
            if (start <= 0xFFFF) {
                char32_t bmpEnd = end < 0xFFFF ? static_cast<char32_t>(end) : 0xFFFF;
                hasBMP = true;
                if (static_cast<char32_t>(start) == bmpEnd) {
                    characterClass->m_matches.append(static_cast<char32_t>(start));
                } else {
                    characterClass->m_ranges.append(CharacterRange(static_cast<char32_t>(start), bmpEnd));
                }
            }
            if (end > 0xFFFF) {
                char32_t nonBmpStart = start > 0xFFFF ? static_cast<char32_t>(start) : 0x10000;
                hasNonBMP = true;
                if (nonBmpStart == static_cast<char32_t>(end)) {
                    characterClass->m_matchesUnicode.append(static_cast<char32_t>(end));
                } else {
                    characterClass->m_rangesUnicode.append(CharacterRange(nonBmpStart, static_cast<char32_t>(end)));
                }
            }
        } else {
            // Multi-code-point string (v-flag sequence property). Decode UTF-16.
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

    if (hasBMP && hasNonBMP) {
        characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    } else if (hasNonBMP) {
        characterClass->m_characterWidths = CharacterClassWidths::HasNonBMPChars;
    } else if (hasBMP) {
        characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    }

    return characterClass;
}
#endif

std::unique_ptr<CharacterClass> createUnicodeCharacterClassFor(BuiltInCharacterClassID unicodeClassID)
{
    unsigned unicodePropertyIndex = static_cast<unsigned>(unicodeClassID) - static_cast<unsigned>(BuiltInCharacterClassID::BaseUnicodePropertyID);
#if defined(ENABLE_ICU)
    return buildUnicodeCharacterClassFromICU(unicodePropertyIndex);
#else
    return nullptr;
#endif
}

bool characterClassMayContainStrings(BuiltInCharacterClassID unicodeClassID)
{
    unsigned unicodePropertyIndex = static_cast<unsigned>(unicodeClassID) - static_cast<unsigned>(BuiltInCharacterClassID::BaseUnicodePropertyID);
#if defined(ENABLE_ICU)
    return unicodeCharacterClassMayContainStrings(unicodePropertyIndex);
#else
    return false;
#endif
}

} } // namespace JSC::Yarr

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
