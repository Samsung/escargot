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
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlNumberFormat.h"

#if defined(ENABLE_INTL_NUMBERFORMAT)

namespace Escargot {

static const char* const intlNumberFormatRelevantExtensionKeys[1] = { "nu" };
static const int intlNumberFormatRelevantExtensionKeysLength = 1;

static size_t currencyDigits(String* currency)
{
    // https://www.currency-iso.org/en/home/tables/table-a1.html
    // we should use ucurr_getDefaultFractionDigits but it returns wrong value for some contury
    if (currency->equals("BHD")) {
        return 3;
    } else if (currency->equals("BIF")) {
        return 0;
    } else if (currency->equals("CLF")) {
        return 4;
    } else if (currency->equals("CLP")) {
        return 0;
    } else if (currency->equals("DJF")) {
        return 0;
    } else if (currency->equals("GNF")) {
        return 0;
    } else if (currency->equals("IQD")) {
        return 3;
    } else if (currency->equals("ISK")) {
        return 0;
    } else if (currency->equals("JOD")) {
        return 3;
    } else if (currency->equals("JPY")) {
        return 0;
    } else if (currency->equals("KMF")) {
        return 0;
    } else if (currency->equals("KRW")) {
        return 0;
    } else if (currency->equals("KWD")) {
        return 3;
    } else if (currency->equals("LYD")) {
        return 3;
    } else if (currency->equals("OMR")) {
        return 3;
    } else if (currency->equals("PYG")) {
        return 0;
    } else if (currency->equals("RWF")) {
        return 0;
    } else if (currency->equals("TND")) {
        return 3;
    } else if (currency->equals("UGX")) {
        return 0;
    } else if (currency->equals("UYI")) {
        return 0;
    } else if (currency->equals("UYW")) {
        return 4;
    } else if (currency->equals("VND")) {
        return 0;
    } else if (currency->equals("VUV")) {
        return 0;
    } else if (currency->equals("XAF")) {
        return 0;
    } else if (currency->equals("XOF")) {
        return 0;
    } else if (currency->equals("XPF")) {
        return 0;
    }

    return 2;
}

static std::vector<std::string> localeDataNumberFormat(String* locale, size_t keyIndex)
{
    return Intl::numberingSystemsForLocale(locale);
}

static Value defaultNumberOption(ExecutionState& state, Value value, double minimum, double maximum, double fallback)
{
    // https://tc39.es/ecma402/#sec-defaultnumberoption
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Let value be ToNumber(value).
        double doubleValue = value.toNumber(state);
        // If value is NaN or less than minimum or greater than maximum, throw a RangeError exception.
        if (std::isnan(doubleValue) || doubleValue < minimum || maximum < doubleValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Got invalid number option value");
        }
        // Return floor(value).
        return Value(floor(doubleValue));
    } else {
        // Else, return fallback.
        return Value(fallback);
    }
}

Object* IntlNumberFormat::create(ExecutionState& state, Context* realm, Value locales, Value options)
{
    Object* numberFormat = new Object(state, realm->globalObject()->intlNumberFormatPrototype());
    initialize(state, numberFormat, locales, options);
    return numberFormat;
}

