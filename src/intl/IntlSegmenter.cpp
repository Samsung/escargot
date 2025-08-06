#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlSegmenter.h"

#if defined(ENABLE_INTL_SEGMENTER)

namespace Escargot {

IntlSegmenterObject::IntlSegmenterObject(ExecutionState& state, Value locales, Value options)
    : IntlSegmenterObject(state, state.context()->globalObject()->intlSegmenterPrototype(), locales, options)
{
}

IntlSegmenterObject::IntlSegmenterObject(ExecutionState& state, Object* proto, Value locales, Value optionsInput)
    : DerivedObject(state, proto)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 69) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.NumberFormat needs 69+ version of ICU");
    }
#endif
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    Optional<Object*> options;
    if (!optionsInput.isUndefined()) {
        options = optionsInput.toObject(state);
    }

    StringMap opt;
    Value localeMatcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    String* matcher = localeMatcherValues[1].asString();
    if (options) {
        matcher = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, localeMatcherValues, 2, localeMatcherValues[1]).asString();
    }
    opt.insert(std::make_pair("matcher", matcher));

    StringMap r = Intl::resolveLocale(state, state.context()->vmInstance()->intlDurationFormatAvailableLocales(), requestedLocales, opt, nullptr, 0, nullptr);
    String* locale = r.at("locale");
    m_locale = locale;

    Value granularityValues[3] = { state.context()->staticStrings().lazyGrapheme().string(), state.context()->staticStrings().lazyWord().string(), state.context()->staticStrings().lazySentence().string() };
    m_granularity = granularityValues[0].asString();
    if (options) {
        m_granularity = Intl::getOption(state, options.value(), state.context()->staticStrings().lazyGranularity().string(), Intl::StringValue, granularityValues, 3, granularityValues[0]).asString();
    }

    UBreakIteratorType type = UBRK_CHARACTER;
    if (m_granularity->equals("grapheme")) {
        type = UBRK_CHARACTER;
    } else if (m_granularity->equals("word")) {
        type = UBRK_WORD;
    } else if (m_granularity->equals("sentence")) {
        type = UBRK_SENTENCE;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_icuSegmenter = ubrk_open(type, m_locale->toNonGCUTF8StringData().data(), nullptr, 0, &status);

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to initialize Segmenter");
    }

    addFinalizer([](PointerValue* obj, void* data) {
        IntlSegmenterObject* self = (IntlSegmenterObject*)obj;
        ubrk_close(self->m_icuSegmenter);
    },
                 nullptr);
}

Object* IntlSegmenterObject::resolvedOptions(ExecutionState& state)
{
    Object* options = new Object(state);
    auto& ss = state.context()->staticStrings();
    options->directDefineOwnProperty(state, ObjectPropertyName(ss.lazySmallLetterLocale()), ObjectPropertyDescriptor(m_locale, ObjectPropertyDescriptor::AllPresent));
    options->directDefineOwnProperty(state, ObjectPropertyName(ss.lazyGranularity()), ObjectPropertyDescriptor(m_granularity, ObjectPropertyDescriptor::AllPresent));
    return options;
}

IntlSegmentsObject* IntlSegmenterObject::segment(ExecutionState& state, String* string)
{
    UErrorCode status = U_ZERO_ERROR;
    auto newIterator = ubrk_clone(m_icuSegmenter, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to clone ICU break iterator");
    }
    String* u16String = new UTF16String(string->toUTF16StringData());
    ubrk_setText(newIterator, u16String->bufferAccessData().bufferAs16Bit, string->length(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to init ICU break iterator");
    }
    return new IntlSegmentsObject(state, string, u16String, m_granularity, newIterator);
}

static Object* createSegmentDataObject(ExecutionState& state, String* string, int32_t startIndex, int32_t endIndex, UBreakIterator* segmenter, String* granularity)
{
    Object* result = new Object(state);
    result->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazySegment()),
                                    ObjectPropertyDescriptor(string->substring(startIndex, endIndex), ObjectPropertyDescriptor::AllPresent));
    result->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().index),
                                    ObjectPropertyDescriptor(Value(startIndex), ObjectPropertyDescriptor::AllPresent));
    result->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().input),
                                    ObjectPropertyDescriptor(string, ObjectPropertyDescriptor::AllPresent));
    if (granularity->equals("word")) {
        int32_t ruleStatus = ubrk_getRuleStatus(segmenter);
        result->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyIsWordLike()),
                                        ObjectPropertyDescriptor(Value(!(ruleStatus >= UBRK_WORD_NONE && ruleStatus <= UBRK_WORD_NONE_LIMIT)), ObjectPropertyDescriptor::AllPresent));
    }
    return result;
}

IntlSegmentsObject::IntlSegmentsObject(ExecutionState& state, String* string, String* u16String, String* granularity, UBreakIterator* icuSegmenter)
    : DerivedObject(state, state.context()->globalObject()->intlSegmentsPrototype())
    , m_string(string)
    , m_u16String(u16String)
    , m_granularity(granularity)
    , m_icuSegmenter(icuSegmenter)
{
    addFinalizer([](PointerValue* obj, void* data) {
        IntlSegmentsObject* self = (IntlSegmentsObject*)obj;
        ubrk_close(self->m_icuSegmenter);
    },
                 nullptr);
}

Value IntlSegmentsObject::containing(ExecutionState& state, const Value& indexInput)
{
    double value = indexInput.toInteger(state);

    if (value < 0 || value >= m_string->length()) {
        return Value();
    }
    int32_t index = Value(Value::DoubleToIntConvertibleTestNeeds, value).toInt32(state);

    int32_t startIndex = ubrk_preceding(m_icuSegmenter, index + 1);
    if (startIndex == UBRK_DONE) {
        startIndex = 0;
    }
    int32_t endIndex = ubrk_following(m_icuSegmenter, index);
    if (endIndex == UBRK_DONE) {
        endIndex = m_string->length();
    }

    return createSegmentDataObject(state, m_string, startIndex, endIndex, m_icuSegmenter, m_granularity);
}

IntlSegmentsIteratorObject* IntlSegmentsObject::createIteratorObject(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    auto newIterator = ubrk_clone(m_icuSegmenter, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to clone ICU break iterator");
    }
    ubrk_first(newIterator);
    return new IntlSegmentsIteratorObject(state, this, newIterator);
}

IntlSegmentsIteratorObject::IntlSegmentsIteratorObject(ExecutionState& state, IntlSegmentsObject* segments, UBreakIterator* icuSegmenter)
    : IteratorObject(state, state.context()->globalObject()->intlSegmentsIteratorPrototype())
    , m_segments(segments)
    , m_icuSegmenter(icuSegmenter)
{
    addFinalizer([](PointerValue* obj, void* data) {
        IntlSegmentsIteratorObject* self = (IntlSegmentsIteratorObject*)obj;
        ubrk_close(self->m_icuSegmenter);
    },
                 nullptr);
}

std::pair<Value, bool> IntlSegmentsIteratorObject::advance(ExecutionState& state)
{
    int32_t startIndex = ubrk_current(m_icuSegmenter);
    int32_t endIndex = ubrk_next(m_icuSegmenter);
    if (endIndex == UBRK_DONE) {
        return std::make_pair(Value(), true);
    }

    return std::make_pair(Value(createSegmentDataObject(state, m_segments->string(), startIndex, endIndex, m_icuSegmenter, m_segments->granularity())), false);
}

} // namespace Escargot
#endif
#endif
