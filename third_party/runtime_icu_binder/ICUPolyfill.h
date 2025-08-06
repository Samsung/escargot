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

#ifndef __ICUPolyfill__
#define __ICUPolyfill__

#include "RuntimeICUBinder.h"

#define u_getVersion RuntimeICUBinder::ICU::instance().u_getVersion

#define u_tolower RuntimeICUBinder::ICU::instance().u_tolower
#define u_toupper RuntimeICUBinder::ICU::instance().u_toupper
#define u_islower RuntimeICUBinder::ICU::instance().u_islower
#define u_isupper RuntimeICUBinder::ICU::instance().u_isupper
#define u_totitle RuntimeICUBinder::ICU::instance().u_totitle
#define u_charMirror RuntimeICUBinder::ICU::instance().u_charMirror
#define u_countChar32 RuntimeICUBinder::ICU::instance().u_countChar32
#define u_strToLower RuntimeICUBinder::ICU::instance().u_strToLower
#define u_strToUpper RuntimeICUBinder::ICU::instance().u_strToUpper
#define uloc_getDefault RuntimeICUBinder::ICU::instance().uloc_getDefault
#define uloc_getName RuntimeICUBinder::ICU::instance().uloc_getName
#define uloc_canonicalize RuntimeICUBinder::ICU::instance().uloc_canonicalize
#define uloc_getBaseName RuntimeICUBinder::ICU::instance().uloc_getBaseName
#define uloc_forLanguageTag RuntimeICUBinder::ICU::instance().uloc_forLanguageTag
#define uloc_getLanguage RuntimeICUBinder::ICU::instance().uloc_getLanguage
#define uloc_getScript RuntimeICUBinder::ICU::instance().uloc_getScript
#define uloc_getCountry RuntimeICUBinder::ICU::instance().uloc_getCountry
#define uloc_getVariant RuntimeICUBinder::ICU::instance().uloc_getVariant
#define uloc_toLegacyKey RuntimeICUBinder::ICU::instance().uloc_toLegacyKey
#define uloc_toLegacyType RuntimeICUBinder::ICU::instance().uloc_toLegacyType
#define uloc_toLanguageTag RuntimeICUBinder::ICU::instance().uloc_toLanguageTag
#define uloc_addLikelySubtags RuntimeICUBinder::ICU::instance().uloc_addLikelySubtags
#define uloc_minimizeSubtags RuntimeICUBinder::ICU::instance().uloc_minimizeSubtags
#define uloc_getCharacterOrientation RuntimeICUBinder::ICU::instance().uloc_getCharacterOrientation
#define uloc_countAvailable RuntimeICUBinder::ICU::instance().uloc_countAvailable
#define uloc_getAvailable RuntimeICUBinder::ICU::instance().uloc_getAvailable
#define uloc_toUnicodeLocaleType RuntimeICUBinder::ICU::instance().uloc_toUnicodeLocaleType
#define uloc_setKeywordValue RuntimeICUBinder::ICU::instance().uloc_setKeywordValue
#define uloc_getKeywordValue RuntimeICUBinder::ICU::instance().uloc_getKeywordValue

#define u_getIntPropertyValue RuntimeICUBinder::ICU::instance().u_getIntPropertyValue
#define u_getIntPropertyMaxValue RuntimeICUBinder::ICU::instance().u_getIntPropertyMaxValue
#define u_getIntPropertyMinValue RuntimeICUBinder::ICU::instance().u_getIntPropertyMinValue
#define u_charType RuntimeICUBinder::ICU::instance().u_charType
#define u_getCombiningClass RuntimeICUBinder::ICU::instance().u_getCombiningClass
#define u_charDirection RuntimeICUBinder::ICU::instance().u_charDirection
#define u_isblank RuntimeICUBinder::ICU::instance().u_isblank

#define unorm2_getNFCInstance RuntimeICUBinder::ICU::instance().unorm2_getNFCInstance
#define unorm2_getNFDInstance RuntimeICUBinder::ICU::instance().unorm2_getNFDInstance
#define unorm2_getNFKCInstance RuntimeICUBinder::ICU::instance().unorm2_getNFKCInstance
#define unorm2_getNFKDInstance RuntimeICUBinder::ICU::instance().unorm2_getNFKDInstance
#define unorm2_normalize RuntimeICUBinder::ICU::instance().unorm2_normalize
#define unorm2_composePair RuntimeICUBinder::ICU::instance().unorm2_composePair
#define unorm2_getRawDecomposition RuntimeICUBinder::ICU::instance().unorm2_getRawDecomposition

