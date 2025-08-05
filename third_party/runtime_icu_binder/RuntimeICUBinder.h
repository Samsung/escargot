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
#include <thread>
#include <mutex>
#include <inttypes.h>

#include "ICUTypes.h"

namespace RuntimeICUBinder {

#if defined(_WIN32) || defined(_WIN64)
#define CALLCONV __cdecl
#else
#define CALLCONV
#endif

#define FOR_EACH_UC_OP(F)                                                                                                                                                         \
    F(u_tolower, UChar32(CALLCONV*)(UChar32), UChar32)                                                                                                                            \
    F(u_toupper, UChar32(CALLCONV*)(UChar32), UChar32)                                                                                                                            \
    F(u_islower, UBool(CALLCONV*)(UChar32), UBool)                                                                                                                                \
    F(u_isupper, UBool(CALLCONV*)(UChar32), UBool)                                                                                                                                \
    F(u_totitle, UChar32(CALLCONV*)(UChar32), UChar32)                                                                                                                            \
    F(u_charMirror, UChar32(CALLCONV*)(UChar32), UChar32)                                                                                                                         \
    F(u_countChar32, int32_t(CALLCONV*)(const UChar* s, int32_t length), int32_t)                                                                                                 \
    F(u_strToLower, int32_t(CALLCONV*)(UChar * dest, int32_t destCapacity, const UChar* src, int32_t srcLength, const char* locale, UErrorCode* pErrorCode), int32_t)             \
    F(u_strToUpper, int32_t(CALLCONV*)(UChar * dest, int32_t destCapacity, const UChar* src, int32_t srcLength, const char* locale, UErrorCode* pErrorCode), int32_t)             \
    F(unorm2_getNFCInstance, const UNormalizer2*(CALLCONV*)(UErrorCode*), const UNormalizer2*)                                                                                    \
    F(unorm2_getNFDInstance, const UNormalizer2*(CALLCONV*)(UErrorCode*), const UNormalizer2*)                                                                                    \
    F(unorm2_getNFKCInstance, const UNormalizer2*(CALLCONV*)(UErrorCode*), const UNormalizer2*)                                                                                   \
    F(unorm2_getNFKDInstance, const UNormalizer2*(CALLCONV*)(UErrorCode*), const UNormalizer2*)                                                                                   \
    F(unorm2_normalize, int32_t(CALLCONV*)(const UNormalizer2*, const UChar*, int32_t, UChar*, int32_t, UErrorCode*), int32_t)                                                    \
    F(unorm2_composePair, UChar32(CALLCONV*)(const UNormalizer2* norm2, UChar32 a, UChar32 b), UChar32)                                                                           \
    F(unorm2_getRawDecomposition, int32_t(CALLCONV*)(const UNormalizer2* norm2, UChar32 c, UChar* decomposition, int32_t capacity, UErrorCode* pErrorCode), int32_t)              \
    F(u_getIntPropertyValue, int32_t(CALLCONV*)(UChar32, UProperty), int32_t)                                                                                                     \
    F(u_getIntPropertyMaxValue, int32_t(CALLCONV*)(UProperty), int32_t)                                                                                                           \
    F(u_getIntPropertyMinValue, int32_t(CALLCONV*)(UProperty), int32_t)                                                                                                           \
    F(u_getCombiningClass, uint8_t(CALLCONV*)(UChar32), uint8_t)                                                                                                                  \
    F(u_charType, int8_t(CALLCONV*)(UChar32 c), int8_t)                                                                                                                           \
    F(u_charDirection, UCharDirection(CALLCONV*)(UChar32 c), UCharDirection)                                                                                                      \
    F(u_isblank, UBool(CALLCONV*)(UChar32 c), UBool)                                                                                                                              \
    F(uloc_getDefault, const char*(CALLCONV*)(), const char*)                                                                                                                     \
    F(uloc_getName, int32_t(CALLCONV*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                         \
    F(uloc_canonicalize, int32_t(CALLCONV*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                    \
    F(uloc_getBaseName, int32_t(CALLCONV*)(const char* localeID, char* name, int32_t nameCapacity, UErrorCode* err), int32_t)                                                     \
    F(uloc_forLanguageTag, int32_t(CALLCONV*)(const char* langtag, char* localeID, int32_t localeIDCapacity, int32_t* parsedLength, UErrorCode* err), int32_t)                    \
    F(uloc_getLanguage, int32_t(CALLCONV*)(const char* localeID, char* language, int32_t languageCapacity, UErrorCode* err), int32_t)                                             \
    F(uloc_getScript, int32_t(CALLCONV*)(const char* localeID, char* script, int32_t scriptCapacity, UErrorCode* err), int32_t)                                                   \
    F(uloc_getCountry, int32_t(CALLCONV*)(const char* localeID, char* country, int32_t countryCapacity, UErrorCode* err), int32_t)                                                \
    F(uloc_getVariant, int32_t(CALLCONV*)(const char* localeID, char* variant, int32_t variantCapacity, UErrorCode* err), int32_t)                                                \
    F(uloc_toLegacyKey, const char*(CALLCONV*)(const char* keyword), const char*)                                                                                                 \
    F(uloc_toLegacyType, const char*(CALLCONV*)(const char* keyword, const char* value), const char*)                                                                             \
    F(uloc_toLanguageTag, int32_t(CALLCONV*)(const char* localeID, char* langtag, int32_t langtagCapacity, UBool strict, UErrorCode* err), int32_t)                               \
    F(uloc_addLikelySubtags, int32_t(CALLCONV*)(const char* localeID, char* maximizedLocaleID, int32_t maximizedLocaleIDCapacity, UErrorCode* err), int32_t)                      \
    F(uloc_minimizeSubtags, int32_t(CALLCONV*)(const char* localeID, char* minimizedLocaleID, int32_t minimizedLocaleIDCapacity, UErrorCode* err), int32_t)                       \
    F(uloc_getCharacterOrientation, ULayoutType(CALLCONV*)(const char* localeID, UErrorCode* err), ULayoutType)                                                                   \
    F(uloc_countAvailable, int32_t(CALLCONV*)(), int32_t)                                                                                                                         \
    F(uloc_getAvailable, const char*(CALLCONV*)(int32_t n), const char*)                                                                                                          \
    F(uloc_toUnicodeLocaleType, const char*(CALLCONV*)(const char*, const char*), const char*)                                                                                    \
    F(uloc_setKeywordValue, int32_t(CALLCONV*)(const char *keywordName, const char *keywordValue, char *buffer, int32_t bufferCapacity, UErrorCode *status), int32_t)             \
    F(uloc_getKeywordValue, int32_t(CALLCONV*)(const char *localeID, const char *keywordName, char *buffer, int32_t bufferCapacity, UErrorCode *status), int32_t)                 \
    F(ucnv_open, UConverter*(CALLCONV*)(const char* converterName, UErrorCode* err), UConverter*)                                                                                 \
    F(ucnv_compareNames, int(CALLCONV*)(const char* name1, const char* name2), int)                                                                                               \
    F(ucnv_getDisplayName, int32_t(CALLCONV*)(const UConverter* converter, const char* displayLocale, UChar* displayName, int32_t displayNameCapacity, UErrorCode* err), int32_t) \
    F(ucnv_getName, const char*(CALLCONV*)(const UConverter* converter, UErrorCode* err), const char*)                                                                            \
    F(ubidi_open, UBiDi*(CALLCONV*)(), UBiDi*)                                                                                                                                    \
    F(ubidi_getBaseDirection, UBiDiDirection(CALLCONV*)(const UChar* text, int32_t length), UBiDiDirection)                                                                       \
    F(ubidi_countRuns, int32_t(CALLCONV*)(UBiDi * pBiDi, UErrorCode * pErrorCode), int32_t)                                                                                       \
    F(ublock_getCode, UBlockCode(CALLCONV*)(UChar32 c), UBlockCode)                                                                                                               \
    F(uscript_getScript, UScriptCode(CALLCONV*)(UChar32 codepoint, UErrorCode * err), UScriptCode)                                                                                \
    F(uscript_hasScript, UBool(CALLCONV*)(UChar32 c, UScriptCode sc), UBool)                                                                                                      \
    F(uscript_getShortName, const char*(CALLCONV*)(UScriptCode scriptCode), const char*)                                                                                          \
    F(uldn_open, ULocaleDisplayNames*(CALLCONV*)(const char* locale, UDialectHandling dialectHandling, UErrorCode* pErrorCode), ULocaleDisplayNames*)                             \
    F(uldn_openForContext, ULocaleDisplayNames*(CALLCONV*)(const char* locale, UDisplayContext* contexts, int32_t length, UErrorCode* pErrorCode), ULocaleDisplayNames*)          \
    F(uldn_localeDisplayName, int32_t(CALLCONV*)(const ULocaleDisplayNames* ldn, const char* locale, UChar* result, int32_t maxResultSize, UErrorCode* pErrorCode), int32_t)      \
    F(uldn_languageDisplayName, int32_t(CALLCONV*)(const ULocaleDisplayNames* ldn, const char* lang, UChar* result, int32_t maxResultSize, UErrorCode* pErrorCode), int32_t)      \
    F(uldn_regionDisplayName, int32_t(CALLCONV*)(const ULocaleDisplayNames* ldn, const char* region, UChar* result, int32_t maxResultSize, UErrorCode* pErrorCode), int32_t)      \
    F(uldn_scriptDisplayName, int32_t(CALLCONV*)(const ULocaleDisplayNames* ldn, const char* lang, UChar* result, int32_t maxResultSize, UErrorCode* pErrorCode), int32_t)        \
    F(uldn_keyValueDisplayName, int32_t(CALLCONV*)(const ULocaleDisplayNames* ldn, const char* key, const char* value, UChar* result, int32_t maxResultSize, UErrorCode* pErrorCode), int32_t)

#define FOR_EACH_UC_VOID_OP(F)                                                                                                                                                                                 \
    F(u_getVersion, void(CALLCONV*)(UVersionInfo versionArray), void)                                                                                                                                          \
    F(uiter_setString, void(CALLCONV*)(UCharIterator * iter, const UChar* s, int32_t length), void)                                                                                                            \
    F(ucnv_close, void(CALLCONV*)(UConverter * converter), void)                                                                                                                                               \
    F(ucnv_toUnicode, void(CALLCONV*)(UConverter * converter, UChar * *target, const UChar* targetLimit, const char** source, const char* sourceLimit, int32_t* offsets, UBool flush, UErrorCode* err), void)  \
    F(ucnv_fromUnicode, void(CALLCONV*)(UConverter * converter, char** target, const char* targetLimit, const UChar** source, const UChar* sourceLimit, int32_t* offsets, UBool flush, UErrorCode* err), void) \
    F(ubidi_close, void(CALLCONV*)(UBiDi * pBiDi), void)                                                                                                                                                       \
    F(ubidi_setPara, void(CALLCONV*)(UBiDi * pBiDi, const UChar* text, int32_t length, UBiDiLevel paraLevel, UBiDiLevel* embeddingLevels, UErrorCode* pErrorCode), void)                                       \
    F(ubidi_getLogicalRun, void(CALLCONV*)(const UBiDi* pBiDi, int32_t logicalPosition, int32_t* pLogicalLimit, UBiDiLevel* pLevel), void)                                                                     \
    F(uldn_close, void(CALLCONV*)(ULocaleDisplayNames * ldn), void)

#define FOR_EACH_I18N_OP(F)                                                                                                                                                                                                                \
    F(vzone_openID, VZone*(CALLCONV*)(const UChar*, int32_t), VZone*)                                                                                                                                                                      \
    F(vzone_getRawOffset, int32_t(CALLCONV*)(VZone*), int32_t)                                                                                                                                                                             \
    F(ucol_countAvailable, int32_t(CALLCONV*)(), int32_t)                                                                                                                                                                                  \
    F(ucol_getAvailable, const char*(CALLCONV*)(int32_t), const char*)                                                                                                                                                                     \
    F(ucol_getKeywordValues, UEnumeration*(CALLCONV*)(const char* key, UErrorCode* status), UEnumeration*)                                                                                                                                 \
    F(ucol_getKeywordValuesForLocale, UEnumeration*(CALLCONV*)(const char* key, const char* locale, UBool commonlyUsed, UErrorCode* status), UEnumeration*)                                                                                \
    F(ucol_getAttribute, UColAttributeValue(CALLCONV*)(const UCollator* coll, UColAttribute attr, UErrorCode* status), UColAttributeValue)                                                                                                 \
    F(ucol_open, UCollator*(CALLCONV*)(const char* loc, UErrorCode* status), UCollator*)                                                                                                                                                   \
    F(ucol_strcollIter, UCollationResult(CALLCONV*)(const UCollator* coll, UCharIterator* sIter, UCharIterator* tIter, UErrorCode* status), UCollationResult)                                                                              \
    F(ucol_strcoll, UCollationResult(CALLCONV*)(const UCollator* coll, const UChar* source, int32_t sourceLength, const UChar* target, int32_t targetLength), UCollationResult)                                                            \
    F(udat_countAvailable, int32_t(CALLCONV*)(), int32_t)                                                                                                                                                                                  \
    F(udat_getAvailable, const char*(CALLCONV*)(int32_t), const char*)                                                                                                                                                                     \
    F(udat_open, UDateFormat*(CALLCONV*)(UDateFormatStyle, UDateFormatStyle, const char*, const UChar*, int32_t, const UChar*, int32_t, UErrorCode*), UDateFormat*)                                                                        \
    F(udat_format, int32_t(CALLCONV*)(const UDateFormat*, UDate, UChar*, int32_t, UFieldPosition*, UErrorCode*), int32_t)                                                                                                                  \
    F(udat_formatForFields, int32_t(CALLCONV*)(const UDateFormat* format, UDate, UChar*, int32_t, UFieldPositionIterator*, UErrorCode*), int32_t)                                                                                          \
    F(udat_getCalendar, const UCalendar*(CALLCONV*)(const UDateFormat* fmt), const UCalendar*)                                                                                                                                             \
    F(udat_toPattern, int32_t(CALLCONV*)(const UDateFormat* fmt, UBool localized, UChar* result, int32_t resultLength, UErrorCode* status), int32_t)                                                                                       \
    F(udat_parse, UDate(CALLCONV*)(const UDateFormat*, const UChar*, int32_t, int32_t*, UErrorCode*), UDate)                                                                                                                               \
    F(uenum_count, int32_t(CALLCONV*)(UEnumeration * en, UErrorCode * status), int32_t)                                                                                                                                                    \
    F(uenum_unext, const UChar*(CALLCONV*)(UEnumeration * en, int32_t* resultLength, UErrorCode* status), const UChar*)                                                                                                                    \
    F(uenum_next, const char*(CALLCONV*)(UEnumeration * en, int32_t* resultLength, UErrorCode* status), const char*)                                                                                                                       \
    F(unumsys_openAvailableNames, UEnumeration*(CALLCONV*)(UErrorCode * status), UEnumeration*)                                                                                                                                            \
    F(unumsys_openByName, UNumberingSystem*(CALLCONV*)(const char* name, UErrorCode* status), UNumberingSystem*)                                                                                                                           \
    F(unumsys_isAlgorithmic, UBool(CALLCONV*)(const UNumberingSystem* unumsys), UBool)                                                                                                                                                     \
    F(unumsys_getName, const char*(CALLCONV*)(const UNumberingSystem* unumsys), const char*)                                                                                                                                               \
    F(unumsys_open, UNumberingSystem*(CALLCONV*)(const char* locale, UErrorCode* status), UNumberingSystem*)                                                                                                                               \
    F(ucal_open, UCalendar*(CALLCONV*)(const UChar* zoneID, int32_t len, const char* locale, UCalendarType type, UErrorCode* status), UCalendar*)                                                                                          \
    F(ucal_getDefaultTimeZone, int32_t(CALLCONV*)(UChar * result, int32_t resultCapacity, UErrorCode* ec), int32_t)                                                                                                                        \
    F(ucal_getTimeZoneDisplayName, int32_t(CALLCONV*)(const UCalendar* cal, UCalendarDisplayNameType type, const char* locale, UChar* result, int32_t resultLength, UErrorCode* status), int32_t)                                          \
    F(ucal_getKeywordValuesForLocale, UEnumeration*(CALLCONV*)(const char* key, const char* locale, UBool commonlyUsed, UErrorCode* status), UEnumeration*)                                                                                \
    F(ucal_openTimeZoneIDEnumeration, UEnumeration*(CALLCONV*)(USystemTimeZoneType zoneType, const char* region, const int32_t* rawOffset, UErrorCode* ec), UEnumeration*)                                                                 \
    F(ucal_openTimeZones, UEnumeration*(CALLCONV*)(UErrorCode * ec), UEnumeration*)                                                                                                                                                        \
    F(ucal_getCanonicalTimeZoneID, int32_t(CALLCONV*)(const UChar* id, int32_t len, UChar* result, int32_t resultCapacity, UBool* isSystemID, UErrorCode* status), int32_t)                                                                \
    F(ucal_get, int32_t(CALLCONV*)(const UCalendar* cal, UCalendarDateFields field, UErrorCode* status), int32_t)                                                                                                                          \
    F(ucal_getType, const char*(CALLCONV*)(const UCalendar* cal, UErrorCode* status), const char*)                                                                                                                                         \
    F(ucal_getAttribute, int32_t(CALLCONV*)(const UCalendar* cal, UCalendarAttribute attr), int32_t)                                                                                                                                       \
    F(ucal_getDayOfWeekType, UCalendarWeekdayType(CALLCONV*)(const UCalendar* cal, UCalendarDaysOfWeek dayOfWeek, UErrorCode* status), UCalendarWeekdayType)                                                                               \
    F(ucal_clone, UCalendar*(CALLCONV*)(const UCalendar* cal, UErrorCode* status), UCalendar*)                                                                                                                                             \
    F(udatpg_open, UDateTimePatternGenerator*(CALLCONV*)(const char* locale, UErrorCode* pErrorCode), UDateTimePatternGenerator*)                                                                                                          \
    F(udatpg_getBestPattern, int32_t(CALLCONV*)(UDateTimePatternGenerator * dtpg, const UChar* skeleton, int32_t length, UChar* bestPattern, int32_t capacity, UErrorCode* pErrorCode), int32_t)                                           \
    F(udatpg_getBestPatternWithOptions, int32_t(CALLCONV*)(UDateTimePatternGenerator*, const UChar*, int32_t, UDateTimePatternMatchOptions, UChar*, int32_t, UErrorCode*), int32_t)                                                        \
    F(udatpg_getSkeleton, int32_t(CALLCONV*)(UDateTimePatternGenerator * unusedDtpg, const UChar* pattern, int32_t length, UChar* skeleton, int32_t capacity, UErrorCode* pErrorCode), int32_t)                                            \
    F(udatpg_getFieldDisplayName, int32_t(CALLCONV*)(const UDateTimePatternGenerator*, UDateTimePatternField, UDateTimePGDisplayWidth, UChar*, int32_t, UErrorCode*), int32_t)                                                             \
    F(unum_countAvailable, int32_t(CALLCONV*)(), int32_t)                                                                                                                                                                                  \
    F(unum_getAvailable, const char*(CALLCONV*)(int32_t), const char*)                                                                                                                                                                     \
    F(unum_open, UNumberFormat*(CALLCONV*)(UNumberFormatStyle style, const UChar* pattern, int32_t patternLength, const char* locale, UParseError* parseErr, UErrorCode* status), UNumberFormat*)                                          \
    F(unum_formatDouble, int32_t(CALLCONV*)(const UNumberFormat* fmt, double number, UChar* result, int32_t resultLength, UFieldPosition* pos, UErrorCode* status), int32_t)                                                               \
    F(unum_formatDoubleForFields, int32_t(CALLCONV*)(const UNumberFormat* format, double number, UChar* result, int32_t resultLength, UFieldPositionIterator* fpositer, UErrorCode* status), int32_t)                                      \
    F(ubrk_open, UBreakIterator*(CALLCONV*)(UBreakIteratorType type, const char* locale, const UChar* text, int32_t textLength, UErrorCode* status), UBreakIterator*)                                                                      \
    F(ubrk_openRules, UBreakIterator*(CALLCONV*)(const UChar* rules, int32_t rulesLength, const UChar* text, int32_t textLength, UParseError* parseErr, UErrorCode* status), UBreakIterator*)                                              \
    F(ubrk_next, int32_t(CALLCONV*)(UBreakIterator * bi), int32_t)                                                                                                                                                                         \
    F(ucsdet_open, UCharsetDetector*(CALLCONV*)(UErrorCode * status), UCharsetDetector*)                                                                                                                                                   \
    F(ucsdet_detectAll, const UCharsetMatch**(CALLCONV*)(UCharsetDetector * ucsd, int32_t* matchesFound, UErrorCode* status), const UCharsetMatch**)                                                                                       \
    F(ucsdet_detect, const UCharsetMatch*(CALLCONV*)(UCharsetDetector * ucsd, UErrorCode * status), const UCharsetMatch*)                                                                                                                  \
    F(ucsdet_getName, const char*(CALLCONV*)(const UCharsetMatch* ucsm, UErrorCode* status), const char*)                                                                                                                                  \
    F(ucsdet_getConfidence, int32_t(CALLCONV*)(const UCharsetMatch* ucsm, UErrorCode* status), int32_t)                                                                                                                                    \
    F(ures_open, UResourceBundle*(CALLCONV*)(const char* packageName, const char* locale, UErrorCode* status), UResourceBundle*)                                                                                                           \
    F(ures_openDirect, UResourceBundle*(CALLCONV*)(const char* packageName, const char* locale, UErrorCode* status), UResourceBundle*)                                                                                                     \
    F(ures_getByKey, UResourceBundle*(CALLCONV*)(const UResourceBundle* resourceBundle, const char* key, UResourceBundle* fillIn, UErrorCode* status), UResourceBundle*)                                                                   \
    F(ures_getKey, const char*(CALLCONV*)(const UResourceBundle* resourceBundle), const char*)                                                                                                                                             \
    F(ures_getNextResource, UResourceBundle*(CALLCONV*)(UResourceBundle * resourceBundle, UResourceBundle * fillIn, UErrorCode * status), UResourceBundle*)                                                                                \
    F(ures_hasNext, UBool(CALLCONV*)(const UResourceBundle* resourceBundle), UBool)                                                                                                                                                        \
    F(ures_getSize, int32_t(CALLCONV*)(const UResourceBundle* resourceBundle), int32_t)                                                                                                                                                    \
    F(ures_getStringByIndex, const UChar*(CALLCONV*)(const UResourceBundle* resourceBundle, int32_t indexS, int32_t* len, UErrorCode* status), const UChar*)                                                                               \
    F(ures_getStringByKey, const UChar*(CALLCONV*)(const UResourceBundle *resB, const char *key, int32_t *len, UErrorCode *status), const UChar*)                                                                                          \
    F(uplrules_select, int32_t(CALLCONV*)(const UPluralRules* uplrules, double number, UChar* keyword, int32_t capacity, UErrorCode* status), int32_t)                                                                                     \
    F(uplrules_getKeywords, UEnumeration*(CALLCONV*)(const UPluralRules* uplrules, UErrorCode* status), UEnumeration*)                                                                                                                     \
    F(uplrules_open, UPluralRules*(CALLCONV*)(const char* locale, UErrorCode* status), UPluralRules*)                                                                                                                                      \
    F(uplrules_openForType, UPluralRules*(CALLCONV*)(const char* locale, UPluralType type, UErrorCode* status), UPluralRules*)                                                                                                             \
    F(uplrules_selectWithFormat, int32_t(CALLCONV*)(const UPluralRules* uplrules, double number, const UNumberFormat* fmt, UChar* keyword, int32_t capacity, UErrorCode* status), int32_t)                                                 \
    F(uplrules_selectFormatted, int32_t(CALLCONV*)(const UPluralRules* uplrules, const struct UFormattedNumber*, UChar*, int32_t, UErrorCode*), int32_t)                                                                                   \
    F(uplrules_selectForRange, int32_t(CALLCONV*)(const UPluralRules*, const struct UFormattedNumberRange*, UChar*, int32_t, UErrorCode*), int32_t)                                                                                        \
    F(unumf_openForSkeletonAndLocale, UNumberFormatter*(CALLCONV*)(const UChar* skeleton, int32_t skeletonLen, const char* locale, UErrorCode* ec), UNumberFormatter*)                                                                     \
    F(unumf_openForSkeletonAndLocaleWithError, UNumberFormatter*(CALLCONV*)(const UChar* skeleton, int32_t skeletonLen, const char* locale, UParseError* perror, UErrorCode* ec), UNumberFormatter*)                                       \
    F(unumf_openResult, UFormattedNumber*(CALLCONV*)(UErrorCode * ec), UFormattedNumber*)                                                                                                                                                  \
    F(unumf_resultToString, int32_t(CALLCONV*)(const UFormattedNumber* uresult, UChar* buffer, int32_t bufferCapacity, UErrorCode* ec), int32_t)                                                                                           \
    F(unumrf_openForSkeletonWithCollapseAndIdentityFallback, UNumberRangeFormatter*(CALLCONV*)(const UChar*, int32_t, UNumberRangeCollapse, UNumberRangeIdentityFallback, const char*, UParseError*, UErrorCode*), UNumberRangeFormatter*) \
    F(unumrf_resultAsValue, const UFormattedValue*(CALLCONV*)(const UFormattedNumberRange* uresult, UErrorCode* ec), const UFormattedValue*)                                                                                               \
    F(unumrf_openResult, UFormattedNumberRange*(CALLCONV*)(UErrorCode * ec), UFormattedNumberRange*)                                                                                                                                       \
    F(ufieldpositer_open, UFieldPositionIterator*(CALLCONV*)(UErrorCode * status), UFieldPositionIterator*)                                                                                                                                \
    F(ufieldpositer_next, int32_t(CALLCONV*)(UFieldPositionIterator * fpositer, int32_t* beginIndex, int32_t* endIndex), int32_t)                                                                                                          \
    F(ucurr_getName, const UChar*(CALLCONV*)(const UChar* currency, const char* locale, UCurrNameStyle nameStyle, UBool* isChoiceFormat, int32_t* len, UErrorCode* ec), const UChar*)                                                      \
    F(ucurr_getDefaultFractionDigits, int32_t(CALLCONV*)(const UChar* currency, UErrorCode* ec), int32_t)                                                                                                                                  \
    F(ucurr_getDefaultFractionDigitsForUsage, int32_t(CALLCONV*)(const UChar* currency, const UCurrencyUsage usage, UErrorCode* ec), int32_t)                                                                                              \
    F(ucurr_openISOCurrencies, UEnumeration*(CALLCONV*)(uint32_t currType, UErrorCode* pErrorCode), UEnumeration*)                                                                                                                         \
    F(ucurr_getKeywordValuesForLocale, UEnumeration*(CALLCONV*)(const char* key, const char* locale, UBool commonlyUsed, UErrorCode* status), UEnumeration*)                                                                               \
    F(ureldatefmt_open, URelativeDateTimeFormatter*(CALLCONV*)(const char*, UNumberFormat*, UDateRelativeDateTimeFormatterStyle, UDisplayContext, UErrorCode*), URelativeDateTimeFormatter*)                                               \
    F(ureldatefmt_openResult, UFormattedRelativeDateTime*(CALLCONV*)(UErrorCode * ec), UFormattedRelativeDateTime*)                                                                                                                        \
    F(ureldatefmt_resultAsValue, const UFormattedValue*(CALLCONV*)(const UFormattedRelativeDateTime* ufrdt, UErrorCode* ec), const UFormattedValue*)                                                                                       \
    F(ureldatefmt_format, int32_t(CALLCONV*)(const URelativeDateTimeFormatter* reldatefmt, double, URelativeDateTimeUnit, UChar*, int32_t, UErrorCode*), int32_t)                                                                          \
    F(ureldatefmt_formatNumeric, int32_t(CALLCONV*)(const URelativeDateTimeFormatter* reldatefmt, double, URelativeDateTimeUnit, UChar*, int32_t, UErrorCode*), int32_t)                                                                   \
    F(ucfpos_open, UConstrainedFieldPosition*(CALLCONV*)(UErrorCode * ec), UConstrainedFieldPosition*)                                                                                                                                     \
    F(ucfpos_getCategory, int32_t(CALLCONV*)(const UConstrainedFieldPosition* ucfpos, UErrorCode* ec), int32_t)                                                                                                                            \
    F(ucfpos_getField, int32_t(CALLCONV*)(const UConstrainedFieldPosition* ucfpos, UErrorCode* ec), int32_t)                                                                                                                               \
    F(ufmtval_getString, const UChar*(CALLCONV*)(const UFormattedValue* ufmtval, int32_t* pLength, UErrorCode* ec), const UChar*)                                                                                                          \
    F(ufmtval_nextPosition, UBool(CALLCONV*)(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, UErrorCode* ec), UBool)                                                                                                    \
    F(ulistfmt_openForType, UListFormatter*(CALLCONV*)(const char* locale, UListFormatterType type, UListFormatterWidth width, UErrorCode* status), UListFormatter*)                                                                       \
    F(ulistfmt_format, int32_t(CALLCONV*)(const UListFormatter* listfmt, const UChar* const strings[], const int32_t*, int32_t, UChar*, int32_t, UErrorCode*), int32_t)                                                                    \
    F(ulistfmt_openResult, UFormattedList*(CALLCONV*)(UErrorCode * ec), UFormattedList*)                                                                                                                                                   \
    F(ulistfmt_resultAsValue, const UFormattedValue*(CALLCONV*)(const UFormattedList* uresult, UErrorCode* ec), const UFormattedValue*)                                                                                                    \
    F(udtitvfmt_open, UDateIntervalFormat*(CALLCONV*)(const char*, const UChar*, int32_t, const UChar*, int32_t, UErrorCode*), UDateIntervalFormat*)                                                                                       \
    F(udtitvfmt_openResult, UFormattedDateInterval*(CALLCONV*)(UErrorCode * ec), UFormattedDateInterval*)                                                                                                                                  \
    F(udtitvfmt_resultAsValue, const UFormattedValue*(CALLCONV*)(const UFormattedDateInterval*, UErrorCode* ec), const UFormattedValue*)

#define FOR_EACH_I18N_VOID_OP(F)                                                                                                                                            \
    F(udat_close, void(CALLCONV*)(UDateFormat * format), void)                                                                                                              \
    F(vzone_close, void(CALLCONV*)(VZone * zone), void)                                                                                                                     \
    F(uenum_close, void(CALLCONV*)(UEnumeration * zone), void)                                                                                                              \
    F(uenum_reset, void(CALLCONV*)(UEnumeration * en, UErrorCode * status), void)                                                                                           \
    F(ucol_setAttribute, void(CALLCONV*)(UCollator * coll, UColAttribute attr, UColAttributeValue value, UErrorCode * status), void)                                        \
    F(ucol_close, void(CALLCONV*)(UCollator * coll), void)                                                                                                                  \
    F(unumsys_close, void(CALLCONV*)(UNumberingSystem*), void)                                                                                                              \
    F(ucal_close, void(CALLCONV*)(UCalendar * cal), void)                                                                                                                   \
    F(ucal_setGregorianChange, void(CALLCONV*)(UCalendar * cal, UDate date, UErrorCode * pErrorCode), void)                                                                 \
    F(ucal_setMillis, void(CALLCONV*)(UCalendar * cal, UDate date, UErrorCode * status), void)                                                                              \
    F(udatpg_close, void(CALLCONV*)(UDateTimePatternGenerator * zone), void)                                                                                                \
    F(unum_close, void(CALLCONV*)(UNumberFormat*), void)                                                                                                                    \
    F(unum_setTextAttribute, void(CALLCONV*)(UNumberFormat * fmt, UNumberFormatTextAttribute tag, const UChar* newValue, int32_t newValueLength, UErrorCode* status), void) \
    F(unum_setAttribute, void(CALLCONV*)(UNumberFormat * fmt, UNumberFormatAttribute attr, int32_t newValue), void)                                                         \
    F(ubrk_setText, void(CALLCONV*)(UBreakIterator * bi, const UChar* text, int32_t textLength, UErrorCode* status), void)                                                  \
    F(ubrk_setUText, void(CALLCONV*)(UBreakIterator * bi, UText * text, UErrorCode * status), void)                                                                         \
    F(ubrk_close, void(CALLCONV*)(UBreakIterator * format), void)                                                                                                           \
    F(ucsdet_setText, void(CALLCONV*)(UCharsetDetector * ucsd, const char* textIn, int32_t len, UErrorCode* status), void)                                                  \
    F(ucsdet_close, void(CALLCONV*)(UCharsetDetector * ucsd), void)                                                                                                         \
    F(uplrules_close, void(CALLCONV*)(UPluralRules * uplrules), void)                                                                                                       \
    F(ures_close, void(CALLCONV*)(UResourceBundle * resourceBundle), void)                                                                                                  \
    F(ures_resetIterator, void(CALLCONV*)(UResourceBundle * resourceBundle), void)                                                                                          \
    F(unumf_formatInt, void(CALLCONV*)(const UNumberFormatter* uformatter, int64_t value, UFormattedNumber* uresult, UErrorCode* ec), void)                                 \
    F(unumf_formatDecimal, void(CALLCONV*)(const UNumberFormatter* uformatter, const char* value, int32_t valueLen, UFormattedNumber* uresult, UErrorCode* ec), void)       \
    F(unumf_formatDouble, void(CALLCONV*)(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult, UErrorCode* ec), void)                               \
    F(unumf_resultGetAllFieldPositions, void(CALLCONV*)(const UFormattedNumber* uresult, UFieldPositionIterator* ufpositer, UErrorCode* ec), void)                          \
    F(unumf_close, void(CALLCONV*)(UNumberFormatter * uformatter), void)                                                                                                    \
    F(unumf_closeResult, void(CALLCONV*)(UFormattedNumber * uresult), void)                                                                                                 \
    F(unumrf_formatDoubleRange, void(CALLCONV*)(const UNumberRangeFormatter*, double, double, UFormattedNumberRange*, UErrorCode*), void)                                   \
    F(unumrf_formatDecimalRange, void(CALLCONV*)(const UNumberRangeFormatter*, const char*, int32_t, const char*, int32_t, UFormattedNumberRange*, UErrorCode*), void)      \
    F(unumrf_close, void(CALLCONV*)(UNumberRangeFormatter*), void)                                                                                                          \
    F(unumrf_closeResult, void(CALLCONV*)(UFormattedNumberRange*), void)                                                                                                    \
    F(ufieldpositer_close, void(CALLCONV*)(UFieldPositionIterator * fpositer), void)                                                                                        \
    F(ureldatefmt_formatNumericToResult, void(CALLCONV*)(const URelativeDateTimeFormatter*, double, URelativeDateTimeUnit, UFormattedRelativeDateTime*, UErrorCode*), void) \
    F(ureldatefmt_formatToResult, void(CALLCONV*)(const URelativeDateTimeFormatter*, double, URelativeDateTimeUnit, UFormattedRelativeDateTime*, UErrorCode*), void)        \
    F(ureldatefmt_close, void(CALLCONV*)(URelativeDateTimeFormatter * reldatefmt), void)                                                                                    \
    F(ureldatefmt_closeResult, void(CALLCONV*)(UFormattedRelativeDateTime * ufrdt), void)                                                                                   \
    F(ucfpos_reset, void(CALLCONV*)(UConstrainedFieldPosition * ucfpos, UErrorCode * ec), void)                                                                             \
    F(ucfpos_close, void(CALLCONV*)(UConstrainedFieldPosition * ucfpos), void)                                                                                              \
    F(ucfpos_constrainCategory, void(CALLCONV*)(UConstrainedFieldPosition * ucfpos, int32_t category, UErrorCode* ec), void)                                                \
    F(ucfpos_constrainField, void(CALLCONV*)(UConstrainedFieldPosition * ucfpos, int32_t category, int32_t field, UErrorCode* ec), void)                                    \
    F(ucfpos_getIndexes, void(CALLCONV*)(const UConstrainedFieldPosition* ucfpos, int32_t* pStart, int32_t* pLimit, UErrorCode* ec), void)                                  \
    F(ulistfmt_close, void(CALLCONV*)(UListFormatter * listfmt), void)                                                                                                      \
    F(ulistfmt_formatStringsToResult, void(CALLCONV*)(const UListFormatter*, const UChar* const[], const int32_t*, int32_t, UFormattedList*, UErrorCode*), void)            \
    F(ulistfmt_closeResult, void(CALLCONV*)(UFormattedList * uresult), void)                                                                                                \
    F(udtitvfmt_close, void(CALLCONV*)(UDateIntervalFormat * formatter), void)                                                                                              \
    F(udtitvfmt_closeResult, void(CALLCONV*)(UFormattedDateInterval * uresult), void)                                                                                       \
    F(udtitvfmt_formatToResult, void(CALLCONV*)(const UDateIntervalFormat*, UDate, UDate, UFormattedDateInterval*, UErrorCode*), void)                                      \
    F(udtitvfmt_formatCalendarToResult, void(CALLCONV*)(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval*, UErrorCode*), void)

#define FOR_EACH_I18N_STICKY_OP(F) \
    F(vzone_getOffset3)

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
        {
            std::lock_guard<std::mutex> guard(m_dataMutex);
            ensureLoadSo(Soname::i18n);
            if (!m_functions[functionvzone_getOffset3]) {
                loadFunction(Soname::i18n, functionvzone_getOffset3);
            }
        }

        typedef void(CALLCONV * FP)(VZone * zone, UDate date, UBool local, int32_t& rawOffset,
                                    int32_t& dstOffset, UErrorCode& ec);
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
        {
            std::lock_guard<std::mutex> guard(m_dataMutex);
            ensureLoadSo(soname);
            if (!m_functions[kind]) {
                loadFunction(soname, kind);
            }
        }

        FunctionPrototype fp = (FunctionPrototype)m_functions[kind];

        return (*fp)(args...);
    }

    template <Soname soname, typename FunctionPrototype, typename... Args>
    void invokeICUWithoutReturn(Function kind, Args... args)
    {
        {
            ensureLoadSo(soname);
            std::lock_guard<std::mutex> guard(m_dataMutex);
            if (!m_functions[kind]) {
                loadFunction(soname, kind);
            }
        }

        FunctionPrototype fp = (FunctionPrototype)m_functions[kind];

        (*fp)(args...);
    }

    void* m_soHandles[SonameMax];
    void* m_functions[FunctionMax];
    int m_icuVersion;
    std::mutex m_dataMutex;
};
} // namespace RuntimeICUBinder

#undef CALLCONV

#endif
