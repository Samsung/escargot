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

#include "util/ISO8601.h"

namespace Escargot {

struct MonthCode {
    unsigned monthNumber = 0;
    bool isLeapMonth = false;
    bool operator==(const MonthCode& src) const
    {
        return monthNumber == src.monthNumber && isLeapMonth == src.isLeapMonth;
    }

    bool operator!=(const MonthCode& src) const
    {
        return !operator==(src);
    }
};

// https://github.com/tc39/proposal-intl-era-monthcode
class Calendar {
public:
#define CALENDAR_ID_RECORDS(F)                                                                \
    F(ISO8601, "iso8601", "iso8601", "iso8601", false)                                        \
    F(Buddhist, "buddhist", "buddhist", "buddhist", false)                                    \
    F(Chinese, "chinese", "chinese", "chinese", false)                                        \
    F(Coptic, "coptic", "coptic", "coptic", false)                                            \
    F(Dangi, "dangi", "dangi", "dangi", false)                                                \
    F(Ethiopian, "ethiopic", "ethiopic", "ethiopic", false)                                   \
    F(EthiopianAmeteAlem, "ethioaa", "ethiopic-amete-alem", "ethioaa", false)                 \
    F(EthiopianAmeteAlemAlias, "ethiopic-amete-alem", "ethiopic-amete-alem", "ethioaa", true) \
    F(Gregorian, "gregory", "gregorian", "gregory", false)                                    \
    F(Hebrew, "hebrew", "hebrew", "hebrew", false)                                            \
    F(Indian, "indian", "indian", "indian", false)                                            \
    F(Islamic, "islamic", "islamic", "islamic", false)                                        \
    F(IslamicCivil, "islamic-civil", "islamic-civil", "islamic-civil", false)                 \
    F(IslamicCivilAlias, "islamicc", "islamic-civil", "islamic-civil", true)                  \
    F(IslamicRGSA, "islamic-rgsa", "islamic-rgsa", "islamic-rgsa", false)                     \
    F(IslamicTabular, "islamic-tbla", "islamic-tbla", "islamic-tbla", false)                  \
    F(IslamicUmmAlQura, "islamic-umalqura", "islamic-umalqura", "islamic-umalqura", false)    \
    F(Japanese, "japanese", "japanese", "japanese", false)                                    \
    F(Persian, "persian", "persian", "persian", false)                                        \
    F(ROC, "roc", "roc", "roc", false)

    enum class ID : int32_t {
#define DEFINE_FIELD(name, string, icuString, fullName, alias) name,
        CALENDAR_ID_RECORDS(DEFINE_FIELD)
#undef DEFINE_FIELD
    };

    Calendar(ID id = ID::ISO8601)
        : m_id(id)
    {
    }

    bool operator==(const Calendar& c) const
    {
        return toICUString() == c.toICUString();
    }

    bool operator!=(const Calendar& c) const
    {
        return !operator==(c);
    }

    bool isISO8601() const
    {
        return m_id == ID::ISO8601;
    }

    ID id() const
    {
        return m_id;
    }

    bool isEraRelated() const;
    bool shouldUseICUExtendedYear() const;
    bool hasLeapMonths() const;
    bool hasEpagomenalMonths() const;
    bool sameAsGregoryExceptHandlingEraAndYear() const;

    // https://tc39.es/proposal-intl-era-monthcode/#table-epoch-years
    int32_t epochISOYear() const;

    // icu4c base year of chinese, dangi, roc are differ with icu4x
    int diffYearDueToICU4CAndSpecDiffer() const;

    static Optional<Calendar> fromString(const std::string&, bool shouldAllowIslamicAndIslamicRGSA = false);
    static Optional<Calendar> fromString(String* str, bool shouldAllowIslamicAndIslamicRGSA = false);
    String* toString() const;
    std::string toICUString() const;
    UCalendar* createICUCalendar(ExecutionState& state);
    void lookupICUEra(ExecutionState& state, const std::function<bool(size_t idx, const std::string& icuEra)>& fn) const;

