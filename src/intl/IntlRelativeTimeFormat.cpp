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
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlRelativeTimeFormat.h"

#if defined(ENABLE_INTL_RELATIVETIMEFORMAT)

namespace Escargot {

static const char* const intlRelativeTimeFormatRelevantExtensionKeys[1] = { "nu" };
static size_t intlRelativeTimeFormatRelevantExtensionKeysLength = 1;
static const size_t indexOfExtensionKeyNu = 0;

static std::vector<std::string> localeDataRelativeTimeFormat(String* locale, size_t keyIndex)
{
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyNu:
        keyLocaleData = Intl::numberingSystemsForLocale(locale);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

IntlRelativeTimeFormatObject::IntlRelativeTimeFormatObject(ExecutionState& state, Value locales, Value options)
    : IntlRelativeTimeFormatObject(state, state.context()->globalObject()->intlRelativeTimeFormatPrototype(), locales, options)
{
}

IntlRelativeTimeFormatObject::IntlRelativeTimeFormatObject(ExecutionState& state, Object* proto, Value locales, Value optionsInput)
    : DerivedObject(state, proto)
    , m_locale(nullptr)
    , m_dataLocale(nullptr)
    , m_numberingSystem(nullptr)
    , m_style(nullptr)
    , m_numeric(nullptr)
    , m_icuRelativeDateTimeFormatter(nullptr)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 62) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.NumberFormat needs 62+ version of ICU");
    }
#endif
    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    Optional<Object*> options;
    // If options is undefined, then
    if (optionsInput.isUndefined()) {
        // Let options be ObjectCreate(null).
    } else {
        // Else,
        // Let options be ? ToObject(options).
        options = optionsInput.toObject(state);
    }

    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    // Set opt.[[localeMatcher]] to matcher.
    Value localeMatcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    String* matcher = localeMatcherValues[1].asString();
    if (options) {
        matcher = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, localeMatcherValues, 2, localeMatcherValues[1]).asString();
    }
    opt.insert(std::make_pair("matcher", matcher));

    // Let numberingSystem be ? GetOption(options, "numberingSystem", "string", undefined, undefined).
    Value numberingSystem;
    if (options) {
        numberingSystem = Intl::getOption(state, options.value(), state.context()->staticStrings().numberingSystem.string(), Intl::StringValue, nullptr, 0, Value());
    }
    // If numberingSystem is not undefined, then
    if (!numberingSystem.isUndefined()) {
        // If numberingSystem does not match the type sequence (from UTS 35 Unicode Locale Identifier, section 3.2), throw a RangeError exception.
        if (!Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(numberingSystem.asString())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "The numberingSystem value you gave is not valid");
        }
    }

    // Set opt.[[nu]] to numberingSystem.
    if (!numberingSystem.isUndefined()) {
        opt.insert(std::make_pair("nu", numberingSystem.asString()));
    }
    // Let localeData be %RelativeTimeFormat%.[[LocaleData]].
    // Let r be ResolveLocale(%RelativeTimeFormat%.[[AvailableLocales]], requestedLocales, opt, %RelativeTimeFormat%.[[RelevantExtensionKeys]], localeData).
    StringMap r = Intl::resolveLocale(state, state.context()->vmInstance()->intlRelativeTimeFormatAvailableLocales(), requestedLocales, opt, intlRelativeTimeFormatRelevantExtensionKeys, intlRelativeTimeFormatRelevantExtensionKeysLength, localeDataRelativeTimeFormat);
    // Let locale be r.[[Locale]].
    String* locale = r.at("locale");
    // Set relativeTimeFormat.[[Locale]] to locale.
    m_locale = locale;
    // Set relativeTimeFormat.[[DataLocale]] to r.[[DataLocale]].
    m_dataLocale = r.at("dataLocale");
    // Set relativeTimeFormat.[[NumberingSystem]] to r.[[nu]].
    m_numberingSystem = r.at("nu");
    // Let s be ? GetOption(options, "style", "string", «"long", "short", "narrow"», "long").

    Value styleValues[3] = { state.context()->staticStrings().lazyLong().string(), state.context()->staticStrings().lazyShort().string(), state.context()->staticStrings().lazyNarrow().string() };
    String* style = styleValues[0].asString();
    // Set relativeTimeFormat.[[Style]] to s.
    if (options) {
        style = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyStyle().string(), Intl::StringValue, styleValues, 3, styleValues[0]).asString();
    }
    m_style = style;
    // Let numeric be ? GetOption(options, "numeric", "string", «"always", "auto"», "always").
    Value numericValues[2] = { state.context()->staticStrings().lazyAlways().string(), state.context()->staticStrings().lazyAuto().string() };
    String* numeric = numericValues[0].asString();
    if (options) {
        numeric = Intl::getOption(state, options.value(), state.context()->staticStrings().numeric.string(), Intl::StringValue, numericValues, 2, numericValues[0]).asString();
    }
    // Set relativeTimeFormat.[[Numeric]] to numeric.
    m_numeric = numeric;
    // Let relativeTimeFormat.[[NumberFormat]] be ! Construct(%NumberFormat%, « locale »).
    // Let relativeTimeFormat.[[PluralRules]] be ! Construct(%PluralRules%, « locale »).
    // Return relativeTimeFormat.

    UDateRelativeDateTimeFormatterStyle icuStyle;
    if (m_style->equals("short")) {
        icuStyle = UDAT_STYLE_SHORT;
    } else if (m_style->equals("narrow")) {
        icuStyle = UDAT_STYLE_NARROW;
    } else {
        ASSERT(m_style->equals("long"));
        icuStyle = UDAT_STYLE_LONG;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_icuRelativeDateTimeFormatter = ureldatefmt_open(m_locale->toNonGCUTF8StringData().data(), nullptr, icuStyle, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize RelativeTimeFormat");
    }

    addFinalizer([](PointerValue* obj, void* data) {
        IntlRelativeTimeFormatObject* self = (IntlRelativeTimeFormatObject*)obj;
        ureldatefmt_close(self->m_icuRelativeDateTimeFormatter);
    },
                 nullptr);
}

