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
#include "IntlCollator.h"
#include "VMInstance.h"

namespace Escargot {

static const char* const intlCollatorRelevantExtensionKeys[3] = { "co", "kn", "kf" };
static const int intlCollatorRelevantExtensionKeysLength = 3;
static const size_t indexOfExtensionKeyCo = 0;
static const size_t indexOfExtensionKeyKn = 1;
static const size_t indexOfExtensionKeyKf = 2;

static std::vector<std::string> sortLocaleData(String* locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo: {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.push_back(std::string());

        UErrorCode status = U_ZERO_ERROR;

        auto utf8 = locale->toUTF8StringData();
        UEnumeration* enumeration = ucol_getKeywordValuesForLocale("collation", utf8.data(), false, &status);
        if (U_SUCCESS(status)) {
            const char* collation;
            while ((collation = uenum_next(enumeration, nullptr, &status)) && U_SUCCESS(status)) {
                // 10.2.3 "The values "standard" and "search" must not be used as elements in any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co array."
                if (!strcmp(collation, "standard") || !strcmp(collation, "search"))
                    continue;

                // Map keyword values to BCP 47 equivalents.
                if (!strcmp(collation, "dictionary"))
                    collation = "dict";
                else if (!strcmp(collation, "gb2312han"))
                    collation = "gb2312";
                else if (!strcmp(collation, "phonebook"))
                    collation = "phonebk";
                else if (!strcmp(collation, "traditional"))
                    collation = "trad";

                keyLocaleData.push_back(std::string(collation));
            }
            uenum_close(enumeration);
        }
        break;
    }
    case indexOfExtensionKeyKn:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("true"));
        break;
    case indexOfExtensionKeyKf:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("lower"));
        keyLocaleData.push_back(std::string("upper"));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

static std::vector<std::string> searchLocaleData(String* locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    std::vector<std::string> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo:
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.push_back(std::string());
        break;
    case indexOfExtensionKeyKn:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("true"));
        break;
    case indexOfExtensionKeyKf:
        keyLocaleData.push_back(std::string("false"));
        keyLocaleData.push_back(std::string("lower"));
        keyLocaleData.push_back(std::string("upper"));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

Object* IntlCollator::create(ExecutionState& state, Context* realm, Value locales, Value options)
{
    Object* collator = new Object(state, realm->globalObject()->objectPrototype());
    initialize(state, collator, realm, locales, options);
    return collator;
}

IntlCollator::CollatorResolvedOptions IntlCollator::resolvedOptions(ExecutionState& state, Object* internalSlot)
{
    CollatorResolvedOptions opt;
    opt.locale = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().lazySmallLetterLocale())).value(state, internalSlot).toString(state);
    opt.sensitivity = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().lazySensitivity())).value(state, internalSlot).toString(state);
    opt.usage = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().lazyUsage())).value(state, internalSlot).toString(state);
    opt.collation = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().collation)).value(state, internalSlot).toString(state);
    opt.numeric = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().numeric)).value(state, internalSlot).toBoolean(state);
    opt.ignorePunctuation = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().lazyIgnorePunctuation())).value(state, internalSlot).toBoolean(state);
    opt.caseFirst = internalSlot->get(state, ObjectPropertyName(state.context()->staticStrings().caseFirst)).value(state, internalSlot).toString(state);
    return opt;
}