// https://tc39.es/ecma402/#sec-issanctionedsimpleunitidentifier
bool isSanctionedSimpleUnitIdentifier(const std::string& s)
{
    if (s == "acre") {
        return true;
    } else if (s == "bit") {
        return true;
    } else if (s == "byte") {
        return true;
    } else if (s == "celsius") {
        return true;
    } else if (s == "centimeter") {
        return true;
    } else if (s == "day") {
        return true;
    } else if (s == "degree") {
        return true;
    } else if (s == "fahrenheit") {
        return true;
    } else if (s == "fluid-ounce") {
        return true;
    } else if (s == "foot") {
        return true;
    } else if (s == "gallon") {
        return true;
    } else if (s == "gigabit") {
        return true;
    } else if (s == "gigabyte") {
        return true;
    } else if (s == "gram") {
        return true;
    } else if (s == "hectare") {
        return true;
    } else if (s == "hour") {
        return true;
    } else if (s == "inch") {
        return true;
    } else if (s == "kilobit") {
        return true;
    } else if (s == "kilobyte") {
        return true;
    } else if (s == "kilogram") {
        return true;
    } else if (s == "kilometer") {
        return true;
    } else if (s == "liter") {
        return true;
    } else if (s == "megabit") {
        return true;
    } else if (s == "megabyte") {
        return true;
    } else if (s == "meter") {
        return true;
    } else if (s == "mile") {
        return true;
    } else if (s == "mile-scandinavian") {
        return true;
    } else if (s == "milliliter") {
        return true;
    } else if (s == "millimeter") {
        return true;
    } else if (s == "millisecond") {
        return true;
    } else if (s == "minute") {
        return true;
    } else if (s == "month") {
        return true;
    } else if (s == "ounce") {
        return true;
    } else if (s == "percent") {
        return true;
    } else if (s == "petabyte") {
        return true;
    } else if (s == "pound") {
        return true;
    } else if (s == "second") {
        return true;
    } else if (s == "stone") {
        return true;
    } else if (s == "terabit") {
        return true;
    } else if (s == "terabyte") {
        return true;
    } else if (s == "week") {
        return true;
    } else if (s == "yard") {
        return true;
    } else if (s == "year") {
        return true;
    }
    return false;
}

static bool isWellFormedUnitIdenifier(const std::string& s)
{
    // If the result of IsSanctionedSimpleUnitIdentifier(unitIdentifier) is true, then
    // Return true.
    if (isSanctionedSimpleUnitIdentifier(s)) {
        return true;
    }
    // If the substring "-per-" does not occur exactly once in unitIdentifier, then
    // Return false.
    auto ss = split(s, "-per-");
    if (ss.size() != 2) {
        return false;
    }
    // Let numerator be the substring of unitIdentifier from the beginning to just before "-per-".
    // If the result of IsSanctionedSimpleUnitIdentifier(numerator) is false, then
    // Return false.
    // Let denominator be the substring of unitIdentifier from just after "-per-" to the end.
    // If the result of IsSanctionedSimpleUnitIdentifier(denominator) is false, then
    // Return false.
    // Return true.
    return isSanctionedSimpleUnitIdentifier(ss[0]) && isSanctionedSimpleUnitIdentifier(ss[1]);
}

static const char* findICUUnitTypeFromUnitString(const std::string& s)
{
#define COMPARE_ONCE(to, from)                             \
    if (memcmp(s.data(), #from, sizeof(#from) - 1) == 0) { \
        return #to;                                        \
    }
    if (false) {
    }
    // clang-format off
    COMPARE_ONCE(area, acre)
    COMPARE_ONCE(digital, bit)
    COMPARE_ONCE(digital, byte)
    COMPARE_ONCE(temperature, celsius)
    COMPARE_ONCE(length, centimeter)
    COMPARE_ONCE(duration, day)
    COMPARE_ONCE(angle, degree)
    COMPARE_ONCE(temperature, fahrenheit)
    COMPARE_ONCE(volume, fluid-ounce)
    COMPARE_ONCE(length, foot)
    COMPARE_ONCE(volume, gallon)
    COMPARE_ONCE(digital, gigabit)
    COMPARE_ONCE(digital, gigabyte)
    COMPARE_ONCE(mass, gram)
    COMPARE_ONCE(area, hectare)
    COMPARE_ONCE(duration, hour)
    COMPARE_ONCE(length, inch)
    COMPARE_ONCE(digital, kilobit)
    COMPARE_ONCE(digital, kilobyte)
    COMPARE_ONCE(mass, kilogram)
    COMPARE_ONCE(length, kilometer)
    COMPARE_ONCE(volume, liter)
    COMPARE_ONCE(digital, megabit)
    COMPARE_ONCE(digital, megabyte)
    COMPARE_ONCE(length, meter)
    COMPARE_ONCE(length, mile)
    COMPARE_ONCE(length, mile-scandinavian)
    COMPARE_ONCE(volume, milliliter)
    COMPARE_ONCE(length, millimeter)
    COMPARE_ONCE(duration, millisecond)
    COMPARE_ONCE(duration, minute)
    COMPARE_ONCE(duration, month)
    COMPARE_ONCE(mass, ounce)
    COMPARE_ONCE(concentr, percent)
    COMPARE_ONCE(digital, petabyte)
    COMPARE_ONCE(mass, pound)
    COMPARE_ONCE(duration, second)
    COMPARE_ONCE(mass, stone)
    COMPARE_ONCE(digital, terabit)
    COMPARE_ONCE(digital, terabyte)
    COMPARE_ONCE(duration, week)
    COMPARE_ONCE(length, yard)
    COMPARE_ONCE(duration, year)
// clang-format on
#undef COMPARE_ONCE
    ASSERT_NOT_REACHED();

    return "";
}

void IntlNumberFormat::initialize(ExecutionState& state, Object* numberFormat, Value locales, Value options)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 62) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.NumberFormat needs 62+ version of ICU");
    }