static URelativeDateTimeUnit icuRelativeTimeUnitFromString(String* unit)
{
    if (unit->equals("seconds") || unit->equals("second")) {
        return UDAT_REL_UNIT_SECOND;
    } else if (unit->equals("minutes") || unit->equals("minute")) {
        return UDAT_REL_UNIT_MINUTE;
    } else if (unit->equals("hours") || unit->equals("hour")) {
        return UDAT_REL_UNIT_HOUR;
    } else if (unit->equals("days") || unit->equals("day")) {
        return UDAT_REL_UNIT_DAY;
    } else if (unit->equals("weeks") || unit->equals("week")) {
        return UDAT_REL_UNIT_WEEK;
    } else if (unit->equals("months") || unit->equals("month")) {
        return UDAT_REL_UNIT_MONTH;
    } else if (unit->equals("quarters") || unit->equals("quarter")) {
        return UDAT_REL_UNIT_QUARTER;
    } else if (unit->equals("years") || unit->equals("year")) {
        return UDAT_REL_UNIT_YEAR;
    } else {
        return UDAT_REL_UNIT_COUNT;
    }
}

String* IntlRelativeTimeFormatObject::format(ExecutionState& state, double value, String* unit)
{
    // https://tc39.es/ecma402/#sec-PartitionRelativeTimePattern
    if (std::isinf(value) || std::isnan(value) || !IS_IN_TIME_RANGE(value)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "value is valid in RelativeTimeFormat format()");
    }

    // https://tc39.es/ecma402/#sec-singularrelativetimeunit
    URelativeDateTimeUnit icuUnit = icuRelativeTimeUnitFromString(unit);

    if (icuUnit == UDAT_REL_UNIT_COUNT) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "unit is invalid in RelativeTimeFormat format()");
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t bufSize;
    if (m_numeric->equals("always")) {
        bufSize = ureldatefmt_formatNumeric(m_icuRelativeDateTimeFormatter, value, icuUnit, nullptr, 0, &status);
    } else {
        bufSize = ureldatefmt_format(m_icuRelativeDateTimeFormatter, value, icuUnit, nullptr, 0, &status);
    }

    if (status != U_BUFFER_OVERFLOW_ERROR) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    status = U_ZERO_ERROR;
    UChar* buf = (UChar*)alloca(sizeof(UChar) * (bufSize + 1));
    if (m_numeric->equals("always")) {
        ureldatefmt_formatNumeric(m_icuRelativeDateTimeFormatter, value, icuUnit, buf, bufSize + 1, &status);
    } else {
        ureldatefmt_format(m_icuRelativeDateTimeFormatter, value, icuUnit, buf, bufSize + 1, &status);
    }

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }
    return new UTF16String(buf, bufSize);
}