#define vzone_openID RuntimeICUBinder::ICU::instance().vzone_openID
#define vzone_getTZURL RuntimeICUBinder::ICU::instance().vzone_getTZURL
#define vzone_getRawOffset RuntimeICUBinder::ICU::instance().vzone_getRawOffset
#define vzone_getOffset3 RuntimeICUBinder::ICU::instance().vzone_getOffset3
#define vzone_close RuntimeICUBinder::ICU::instance().vzone_close

#define ucol_countAvailable RuntimeICUBinder::ICU::instance().ucol_countAvailable
#define ucol_getAvailable RuntimeICUBinder::ICU::instance().ucol_getAvailable
#define ucol_setAttribute RuntimeICUBinder::ICU::instance().ucol_setAttribute
#define ucol_close RuntimeICUBinder::ICU::instance().ucol_close
#define ucol_getKeywordValues RuntimeICUBinder::ICU::instance().ucol_getKeywordValues
#define ucol_getKeywordValuesForLocale RuntimeICUBinder::ICU::instance().ucol_getKeywordValuesForLocale
#define ucol_getAttribute RuntimeICUBinder::ICU::instance().ucol_getAttribute
#define ucol_open RuntimeICUBinder::ICU::instance().ucol_open
#define ucol_strcollIter RuntimeICUBinder::ICU::instance().ucol_strcollIter
#define ucol_strcoll RuntimeICUBinder::ICU::instance().ucol_strcoll

#define unum_countAvailable RuntimeICUBinder::ICU::instance().unum_countAvailable
#define unum_getAvailable RuntimeICUBinder::ICU::instance().unum_getAvailable
#define unum_setTextAttribute RuntimeICUBinder::ICU::instance().unum_setTextAttribute
#define unum_setAttribute RuntimeICUBinder::ICU::instance().unum_setAttribute
#define unum_open RuntimeICUBinder::ICU::instance().unum_open
#define unum_formatDouble RuntimeICUBinder::ICU::instance().unum_formatDouble
#define unum_formatDoubleForFields RuntimeICUBinder::ICU::instance().unum_formatDoubleForFields
#define unum_close RuntimeICUBinder::ICU::instance().unum_close
#define unum_clone RuntimeICUBinder::ICU::instance().unum_clone

#define udat_getAvailable RuntimeICUBinder::ICU::instance().udat_getAvailable
#define udat_countAvailable RuntimeICUBinder::ICU::instance().udat_countAvailable
#define udat_open RuntimeICUBinder::ICU::instance().udat_open
#define udat_format RuntimeICUBinder::ICU::instance().udat_format
#define udat_formatForFields RuntimeICUBinder::ICU::instance().udat_formatForFields
#define udat_close RuntimeICUBinder::ICU::instance().udat_close
#define udat_getCalendar RuntimeICUBinder::ICU::instance().udat_getCalendar
#define udat_toPattern RuntimeICUBinder::ICU::instance().udat_toPattern
#define udat_parse RuntimeICUBinder::ICU::instance().udat_parse

#define uenum_count RuntimeICUBinder::ICU::instance().uenum_count
#define uenum_unext RuntimeICUBinder::ICU::instance().uenum_unext
#define uenum_next RuntimeICUBinder::ICU::instance().uenum_next
#define uenum_reset RuntimeICUBinder::ICU::instance().uenum_reset
#define uenum_close RuntimeICUBinder::ICU::instance().uenum_close

#define unumsys_openAvailableNames RuntimeICUBinder::ICU::instance().unumsys_openAvailableNames
#define unumsys_openByName RuntimeICUBinder::ICU::instance().unumsys_openByName
#define unumsys_close RuntimeICUBinder::ICU::instance().unumsys_close
#define unumsys_isAlgorithmic RuntimeICUBinder::ICU::instance().unumsys_isAlgorithmic
#define unumsys_getName RuntimeICUBinder::ICU::instance().unumsys_getName
#define unumsys_open RuntimeICUBinder::ICU::instance().unumsys_open

#define uiter_setString RuntimeICUBinder::ICU::instance().uiter_setString