    void setYear(ExecutionState& state, UCalendar* calendar, int32_t year);
    void setYear(ExecutionState& state, UCalendar* calendar, const String* era, int32_t year);

    int32_t year(ExecutionState& state, UCalendar* calendar);
    int32_t eraYear(ExecutionState& state, UCalendar* calendar);
    String* era(ExecutionState& state, UCalendar* calendar);

    void setOrdinalMonth(ExecutionState& state, UCalendar* calendar, int32_t month);
    void setMonth(UCalendar* calendar, MonthCode mc);

    int32_t ordinalMonth(ExecutionState& state, UCalendar* calendar);
    MonthCode monthCode(ExecutionState& state, UCalendar* calendar);
    bool inLeapMonth(ExecutionState& state, UCalendar* calendar);

    static ISO8601::PlainDate computeISODate(ExecutionState& state, UCalendar* ucal);

private:
    UCalendar* createICUCalendar(ExecutionState& state, const std::string& name);
    ID m_id;
};

class Intl {
public:
    typedef std::vector<std::string> (*LocaleDataImplFunction)(String* locale, size_t keyIndex);

    struct IntlMatcherResult {
        IntlMatcherResult()
        {
            extension = locale = String::emptyString();
            extensionIndex = SIZE_MAX;
        }

        String* locale;
        String* extension;
        size_t extensionIndex;
    };

    static void availableTimeZones(const std::function<void(const char* buf, size_t len)>& callback, bool includeNonCanonical = false);
    static UTF16StringDataNonGCStd canonicalTimeZoneID(String* timezoneId);
    static ValueVector canonicalizeLocaleList(ExecutionState& state, Value locales);
    static StringMap resolveLocale(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, const StringMap& options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData);
    static Value supportedLocales(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options);
    static Optional<Object*> getOptionsObject(ExecutionState& state, const Value& options); // https://tc39.es/proposal-temporal/#sec-getoptionsobject
    enum OptionValueType {
        StringValue,
        BooleanValue,
    };
    static Value getOption(ExecutionState& state, Object* options, Value property, OptionValueType type, Value* values, size_t valuesLength, Optional<Value> fallback);
    static Value getOption(ExecutionState& state, Object* options, Value property, OptionValueType type, Value* values, size_t valuesLength, Value fallback)
    {
        return getOption(state, options, property, type, values, valuesLength, Optional<Value>(fallback));
    }
    template <typename T>
    static T getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const T& fallback);
    static std::string preferredLanguage(const std::string& language);
    static String* icuLocaleToBCP47Tag(String* string);
    static std::string convertICUCalendarKeywordToBCP47KeywordIfNeeds(const std::string& icuCalendar);
    static std::string convertBCP47KeywordToICUCalendarKeywordIfNeeds(const std::string& icuCalendar);
    static std::string convertICUCollationKeywordToBCP47KeywordIfNeeds(const std::string& icuCollation);
    static std::string localeIDBufferForLanguageTagWithNullTerminator(const std::string& tag);
    static std::string canonicalizeUnicodeExtensionsAfterICULocaleCanonicalization(std::string&& buffer);
    static Optional<std::string> canonicalizeLocaleID(const char* localeID);
    static bool isUnicodeLocaleIdentifierType(const std::string& src);
    static bool isUnicodeLanguageSubtag(const std::string& src);
    static bool isUnicodeScriptSubtag(const std::string& src);
    static bool isUnicodeRegionSubtag(const std::string& src);
    static bool isUnicodeVariantSubtag(const std::string& src);
    static bool isUnicodeExtensionAttribute(const std::string& src);
    static bool isUnicodeExtensionKey(const std::string& src);
    static Optional<std::string> languageTagForLocaleID(const char* localeID);

