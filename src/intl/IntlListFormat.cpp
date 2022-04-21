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
#include "runtime/ArrayObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/IteratorObject.h"
#include "Intl.h"
#include "IntlListFormat.h"

#if defined(ENABLE_INTL_LISTFORMAT)

namespace Escargot {

IntlListFormatObject::IntlListFormatObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : DerivedObject(state, proto)
{
    // https://tc39.es/ecma402/#sec-Intl.ListFormat
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 67) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Intl.ListFormat needs 67+ version of ICU");
    }
#endif

    auto& staticStrings = state.context()->staticStrings();

    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // Set options to ? GetOptionsObject(options).
    if (options.isUndefined()) {
        options = new Object(state, Object::PrototypeIsNull);
    }
    if (!options.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "options must be object");
    }
    Object* optionsObject = options.asObject();
    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    Value matcherValues[2] = { staticStrings.lazyLookup().string(), staticStrings.lazyBestFit().string() };
    Value matcher = Intl::getOption(state, optionsObject, staticStrings.lazyLocaleMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);
    // Set opt.[[localeMatcher]] to matcher.
    opt.insert(std::make_pair("localeMatcher", matcher.asString()));
    // Let localeData be %ListFormat%.[[LocaleData]].
    // Let r be ResolveLocale(%ListFormat%.[[AvailableLocales]], requestedLocales, opt, %ListFormat%.[[RelevantExtensionKeys]], localeData).
    const auto& availableLocales = state.context()->vmInstance()->intlListFormatAvailableLocales();
    StringMap r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, nullptr, 0, nullptr);

    // Set listFormat.[[Locale]] to r.[[locale]].
    m_locale = r.at("locale");
    // Let type be ? GetOption(options, "type", "string", « "conjunction", "disjunction", "unit" », "conjunction").
    // Set listFormat.[[Type]] to type.
    Value typeValues[] = { staticStrings.lazyConjunction().string(), staticStrings.lazyDisjunction().string(), staticStrings.lazyUnit().string() };
    Value type = Intl::getOption(state, optionsObject, staticStrings.lazyType().string(), Intl::StringValue, typeValues, 3, typeValues[0]);
    m_type = type.asString();
    // Let style be ? GetOption(options, "style", "string", « "long", "short", "narrow" », "long").
    // Set listFormat.[[Style]] to style.
    Value styleValues[] = { staticStrings.lazyNarrow().string(), staticStrings.lazyShort().string(), staticStrings.lazyLong().string() };
    Value style = Intl::getOption(state, optionsObject, staticStrings.lazyStyle().string(), Intl::StringValue, styleValues, 3, styleValues[2]);
    m_style = style.asString();
    // Let dataLocale be r.[[dataLocale]].
    // Let dataLocaleData be localeData.[[<dataLocale>]].
    // Let dataLocaleTypes be dataLocaleData.[[<type>]].
    // Set listFormat.[[Templates]] to dataLocaleTypes.[[<style>]].
    // Return listFormat.

    UListFormatterType listFormatterType;
    if (m_type->equals("conjunction")) {
        listFormatterType = ULISTFMT_TYPE_AND;
    } else if (m_type->equals("disjunction")) {
        listFormatterType = ULISTFMT_TYPE_OR;
    } else {
        ASSERT(m_type->equals("unit"));
        listFormatterType = ULISTFMT_TYPE_UNITS;
    }
    UListFormatterWidth listFormatterStyle;
    if (m_style->equals("long")) {
        listFormatterStyle = ULISTFMT_WIDTH_WIDE;
    } else if (m_style->equals("short")) {
        listFormatterStyle = ULISTFMT_WIDTH_SHORT;
    } else {
        ASSERT(m_style->equals("narrow"));
        listFormatterStyle = ULISTFMT_WIDTH_NARROW;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_icuListFormatter = ulistfmt_openForType(m_locale->toNonGCUTF8StringData().data(), listFormatterType, listFormatterStyle, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize ListFormat");
        return;
    }

    addFinalizer([](Object* obj, void* data) {
        IntlListFormatObject* self = (IntlListFormatObject*)obj;
        ulistfmt_close(self->m_icuListFormatter);
    },
                 nullptr);
}

// https://tc39.es/ecma402/#sec-createstringlistfromiterable
static StringVector stringListFromIterable(ExecutionState& state, const Value& iterable)
{
    // If iterable is undefined, then
    if (iterable.isUndefined()) {
        // Return a new empty List.
        return StringVector();
    }
    // Let iteratorRecord be ? GetIterator(iterable).
    IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, iterable);
    // Let list be a new empty List.
    StringVector list;
    // Let next be true.
    // Repeat, while next is not false,
    while (true) {
        // Set next to ? IteratorStep(iteratorRecord).
        Optional<Object*> next = IteratorObject::iteratorStep(state, iteratorRecord);
        // If next is not false, then
        if (next) {
            // Let nextValue be ? IteratorValue(next).
            Value nextValue = IteratorObject::iteratorValue(state, next.value());
            // If Type(nextValue) is not String, then
            if (!nextValue.isString()) {
                // Let error be ThrowCompletion(a newly created TypeError object).
                // Return ? IteratorClose(iteratorRecord, error).
                IteratorObject::iteratorClose(state, iteratorRecord, ErrorObject::createBuiltinError(state, ErrorObject::TypeError, "iterator value is not String"), true);
            }
            // Append nextValue to the end of the List list.
            list.pushBack(nextValue.asString());
        } else {
            break;
        }
    }

    // Return list.
    return list;
}