#define ucal_open RuntimeICUBinder::ICU::instance().ucal_open
#define ucal_close RuntimeICUBinder::ICU::instance().ucal_close
#define ucal_getDefaultTimeZone RuntimeICUBinder::ICU::instance().ucal_getDefaultTimeZone
#define ucal_getTimeZoneDisplayName RuntimeICUBinder::ICU::instance().ucal_getTimeZoneDisplayName
#define ucal_getKeywordValuesForLocale RuntimeICUBinder::ICU::instance().ucal_getKeywordValuesForLocale
#define ucal_openTimeZoneIDEnumeration RuntimeICUBinder::ICU::instance().ucal_openTimeZoneIDEnumeration
#define ucal_openTimeZones RuntimeICUBinder::ICU::instance().ucal_openTimeZones
#define ucal_getCanonicalTimeZoneID RuntimeICUBinder::ICU::instance().ucal_getCanonicalTimeZoneID
#define ucal_get RuntimeICUBinder::ICU::instance().ucal_get
#define ucal_getType RuntimeICUBinder::ICU::instance().ucal_getType
#define ucal_getAttribute RuntimeICUBinder::ICU::instance().ucal_getAttribute
#define ucal_getDayOfWeekType RuntimeICUBinder::ICU::instance().ucal_getDayOfWeekType
#define ucal_setGregorianChange RuntimeICUBinder::ICU::instance().ucal_setGregorianChange
#define ucal_setMillis RuntimeICUBinder::ICU::instance().ucal_setMillis
#define ucal_clone RuntimeICUBinder::ICU::instance().ucal_clone

#define udatpg_close RuntimeICUBinder::ICU::instance().udatpg_close
#define udatpg_open RuntimeICUBinder::ICU::instance().udatpg_open
#define udatpg_getBestPattern RuntimeICUBinder::ICU::instance().udatpg_getBestPattern
#define udatpg_getBestPatternWithOptions RuntimeICUBinder::ICU::instance().udatpg_getBestPatternWithOptions
#define udatpg_getSkeleton RuntimeICUBinder::ICU::instance().udatpg_getSkeleton
#define udatpg_getFieldDisplayName RuntimeICUBinder::ICU::instance().udatpg_getFieldDisplayName

#define ubrk_open RuntimeICUBinder::ICU::instance().ubrk_open
#define ubrk_openRules RuntimeICUBinder::ICU::instance().ubrk_openRules
#define ubrk_next RuntimeICUBinder::ICU::instance().ubrk_next
#define ubrk_setText RuntimeICUBinder::ICU::instance().ubrk_setText
#define ubrk_setUText RuntimeICUBinder::ICU::instance().ubrk_setUText
#define ubrk_close RuntimeICUBinder::ICU::instance().ubrk_close
#define ubrk_clone RuntimeICUBinder::ICU::instance().ubrk_clone
#define ubrk_preceding RuntimeICUBinder::ICU::instance().ubrk_preceding
#define ubrk_following RuntimeICUBinder::ICU::instance().ubrk_following
#define ubrk_getRuleStatus RuntimeICUBinder::ICU::instance().ubrk_getRuleStatus
#define ubrk_first RuntimeICUBinder::ICU::instance().ubrk_first
#define ubrk_current RuntimeICUBinder::ICU::instance().ubrk_current

#define ucnv_open RuntimeICUBinder::ICU::instance().ucnv_open
#define ucnv_compareNames RuntimeICUBinder::ICU::instance().ucnv_compareNames
#define ucnv_close RuntimeICUBinder::ICU::instance().ucnv_close
#define ucnv_toUnicode RuntimeICUBinder::ICU::instance().ucnv_toUnicode
#define ucnv_fromUnicode RuntimeICUBinder::ICU::instance().ucnv_fromUnicode
#define ucnv_getDisplayName RuntimeICUBinder::ICU::instance().ucnv_getDisplayName
#define ucnv_getName RuntimeICUBinder::ICU::instance().ucnv_getName

#define ucsdet_open RuntimeICUBinder::ICU::instance().ucsdet_open
#define ucsdet_detectAll RuntimeICUBinder::ICU::instance().ucsdet_detectAll
#define ucsdet_detect RuntimeICUBinder::ICU::instance().ucsdet_detect
#define ucsdet_getName RuntimeICUBinder::ICU::instance().ucsdet_getName
#define ucsdet_getConfidence RuntimeICUBinder::ICU::instance().ucsdet_getConfidence
#define ucsdet_setText RuntimeICUBinder::ICU::instance().ucsdet_setText
#define ucsdet_close RuntimeICUBinder::ICU::instance().ucsdet_close

#define ubidi_open RuntimeICUBinder::ICU::instance().ubidi_open
#define ubidi_getBaseDirection RuntimeICUBinder::ICU::instance().ubidi_getBaseDirection
#define ubidi_countRuns RuntimeICUBinder::ICU::instance().ubidi_countRuns
#define ubidi_close RuntimeICUBinder::ICU::instance().ubidi_close
#define ubidi_setPara RuntimeICUBinder::ICU::instance().ubidi_setPara
#define ubidi_getLogicalRun RuntimeICUBinder::ICU::instance().ubidi_getLogicalRun