    static std::vector<std::string> calendarsForLocale(String* locale, bool includeAlias);
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
    static bool isStructurallyValidLanguageTag(const std::string& string);
    static CanonicalizedLangunageTag isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(const std::string& locale);
    static String* getLocaleForStringLocaleConvertCase(ExecutionState& state, Value locales);
    static std::string canonicalizeCalendarTag(const std::string& s);

    static bool isValidUnicodeLocaleIdentifier(String* value);
    // test string is `(3*8alphanum) *("-" (3*8alphanum))` sequence
    static bool isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(String* value);

    struct NumberFieldItem {
        int32_t start;
        int32_t end;
        int32_t type;
    };
    static void convertICUNumberFieldToEcmaNumberField(std::vector<NumberFieldItem>& fields, double x, const UTF16StringDataNonGCStd& resultString);
    static String* icuNumberFieldToString(ExecutionState& state, int32_t fieldName, double d, String* style);

    enum class RoundingType {
        MorePrecision,
        LessPrecision,
        SignificantDigits,
        FractionDigits,
    };

    enum class RoundingPriority {
        Auto,
        LessPrecision,
        MorePrecision
    };
    struct SetNumberFormatDigitOptionsResult {
        double minimumIntegerDigits;
        double roundingIncrement;
        String* roundingMode;
        String* trailingZeroDisplay;
        Value minimumSignificantDigits;
        Value maximumSignificantDigits;
        Value minimumFractionDigits;
        Value maximumFractionDigits;
        RoundingType roundingType;
        RoundingPriority computedRoundingPriority;
    };
    // https://402.ecma-international.org/12.0/index.html#sec-setnumberformatdigitoptions
    static SetNumberFormatDigitOptionsResult setNumberFormatDigitOptions(ExecutionState& state, Object* options,
                                                                         double mnfdDefault, double mxfdDefault, String* notation);
    static void initNumberFormatSkeleton(ExecutionState& state, const Intl::SetNumberFormatDigitOptionsResult& formatResult, String* notation, String* compactDisplay, UTF16StringDataNonGCStd& skeleton);

    // https://tc39.es/ecma402/#sec-defaultnumberoption
    static Value defaultNumberOption(ExecutionState& state, Value value, double minimum, double maximum, double fallback);
    static Value defaultNumberOption(ExecutionState& state, Value value, double minimum, double maximum, Value fallback);

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

#define INTL_ICU_STD_STRING_BUFFER_OPERATION(icuFnName, ...)                     \
    ([&]() -> std::pair<UErrorCode, std::string> {                               \
        std::string output;                                                      \
        UErrorCode status = U_ZERO_ERROR;                                        \
        auto resultLength = icuFnName(__VA_ARGS__, nullptr, 0, &status);         \
        if (status == U_BUFFER_OVERFLOW_ERROR) {                                 \
            status = U_ZERO_ERROR;                                               \
            output.resize(resultLength);                                         \
            icuFnName(__VA_ARGS__, (char*)output.data(), resultLength, &status); \
        }                                                                        \
        return std::make_pair(status, output);                                   \
    })()

#define INTL_ICU_STD_STRING_BUFFER_OPERATION_COMPLEX(icuFnName, extra, ...)             \
    ([&]() -> std::pair<UErrorCode, std::string> {                                      \
        std::string output;                                                             \
        UErrorCode status = U_ZERO_ERROR;                                               \
        auto resultLength = icuFnName(__VA_ARGS__, nullptr, 0, extra, &status);         \
        if (status == U_BUFFER_OVERFLOW_ERROR) {                                        \
            status = U_ZERO_ERROR;                                                      \
            output.resize(resultLength);                                                \
            icuFnName(__VA_ARGS__, (char*)output.data(), resultLength, extra, &status); \
        }                                                                               \
        return std::make_pair(status, output);                                          \
    })()
};

} // namespace Escargot

#endif
#endif
