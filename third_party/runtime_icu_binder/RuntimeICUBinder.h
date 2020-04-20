/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the MIT License (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License
 * at
 *
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef __RuntimeICUBinder__
#define __RuntimeICUBinder__

#define RUNTIME_ICU_BINDER

#include <string>
#include <type_traits>
#include <inttypes.h>

#include "ICUTypes.h"

namespace RuntimeICUBinder {

// TODO define calling convention
#define FOR_EACH_UC_OP(F)                                                                                                                                                  \
    F(u_tolower, UChar32 (*)(UChar32), UChar32)                                                                                                                            \
    F(u_toupper, UChar32 (*)(UChar32), UChar32)                                                                                                                            \
    F(u_islower, UBool (*)(UChar32), UBool)                                                                                                                                \
    F(u_isupper, UBool (*)(UChar32), UBool)                                                                                                                                \
    F(u_totitle, UChar32 (*)(UChar32), UChar32)                                                                                                                            \
    F(u_charMirror, UChar32 (*)(UChar32), UChar32)                                                                                                                         \
    F(u_countChar32, int32_t (*)(const UChar* s, int32_t length), int32_t)                                                                                                 \
    F(u_strToLower, int32_t (*)(UChar * dest, int32_t destCapacity, const UChar* src, int32_t srcLength, const char* locale, UErrorCode* pErrorCode), int32_t)             \
    F(u_strToUpper, int32_t (*)(UChar * dest, int32_t destCapacity, const UChar* src, int32_t srcLength, const char* locale, UErrorCode* pErrorCode), int32_t)             \
    F(unorm2_getNFCInstance, const UNormalizer2* (*)(UErrorCode*), const UNormalizer2*)                                                                                    \
    F(unorm2_getNFDInstance, const UNormalizer2* (*)(UErrorCode*), const UNormalizer2*)                                                                                    \
    F(unorm2_getNFKCInstance, const UNormalizer2* (*)(UErrorCode*), const UNormalizer2*)                                                                                   \
    F(unorm2_getNFKDInstance, const UNormalizer2* (*)(UErrorCode*), const UNormalizer2*)                                                                                   \
    F(unorm2_normalize, int32_t (*)(const UNormalizer2*, const UChar*, int32_t, UChar*, int32_t, UErrorCode*), int32_t)                                                    \
    F(unorm2_composePair, UChar32 (*)(const UNormalizer2* norm2, UChar32 a, UChar32 b), UChar32)                                                                           \
    F(unorm2_getRawDecomposition, int32_t (*)(const UNormalizer2* norm2, UChar32 c, UChar* decomposition, int32_t capacity, UErrorCode* pErrorCode), int32_t)              \
    F(u_getIntPropertyValue, int32_t (*)(UChar32, UProperty), int32_t)                                                                                                     \
    F(u_getIntPropertyMaxValue, int32_t (*)(UProperty), int32_t)                                                                                                           \
    F(u_getIntPropertyMinValue, int32_t (*)(UProperty), int32_t)                                                                                                           \
    F(u_getCombiningClass, uint8_t (*)(UChar32), uint8_t)                                                                                                                  \
    F(u_charType, int8_t (*)(UChar32 c), int8_t)                                                                                                                           \
    F(u_charDirection, UCharDirection (*)(UChar32 c), UCharDirection)                                                                                                      \
    F(u_isblank, UBool (*)(UChar32 c), UBool)                                                                                                                              \
    F(uloc_getDefault, const char* (*)(), const char*)                                                                                                                     \
    F(uloc_getName, int32_t (*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                         \
    F(uloc_canonicalize, int32_t (*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                    \
    F(uloc_getBaseName, int32_t (*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                     \
    F(uloc_forLanguageTag, int32_t (*)(const char* langtag, char* localeID, int32_t localeIDCapacity, int32_t* parsedLength, UErrorCode* err), int32_t)                    \
    F(uloc_getLanguage, int32_t (*)(const char* localeID, char* language, int32_t languageCapacity, UErrorCode* err), int32_t)                                             \
    F(uloc_getScript, int32_t (*)(const char* localeID, char* script, int32_t scriptCapacity, UErrorCode* err), int32_t)                                                   \
    F(uloc_getCountry, int32_t (*)(const char* localeID, char* country, int32_t countryCapacity, UErrorCode* err), int32_t)                                                \
    F(uloc_getVariant, int32_t (*)(const char* localeID, char* variant, int32_t variantCapacity, UErrorCode* err), int32_t)                                                \
    F(uloc_toLegacyKey, const char* (*)(const char* keyword), const char*)                                                                                                 \
    F(uloc_toLegacyType, const char* (*)(const char* keyword, const char* value), const char*)                                                                             \
    F(uloc_toLanguageTag, int32_t (*)(const char* localeID, char* langtag, int32_t langtagCapacity, UBool strict, UErrorCode* err), int32_t)                               \
    F(uloc_addLikelySubtags, int32_t (*)(const char* localeID, char* maximizedLocaleID, int32_t maximizedLocaleIDCapacity, UErrorCode* err), int32_t)                      \
    F(uloc_minimizeSubtags, int32_t (*)(const char* localeID, char* minimizedLocaleID, int32_t minimizedLocaleIDCapacity, UErrorCode* err), int32_t)                       \
    F(ucnv_open, UConverter* (*)(const char* converterName, UErrorCode* err), UConverter*)                                                                                 \
    F(ucnv_compareNames, int (*)(const char* name1, const char* name2), int)                                                                                               \
    F(ucnv_getDisplayName, int32_t (*)(const UConverter* converter, const char* displayLocale, UChar* displayName, int32_t displayNameCapacity, UErrorCode* err), int32_t) \
    F(ucnv_getName, const char* (*)(const UConverter* converter, UErrorCode* err), const char*)                                                                            \
    F(ubidi_open, UBiDi* (*)(), UBiDi*)                                                                                                                                    \
    F(ubidi_getBaseDirection, UBiDiDirection (*)(const UChar* text, int32_t length), UBiDiDirection)                                                                       \
    F(ubidi_countRuns, int32_t (*)(UBiDi * pBiDi, UErrorCode * pErrorCode), int32_t)                                                                                       \
    F(ublock_getCode, UBlockCode (*)(UChar32 c), UBlockCode)                                                                                                               \
    F(uscript_getScript, UScriptCode (*)(UChar32 codepoint, UErrorCode * err), UScriptCode)                                                                                \
    F(uscript_hasScript, UBool (*)(UChar32 c, UScriptCode sc), UBool)                                                                                                      \
    F(uscript_getShortName, const char* (*)(UScriptCode scriptCode), const char*)

#define FOR_EACH_UC_VOID_OP(F)                                                                                                                                                                         \
    F(u_getVersion, void (*)(UVersionInfo versionArray), void)                                                                                                                                         \
    F(uiter_setString, void (*)(UCharIterator * iter, const UChar* s, int32_t length), void)                                                                                                           \
    F(ucnv_close, void (*)(UConverter * converter), void)                                                                                                                                              \
    F(ucnv_toUnicode, void (*)(UConverter * converter, UChar * *target, const UChar* targetLimit, const char** source, const char* sourceLimit, int32_t* offsets, UBool flush, UErrorCode* err), void) \
    F(ubidi_close, void (*)(UBiDi * pBiDi), void)                                                                                                                                                      \
    F(ubidi_setPara, void (*)(UBiDi * pBiDi, const UChar* text, int32_t length, UBiDiLevel paraLevel, UBiDiLevel* embeddingLevels, UErrorCode* pErrorCode), void)                                      \
    F(ubidi_getLogicalRun, void (*)(const UBiDi* pBiDi, int32_t logicalPosition, int32_t* pLogicalLimit, UBiDiLevel* pLevel), void)

#define FOR_EACH_I18N_OP(F)                                                                                                                                                                   \
    F(vzone_openID, VZone* (*)(const UChar*, int32_t), VZone*)                                                                                                                                \
    F(vzone_getRawOffset, int32_t (*)(VZone*), int32_t)                                                                                                                                       \
    F(ucol_countAvailable, int32_t (*)(), int32_t)                                                                                                                                            \
    F(ucol_getAvailable, const char* (*)(int32_t), const char*)                                                                                                                               \
    F(ucol_getKeywordValuesForLocale, UEnumeration* (*)(const char* key, const char* locale, UBool commonlyUsed, UErrorCode* status), UEnumeration*)                                          \
    F(ucol_open, UCollator* (*)(const char* loc, UErrorCode* status), UCollator*)                                                                                                             \
    F(ucol_strcollIter, UCollationResult (*)(const UCollator* coll, UCharIterator* sIter, UCharIterator* tIter, UErrorCode* status), UCollationResult)                                        \
    F(ucol_strcoll, UCollationResult (*)(const UCollator* coll, const UChar* source, int32_t sourceLength, const UChar* target, int32_t targetLength), UCollationResult)                      \
    F(udat_countAvailable, int32_t (*)(), int32_t)                                                                                                                                            \
    F(udat_getAvailable, const char* (*)(int32_t), const char*)                                                                                                                               \
    F(udat_open, UDateFormat* (*)(UDateFormatStyle, UDateFormatStyle, const char*, const UChar*, int32_t, const UChar*, int32_t, UErrorCode*), UDateFormat*)                                  \
    F(udat_format, int32_t (*)(const UDateFormat*, UDate, UChar*, int32_t, UFieldPosition*, UErrorCode*), int32_t)                                                                            \
    F(udat_getCalendar, const UCalendar* (*)(const UDateFormat* fmt), const UCalendar*)                                                                                                       \
    F(uenum_count, int32_t (*)(UEnumeration * en, UErrorCode * status), int32_t)                                                                                                              \
    F(uenum_unext, const UChar* (*)(UEnumeration * en, int32_t * resultLength, UErrorCode * status), const UChar*)                                                                            \
    F(uenum_next, const char* (*)(UEnumeration * en, int32_t * resultLength, UErrorCode * status), const char*)                                                                               \
    F(unumsys_openAvailableNames, UEnumeration* (*)(UErrorCode * status), UEnumeration*)                                                                                                      \
    F(unumsys_openByName, UNumberingSystem* (*)(const char* name, UErrorCode* status), UNumberingSystem*)                                                                                     \
    F(unumsys_isAlgorithmic, UBool (*)(const UNumberingSystem* unumsys), UBool)                                                                                                               \
    F(unumsys_getName, const char* (*)(const UNumberingSystem* unumsys), const char*)                                                                                                         \
    F(unumsys_open, UNumberingSystem* (*)(const char* locale, UErrorCode* status), UNumberingSystem*)                                                                                         \
    F(ucal_getKeywordValuesForLocale, UEnumeration* (*)(const char* key, const char* locale, UBool commonlyUsed, UErrorCode* status), UEnumeration*)                                          \
    F(ucal_openTimeZones, UEnumeration* (*)(UErrorCode * ec), UEnumeration*)                                                                                                                  \
    F(ucal_getCanonicalTimeZoneID, int32_t (*)(const UChar* id, int32_t len, UChar* result, int32_t resultCapacity, UBool* isSystemID, UErrorCode* status), int32_t)                          \
    F(ucal_getType, const char* (*)(const UCalendar* cal, UErrorCode* status), const char*)                                                                                                   \
    F(udatpg_open, UDateTimePatternGenerator* (*)(const char* locale, UErrorCode* pErrorCode), UDateTimePatternGenerator*)                                                                    \
    F(udatpg_getBestPattern, int32_t (*)(UDateTimePatternGenerator * dtpg, const UChar* skeleton, int32_t length, UChar* bestPattern, int32_t capacity, UErrorCode* pErrorCode), int32_t)     \
    F(unum_countAvailable, int32_t (*)(), int32_t)                                                                                                                                            \
    F(unum_getAvailable, const char* (*)(int32_t), const char*)                                                                                                                               \
    F(unum_open, UNumberFormat* (*)(UNumberFormatStyle style, const UChar* pattern, int32_t patternLength, const char* locale, UParseError* parseErr, UErrorCode* status), UNumberFormat*)    \
    F(unum_formatDouble, int32_t (*)(const UNumberFormat* fmt, double number, UChar* result, int32_t resultLength, UFieldPosition* pos, UErrorCode* status), int32_t)                         \
    F(ubrk_open, UBreakIterator* (*)(UBreakIteratorType type, const char* locale, const UChar* text, int32_t textLength, UErrorCode* status), UBreakIterator*)                                \
    F(ubrk_openRules, UBreakIterator* (*)(const UChar* rules, int32_t rulesLength, const UChar* text, int32_t textLength, UParseError* parseErr, UErrorCode* status), UBreakIterator*)        \
    F(ubrk_next, int32_t (*)(UBreakIterator * bi), int32_t)                                                                                                                                   \
    F(ucsdet_open, UCharsetDetector* (*)(UErrorCode * status), UCharsetDetector*)                                                                                                             \
    F(ucsdet_detectAll, const UCharsetMatch** (*)(UCharsetDetector * ucsd, int32_t * matchesFound, UErrorCode * status), const UCharsetMatch**)                                               \
    F(ucsdet_detect, const UCharsetMatch* (*)(UCharsetDetector * ucsd, UErrorCode * status), const UCharsetMatch*)                                                                            \
    F(ucsdet_getName, const char* (*)(const UCharsetMatch* ucsm, UErrorCode* status), const char*)                                                                                            \
    F(ucsdet_getConfidence, int32_t (*)(const UCharsetMatch* ucsm, UErrorCode* status), int32_t)                                                                                              \
    F(ures_openDirect, UResourceBundle* (*)(const char* packageName, const char* locale, UErrorCode* status), UResourceBundle*)                                                               \
    F(ures_getByKey, UResourceBundle* (*)(const UResourceBundle* resourceBundle, const char* key, UResourceBundle* fillIn, UErrorCode* status), UResourceBundle*)                             \
    F(ures_getKey, const char* (*)(const UResourceBundle* resourceBundle), const char*)                                                                                                       \
    F(ures_getNextResource, UResourceBundle* (*)(UResourceBundle * resourceBundle, UResourceBundle * fillIn, UErrorCode * status), UResourceBundle*)                                          \
    F(ures_hasNext, UBool (*)(const UResourceBundle* resourceBundle), UBool)                                                                                                                  \
    F(uplrules_select, int32_t (*)(const UPluralRules* uplrules, double number, UChar* keyword, int32_t capacity, UErrorCode* status), int32_t)                                               \
    F(uplrules_getKeywords, UEnumeration* (*)(const UPluralRules* uplrules, UErrorCode* status), UEnumeration*)                                                                               \
    F(uplrules_open, UPluralRules* (*)(const char* locale, UErrorCode* status), UPluralRules*)                                                                                                \
    F(uplrules_openForType, UPluralRules* (*)(const char* locale, UPluralType type, UErrorCode* status), UPluralRules*)                                                                       \
    F(uplrules_selectWithFormat, int32_t (*)(const UPluralRules* uplrules, double number, const UNumberFormat* fmt, UChar* keyword, int32_t capacity, UErrorCode* status), int32_t)           \
    F(unumf_openForSkeletonAndLocale, UNumberFormatter* (*)(const UChar* skeleton, int32_t skeletonLen, const char* locale, UErrorCode* ec), UNumberFormatter*)                               \
    F(unumf_openForSkeletonAndLocaleWithError, UNumberFormatter* (*)(const UChar* skeleton, int32_t skeletonLen, const char* locale, UParseError* perror, UErrorCode* ec), UNumberFormatter*) \
    F(unumf_openResult, UFormattedNumber* (*)(UErrorCode * ec), UFormattedNumber*)                                                                                                            \
    F(unumf_resultToString, int32_t (*)(const UFormattedNumber* uresult, UChar* buffer, int32_t bufferCapacity, UErrorCode* ec), int32_t)

#define FOR_EACH_I18N_VOID_OP(F)                                                                                                                                     \
    F(udat_close, void (*)(UDateFormat * format), void)                                                                                                              \
    F(vzone_close, void (*)(VZone * zone), void)                                                                                                                     \
    F(uenum_close, void (*)(UEnumeration * zone), void)                                                                                                              \
    F(uenum_reset, void (*)(UEnumeration * en, UErrorCode * status), void)                                                                                           \
    F(ucol_setAttribute, void (*)(UCollator * coll, UColAttribute attr, UColAttributeValue value, UErrorCode * status), void)                                        \
    F(ucol_close, void (*)(UCollator * coll), void)                                                                                                                  \
    F(unumsys_close, void (*)(UNumberingSystem*), void)                                                                                                              \
    F(ucal_setGregorianChange, void (*)(UCalendar * cal, UDate date, UErrorCode * pErrorCode), void)                                                                 \
    F(udatpg_close, void (*)(UDateTimePatternGenerator * zone), void)                                                                                                \
    F(unum_close, void (*)(UNumberFormat*), void)                                                                                                                    \
    F(unum_setTextAttribute, void (*)(UNumberFormat * fmt, UNumberFormatTextAttribute tag, const UChar* newValue, int32_t newValueLength, UErrorCode* status), void) \
    F(unum_setAttribute, void (*)(UNumberFormat * fmt, UNumberFormatAttribute attr, int32_t newValue), void)                                                         \
    F(ubrk_setText, void (*)(UBreakIterator * bi, const UChar* text, int32_t textLength, UErrorCode* status), void)                                                  \
    F(ubrk_setUText, void (*)(UBreakIterator * bi, UText * text, UErrorCode * status), void)                                                                         \
    F(ubrk_close, void (*)(UBreakIterator * format), void)                                                                                                           \
    F(ucsdet_setText, void (*)(UCharsetDetector * ucsd, const char* textIn, int32_t len, UErrorCode* status), void)                                                  \
    F(ucsdet_close, void (*)(UCharsetDetector * ucsd), void)                                                                                                         \
    F(uplrules_close, void (*)(UPluralRules * uplrules), void)                                                                                                       \
    F(ures_close, void (*)(UResourceBundle * resourceBundle), void)                                                                                                  \
    F(ures_resetIterator, void (*)(UResourceBundle * resourceBundle), void)                                                                                          \
    F(unumf_formatDouble, void (*)(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult, UErrorCode* ec), void)                               \
    F(unumf_close, void (*)(UNumberFormatter * uformatter), void)                                                                                                    \
    F(unumf_closeResult, void (*)(UFormattedNumber * uresult), void)

#define FOR_EACH_I18N_STICKY_OP(F) \
    F(vzone_getOffset3)

// TODO make this class thread-safe
class ICU {
private:
    enum Function {
// clang-format off
#define DECLARE_ENUM(name, fnType, fnReturnType) function##name,
        FOR_EACH_UC_OP(DECLARE_ENUM)
#undef DECLARE_ENUM

#define DECLARE_ENUM(name, fnType, fnReturnType) function##name,
        FOR_EACH_UC_VOID_OP(DECLARE_ENUM)
#undef DECLARE_ENUM

#define DECLARE_ENUM(name, fnType, fnReturnType) function##name,
        FOR_EACH_I18N_OP(DECLARE_ENUM)
#undef DECLARE_ENUM

#define DECLARE_ENUM(name, fnType, fnReturnType) function##name,
        FOR_EACH_I18N_VOID_OP(DECLARE_ENUM)
#undef DECLARE_ENUM

#define DECLARE_ENUM(name) function##name,
        FOR_EACH_I18N_STICKY_OP(DECLARE_ENUM)
#undef DECLARE_ENUM
        FunctionMax,
        // clang-format on
    };

public:
    static ICU& instance();
    static std::string findSystemLocale();
    static std::string findSystemTimezoneName();

    enum Soname {
        uc,
        i18n,
        io,
        SonameMax
    };

    void* ensureLoadSo(Soname name)
    {
        if (!m_soHandles[name]) {
            loadSo(name);
        }
        return so(name);
    }
    void* so(Soname name)
    {
        return m_soHandles[name];
    }

#define DECLARE_UC_FN(name, fnType, fnReturnType)                                              \
    template <typename... Args>                                                                \
    fnReturnType name(Args... args)                                                            \
    {                                                                                          \
        return invokeICU<Soname::uc, fnType, fnReturnType>(Function::function##name, args...); \
    }

    FOR_EACH_UC_OP(DECLARE_UC_FN)
#undef DECLARE_UC_FN

#define DECLARE_UC_FN(name, fnType, fnReturnType)                                        \
    template <typename... Args>                                                          \
    void name(Args... args)                                                              \
    {                                                                                    \
        invokeICUWithoutReturn<Soname::i18n, fnType>(Function::function##name, args...); \
    }

    FOR_EACH_UC_VOID_OP(DECLARE_UC_FN)
#undef DECLARE_UC_FN

#define DECLARE_I18N_FN(name, fnType, fnReturnType)                                              \
    template <typename... Args>                                                                  \
    fnReturnType name(Args... args)                                                              \
    {                                                                                            \
        return invokeICU<Soname::i18n, fnType, fnReturnType>(Function::function##name, args...); \
    }

    FOR_EACH_I18N_OP(DECLARE_I18N_FN)
#undef DECLARE_I18N_FN

#define DECLARE_I18N_FN(name, fnType, fnReturnType)                                      \
    template <typename... Args>                                                          \
    void name(Args... args)                                                              \
    {                                                                                    \
        invokeICUWithoutReturn<Soname::i18n, fnType>(Function::function##name, args...); \
    }

    FOR_EACH_I18N_VOID_OP(DECLARE_I18N_FN)
#undef DECLARE_I18N_FN

    void vzone_getOffset3(VZone* zone, UDate date, UBool local, int32_t& rawOffset,
                          int32_t& dstOffset, UErrorCode& ec)
    {
        ensureLoadSo(Soname::i18n);

        if (!m_functions[functionvzone_getOffset3]) {
            loadFunction(Soname::i18n, functionvzone_getOffset3);
        }

        typedef void (*FP)(VZone * zone, UDate date, UBool local, int32_t & rawOffset,
                           int32_t & dstOffset, UErrorCode & ec);
        FP fp = (FP)m_functions[functionvzone_getOffset3];

        return (*fp)(zone, date, local, rawOffset, dstOffset, ec);
    }

private:
    ICU();
    void loadSo(Soname name);
    void* loadFunction(Soname soname, Function kind);

    template <Soname soname, typename FunctionPrototype, typename FunctionReturnType, typename... Args>
    FunctionReturnType invokeICU(Function kind, Args... args)
    {
        ensureLoadSo(soname);

        if (!m_functions[kind]) {
            loadFunction(soname, kind);
        }

        FunctionPrototype fp = (FunctionPrototype)m_functions[kind];

        return (*fp)(args...);
    }

    template <Soname soname, typename FunctionPrototype, typename... Args>
    void invokeICUWithoutReturn(Function kind, Args... args)
    {
        ensureLoadSo(soname);

        if (!m_functions[kind]) {
            loadFunction(soname, kind);
        }

        FunctionPrototype fp = (FunctionPrototype)m_functions[kind];

        (*fp)(args...);
    }

    void* m_soHandles[SonameMax];
    void* m_functions[FunctionMax];
    int m_icuVersion;
};
}

#endif