class StringVectorToUCharList {
public:
    StringVectorToUCharList(const StringVector& v)
    {
        m_strings.resize(v.size());
        m_stringLengths = new int32_t[v.size()];

        for (size_t i = 0; i < m_strings.size(); i++) {
            auto data = v[i]->toUTF16StringData();
            m_strings[i] = new UChar[data.size() + 1];
            memcpy(m_strings[i], data.data(), sizeof(UChar) * data.size());
            m_strings[i][data.size()] = 0;
            m_stringLengths[i] = data.size();
        }
    }

    ~StringVectorToUCharList()
    {
        for (size_t i = 0; i < m_strings.size(); i++) {
            delete[] m_strings[i];
        }
        delete[] m_stringLengths;
    }

    UChar** strings()
    {
        return m_strings.data();
    }

    int32_t* stringLengths()
    {
        return m_stringLengths;
    }

    int32_t stringCount()
    {
        return m_strings.size();
    }

private:
    std::vector<UChar*> m_strings;
    int32_t* m_stringLengths;
};

Value IntlListFormatObject::format(ExecutionState& state, const Value& list)
{
    // https://tc39.es/ecma402/#sec-Intl.ListFormat.prototype.format
    // Let lf be the this value.
    // Perform ? RequireInternalSlot(lf, [[InitializedListFormat]]).
    // Let stringList be ? StringListFromIterable(list).
    StringVector stringList = stringListFromIterable(state, list);
    // Return FormatList(lf, stringList).
    StringVectorToUCharList ucharList(stringList);
    auto result = INTL_ICU_STRING_BUFFER_OPERATION(ulistfmt_format, m_icuListFormatter, ucharList.strings(), ucharList.stringLengths(), ucharList.stringCount());
    if (U_FAILURE(result.first)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format string list");
    }
    return Value(new UTF16String(result.second.data(), result.second.length()));
}

Value IntlListFormatObject::formatToParts(ExecutionState& state, const Value& list)
{
#define THROW_IF_FAILED()                                                                              \
    if (U_FAILURE(status)) {                                                                           \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to format string list"); \
    }
    // https://tc39.es/ecma402/#sec-Intl.ListFormat.prototype.formatToParts
    // Let lf be the this value.
    // Perform ? RequireInternalSlot(lf, [[InitializedListFormat]]).
    // Let stringList be ? StringListFromIterable(list).
    StringVector stringList = stringListFromIterable(state, list);
    // Return FormatListToParts(lf, stringList).
    StringVectorToUCharList ucharList(stringList);

    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UFormattedList> uresult(
        ulistfmt_openResult(&status),
        [](UFormattedList* uresult) {
            ulistfmt_closeResult(uresult);
        });

    ulistfmt_formatStringsToResult(m_icuListFormatter, ucharList.strings(), ucharList.stringLengths(), ucharList.stringCount(), uresult.get(), &status);
    THROW_IF_FAILED()

    // we should not close UFormattedValue. it is freed by ulistfmt_closeResult
    const UFormattedValue* formattedValue = ulistfmt_resultAsValue(uresult.get(), &status);

    ArrayObject* resultArray = new ArrayObject(state);

    int32_t resultStringLength = 0;
    const UChar* resultStringPointer = ufmtval_getString(formattedValue, &resultStringLength, &status);
    THROW_IF_FAILED()

    LocalResourcePointer<UConstrainedFieldPosition> iterator(
        ucfpos_open(&status),
        [](UConstrainedFieldPosition* uresult) {
            ucfpos_close(uresult);
        });
    THROW_IF_FAILED()

    ucfpos_constrainField(iterator.get(), UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, &status);
    THROW_IF_FAILED()

    auto& staticStrings = state.context()->staticStrings();
    AtomicString literal = staticStrings.lazyLiteral();
    AtomicString type = staticStrings.lazyType();
    AtomicString value = staticStrings.value;
    AtomicString element(state, "element");

    int32_t resultIndex = 0;
    int32_t previousEnd = 0;
    while (true) {
        bool next = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
        THROW_IF_FAILED()

        if (!next) {
            break;
        }

        int32_t begin = 0;
        int32_t end = 0;
        ucfpos_getIndexes(iterator.get(), &begin, &end, &status);
        THROW_IF_FAILED()

        if (previousEnd < begin) {
            Object* part = new Object(state);
            part->set(state, type, literal.string(), part);
            part->set(state, value, new UTF16String(&resultStringPointer[previousEnd], begin - previousEnd), part);
            resultArray->setIndexedProperty(state, Value(resultIndex++), part, resultArray);
        }
        previousEnd = end;

        Object* part = new Object(state);
        part->set(state, type, element.string(), part);
        part->set(state, value, new UTF16String(&resultStringPointer[begin], end - begin), part);
        resultArray->setIndexedProperty(state, Value(resultIndex++), part, resultArray);
    }

    if (previousEnd < resultStringLength) {
        Object* part = new Object(state);
        part->set(state, type, literal.string(), part);
        part->set(state, value, new UTF16String(&resultStringPointer[previousEnd], resultStringLength - previousEnd), part);
        resultArray->setIndexedProperty(state, Value(resultIndex++), part, resultArray);
    }

    return resultArray;
}

} // namespace Escargot
#endif
#endif
