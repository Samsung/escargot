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
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlPluralRules.h"

#if defined(ENABLE_INTL_PLURALRULES)

namespace Escargot {

static double getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, double fallback)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.10
    if (!options) {
        return fallback;
    }

    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options.value()->get(state, ObjectPropertyName(state, property)).value(state, options.value());
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Let value be ToNumber(value).
        double doubleValue = value.toNumber(state);
        // If value is NaN or less than minimum or greater than maximum, throw a RangeError exception.
        if (std::isnan(doubleValue) || doubleValue < minimum || maximum < doubleValue) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Got invalid number option value");
        }
        // Return floor(value).
        return floor(doubleValue);
    } else {
        // Else return fallback.
        return fallback;
    }
}

void* IntlPluralRulesObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(IntlPluralRulesObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_locale));
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_type));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(IntlPluralRulesObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

IntlPluralRulesObject::IntlPluralRulesObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : Object(state, proto)
    , m_icuPluralRules(nullptr)
    , m_icuNumberFormat(nullptr)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-initializepluralrules
    // [[InitializedPluralRules]], [[Locale]], [[Type]], [[MinimumIntegerDigits]], [[MinimumFractionDigits]], [[MaximumFractionDigits]], [[MinimumSignificantDigits]], [[MaximumSignificantDigits]

    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    Optional<Object*> optionObject;
    // If options is undefined, then
    if (options.isUndefined()) {
        // Let options be ObjectCreate(null).
    } else {
        // Let options be ? ToObject(options).
        optionObject = options.toObject(state);
    }

    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    // Set opt.[[localeMatcher]] to matcher.
    Value localeMatcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    String* matcher = localeMatcherValues[1].asString();
    if (optionObject) {
        matcher = Intl::getOption(state, optionObject.value(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, localeMatcherValues, 2, localeMatcherValues[1]).asString();
    }
    opt.insert(std::make_pair("matcher", matcher));

    // Let t be ? GetOption(options, "type", "string", « "cardinal", "ordinal" », "cardinal").
    Value typeValues[2] = { state.context()->staticStrings().lazyCardinal().string(), state.context()->staticStrings().lazyOrdinal().string() };
    String* t = typeValues[0].asString();
    if (optionObject) {
        t = Intl::getOption(state, optionObject.value(), state.context()->staticStrings().lazyType().string(), Intl::StringValue, typeValues, 2, typeValues[0]).asString();
    }

    // Perform ? SetNumberFormatDigitOptions(pluralRules, options, 0, 3).
    // Let mnid be the result of calling the GetNumberOption abstract operation (defined in 9.2.10) with arguments options, "minimumIntegerDigits", 1, 21, and 1.
    double mnid = getNumberOption(state, optionObject, state.context()->staticStrings().lazyMinimumIntegerDigits().string(), 1, 21, 1);
    // Set the [[minimumIntegerDigits]] internal property of numberFormat to mnid.
    m_minimumIntegerDigits = mnid;

    double mnfdDefault = 0;
    // Let mnfd be the result of calling the GetNumberOption abstract operation with arguments options, "minimumFractionDigits", 0, 20, and mnfdDefault.
    double mnfd = getNumberOption(state, optionObject, state.context()->staticStrings().lazyMinimumFractionDigits().string(), 0, 20, mnfdDefault);

    // Set the [[minimumFractionDigits]] internal property of numberFormat to mnfd.
    m_minimumFractionDigits = mnfd;

    double mxfdDefault = 3;

    // Let mxfd be the result of calling the GetNumberOption abstract operation with arguments options, "maximumFractionDigits", mnfd, 20, and mxfdDefault.
    double mxfd = getNumberOption(state, optionObject, state.context()->staticStrings().lazyMaximumFractionDigits().string(), mnfd, 20, mxfdDefault);

    // Set the [[maximumFractionDigits]] internal property of numberFormat to mxfd.
    m_maximumFractionDigits = mxfd;

    if (optionObject) {
        // Let mnsd be the result of calling the [[Get]] internal method of options with argument "minimumSignificantDigits".
        Value mnsd = optionObject.value()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumSignificantDigits())).value(state, optionObject.value());
        // Let mxsd be the result of calling the [[Get]] internal method of options with argument "maximumSignificantDigits".
        Value mxsd = optionObject.value()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumSignificantDigits())).value(state, optionObject.value());

        // If mnsd is not undefined or mxsd is not undefined, then:
        if (!mnsd.isUndefined() || !mxsd.isUndefined()) {
            // Let mnsd be the result of calling the GetNumberOption abstract operation with arguments options, "minimumSignificantDigits", 1, 21, and 1.
            mnsd = Value(getNumberOption(state, optionObject.value(), state.context()->staticStrings().lazyMinimumSignificantDigits().string(), 1, 21, 1));
            // Let mxsd be the result of calling the GetNumberOption abstract operation with arguments options, "maximumSignificantDigits", mnsd, 21, and 21.
            mxsd = Value(getNumberOption(state, optionObject.value(), state.context()->staticStrings().lazyMaximumSignificantDigits().string(), mnsd.asNumber(), 21, 21));
            // Set the [[minimumSignificantDigits]] internal property of numberFormat to mnsd,
            // and the [[maximumSignificantDigits]] internal property of numberFormat to mxsd.
            m_minimumSignificantDigits = mnsd.asNumber();
            m_maximumSignificantDigits = mxsd.asNumber();
        }
    }

    // Let localeData be %PluralRules%.[[LocaleData]].
    // Let r be ResolveLocale(%PluralRules%.[[AvailableLocales]], requestedLocales, opt, %PluralRules%.[[RelevantExtensionKeys]], localeData).
    // Set pluralRules.[[Locale]] to the value of r.[[locale]].
    auto r = Intl::resolveLocale(state, state.context()->vmInstance()->intlPluralRulesAvailableLocales(), requestedLocales, opt, nullptr, 0, nullptr);
    String* foundLocale = r.at("locale");

    UErrorCode status = U_ZERO_ERROR;
    m_icuNumberFormat = unum_open(UNUM_DEFAULT, nullptr, 0, foundLocale->toNonGCUTF8StringData().data(), nullptr, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to init PluralRule");
    }

    if (!m_minimumSignificantDigits.hasValue()) {
        unum_setAttribute(m_icuNumberFormat, UNUM_MIN_INTEGER_DIGITS, m_minimumIntegerDigits);
        unum_setAttribute(m_icuNumberFormat, UNUM_MIN_FRACTION_DIGITS, m_minimumFractionDigits);
        unum_setAttribute(m_icuNumberFormat, UNUM_MAX_FRACTION_DIGITS, m_maximumFractionDigits);
    } else {
        unum_setAttribute(m_icuNumberFormat, UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(m_icuNumberFormat, UNUM_MIN_SIGNIFICANT_DIGITS, m_minimumSignificantDigits);
        unum_setAttribute(m_icuNumberFormat, UNUM_MAX_SIGNIFICANT_DIGITS, m_maximumSignificantDigits);
    }
    unum_setAttribute(m_icuNumberFormat, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to init NumberFormat");
    }

    ASSERT(U_SUCCESS(status));

    UPluralType icuType = UPLURAL_TYPE_CARDINAL;
    if (t->equals("ordinal")) {
        icuType = UPLURAL_TYPE_ORDINAL;
    }

    m_icuPluralRules = uplrules_openForType(foundLocale->toNonGCUTF8StringData().data(), icuType, &status);
    if (!m_icuPluralRules) {
        auto parseResult = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(foundLocale->toNonGCUTF8StringData());
        // Remove extensions and try again.
        m_icuPluralRules = uplrules_openForType(parseResult.language.data(), icuType, &status);
        ASSERT(U_SUCCESS(status));
    }

    m_locale = foundLocale;
    m_type = t;

    addFinalizer([](Object* obj, void* data) {
        IntlPluralRulesObject* self = (IntlPluralRulesObject*)obj;
        uplrules_close(self->m_icuPluralRules);
        unum_close(self->m_icuNumberFormat);
    },
                 nullptr);

    // Return pluralRules.
}

String* IntlPluralRulesObject::resolvePlural(double number)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-resolveplural
    // If n is not a finite Number, then
    if (std::isinf(number)) {
        // Return "other".
        return String::fromASCII("other");
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t len = uplrules_selectWithFormat(m_icuPluralRules, number, m_icuNumberFormat, nullptr, 0, &status);
    UChar* buf = (UChar*)alloca((len + 1) * sizeof(UChar));
    status = U_ZERO_ERROR;
    uplrules_selectWithFormat(m_icuPluralRules, number, m_icuNumberFormat, buf, len + 1, &status);
    ASSERT(U_SUCCESS(status));

    return new UTF16String(buf, len);
}

} // namespace Escargot
#endif
#endif