void IntlCollator::initialize(ExecutionState& state, Object* collator, Context* realm, Value locales, Value options)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.1.1

    collator->setPrototype(state, realm->globalObject()->intlCollator()->getFunctionPrototype(state));
    // If collator has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    AtomicString initializedIntlObject = state.context()->staticStrings().lazyInitializedIntlObject();
    if (collator->hasInternalSlot() && collator->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, ObjectStructurePropertyName(initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    // If options is undefined, then
    // Let options be the result of creating a new object as
    // if by the expression new Object() where Object is the standard built-in constructor with that name.
    if (options.isUndefined()) {
        options = new Object(state, Object::PrototypeIsNull);
    } else {
        // Let options be ToObject(options).
        options = options.toObject(state);
    }
    // Let u be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "usage", "string", a List containing the two String values "sort" and "search", and "sort".
    Value usageValues[2] = { state.context()->staticStrings().sort.string(), state.context()->staticStrings().search.string() };
    Value u = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyUsage().string(), Intl::StringValue, usageValues, 2, usageValues[0]);

    // Set the [[usage]] internal property of collator to u.
    collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazyUsage()), u, collator->internalSlot());

    // Let Collator be the standard built-in object that is the initial value of Intl.Collator.
    FunctionObject* Collator = realm->globalObject()->intlCollator();
    UNUSED_VARIABLE(Collator);

    // If u is "sort", then let localeData be the value of the [[sortLocaleData]] internal property of Collator;
    Intl::LocaleDataImplFunction localeData;
    if (u.equalsTo(state, state.context()->staticStrings().sort.string())) {
        localeData = sortLocaleData;
    } else {
        // else let localeData be the value of the [[searchLocaleData]] internal property of Collator.
        localeData = searchLocaleData;
    }

    // Let opt be a new Record.
    StringMap opt;

    // Let matcher be the result of calling the GetOption abstract operation with arguments
    // options, "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit"
    Value matcherValues[2] = { state.context()->staticStrings().lazyLookup().string(), state.context()->staticStrings().lazyBestFit().string() };
    Value matcher = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyLocaleMatcher().string(), Intl::StringValue, matcherValues, 2, matcherValues[1]);
    // Set opt.[[localeMatcher]] to matcher.
    opt.insert(std::make_pair("localeMatcher", matcher.toString(state)));

    // Table 1 – Collator options settable through both extension keys and options properties
    // Key Property    Type            Values
    // kn  numeric    "boolean"
    // kf  caseFirst  "string"   "upper", "lower", "false"
    std::function<void(String * keyColumn, Value propertyColumn, Intl::OptionValueType type, Value * values, size_t valuesSize)> doTable1 = [&](String* keyColumn, Value propertyColumn, Intl::OptionValueType type, Value* values, size_t valuesSize) {
        // Let key be the name given in the Key column of the row.
        Value key = keyColumn;

        // Let value be the result of calling the GetOption abstract operation, passing as arguments options, the name given in the Property column of the row,
        // the string given in the Type column of the row,
        // a List containing the Strings given in the Values column of the row or undefined if no strings are given, and undefined.

        Value value = Intl::getOption(state, options.asObject(), propertyColumn, type, values, valuesSize, Value());
        // If the string given in the Type column of the row is "boolean" and value is not undefined, then
        // Let value be ToString(value).
        if (type == Intl::BooleanValue && !value.isUndefined()) {
            value = value.toString(state);
        }
        // Set opt.[[<key>]] to value.
        opt.insert(std::make_pair(keyColumn->toNonGCUTF8StringData(), value.toString(state)));
    };

    doTable1(state.context()->staticStrings().lazyKn().string(), state.context()->staticStrings().numeric.string(), Intl::BooleanValue, nullptr, 0);
    Value caseFirstValue[3] = { state.context()->staticStrings().lazyUpper().string(), state.context()->staticStrings().lazyLower().string(), state.context()->staticStrings().stringFalse.string() };
    doTable1(state.context()->staticStrings().lazyKf().string(), state.context()->staticStrings().caseFirst.string(), Intl::StringValue, caseFirstValue, 3);

    // Let relevantExtensionKeys be the value of the [[relevantExtensionKeys]] internal property of Collator.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of Collator, requestedLocales, opt, relevantExtensionKeys, and localeData.
    StringMap r = Intl::resolveLocale(state, realm->vmInstance()->intlCollatorAvailableLocales(), requestedLocales, opt, intlCollatorRelevantExtensionKeys, intlCollatorRelevantExtensionKeysLength, localeData);

    // Set the [[locale]] internal property of collator to the value of r.[[locale]].
    collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySmallLetterLocale()), r.at("locale"), collator->internalSlot());

    // Let i be 0.
    size_t i = 0;
    // Let len be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument "length".
    size_t len = intlCollatorRelevantExtensionKeysLength;
    // Repeat while i < len:
    while (i < len) {
        // Let key be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument ToString(i).
        const char* key = intlCollatorRelevantExtensionKeys[i];
        // If key is "co", then
        AtomicString property;
        Value value;
        if (strcmp(key, "co") == 0) {
            // Let property be "collation".
            property = state.context()->staticStrings().collation;
            // Let value be the value of r.[[co]].
            auto iter = r.find("co");
            // If value is null, then let value be "default".
            if (r.end() == iter || iter->second->equals("")) {
                value = state.context()->staticStrings().stringDefault.string();
            } else {
                value = iter->second;
            }
        } else if (strcmp(key, "kn") == 0) {
            // Table 1 – Collator options settable through both extension keys and options properties
            // Key Property    Type            Values
            // kn  numeric    "boolean"
            // kf  caseFirst  "string"   "upper", "lower", "false"

            // Else use the row of Table 1 that contains the value of key in the Key column:
            // Let property be the name given in the Property column of the row.
            property = state.context()->staticStrings().numeric;
            // Let value be the value of r.[[<key>]].
            value = r.at("kn");
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
            value = Value(value.equalsTo(state, state.context()->staticStrings().stringTrue.string()));

        } else if (strcmp(key, "kf") == 0) {
            // Table 1 – Collator options settable through both extension keys and options properties
            // Key Property    Type            Values
            // kn  numeric    "boolean"
            // kf  caseFirst  "string"   "upper", "lower", "false"

            // Else use the row of Table 1 that contains the value of key in the Key column:
            // Let property be the name given in the Property column of the row.
            property = state.context()->staticStrings().caseFirst;
            // Let value be the value of r.[[<key>]].
            value = r.at("kf");
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
        // Set the [[<property>]] internal property of collator to value.
        collator->internalSlot()->set(state, ObjectPropertyName(property), value, collator->internalSlot());
        // Increase i by 1.
        i++;
    }
    // Let s be the result of calling the GetOption abstract operation with arguments
    // options, "sensitivity", "string", a List containing the four String values "base", "accent", "case", and "variant", and undefined.
    Value sensitivityValue[4] = { state.context()->staticStrings().lazyBase().string(), state.context()->staticStrings().lazyAccent().string(), state.context()->staticStrings().lazyCase().string(), state.context()->staticStrings().lazyVariant().string() };
    Value s = Intl::getOption(state, options.asObject(), Value(state.context()->staticStrings().lazySensitivity().string()), Intl::StringValue, sensitivityValue, 4, Value());
    String* sensitivityString = s.toString(state);
    // If s is undefined, then
    // If u is "sort", then let s be "variant".
    // Else
    // Let dataLocale be the value of r.[[dataLocale]].
    // Let dataLocaleData be the result of calling the [[Get]] internal operation of localeData with argument dataLocale.
    // Let s be the result of calling the [[Get]] internal operation of dataLocaleData with argument "sensitivity".
    if (sensitivityString->equals("base")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySensitivity()), state.context()->staticStrings().lazyBase().string(), collator->internalSlot());
    } else if (sensitivityString->equals("accent")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySensitivity()), state.context()->staticStrings().lazyAccent().string(), collator->internalSlot());
    } else if (sensitivityString->equals("case")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySensitivity()), state.context()->staticStrings().lazyCase().string(), collator->internalSlot());
    } else {
        collator->internalSlot()->set(state, ObjectPropertyName(state.context()->staticStrings().lazySensitivity()), state.context()->staticStrings().lazyVariant().string(), collator->internalSlot());
    }

    // Let ip be the result of calling the GetOption abstract operation with arguments options, "ignorePunctuation", "boolean", undefined, and false.
    Value ip = Intl::getOption(state, options.asObject(), state.context()->staticStrings().lazyIgnorePunctuation().string(), Intl::BooleanValue, nullptr, 0, Value(false));
    // Set the [[ignorePunctuation]] internal property of collator to ip.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyIgnorePunctuation()), ObjectPropertyDescriptor(ip));
    // Set the [[boundCompare]] internal property of collator to undefined.
    // Set the [[initializedCollator]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyInitializedCollator()), ObjectPropertyDescriptor(Value(true)));
    // Return collator.

    {
        Object* internalSlot = collator->internalSlot();
        CollatorResolvedOptions opt = resolvedOptions(state, internalSlot);
        UErrorCode status = U_ZERO_ERROR;
        String* locale = opt.locale;
        UColAttributeValue strength = UCOL_DEFAULT;
        UColAttributeValue caseLevel = UCOL_OFF;
        UColAttributeValue alternate = UCOL_DEFAULT;
        UColAttributeValue numeric = UCOL_OFF;
        UColAttributeValue normalization = UCOL_ON; // normalization is always on. ecma-402 needs this
        UColAttributeValue caseFirst = UCOL_DEFAULT;

        if (opt.usage->equals("search")) {
            // If usage is search, we should append "-co-search" extension
            size_t u = locale->find("-u-");
            if (u == SIZE_MAX) {
                StringBuilder sb;
                sb.appendString(locale);
                sb.appendString("-u-co-search");
                locale = sb.finalize();
            } else {
                StringBuilder sb;
                sb.appendString(locale);
                sb.appendString("-co-search");
                locale = sb.finalize();
            }
        } else {
            ASSERT(opt.usage->equals("sort"));
        }


        String* sensitivity = opt.sensitivity;

        if (sensitivity->equals("base")) {
            strength = UCOL_PRIMARY;
        } else if (sensitivity->equals("accent")) {
            strength = UCOL_SECONDARY;
        } else if (sensitivity->equals("case")) {
            strength = UCOL_PRIMARY;
            caseLevel = UCOL_ON;
        } else {
            ASSERT(sensitivity->equals("variant"));
            strength = UCOL_TERTIARY;
        }

        if (opt.ignorePunctuation) {
            alternate = UCOL_SHIFTED;
        }

        if (opt.numeric) {
            numeric = UCOL_ON;
        }

        String* caseFirstString = opt.caseFirst;
        if (caseFirstString->equals("upper")) {
            caseFirst = UCOL_UPPER_FIRST;
        } else if (caseFirstString->equals("lower")) {
            caseFirst = UCOL_LOWER_FIRST;
        } else {
            ASSERT(caseFirstString->equals("false"));
            caseFirst = UCOL_OFF;
        }

        UCollator* ucollator = ucol_open(locale->toUTF8StringData().data(), &status);
        if (U_FAILURE(status)) {
            return;
        }

        ucol_setAttribute(ucollator, UCOL_STRENGTH, strength, &status);
        ucol_setAttribute(ucollator, UCOL_CASE_LEVEL, caseLevel, &status);
        ucol_setAttribute(ucollator, UCOL_ALTERNATE_HANDLING, alternate, &status);
        ucol_setAttribute(ucollator, UCOL_NUMERIC_COLLATION, numeric, &status);
        ucol_setAttribute(ucollator, UCOL_NORMALIZATION_MODE, normalization, &status);
        ucol_setAttribute(ucollator, UCOL_CASE_FIRST, caseFirst, &status);

        if (U_FAILURE(status)) {
            ucol_close(ucollator);
            return;
        }

        internalSlot->setExtraData(ucollator);

        GC_REGISTER_FINALIZER_NO_ORDER(internalSlot, [](void* obj, void*) {
            Object* self = (Object*)obj;
            ucol_close((UCollator*)self->extraData());
        },
                                       nullptr, nullptr, nullptr);
    }
}

int IntlCollator::compare(ExecutionState& state, Object* collator, String* a, String* b)
{
    if (a->equals(b)) {
        return 0;
    }

    auto utf16A = a->toUTF16StringData();
    auto utf16B = b->toUTF16StringData();

    UCollator* ucol = (UCollator*)collator->internalSlot()->extraData();
    UCollationResult result = ucol_strcoll(ucol, utf16A.data(), utf16A.length(), utf16B.data(), utf16B.length());

    switch (result) {
    case UCOL_LESS:
        return -1;
    case UCOL_EQUAL:
        return 0;
    case UCOL_GREATER:
        return 1;
    default:
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to compare string a and b");
    }

    ASSERT_NOT_REACHED();
    return 0;
}
} // namespace Escargot
#endif