#endif

    // If dateTimeFormat has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    AtomicString initializedIntlObject = state.context()->staticStrings().lazyInitializedIntlObject();
    if (numberFormat->hasInternalSlot() && numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, ObjectStructurePropertyName(initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of dateTimeFormat to true.
    numberFormat->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    // If options is undefined, then
    if (options.isUndefined()) {
        // Let options be the result of creating a new object as if by the expression new Object() where Object is the standard built-in constructor with that name.
        options = new Object(state, Object::PrototypeIsNull);
    } else {
        // Else
        // Let options be ToObject(options).
        options = options.toObject(state);
    }

    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be the result of calling the GetOption abstract operation (defined in 9.2.9) with the arguments options, "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit".
    // Set opt.[[localeMatcher]] to matcher.
    Value matcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    Value matcher = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);
    // Set opt.[[localeMatcher]] to matcher.
    opt.insert(std::make_pair("localeMatcher", matcher.toString(state)));

    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem = Intl::getOption(state, options.asObject(), state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value());
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(numberingSystem.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "The numberingSystem value you gave is not valid");
        }
    }
    // Set opt.[[nu]] to numberingSystem.
    if (!numberingSystem.isUndefined()) {
        opt.insert(std::make_pair("nu", numberingSystem.asString()));
    }

    // Let NumberFormat be the standard built-in object that is the initial value of Intl.NumberFormat.
    // Let localeData be the value of the [[localeData]] internal property of NumberFormat.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of NumberFormat, requestedLocales, opt, the [[relevantExtensionKeys]] internal property of NumberFormat, and localeData.
    StringMap r = Intl::resolveLocale(state, state.context()->vmInstance()->intlNumberFormatAvailableLocales(), requestedLocales, opt, intlNumberFormatRelevantExtensionKeys, intlNumberFormatRelevantExtensionKeysLength, localeDataNumberFormat);

    // Set the [[locale]] internal property of numberFormat to the value of r.[[locale]].
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySmallLetterLocale()), r.at("locale"), numberFormat->internalSlot());
    // Set numberFormat.[[DataLocale]] to r.[[dataLocale]].
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyDataLocale()), r.at("dataLocale"), numberFormat->internalSlot());
    // Set the [[numberingSystem]] internal property of numberFormat to the value of r.[[nu]].
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().numberingSystem), r.at("nu"), numberFormat->internalSlot());

    // SetNumberFormatUnitOptions ( intlObj, options )
    // Let s be the result of calling the GetOption abstract operation with the arguments options, "style", "string",
    // a List containing the three String values "decimal", "percent", and "currency", and "decimal".
    Value styleValues[4] = { state.context()->staticStrings().lazyDecimal().string(), state.context()->staticStrings().lazyPercent().string(), state.context()->staticStrings().lazyCurrency().string(), state.context()->staticStrings().lazyUnit().string() };
    Value style = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyStyle().string(), Intl::StringValue, styleValues, 4, styleValues[0]);

    // Set the [[style]] internal property of numberFormat to s.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyStyle()), style, numberFormat->internalSlot());

    // Let currency be the result of calling the GetOption abstract operation with the arguments options, "currency", "string", undefined, and undefined.
    Value currency = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyCurrency().string(), Intl::StringValue, nullptr, 0, Value());
    // If currency is not undefined and the result of calling the IsWellFormedCurrencyCode abstract operation (defined in 6.3.1) with argument c is false, then throw a RangeError exception.
    if (!currency.isUndefined()) {
        String* currencyString = currency.asString();
        UTF8StringDataNonGCStd cc = currencyString->toUTF8StringData().data();
        if (currencyString->length() != 3 || !isAllSpecialCharacters(cc, isASCIIAlpha)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "currency is not a well-formed currency code");
        }
    }

    // If style is "currency" and currency is undefined, throw a TypeError exception.
    if (style.equalsTo(state, state.context()->staticStrings().lazyCurrency().string()) && currency.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "if style option is `currency`, you must specify currency");
    }

    // Let currencyDisplay be ? GetOption(options, "currencyDisplay", "string", « "code", "symbol", "narrowSymbol", "name" », "symbol").
    Value currencyDisplayValues[4] = { state.context()->staticStrings().lazyCode().string(), state.context()->staticStrings().symbol.string(), state.context()->staticStrings().lazyNarrowSymbol().string(), state.context()->staticStrings().name.string() };
    Value currencyDisplay = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyCurrencyDisplay().string(), Intl::StringValue, currencyDisplayValues, 4, currencyDisplayValues[1]);
    // Let currencySign be ? GetOption(options, "currencySign", "string", « "standard", "accounting" », "standard").
    Value currencySignValues[2] = { state.context()->staticStrings().lazyStandard().string(), state.context()->staticStrings().lazyAccounting().string() };
    Value currencySign = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyCurrencySign().string(), Intl::StringValue, currencySignValues, 2, currencySignValues[0]);
    // Let unit be ? GetOption(options, "unit", "string", undefined, undefined).
    Value unit = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyUnit().string(), Intl::StringValue, nullptr, 0, Value());
    if (!unit.isUndefined()) {
        String* unitString = unit.asString();
        UTF8StringDataNonGCStd cc = unitString->toUTF8StringData().data();
        if (!isWellFormedUnitIdenifier(cc)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "unit is not a well-formed unit idenitfier");
        }
    }

    // If style is "unit" and unit is undefined, throw a TypeError exception.
    if (style.equalsTo(state, state.context()->staticStrings().lazyUnit().string()) && unit.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "if style option is `unit`, you must specify unit");
    }

    // Let unitDisplay be ? GetOption(options, "unitDisplay", "string", « "short", "narrow", "long" », "short").
    Value unitDisplayValues[3] = { state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyNarrow().string(), state.context()->staticStrings().lazyLong().string() };
    Value unitDisplay = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyUnitDisplay().string(), Intl::StringValue, unitDisplayValues, 3, unitDisplayValues[0]);

    // If style is "currency", then
    if (style.equalsTo(state, state.context()->staticStrings().lazyCurrency().string())) {
        // Let currency be the result of converting currency to upper case as specified in 6.1.
        String* cString = currency.toString(state);
        std::string str = cString->toUTF8StringData().data();
        std::transform(str.begin(), str.end(), str.begin(), toupper);
        currency = String::fromASCII(str.data(), str.length());
        // Set intlObj.[[Currency]] to currency.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyCurrency()), currency, numberFormat->internalSlot());
        // Set intlObj.[[CurrencyDisplay]] to currencyDisplay.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyCurrencyDisplay()), currencyDisplay.asString(), numberFormat->internalSlot());
        // Set intlObj.[[CurrencySign]] to currencySign.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyCurrencySign()), currencySign.asString(), numberFormat->internalSlot());
    }

    // If style is "unit", then
    if (style.equalsTo(state, state.context()->staticStrings().lazyUnit().string())) {
        // Set intlObj.[[Unit]] to unit.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyUnit()), unit.asString(), numberFormat->internalSlot());
        // Set intlObj.[[UnitDisplay]] to unitDisplay.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyUnitDisplay()), unitDisplay.asString(), numberFormat->internalSlot());
    }

    double mnfdDefault = 0;
    double mxfdDefault = 0;

    // Let style be numberFormat.[[Style]].
    // If style is "currency", then
    if (style.equalsTo(state, state.context()->staticStrings().lazyCurrency().string())) {
        // Let currency be numberFormat.[[Currency]].
        // Let cDigits be CurrencyDigits(currency).
        size_t cDigits = currencyDigits(currency.asString());
        // Let mnfdDefault be cDigits.
        mnfdDefault = cDigits;
        // Let mxfdDefault be cDigits.
        mxfdDefault = cDigits;
    } else {
        // Else,
        // Let mnfdDefault be 0.
        mnfdDefault = 0;
        // If style is "percent", then
        if (style.equalsTo(state, state.context()->staticStrings().lazyCurrency().string())) {
            // Let mxfdDefault be 0.
            mxfdDefault = 0;
        } else {
            // Else,
            // Let mxfdDefault be 3.
            mxfdDefault = 3;
        }
    }

    // Let notation be ? GetOption(options, "notation", "string", « "standard", "scientific", "engineering", "compact" », "standard").
    Value notationValues[4] = { state.context()->staticStrings().lazyStandard().string(), state.context()->staticStrings().lazyScientific().string(), state.context()->staticStrings().lazyEngineering().string(), state.context()->staticStrings().lazyCompact().string() };
    Value notation = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyNotation().string(), Intl::StringValue, notationValues, 4, notationValues[0]);
    // Set numberFormat.[[Notation]] to notation.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyNotation()), notation, numberFormat->internalSlot());

    // Perform ? SetNumberFormatDigitOptions(numberFormat, options, mnfdDefault, mxfdDefault, notation).
    // Let mnid be ? GetNumberOption(options, "minimumIntegerDigits,", 1, 21, 1).
    double mnid = Intl::getNumberOption(state, options.asObject(), state.context()->staticStrings().lazyMinimumIntegerDigits().string(), 1, 21, 1);
    // Let mnfd be ? Get(options, "minimumFractionDigits").
    Value mnfd;
    auto g = options.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumFractionDigits()));
    if (g.hasValue()) {
        mnfd = g.value(state, options.asObject());
    }
    // Let mxfd be ? Get(options, "maximumFractionDigits").
    Value mxfd;
    g = options.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumFractionDigits()));
    if (g.hasValue()) {
        mxfd = g.value(state, options.asObject());
    }
    // Let mnsd be ? Get(options, "minimumSignificantDigits").
    Value mnsd;
    g = options.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumSignificantDigits()));
    if (g.hasValue()) {
        mnsd = g.value(state, options.asObject());
    }
    // Let mxsd be ? Get(options, "maximumSignificantDigits").
    Value mxsd;
    g = options.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumSignificantDigits()));
    if (g.hasValue()) {
        mxsd = g.value(state, options.asObject());
    }

    // Set intlObj.[[MinimumIntegerDigits]] to mnid.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumIntegerDigits()), Value(mnid), numberFormat->internalSlot());

    // If mnsd is not undefined or mxsd is not undefined, then
    if (!mnsd.isUndefined() || !mxsd.isUndefined()) {
        // Set intlObj.[[RoundingType]] to significantDigits.
        // Let mnsd be ? DefaultNumberOption(mnsd, 1, 21, 1).
        mnsd = defaultNumberOption(state, mnsd, 1, 21, 1);
        // Let mxsd be ? DefaultNumberOption(mxsd, mnsd, 21, 21).
        mxsd = defaultNumberOption(state, mxsd, mnsd.asNumber(), 21, 21);
        // Set intlObj.[[MinimumSignificantDigits]] to mnsd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumSignificantDigits()), mnsd, numberFormat->internalSlot());
        // Set intlObj.[[MaximumSignificantDigits]] to mxsd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumSignificantDigits()), mxsd, numberFormat->internalSlot());
    } else if (!mnfd.isUndefined() || !mxfd.isUndefined()) {
        // Else if mnfd is not undefined or mxfd is not undefined, then
        // Set intlObj.[[RoundingType]] to fractionDigits.
        // Let mnfd be ? DefaultNumberOption(mnfd, 0, 20, mnfdDefault).
        mnfd = defaultNumberOption(state, mnfd, 0, 20, mnfdDefault);
        // Let mxfdActualDefault be max( mnfd, mxfdDefault ).
        double mxfdActualDefault = std::max(mnfd.asNumber(), mxfdDefault);
        // Let mxfd be ? DefaultNumberOption(mxfd, mnfd, 20, mxfdActualDefault).
        mxfd = defaultNumberOption(state, mxfd, mnfd.asNumber(), 20, mxfdActualDefault);
        // Set intlObj.[[MinimumFractionDigits]] to mnfd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumFractionDigits()), mnfd, numberFormat->internalSlot());
        // Set intlObj.[[MaximumFractionDigits]] to mxfd.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumFractionDigits()), mxfd, numberFormat->internalSlot());
    } else if (notation.equalsTo(state, state.context()->staticStrings().lazyCompact().string())) {
        // Else if notation is "compact", then
        // Set intlObj.[[RoundingType]] to compactRounding.
    } else {
        // Set intlObj.[[RoundingType]] to fractionDigits.
        // Set intlObj.[[MinimumFractionDigits]] to mnfdDefault.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumFractionDigits()), Value(mnfdDefault), numberFormat->internalSlot());
        // Set intlObj.[[MaximumFractionDigits]] to mxfdDefault.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumFractionDigits()), Value(mxfdDefault), numberFormat->internalSlot());
    }

    // Let compactDisplay be ? GetOption(options, "compactDisplay", "string", « "short", "long" », "short").
    Value compactDisplayValues[2] = { state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyLong().string() };
    Value compactDisplay = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyCompactDisplay().string(), Intl::StringValue, compactDisplayValues, 2, compactDisplayValues[0]);

    // If notation is "compact", then
    if (notation.equalsTo(state, state.context()->staticStrings().lazyCompact().string())) {
        // Set numberFormat.[[CompactDisplay]] to compactDisplay.
        numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyCompactDisplay()), compactDisplay, numberFormat->internalSlot());
    }

    // Let useGrouping be ? GetOption(options, "useGrouping", "boolean", undefined, true).
    Value useGrouping = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyUseGrouping().string(), Intl::BooleanValue, nullptr, 0, Value(true));
    // Set numberFormat.[[UseGrouping]] to useGrouping.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyUseGrouping()), useGrouping, numberFormat->internalSlot());

    // Let signDisplay be ? GetOption(options, "signDisplay", "string", « "auto", "never", "always", "exceptZero" », "auto").
    Value signDisplayValue[4] = { state.context()->staticStrings().lazyAuto().string(), state.context()->staticStrings().lazyNever().string(), state.context()->staticStrings().lazyAlways().string(), state.context()->staticStrings().lazyExceptZero().string() };
    Value signDisplay = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazySignDisplay().string(), Intl::StringValue, signDisplayValue, 4, signDisplayValue[0]);
    // Set numberFormat.[[SignDisplay]] to signDisplay.
    numberFormat->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySignDisplay()), signDisplay, numberFormat->internalSlot());

    // Set the [[initializedNumberFormat]] internal property of numberFormat to true.
    numberFormat->internalSlot()->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyInitializedNumberFormat()), ObjectPropertyDescriptor(Value(true)));
    // Return numberFormat

    UTF16StringDataNonGCStd skeleton;
    initNumberFormatSkeleton(state, numberFormat, style, currency, currencyDisplay, currencySign, unit, unitDisplay, compactDisplay, signDisplay, useGrouping, notation, skeleton);

    String* localeOption = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazySmallLetterLocale())).value(state, numberFormat->internalSlot()).toString(state);

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormatter* fomatter = unumf_openForSkeletonAndLocale((UChar*)skeleton.data(), skeleton.length(), localeOption->toNonGCUTF8StringData().data(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to init NumberFormat");
    }

    numberFormat->internalSlot()->setExtraData(fomatter);

    numberFormat->internalSlot()->addFinalizer([](Object* obj, void* data) {
        Object* self = (Object*)obj;
        unumf_close((UNumberFormatter*)self->extraData());
    },
                                               nullptr);
}