#define ublock_getCode RuntimeICUBinder::ICU::instance().ublock_getCode

#define uscript_getScript RuntimeICUBinder::ICU::instance().uscript_getScript
#define uscript_hasScript RuntimeICUBinder::ICU::instance().uscript_hasScript
#define uscript_getShortName RuntimeICUBinder::ICU::instance().uscript_getShortName

#define uplrules_open RuntimeICUBinder::ICU::instance().uplrules_open
#define uplrules_openForType RuntimeICUBinder::ICU::instance().uplrules_openForType
#define uplrules_close RuntimeICUBinder::ICU::instance().uplrules_close
#define uplrules_select RuntimeICUBinder::ICU::instance().uplrules_select
#define uplrules_getKeywords RuntimeICUBinder::ICU::instance().uplrules_getKeywords
#define uplrules_selectWithFormat RuntimeICUBinder::ICU::instance().uplrules_selectWithFormat
#define uplrules_selectFormatted RuntimeICUBinder::ICU::instance().uplrules_selectFormatted
#define uplrules_selectForRange RuntimeICUBinder::ICU::instance().uplrules_selectForRange

#define ures_open RuntimeICUBinder::ICU::instance().ures_open
#define ures_openDirect RuntimeICUBinder::ICU::instance().ures_openDirect
#define ures_getByKey RuntimeICUBinder::ICU::instance().ures_getByKey
#define ures_close RuntimeICUBinder::ICU::instance().ures_close
#define ures_getKey RuntimeICUBinder::ICU::instance().ures_getKey
#define ures_getNextResource RuntimeICUBinder::ICU::instance().ures_getNextResource
#define ures_hasNext RuntimeICUBinder::ICU::instance().ures_hasNext
#define ures_resetIterator RuntimeICUBinder::ICU::instance().ures_resetIterator
#define ures_getSize RuntimeICUBinder::ICU::instance().ures_getSize
#define ures_getStringByIndex RuntimeICUBinder::ICU::instance().ures_getStringByIndex
#define ures_getStringByKey RuntimeICUBinder::ICU::instance().ures_getStringByKey

#define unumf_openForSkeletonAndLocale RuntimeICUBinder::ICU::instance().unumf_openForSkeletonAndLocale
#define unumf_openForSkeletonAndLocaleWithError RuntimeICUBinder::ICU::instance().unumf_openForSkeletonAndLocaleWithError
#define unumf_openResult RuntimeICUBinder::ICU::instance().unumf_openResult
#define unumf_formatInt RuntimeICUBinder::ICU::instance().unumf_formatInt
#define unumf_formatDecimal RuntimeICUBinder::ICU::instance().unumf_formatDecimal
#define unumf_formatDouble RuntimeICUBinder::ICU::instance().unumf_formatDouble
#define unumf_resultGetAllFieldPositions RuntimeICUBinder::ICU::instance().unumf_resultGetAllFieldPositions
#define unumf_resultToString RuntimeICUBinder::ICU::instance().unumf_resultToString
#define unumf_close RuntimeICUBinder::ICU::instance().unumf_close
#define unumf_closeResult RuntimeICUBinder::ICU::instance().unumf_closeResult

#define unumrf_openForSkeletonWithCollapseAndIdentityFallback RuntimeICUBinder::ICU::instance().unumrf_openForSkeletonWithCollapseAndIdentityFallback
#define unumrf_close RuntimeICUBinder::ICU::instance().unumrf_close
#define unumrf_resultAsValue RuntimeICUBinder::ICU::instance().unumrf_resultAsValue
#define unumrf_formatDoubleRange RuntimeICUBinder::ICU::instance().unumrf_formatDoubleRange
#define unumrf_formatDecimalRange RuntimeICUBinder::ICU::instance().unumrf_formatDecimalRange
#define unumrf_openResult RuntimeICUBinder::ICU::instance().unumrf_openResult
#define unumrf_closeResult RuntimeICUBinder::ICU::instance().unumrf_closeResult

#define ufieldpositer_open RuntimeICUBinder::ICU::instance().ufieldpositer_open
#define ufieldpositer_next RuntimeICUBinder::ICU::instance().ufieldpositer_next
#define ufieldpositer_close RuntimeICUBinder::ICU::instance().ufieldpositer_close

#define ucurr_getName RuntimeICUBinder::ICU::instance().ucurr_getName
#define ucurr_getDefaultFractionDigits RuntimeICUBinder::ICU::instance().ucurr_getDefaultFractionDigits
#define ucurr_getDefaultFractionDigitsForUsage RuntimeICUBinder::ICU::instance().ucurr_getDefaultFractionDigitsForUsage
#define ucurr_openISOCurrencies RuntimeICUBinder::ICU::instance().ucurr_openISOCurrencies
#define ucurr_getKeywordValuesForLocale RuntimeICUBinder::ICU::instance().ucurr_getKeywordValuesForLocale