ArrayObject* IntlRelativeTimeFormatObject::formatToParts(ExecutionState& state, double value, String* unit)
{
    // https://tc39.es/ecma402/#sec-PartitionRelativeTimePattern
    if (std::isinf(value) || std::isnan(value) || !IS_IN_TIME_RANGE(value)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "value is valid in RelativeTimeFormat formatToParts()");
    }

    // https://tc39.es/ecma402/#sec-singularrelativetimeunit
    URelativeDateTimeUnit icuUnit = icuRelativeTimeUnitFromString(unit);

    if (icuUnit == UDAT_REL_UNIT_COUNT) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "unit is invalid in RelativeTimeFormat formatToParts()");
    }

    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UFormattedRelativeDateTime> formattedRelativeDateTime(ureldatefmt_openResult(&status), [](UFormattedRelativeDateTime* f) { ureldatefmt_closeResult(f); });
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    if (m_numeric->equals("always")) {
        ureldatefmt_formatNumericToResult(m_icuRelativeDateTimeFormatter, value, icuUnit, formattedRelativeDateTime.get(), &status);
    } else {
        ureldatefmt_formatToResult(m_icuRelativeDateTimeFormatter, value, icuUnit, formattedRelativeDateTime.get(), &status);
    }

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    const UFormattedValue* formattedValue = ureldatefmt_resultAsValue(formattedRelativeDateTime.get(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    int32_t strLength;
    const char16_t* str = ufmtval_getString(formattedValue, &strLength, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    UTF16StringDataNonGCStd resultString(str, strLength);

    LocalResourcePointer<UConstrainedFieldPosition> fpos(ucfpos_open(&status), [](UConstrainedFieldPosition* f) { ucfpos_close(f); });
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    ucfpos_constrainCategory(fpos.get(), UFIELD_CATEGORY_NUMBER, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
    }

    std::vector<Intl::NumberFieldItem> fields;

    while (true) {
        bool hasNext = ufmtval_nextPosition(formattedValue, fpos.get(), &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
        }
        if (!hasNext) {
            break;
        }

        int32_t type = ucfpos_getField(fpos.get(), &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
        }

        int32_t start, end;
        ucfpos_getIndexes(fpos.get(), &start, &end, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to format time");
        }

        Intl::NumberFieldItem item;
        item.start = start;
        item.end = end;
        item.type = type;

        fields.push_back(item);
    }

    Intl::convertICUNumberFieldToEcmaNumberField(fields, value, resultString);

    if (icuUnit == UDAT_REL_UNIT_SECOND) {
        unit = state.context()->staticStrings().lazySecond().string();
    } else if (icuUnit == UDAT_REL_UNIT_MINUTE) {
        unit = state.context()->staticStrings().lazyMinute().string();
    } else if (icuUnit == UDAT_REL_UNIT_HOUR) {
        unit = state.context()->staticStrings().lazyHour().string();
    } else if (icuUnit == UDAT_REL_UNIT_DAY) {
        unit = state.context()->staticStrings().lazyDay().string();
    } else if (icuUnit == UDAT_REL_UNIT_WEEK) {
        unit = state.context()->staticStrings().lazyWeek().string();
    } else if (icuUnit == UDAT_REL_UNIT_MONTH) {
        unit = state.context()->staticStrings().lazyMonth().string();
    } else if (icuUnit == UDAT_REL_UNIT_QUARTER) {
        unit = state.context()->staticStrings().lazyQuarter().string();
    } else {
        ASSERT(icuUnit == UDAT_REL_UNIT_YEAR);
        unit = state.context()->staticStrings().lazyYear().string();
    }

    AtomicString unitAtom(state, "unit");
    AtomicString typeAtom(state, "type");
    AtomicString valueAtom = state.context()->staticStrings().value;

    ArrayObject* result = new ArrayObject(state);
    for (size_t i = 0; i < fields.size(); i++) {
        const Intl::NumberFieldItem& item = fields[i];

        Object* o = new Object(state);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(typeAtom), ObjectPropertyDescriptor(Intl::icuNumberFieldToString(state, item.type, value, String::emptyString()), ObjectPropertyDescriptor::AllPresent));
        auto sub = resultString.substr(item.start, item.end - item.start);
        o->defineOwnPropertyThrowsException(state, ObjectPropertyName(valueAtom), ObjectPropertyDescriptor(new UTF16String(sub.data(), sub.length()), ObjectPropertyDescriptor::AllPresent));
        if (item.type != -1) {
            o->defineOwnPropertyThrowsException(state, ObjectPropertyName(unitAtom), ObjectPropertyDescriptor(unit, ObjectPropertyDescriptor::AllPresent));
        }

        result->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, i), ObjectPropertyDescriptor(o, ObjectPropertyDescriptor::AllPresent));
    }

    return result;
}

} // namespace Escargot
#endif
#endif
