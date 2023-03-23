#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlDisplayNames.h"

#if defined(ENABLE_INTL_DISPLAYNAMES)

namespace Escargot {

IntlDisplayNamesObject::IntlDisplayNamesObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : DerivedObject(state, proto)
    , m_style(nullptr)
    , m_type(nullptr)
    , m_fallback(nullptr)
    , m_locale(nullptr)
    , m_languageDisplay(nullptr)
    , m_icuLocaleDisplayNames(nullptr)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 62) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.DisplayNames needs 61+ version of ICU");
    }
#endif
    // https://402.ecma-international.org/8.0/#sec-Intl.DisplayNames
    // + https://tc39.es/intl-displaynames-v2/
    auto& staticStrings = state.context()->staticStrings();

    // Let displayNames be ? OrdinaryCreateFromConstructor(NewTarget, "%DisplayNames.prototype%", « [[InitializedDisplayNames]], [[Locale]], [[Style]], [[Type]], [[Fallback]], [[Fields]] »).
    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // If options is undefined, throw a TypeError exception.
    // Let options be ? GetOptionsObject(options).
    if (!options.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "options must be object");
    }
    Object* optionsObject = options.asObject();
    // Let opt be a new Record.
    StringMap opt;
    // Let localeData be %DisplayNames%.[[LocaleData]].
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    Value matcherValues[2] = { staticStrings.lazyLookup().string(), staticStrings.lazyBestFit().string() };
    Value matcher = Intl::getOption(state, optionsObject, staticStrings.lazyLocaleMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);
    // Set opt.[[localeMatcher]] to matcher.
    opt.insert(std::make_pair("localeMatcher", matcher.asString()));
    // Let r be ResolveLocale(%DisplayNames%.[[AvailableLocales]], requestedLocales, opt, %DisplayNames%.[[RelevantExtensionKeys]]).
    const auto& availableLocales = state.context()->vmInstance()->intlDisplayNamesAvailableLocales();
    StringMap r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, nullptr, 0, nullptr);

    // Let style be ? GetOption(options, "style", "string", « "narrow", "short", "long" », "long").
    Value styleValues[] = { staticStrings.lazyNarrow().string(), staticStrings.lazyShort().string(), staticStrings.lazyLong().string() };
    Value style = Intl::getOption(state, optionsObject, staticStrings.lazyStyle().string(), Intl::StringValue, styleValues, 3, styleValues[2]);
    // Set displayNames.[[Style]] to style.
    m_style = style.asString();
    // Let type be ? GetOption(options, "type", "string", « "language", "region", "script", "currency",  "calendar", "dateTimeField" », undefined).
    Value typeValues[] = { staticStrings.language.string(), staticStrings.region.string(),
                           staticStrings.script.string(), staticStrings.lazyCurrency().string(), staticStrings.calendar.string(), staticStrings.lazyDateTimeField().string() };
    Value type = Intl::getOption(state, optionsObject, staticStrings.lazyType().string(), Intl::StringValue, typeValues, 6, Value());
    // If type is undefined, throw a TypeError exception.
    if (type.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "type of options is required");
    }
    // Set displayNames.[[Type]] to type.
    m_type = type.asString();
    // Let fallback be ? GetOption(options, "fallback", "string", « "code", "none" », "code").
    Value fallbackValues[2] = { staticStrings.lazyCode().string(), staticStrings.lazyNone().string() };
    Value fallback = Intl::getOption(state, optionsObject, staticStrings.lazyFallback().string(), Intl::StringValue, fallbackValues, 2, fallbackValues[0]);
    // Set displayNames.[[Fallback]] to fallback.
    m_fallback = fallback.asString();
    // Set displayNames.[[Locale]] to the value of r.[[Locale]].
    m_locale = r.at("locale");
    // Let dataLocale be r.[[dataLocale]].
    // Let dataLocaleData be localeData.[[<dataLocale>]].
    // Let types be dataLocaleData.[[types]].
    // Assert: types is a Record (see 12.3.3).
    // Let typeFields be types.[[<type>]].
    // Assert: typeFields is a Record (see 12.3.3).
    // Let languageDisplay be ? GetOption(options, "languageDisplay", "string", « "dialect", "standard" », "dialect").
    Value languageDisplayValues[] = { staticStrings.lazyDialect().string(), staticStrings.lazyStandard().string() };
    Value languageDisplay = Intl::getOption(state, optionsObject, staticStrings.lazyLanguageDisplay().string(), Intl::StringValue,
                                            languageDisplayValues, 2, languageDisplayValues[0]);
    // If type is "language", then
    // Set displayNames.[[LanguageDisplay]] to languageDisplay.
    m_languageDisplay = languageDisplay.asString();
    // Let typeFields be typeFields.[[<languageDisplay>]].
    // Assert: typeFields is a Record (see 1.4.3).

    // Let styleFields be typeFields.[[<style>]].
    // Assert: styleFields is a Record (see 12.3.3).
    // Set displayNames.[[Fields]] to styleFields.
    // Return displayNames.
    UDisplayContext contexts[] = {
        (m_type->equals("language") && m_languageDisplay->equals("standard")) ? UDISPCTX_STANDARD_NAMES : UDISPCTX_DIALECT_NAMES,
        UDISPCTX_CAPITALIZATION_FOR_STANDALONE,
        m_style->equals("long") ? UDISPCTX_LENGTH_FULL : UDISPCTX_LENGTH_SHORT,
        UDISPCTX_NO_SUBSTITUTE,
    };

    UErrorCode status = U_ZERO_ERROR;
    m_icuLocaleDisplayNames = uldn_openForContext(m_locale->toNonGCUTF8StringData().data(), contexts, 4, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize DisplayNames");
        return;
    }

    addFinalizer([](Object* obj, void* data) {
        IntlDisplayNamesObject* self = (IntlDisplayNamesObject*)obj;
        uldn_close(self->m_icuLocaleDisplayNames);
    },
                 nullptr);
}