void IntlNumberFormat::initNumberFormatSkeleton(ExecutionState& state, Object* numberFormat, const Value& style, const Value& currency, const Value& currencyDisplay, const Value& currencySign, const Value& unit, const Value& unitDisplay, const Value& compactDisplay, const Value& signDisplay, const Value& useGrouping, const Value& notation, UTF16StringDataNonGCStd& skeleton)
{
    if (style.asString()->equals("currency")) {
        if (!currency.isUndefined()) {
            skeleton += u"currency/";
            skeleton += currency.asString()->charAt(0);
            skeleton += currency.asString()->charAt(1);
            skeleton += currency.asString()->charAt(2);
            skeleton += u" ";
        }

        if (currencyDisplay.asString()->equals("code")) {
            skeleton += u"unit-width-iso-code ";
        } else if (currencyDisplay.asString()->equals("symbol")) {
            // default
        } else if (currencyDisplay.asString()->equals("narrowSymbol")) {
            skeleton += u"unit-width-narrow ";
        } else if (currencyDisplay.asString()->equals("name")) {
            skeleton += u"unit-width-full-name ";
        } else {
            ASSERT_NOT_REACHED();
        }
    } else if (style.asString()->equals("percent")) {
        skeleton += u"percent scale/100 ";
    } else if (style.asString()->equals("unit")) {
        String* unitString = unit.asString();
        if (unitString->contains("-per-")) {
            size_t pos = unitString->find("-per-");
            skeleton += u"measure-unit/";
            const char* u = findICUUnitTypeFromUnitString(unitString->substring(0, pos)->toNonGCUTF8StringData());
            size_t len = strlen(u);
            for (size_t i = 0; i < len; i++) {
                skeleton += u[i];
            }
            skeleton += '-';
            skeleton += unitString->substring(0, pos)->toUTF16StringData().data();
            skeleton += ' ';

            skeleton += u"per-measure-unit/";
            u = findICUUnitTypeFromUnitString(unitString->substring(pos + 5, unitString->length())->toNonGCUTF8StringData());
            len = strlen(u);
            for (size_t i = 0; i < len; i++) {
                skeleton += u[i];
            }
            skeleton += '-';
            skeleton += unitString->substring(pos + 5, unitString->length())->toUTF16StringData().data();
            skeleton += ' ';
        } else {
            skeleton += u"measure-unit/";
            const char* u = findICUUnitTypeFromUnitString(unitString->toNonGCUTF8StringData());
            size_t len = strlen(u);
            for (size_t i = 0; i < len; i++) {
                skeleton += u[i];
            }
            skeleton += '-';
            skeleton += unitString->toUTF16StringData().data();
            skeleton += ' ';
        }

        String* unitDisplayString = unitDisplay.asString();
        if (unitDisplayString->equals("short")) {
            skeleton += u"unit-width-short ";
        } else if (unitDisplayString->equals("narrow")) {
            skeleton += u"unit-width-narrow ";
        } else {
            ASSERT(unitDisplayString->equals("long"));
            skeleton += u"unit-width-full-name ";
        }
    }

    if (numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumSignificantDigits()))) {
        double mnsd = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumSignificantDigits())).value(state, numberFormat->internalSlot()).toNumber(state);
        double mxsd = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumSignificantDigits())).value(state, numberFormat->internalSlot()).toNumber(state);

        for (double i = 0; i < mnsd; i++) {
            skeleton += '@';
        }

        for (double i = 0; i < mxsd - mnsd; i++) {
            skeleton += '#';
        }

        skeleton += ' ';
    }

    if (numberFormat->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumFractionDigits()))) {
        double mnfd = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumFractionDigits())).value(state, numberFormat->internalSlot()).toNumber(state);
        double mxfd = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMaximumFractionDigits())).value(state, numberFormat->internalSlot()).toNumber(state);

        skeleton += '.';

        for (double i = 0; i < mnfd; i++) {
            skeleton += '0';
        }

        for (double i = 0; i < mxfd - mnfd; i++) {
            skeleton += '#';
        }
        skeleton += ' ';
    }

    {
        double mnid = numberFormat->internalSlot()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyMinimumIntegerDigits())).value(state, numberFormat->internalSlot()).toNumber(state);
        skeleton += u"integer-width/+";
        for (double i = 0; i < mnid; i++) {
            skeleton += '0';
        }
        skeleton += ' ';
    }

    if (!useGrouping.asBoolean()) {
        skeleton += u"group-off ";
    }

    String* notationString = notation.asString();
    if (notationString->equals("standard")) {
    } else if (notationString->equals("scientific")) {
        skeleton += u"scientific ";
    } else if (notationString->equals("engineering")) {
        skeleton += u"engineering ";
    } else {
        if (compactDisplay.asString()->equals("short")) {
            skeleton += u"compact-short ";
        } else {
            skeleton += u"compact-long ";
        }
    }

    String* signDisplayString = signDisplay.asString();
    bool accountingSign = currencySign.asString()->equals("accounting");
    if (signDisplayString->equals("auto")) {
        if (accountingSign) {
            skeleton += u"sign-accounting ";
        }
    } else if (signDisplayString->equals("never")) {
        skeleton += u"sign-never ";
    } else if (signDisplayString->equals("always")) {
        if (accountingSign) {
            skeleton += u"sign-accounting-always ";
        } else {
            skeleton += u"sign-always ";
        }
    } else {
        if (accountingSign) {
            skeleton += u"sign-accounting-except-zero ";
        } else {
            skeleton += u"sign-except-zero ";
        }
    }

    skeleton += u"rounding-mode-half-up ";
}

