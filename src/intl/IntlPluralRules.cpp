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

void* IntlPluralRulesObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(IntlPluralRulesObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_locale));
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_type));
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_notation));
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_roundingMode));
        GC_set_bit(desc, GC_WORD_OFFSET(IntlPluralRulesObject, m_trailingZeroDisplay));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(IntlPluralRulesObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

IntlPluralRulesObject::IntlPluralRulesObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : DerivedObject(state, proto)
    , m_locale(nullptr)
    , m_type(nullptr)
    , m_minimumIntegerDigits(0)
    , m_minimumFractionDigits(0)
    , m_maximumFractionDigits(0)
    , m_icuPluralRules(nullptr)
    , m_icuNumberFormat(nullptr)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-initializepluralrules
    // [[InitializedPluralRules]], [[Locale]], [[Type]], [[MinimumIntegerDigits]], [[MinimumFractionDigits]], [[MaximumFractionDigits]], [[MinimumSignificantDigits]], [[MaximumSignificantDigits]

    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    Object* optionObject;
    // If options is undefined, then
    if (options.isUndefined()) {
        // Let options be ObjectCreate(null).
        optionObject = new Object(state, Object::PrototypeIsNull);
    } else {
        // Let options be ? ToObject(options).
        optionObject = options.toObject(state);
    }

    // Let opt be a new Record.
    StringMap opt;
    // Let matcher be ? GetOption(options, "localeMatcher", "string", « "lookup", "best fit" », "best fit").
    // Set opt.[[localeMatcher]] to matcher.
    Value localeMatcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    String* matcher = Intl::getOption(state, optionObject, state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, localeMatcherValues, 2, localeMatcherValues[1]).asString();
    opt.insert(std::make_pair("matcher", matcher));

    // Let t be ? GetOption(options, "type", "string", « "cardinal", "ordinal" », "cardinal").
    Value typeValues[2] = { state.context()->staticStrings().lazyCardinal().string(), state.context()->staticStrings().lazyOrdinal().string() };
    String* t = Intl::getOption(state, optionObject, state.context()->staticStrings().lazyType().string(), Intl::StringValue, typeValues, 2, typeValues[0]).asString();

    // Let notation be ? GetOption(options, "notation", "string", « "standard", "scientific", "engineering", "compact" », "standard").
    Value notationValues[4] = { state.context()->staticStrings().lazyStandard().string(), state.context()->staticStrings().lazyScientific().string(), state.context()->staticStrings().lazyEngineering().string(), state.context()->staticStrings().lazyCompact().string() };
    Value notation = Intl::getOption(state, optionObject, state.context()->staticStrings().lazyNotation().string(), Intl::StringValue, notationValues, 4, notationValues[0]);
    m_notation = notation.asString();

    // Perform ? SetNumberFormatDigitOptions(pluralRules, options, 0, 3).
    auto optionsResult = Intl::setNumberFormatDigitOptions(state, optionObject, 0, 3, m_notation);

    // Let localeData be %PluralRules%.[[LocaleData]].
    // Let r be ResolveLocale(%PluralRules%.[[AvailableLocales]], requestedLocales, opt, %PluralRules%.[[RelevantExtensionKeys]], localeData).
    // Set pluralRules.[[Locale]] to the value of r.[[locale]].
    auto r = Intl::resolveLocale(state, state.context()->vmInstance()->intlPluralRulesAvailableLocales(), requestedLocales, opt, nullptr, 0, nullptr);
    String* foundLocale = r.at("locale");

    m_minimumIntegerDigits = optionsResult.minimumIntegerDigits;
    m_roundingIncrement = optionsResult.roundingIncrement;
    m_trailingZeroDisplay = optionsResult.trailingZeroDisplay;
    m_minimumSignificantDigits = optionsResult.minimumSignificantDigits;
    m_maximumSignificantDigits = optionsResult.maximumSignificantDigits;
    m_minimumFractionDigits = optionsResult.minimumFractionDigits;
    m_maximumFractionDigits = optionsResult.maximumFractionDigits;
    m_roundingType = optionsResult.roundingType;
    m_roundingMode = optionsResult.roundingMode;
    m_computedRoundingPriority = optionsResult.computedRoundingPriority;

    UErrorCode status = U_ZERO_ERROR;
    UTF16StringDataNonGCStd skeleton;
    Intl::initNumberFormatSkeleton(state, optionsResult, m_notation, state.context()->staticStrings().lazyCompact().string(), skeleton);
    m_icuNumberFormat = unumf_openForSkeletonAndLocale((UChar*)skeleton.data(), skeleton.length(), foundLocale->toNonGCUTF8StringData().data(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to init PluralRules");
    }

    ASSERT(U_SUCCESS(status));

#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
#endif

    if (true
#if defined(ENABLE_RUNTIME_ICU_BINDER)
        && versionArray[0] >= 68
#endif
    ) {
        m_icuNumberRangeFormat = unumrf_openForSkeletonWithCollapseAndIdentityFallback((UChar*)skeleton.data(), skeleton.length(),
                                                                                       UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY, foundLocale->toNonGCUTF8StringData().data(), nullptr, &status);
        if (U_FAILURE(status)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to init PluralRules");
        }
    }

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

    addFinalizer([](PointerValue* obj, void* data) {
        IntlPluralRulesObject* self = (IntlPluralRulesObject*)obj;
        uplrules_close(self->m_icuPluralRules);
        unumf_close(self->m_icuNumberFormat);
        if (self->m_icuNumberRangeFormat) {
            unumrf_close(self->m_icuNumberRangeFormat.value());
        }
    },
                 nullptr);

    // Return pluralRules.
}

String* IntlPluralRulesObject::resolvePlural(ExecutionState& state, double number)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-resolveplural
    // If n is not a finite Number, then
    if (std::isinf(number)) {
        // Return "other".
        return String::fromASCII("other");
    }
    UErrorCode status = U_ZERO_ERROR;

    LocalResourcePointer<UFormattedNumber> formattedNumber(unumf_openResult(&status), [](UFormattedNumber* f) { unumf_closeResult(f); });
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural");
    }
    unumf_formatDouble(m_icuNumberFormat, number, formattedNumber.get(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural");
    }

    int32_t len = uplrules_selectFormatted(m_icuPluralRules, formattedNumber.get(), nullptr, 0, &status);
    UChar* buf = ALLOCA((len + 1) * sizeof(UChar), UChar);
    status = U_ZERO_ERROR;
    uplrules_selectFormatted(m_icuPluralRules, formattedNumber.get(), buf, len + 1, &status);
    ASSERT(U_SUCCESS(status));
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural");
    }

    return new UTF16String(buf, len);
}

String* IntlPluralRulesObject::resolvePluralRange(ExecutionState& state, double x, double y)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    if (!m_icuNumberRangeFormat) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Intl.PluralRules.formatRange needs 68+ version of ICU");
    }
#endif
    if (std::isnan(x) || std::isnan(y)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Failed to resolve Plural range");
    }

    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UFormattedNumberRange> uresult(unumrf_openResult(&status), [](UFormattedNumberRange* f) { unumrf_closeResult(f); });

    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural range");
    }

    unumrf_formatDoubleRange(m_icuNumberRangeFormat.value(), x, y, uresult.get(), &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural range");
    }

    int32_t len = uplrules_selectForRange(m_icuPluralRules, uresult.get(), nullptr, 0, &status);
    UChar* buf = ALLOCA((len + 1) * sizeof(UChar), UChar);
    status = U_ZERO_ERROR;
    uplrules_selectForRange(m_icuPluralRules, uresult.get(), buf, len + 1, &status);
    ASSERT(U_SUCCESS(status));
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Failed to resolve Plural range");
    }

    return new UTF16String(buf, len);
}

} // namespace Escargot
#endif
#endif