// https://tc39.es/intl-displaynames-v2/#sec-isvaliddatetimefieldcode
static bool isValidDatetimeFieldCode(String* code)
{
#define TEST_ONCE(name) \
    else if (code->equals(#name)) { return true; }
    if (false) {}
    TEST_ONCE(era)
    TEST_ONCE(year)
    TEST_ONCE(quarter)
    TEST_ONCE(month)
    TEST_ONCE(weekOfYear)
    TEST_ONCE(weekday)
    TEST_ONCE(day)
    TEST_ONCE(dayPeriod)
    TEST_ONCE(hour)
    TEST_ONCE(minute)
    TEST_ONCE(second)
    TEST_ONCE(timeZoneName)
#undef TEST_ONCE

    return false;
}

// https://tc39.es/proposal-intl-displaynames/#sec-canonicalcodefordisplaynames
// + https://tc39.es/intl-displaynames-v2/#sec-canonicalcodefordisplaynames
static String* canonicalCodeForDisplayNames(ExecutionState& state, String* type, String* code)
{
    if (type->equals("language")) {
        // If type is "language", then
        // a. If code does not matches the unicode_language_id production, throw a RangeError exceptipon.
        // b. If IsStructurallyValidLanguageTag(code) is false, throw a RangeError exception.
        // c. Set code to CanonicalizeUnicodeLocaleId(code).
        // d. Return code.
        auto parsedResult = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(code->toNonGCUTF8StringData().data());
        if (!parsedResult.canonicalizedTag) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid language code");
        }
        return parsedResult.canonicalizedTag.value();
    } else if (type->equals("region")) {
        // 2. If type is "region", then
        // a. If code does not matches the unicode_region_subtag production, throw a RangeError exceptipon.
        // b. Let code be the result of mapping code to upper case as described in 6.1.
        // c. Return code.

        // unicode_region_subtag = alpha{2} | digit{3} ;
        bool isValid = false;
        if (code->length() == 2 && code->isAllSpecialCharacters(isASCIIAlpha)) {
            isValid = true;
        } else if (code->length() == 3 && code->isAllSpecialCharacters(isASCIIDigit)) {
            isValid = true;
        }
        if (!isValid) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid region code");
        }

        std::string s = code->toNonGCUTF8StringData();
        for (size_t i = 0; i < s.length(); i++) {
            s[i] = toupper(s[i]);
        }
        return String::fromUTF8(s.data(), s.length());
    } else if (type->equals("script")) {
        // 3. If type is "script", then
        // a. If code does not matches the unicode_script_subtag production, throw a RangeError exceptipon.
        // b. Let code be the result of mapping the first character in code to upper case, and mapping the second, third and fourth character in code to lower case, as described in 6.1.
        // c. Return code.

        // unicode_script_subtag = alpha{4} ;
        if (code->length() != 4 || !code->isAllSpecialCharacters(isASCIIAlphanumeric)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid script code");
        }
        std::string s = code->toNonGCUTF8StringData();
        ASSERT(s.length() == 4);
        s[0] = toupper(s[0]);
        for (size_t i = 1; i < 4; i++) {
            s[i] = tolower(s[i]);
        }
        return String::fromUTF8(s.data(), s.length());
    } else if (type->equals("calendar")) {
        // 4. If type is "calendar", then
        // a. If code does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        // b. Let code be the result of mapping code to lower case as described in 6.1.
        // c. Return code.

        if (code->equals("gregory")) {
            code = String::fromASCII("gregorian", sizeof("gregorian") - 1);
        } else if (code->equals("islamicc")) {
            code = String::fromASCII("islamic-civil", sizeof("islamic-civil") - 1);
        } else if (code->equals("ethioaa")) {
            code = String::fromASCII("ethiopic-amete-alem", sizeof("ethiopic-amete-alem") - 1);
        }

        if (!Intl::isValidUnicodeLocaleIdentifier(code)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar code");
        }

        std::string s = code->toNonGCUTF8StringData();
        for (size_t i = 0; i < s.length(); i++) {
            s[i] = tolower(s[i]);
        }

        return String::fromUTF8(s.data(), s.length());
    } else if (type->equals("dateTimeField")) {
        // 5. If type is "dateTimeField", then
        // a. If the result of IsValidDateTimeFieldCode(code) is false, throw a RangeError exception.
        // b. Return code.
        if (!isValidDatetimeFieldCode(code)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid dateTimeField code");
        }
        return code;
    } else {
        // 6. Assert: type is "currency".
        ASSERT(type->equals("currency"));
        // 7. If ! IsWellFormedCurrencyCode(code) is false, throw a RangeError exceptipon.
        // 8. Let code be the result of mapping code to upper case as described in 6.1.
        // 9. Return code.
        std::string s = code->toNonGCUTF8StringData();
        if (s.length() != 3 || !isAllSpecialCharacters(s, isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid dateTimeField code");
        }

        for (size_t i = 0; i < s.length(); i++) {
            s[i] = toupper(s[i]);
        }
        return String::fromUTF8(s.data(), s.length());
    }
}

// https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.prototype.of
// + https://tc39.es/intl-displaynames-v2/
Value IntlDisplayNamesObject::of(ExecutionState& state, const Value& codeInput)
{
    // Let displayNames be this value.
    // If Type(displayNames) is not Object, throw a TypeError exception.
    // If displayNames does not have an [[InitializedDisplayNames]] internal slot, throw a TypeError exception.
    // Let code be ? ToString(code).
    String* code = codeInput.toString(state);
    // Let code be ? CanonicalCodeForDisplayNames(displayNames.[[Type]], code).
    code = canonicalCodeForDisplayNames(state, m_type, code);
    // Let fields be displayNames.[[Fields]].
    // Let name be fields[[<code>]].
    // If name is not undefined, return name.
    // If displayNames.[[Fallback]] is "code", return code.
    // Return undefined.

    const char* errorMessage = "Failed to query a display name";

    std::pair<UErrorCode, UTF16StringDataNonGCStd> result;
    if (m_type->equals("language")) {
        result = INTL_ICU_STRING_BUFFER_OPERATION(uldn_localeDisplayName, m_icuLocaleDisplayNames, code->toNonGCUTF8StringData().data());
    } else if (m_type->equals("region")) {
        result = INTL_ICU_STRING_BUFFER_OPERATION(uldn_regionDisplayName, m_icuLocaleDisplayNames, code->toNonGCUTF8StringData().data());
    } else if (m_type->equals("script")) {
        result = INTL_ICU_STRING_BUFFER_OPERATION(uldn_scriptDisplayName, m_icuLocaleDisplayNames, code->toNonGCUTF8StringData().data());
    } else if (m_type->equals("calendar")) {
        result = INTL_ICU_STRING_BUFFER_OPERATION(uldn_keyValueDisplayName, m_icuLocaleDisplayNames, "calendar", code->toNonGCUTF8StringData().data());
    } else if (m_type->equals("currency")) {
        UCurrNameStyle style = UCURR_LONG_NAME;
        if (m_style->equals("long")) {
            style = UCURR_LONG_NAME;
        } else if (m_style->equals("short")) {
            style = UCURR_SYMBOL_NAME;
        } else if (m_style->equals("narrow")) {
            style = UCURR_NARROW_SYMBOL_NAME;
        }

        int32_t length = 0;
        UBool unused = false;
        result.first = U_ZERO_ERROR;
        const UChar* buffer = ucurr_getName(code->toUTF16StringData().data(), m_locale->toNonGCUTF8StringData().data(), style, &unused, &length, &result.first);
        if (U_FAILURE(result.first)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to query a display name");
        }
        result.second.assign(buffer, length);
        auto u16Code = code->toUTF16StringData();
        if (result.first == U_USING_DEFAULT_WARNING && result.second.compare(0, u16Code.size(), u16Code.data()) == 0)
            return (m_fallback->equals("none")) ? Value() : code;
    } else {
        ASSERT(m_type->equals("dateTimeField"));
        UDateTimePatternField fieldCode;
        if (code->equals("era")) {
            fieldCode = UDATPG_ERA_FIELD;
        } else if (code->equals("year")) {
            fieldCode = UDATPG_YEAR_FIELD;
        } else if (code->equals("quarter")) {
            fieldCode = UDATPG_QUARTER_FIELD;
        } else if (code->equals("month")) {
            fieldCode = UDATPG_MONTH_FIELD;
        } else if (code->equals("weekOfYear")) {
            fieldCode = UDATPG_WEEK_OF_YEAR_FIELD;
        } else if (code->equals("weekday")) {
            fieldCode = UDATPG_WEEKDAY_FIELD;
        } else if (code->equals("day")) {
            fieldCode = UDATPG_DAY_FIELD;
        } else if (code->equals("dayPeriod")) {
            fieldCode = UDATPG_DAYPERIOD_FIELD;
        } else if (code->equals("hour")) {
            fieldCode = UDATPG_HOUR_FIELD;
        } else if (code->equals("minute")) {
            fieldCode = UDATPG_MINUTE_FIELD;
        } else if (code->equals("second")) {
            fieldCode = UDATPG_SECOND_FIELD;
        } else {
            ASSERT(code->equals("timeZoneName"));
            fieldCode = UDATPG_ZONE_FIELD;
        }

        UDateTimePGDisplayWidth style = UDATPG_WIDE;
        if (m_style->equals("long")) {
            style = UDATPG_WIDE;
        } else if (m_style->equals("short")) {
            style = UDATPG_ABBREVIATED;
        } else {
            ASSERT(m_style->equals("narrow"));
            style = UDATPG_NARROW;
        }

        UErrorCode status = U_ZERO_ERROR;
        LocalResourcePointer<UDateTimePatternGenerator> generator(udatpg_open(m_locale->toNonGCUTF8StringData().data(), &status),
                                                                  [](UDateTimePatternGenerator* d) { udatpg_close(d); });
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, errorMessage);
        }

        result = INTL_ICU_STRING_BUFFER_OPERATION(udatpg_getFieldDisplayName, generator.get(), fieldCode, style);
    }

    if (U_FAILURE(result.first)) {
        if (result.first == U_ILLEGAL_ARGUMENT_ERROR) {
            if (m_fallback->equals("none")) {
                return Value();
            } else {
                return code;
            }
        }
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, errorMessage);
    }
    return new UTF16String(result.second.data(), result.second.length());
}

} // namespace Escargot
#endif
#endif
