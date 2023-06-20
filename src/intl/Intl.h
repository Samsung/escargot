#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
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

#ifndef __EscargotIntlObject__
#define __EscargotIntlObject__

namespace Escargot {

class Intl {
public:
    typedef std::vector<std::string> (*LocaleDataImplFunction)(String* locale, size_t keyIndex);

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

    static ValueVector canonicalizeLocaleList(ExecutionState& state, Value locales);
    static StringMap resolveLocale(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, const StringMap& options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData);
    static Value supportedLocales(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options);
    enum OptionValueType {
        StringValue,
        BooleanValue,
#if defined(ENABLE_TEMPORAL)
        NumberValue
#endif
    };
    static Value getOption(ExecutionState& state, Object* options, Value property, OptionValueType type, Value* values, size_t valuesLength, const Value& fallback);
    template <typename T>
    static T getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const T& fallback);
    static std::string preferredLanguage(const std::string& language);
    static String* icuLocaleToBCP47Tag(String* string);
    static std::string convertICUCalendarKeywordToBCP47KeywordIfNeeds(const std::string& icuCalendar);
    static std::string convertICUCollationKeywordToBCP47KeywordIfNeeds(const std::string& icuCollation);
    static std::vector<std::string> calendarsForLocale(String* locale);
    static std::vector<std::string> numberingSystemsForLocale(String* locale);
    struct CanonicalizedLangunageTag {
        Optional<String*> canonicalizedTag;
        std::string language;
        std::vector<std::string> extLang;
        std::string script;
        std::string region;
        std::vector<std::string> variant;
        struct ExtensionSingleton {
            char key;
            std::string value;
        };
        std::vector<ExtensionSingleton> extensions;
        std::vector<std::pair<std::string, std::string>> unicodeExtension;
        std::string privateUse;
    };
    static CanonicalizedLangunageTag canonicalizeLanguageTag(const std::string& locale, const std::string& unicodeExtensionNameShouldIgnored = "");
    static CanonicalizedLangunageTag isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(const std::string& locale);
    static String* getLocaleForStringLocaleConvertCase(ExecutionState& state, Value locales);

    static bool isValidUnicodeLocaleIdentifier(String* value);
    // test string is `(3*8alphanum) *("-" (3*8alphanum))` sequence
    static bool isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(String* value);

    struct NumberFieldItem {
        int32_t start;
        int32_t end;
        int32_t type;
    };
    static void convertICUNumberFieldToEcmaNumberField(std::vector<NumberFieldItem>& fields, double x, const UTF16StringDataNonGCStd& resultString);
    static String* icuNumberFieldToString(ExecutionState& state, int32_t fieldName, double d);

#define INTL_ICU_STRING_BUFFER_OPERATION(icuFnName, ...)                                                     \
    ([&]() -> std::pair<UErrorCode, UTF16StringDataNonGCStd> {                                               \
        UTF16StringDataNonGCStd output;                                                                      \
        UErrorCode status = U_ZERO_ERROR;                                                                    \
        const size_t defaultStringBufferSize = 32;                                                           \
        output.resize(defaultStringBufferSize);                                                              \
        status = U_ZERO_ERROR;                                                                               \
        auto resultLength = icuFnName(__VA_ARGS__, (UChar*)output.data(), defaultStringBufferSize, &status); \
        if (U_SUCCESS(status)) {                                                                             \
            output.resize(resultLength);                                                                     \
        } else if (status == U_BUFFER_OVERFLOW_ERROR) {                                                      \
            status = U_ZERO_ERROR;                                                                           \
            output.resize(resultLength);                                                                     \
            icuFnName(__VA_ARGS__, (UChar*)output.data(), resultLength, &status);                            \
        }                                                                                                    \
        return std::make_pair(status, output);                                                               \
    })()

#define INTL_ICU_STRING_BUFFER_OPERATION_COMPLEX(icuFnName, extra, ...)                                             \
    ([&]() -> std::pair<UErrorCode, UTF16StringDataNonGCStd> {                                                      \
        UTF16StringDataNonGCStd output;                                                                             \
        UErrorCode status = U_ZERO_ERROR;                                                                           \
        const size_t defaultStringBufferSize = 32;                                                                  \
        output.resize(defaultStringBufferSize);                                                                     \
        status = U_ZERO_ERROR;                                                                                      \
        auto resultLength = icuFnName(__VA_ARGS__, (UChar*)output.data(), defaultStringBufferSize, extra, &status); \
        if (U_SUCCESS(status)) {                                                                                    \
            output.resize(resultLength);                                                                            \
        } else if (status == U_BUFFER_OVERFLOW_ERROR) {                                                             \
            status = U_ZERO_ERROR;                                                                                  \
            output.resize(resultLength);                                                                            \
            icuFnName(__VA_ARGS__, (UChar*)output.data(), resultLength, extra, &status);                            \
        }                                                                                                           \
        return std::make_pair(status, output);                                                                      \
    })()
};
} // namespace Escargot

#endif
#endif
