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

#include "Escargot.h"
#include "Context.h"
#include "ExecutionState.h"
#include "Value.h"
#include "Intl.h"
#include "IntlNumberFormat.h"

namespace Escargot {

static const char* const intlNumberFormatRelevantExtensionKeys[1] = { "nu" };
static const int intlNumberFormatRelevantExtensionKeysLength = 1;

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

static std::vector<std::string> localeDataNumberFormat(String* locale, size_t keyIndex)
{
    return Intl::numberingSystemsForLocale(locale);
}

static double getNumberOption(ExecutionState& state, Object* options, String* property, double minimum, double maximum, double fallback)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.10

    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
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


Object* IntlNumberFormat::create(ExecutionState& state, Value locales, Value options)
{
    Object* numberFormat = new Object(state);
    initialize(state, numberFormat, locales, options);
    return numberFormat;
}

void IntlNumberFormat::initialize(ExecutionState& state, Object* numberFormat, Value locales, Value options)
{
    numberFormat->setPrototype(state, state.context()->globalObject()->intlNumberFormat()->getFunctionPrototype(state));
    // If dateTimeFormat has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (numberFormat->hasInternalSlot() && numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, ObjectStructurePropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of dateTimeFormat to true.
    numberFormat->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

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
    Value matcher = Intl::getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), values, String::fromASCII("best fit"));
    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Let NumberFormat be the standard built-in object that is the initial value of Intl.NumberFormat.
    // Let localeData be the value of the [[localeData]] internal property of NumberFormat.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of NumberFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of NumberFormat, and localeData.
    StringMap* r = Intl::resolveLocale(state, state.context()->globalObject()->intlNumberFormatAvailableLocales(), requestedLocales, opt, intlNumberFormatRelevantExtensionKeys, intlNumberFormatRelevantExtensionKeysLength, localeDataNumberFormat);

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
    Value s = Intl::getOption(state, options.asObject(), String::fromASCII("style"), String::fromASCII("string"), values, String::fromASCII("decimal"));

    // Set the [[style]] internal property of numberFormat to s.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("style")), s, numberFormat->internalSlot());

    // Let c be the result of calling the GetOption abstract operation with the arguments options, "currency", "string", undefined, and undefined.
    Value c = Intl::getOption(state, options.asObject(), String::fromASCII("currency"), String::fromASCII("string"), ValueVector(), Value());
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
        std::transform(str.begin(), str.end(), str.begin(), toupper);
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
    Value cd = Intl::getOption(state, options.asObject(), String::fromASCII("currencyDisplay"), String::fromASCII("string"), values, String::fromASCII("symbol"));
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
        mxsd = Value(getNumberOption(state, options.asObject(), String::fromASCII("maximumSignificantDigits"), mnsd.asNumber(), 21, 21));
        // Set the [[minimumSignificantDigits]] internal property of numberFormat to mnsd,
        // and the [[maximumSignificantDigits]] internal property of numberFormat to mxsd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("minimumSignificantDigits")), mnsd, numberFormat->internalSlot());
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("maximumSignificantDigits")), mxsd, numberFormat->internalSlot());
    }

    // Let g be the result of calling the GetOption abstract operation with the arguments options, "useGrouping", "boolean", undefined, and true.
    Value g = Intl::getOption(state, options.asObject(), String::fromASCII("useGrouping"), String::fromASCII("boolean"), ValueVector(), Value(true));
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


    UNumberFormatStyle style = UNUM_DEFAULT;

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

    GC_REGISTER_FINALIZER_NO_ORDER(numberFormat->internalSlot(), [](void* obj, void*) {
        Object* self = (Object*)obj;
        unum_close((UNumberFormat*)self->extraData());
    },
                                   nullptr, nullptr, nullptr);
}

UTF16StringDataNonGCStd IntlNumberFormat::format(ExecutionState& state, Object* numberFormat, double x)
{
    UNumberFormat* uformat = (UNumberFormat*)numberFormat->internalSlot()->extraData();

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd result;

    // Map negative zero to positive zero.
    if (!x) {
        x = 0.0;
    }

    result.resize(32);
    auto length = unum_formatDouble(uformat, x, (UChar*)result.data(), result.size(), nullptr, &status);
    result.resize(length);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        unum_formatDouble(uformat, x, (UChar*)result.data(), length, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to format a number");
    }
    return result;
}

} // namespace Escargot
#endif