#define ureldatefmt_open RuntimeICUBinder::ICU::instance().ureldatefmt_open
#define ureldatefmt_openResult RuntimeICUBinder::ICU::instance().ureldatefmt_openResult
#define ureldatefmt_resultAsValue RuntimeICUBinder::ICU::instance().ureldatefmt_resultAsValue
#define ureldatefmt_format RuntimeICUBinder::ICU::instance().ureldatefmt_format
#define ureldatefmt_formatNumeric RuntimeICUBinder::ICU::instance().ureldatefmt_formatNumeric
#define ureldatefmt_formatNumericToResult RuntimeICUBinder::ICU::instance().ureldatefmt_formatNumericToResult
#define ureldatefmt_formatToResult RuntimeICUBinder::ICU::instance().ureldatefmt_formatToResult
#define ureldatefmt_close RuntimeICUBinder::ICU::instance().ureldatefmt_close
#define ureldatefmt_closeResult RuntimeICUBinder::ICU::instance().ureldatefmt_closeResult

#define ucfpos_open RuntimeICUBinder::ICU::instance().ucfpos_open
#define ucfpos_getCategory RuntimeICUBinder::ICU::instance().ucfpos_getCategory
#define ucfpos_getField RuntimeICUBinder::ICU::instance().ucfpos_getField
#define ucfpos_reset RuntimeICUBinder::ICU::instance().ucfpos_reset
#define ucfpos_close RuntimeICUBinder::ICU::instance().ucfpos_close
#define ucfpos_constrainCategory RuntimeICUBinder::ICU::instance().ucfpos_constrainCategory
#define ucfpos_constrainField RuntimeICUBinder::ICU::instance().ucfpos_constrainField
#define ucfpos_getIndexes RuntimeICUBinder::ICU::instance().ucfpos_getIndexes

#define ufmtval_getString RuntimeICUBinder::ICU::instance().ufmtval_getString
#define ufmtval_nextPosition RuntimeICUBinder::ICU::instance().ufmtval_nextPosition

#define uldn_open RuntimeICUBinder::ICU::instance().uldn_open
#define uldn_openForContext RuntimeICUBinder::ICU::instance().uldn_openForContext
#define uldn_localeDisplayName RuntimeICUBinder::ICU::instance().uldn_localeDisplayName
#define uldn_regionDisplayName RuntimeICUBinder::ICU::instance().uldn_regionDisplayName
#define uldn_languageDisplayName RuntimeICUBinder::ICU::instance().uldn_languageDisplayName
#define uldn_scriptDisplayName RuntimeICUBinder::ICU::instance().uldn_scriptDisplayName
#define uldn_keyValueDisplayName RuntimeICUBinder::ICU::instance().uldn_keyValueDisplayName
#define uldn_close RuntimeICUBinder::ICU::instance().uldn_close

#define ulistfmt_openForType RuntimeICUBinder::ICU::instance().ulistfmt_openForType
#define ulistfmt_close RuntimeICUBinder::ICU::instance().ulistfmt_close
#define ulistfmt_format RuntimeICUBinder::ICU::instance().ulistfmt_format
#define ulistfmt_formatStringsToResult RuntimeICUBinder::ICU::instance().ulistfmt_formatStringsToResult
#define ulistfmt_openResult RuntimeICUBinder::ICU::instance().ulistfmt_openResult
#define ulistfmt_resultAsValue RuntimeICUBinder::ICU::instance().ulistfmt_resultAsValue
#define ulistfmt_closeResult RuntimeICUBinder::ICU::instance().ulistfmt_closeResult

#define udtitvfmt_open RuntimeICUBinder::ICU::instance().udtitvfmt_open
#define udtitvfmt_openResult RuntimeICUBinder::ICU::instance().udtitvfmt_openResult
#define udtitvfmt_close RuntimeICUBinder::ICU::instance().udtitvfmt_close
#define udtitvfmt_closeResult RuntimeICUBinder::ICU::instance().udtitvfmt_closeResult
#define udtitvfmt_formatToResult RuntimeICUBinder::ICU::instance().udtitvfmt_formatToResult
#define udtitvfmt_formatCalendarToResult RuntimeICUBinder::ICU::instance().udtitvfmt_formatCalendarToResult
#define udtitvfmt_resultAsValue RuntimeICUBinder::ICU::instance().udtitvfmt_resultAsValue

#endif