UTF16StringDataNonGCStd IntlNumberFormat::format(ExecutionState& state, Object* numberFormat, double x)
{
    UNumberFormatter* formatter = (UNumberFormatter*)numberFormat->internalSlot()->extraData();

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd resultString;

    LocalResourcePointer<UFormattedNumber> uresult(unumf_openResult(&status), [](UFormattedNumber* f) { unumf_closeResult(f); });

    unumf_formatDouble(formatter, x, uresult.get(), &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    resultString.resize(32);
    auto length = unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    resultString.resize(length);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    return resultString;
}

UTF16StringDataNonGCStd IntlNumberFormat::format(ExecutionState& state, Object* numberFormat, String* str)
{
    UNumberFormatter* formatter = (UNumberFormatter*)numberFormat->internalSlot()->extraData();

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd resultString;

    LocalResourcePointer<UFormattedNumber> uresult(unumf_openResult(&status), [](UFormattedNumber* f) { unumf_closeResult(f); });

    auto s = str->toNonGCUTF8StringData();

    unumf_formatDecimal(formatter, s.data(), s.length(), uresult.get(), &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    resultString.resize(32);
    auto length = unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    resultString.resize(length);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    return resultString;
}

ArrayObject* IntlNumberFormat::formatToParts(ExecutionState& state, Object* numberFormat, double x)
{
    UNumberFormatter* formatter = (UNumberFormatter*)numberFormat->internalSlot()->extraData();

    ArrayObject* result = new ArrayObject(state);

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd resultString;

    LocalResourcePointer<UFormattedNumber> uresult(unumf_openResult(&status), [](UFormattedNumber* f) { unumf_closeResult(f); });

    unumf_formatDouble(formatter, x, uresult.get(), &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    resultString.resize(32);
    auto length = unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    resultString.resize(length);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        unumf_resultToString(uresult.get(), (UChar*)resultString.data(), resultString.size(), &status);
    }
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    LocalResourcePointer<UFieldPositionIterator> fpositer(ufieldpositer_open(&status), [](UFieldPositionIterator* f) { ufieldpositer_close(f); });
    ASSERT(U_SUCCESS(status));

    unumf_resultGetAllFieldPositions(uresult.get(), fpositer.get(), &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to format a number");
    }

    std::vector<Intl::NumberFieldItem> fields;

    while (true) {
        int32_t start;
        int32_t end;
        int32_t type = ufieldpositer_next(fpositer.get(), &start, &end);
        if (type < 0) {
            break;
        }

        if (std::isnan(x) || std::isinf(x)) {
            // We should ignore `E0` field of NaN, Infinity
            // NaNE0->NaN
            if (type == UNUM_EXPONENT_SYMBOL_FIELD) {
                continue;
            }
            if (type == UNUM_EXPONENT_FIELD) {
                continue;
            }
        }

        Intl::NumberFieldItem item;
        item.start = start;
        item.end = end;
        item.type = type;

        fields.push_back(item);
    }

    Intl::convertICUNumberFieldToEcmaNumberField(fields, x, resultString);

    AtomicString typeAtom(state, "type", 4);
    AtomicString valueAtom = state.context()->staticStrings().value;

    for (size_t i = 0; i < fields.size(); i++) {
        const Intl::NumberFieldItem& item = fields[i];

        Object* o = new Object(state);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(typeAtom), ObjectPropertyDescriptor(Intl::icuNumberFieldToString(state, item.type, x), ObjectPropertyDescriptor::AllPresent));
        auto sub = resultString.substr(item.start, item.end - item.start);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(valueAtom), ObjectPropertyDescriptor(new UTF16String(sub.data(), sub.length()), ObjectPropertyDescriptor::AllPresent));

        result->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, i), ObjectPropertyDescriptor(o, ObjectPropertyDescriptor::AllPresent));
    }

    return result;
}

} // namespace Escargot
#endif
#endif
