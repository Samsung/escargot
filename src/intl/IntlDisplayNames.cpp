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

namespace Escargot {

static const char* const intlDisplayNamesRelevantExtensionKeys[0] = {};
static size_t intlDisplayNamesRelevantExtensionKeysLength = 0;

static std::vector<std::string> localeDataDisplayNames(String* locale, size_t keyIndex)
{
    return std::vector<std::string>();
}

IntlDisplayNamesObject::IntlDisplayNamesObject(ExecutionState& state, Object* proto, Value locales, Value options)
    : Object(state, proto)
{
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    UVersionInfo versionArray;
    u_getVersion(versionArray);
    if (versionArray[0] < 62) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Intl.DisplayNames needs 51+ version of ICU");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "options must be object");
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
    StringMap r = Intl::resolveLocale(state, availableLocales, requestedLocales, opt, intlDisplayNamesRelevantExtensionKeys, intlDisplayNamesRelevantExtensionKeysLength, localeDataDisplayNames);

    // Let style be ? GetOption(options, "style", "string", « "narrow", "short", "long" », "long").
    Value styleValues[] = { staticStrings.lazyNarrow().string(), staticStrings.lazyShort().string(), staticStrings.lazyLong().string() };
    Value style = Intl::getOption(state, optionsObject, staticStrings.lazyStyle().string(), Intl::StringValue, styleValues, 3, styleValues[2]);
    // Set displayNames.[[Style]] to style.
    m_style = style.asString();
    // Let type be ? GetOption(options, "type", "string", « "language", "region", "script", "currency" », undefined).
    Value typeValues[] = { staticStrings.language.string(), staticStrings.region.string(), staticStrings.script.string(), staticStrings.lazyCurrency().string() };
    Value type = Intl::getOption(state, optionsObject, staticStrings.lazyType().string(), Intl::StringValue, typeValues, 4, Value());
    // If type is undefined, throw a TypeError exception.
    if (type.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "type of options is required");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "failed to initialize DisplayNames");
        return;
    }

    addFinalizer([](Object* obj, void* data) {
        IntlDisplayNamesObject* self = (IntlDisplayNamesObject*)obj;
        uldn_close(self->m_icuLocaleDisplayNames);
    },
                 nullptr);
}

} // namespace Escargot
#endif
