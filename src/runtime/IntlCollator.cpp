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

Object* IntlCollator::create(ExecutionState& state, Value locales, Value options)
{
    Object* collator = new Object(state);
    initialize(state, collator, locales, options);
    return collator;
}

IntlCollator::CollatorResolvedOptions IntlCollator::resolvedOptions(ExecutionState& state, Object* internalSlot)
{
    CollatorResolvedOptions opt;
    opt.locale = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("locale"))).value(state, internalSlot).toString(state);
    opt.sensitivity = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("sensitivity"))).value(state, internalSlot).toString(state);
    opt.usage = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("usage"))).value(state, internalSlot).toString(state);
    opt.collation = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("collation"))).value(state, internalSlot).toString(state);
    opt.numeric = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("numeric"))).value(state, internalSlot).toBoolean(state);
    opt.ignorePunctuation = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation"))).value(state, internalSlot).toBoolean(state);
    opt.caseFirst = internalSlot->get(state, ObjectPropertyName(state, String::fromASCII("caseFirst"))).value(state, internalSlot).toString(state);
    return opt;
}

void IntlCollator::initialize(ExecutionState& state, Object* collator, Value locales, Value options)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.1.1

    collator->setPrototype(state, state.context()->globalObject()->intlCollator()->getFunctionPrototype(state));
    // If collator has an [[initializedIntlObject]] internal property with value true, throw a TypeError exception.
    String* initializedIntlObject = String::fromASCII("initializedIntlObject");
    if (collator->hasInternalSlot() && collator->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, ObjectStructurePropertyName(state, initializedIntlObject)))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot initialize Intl Object twice");
    }

    // Set the [[initializedIntlObject]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, initializedIntlObject), ObjectPropertyDescriptor(Value(true)));

    // Let requestedLocales be the result of calling the
    // CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);

    // If options is undefined, then
    // Let options be the result of creating a new object as
    // if by the expression new Object() where Object is the standard built-in constructor with that name.
    if (options.isUndefined()) {
        options = new Object(state);
    } else {
        // Let options be ToObject(options).
        options = options.toObject(state);
    }
    // Let u be the result of calling the GetOption abstract operation (defined in 9.2.9) with arguments options,
    // "usage", "string", a List containing the two String values "sort" and "search", and "sort".
    ValueVector values;
    values.pushBack(String::fromASCII("sort"));
    values.pushBack(String::fromASCII("search"));
    Value u = Intl::getOption(state, options.asObject(), String::fromASCII("usage"), String::fromASCII("string"), values, String::fromASCII("sort"));

    // Set the [[usage]] internal property of collator to u.
    collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("usage")), u, collator->internalSlot());

    // Let Collator be the standard built-in object that is the initial value of Intl.Collator.
    FunctionObject* Collator = state.context()->globalObject()->intlCollator();

    // If u is "sort", then let localeData be the value of the [[sortLocaleData]] internal property of Collator;
    Intl::LocaleDataImplFunction localeData;
    if (u.equalsTo(state, String::fromASCII("sort"))) {
        localeData = sortLocaleData;
    } else {
        // else let localeData be the value of the [[searchLocaleData]] internal property of Collator.
        localeData = searchLocaleData;
    }

    // Let opt be a new Record.
    StringMap* opt = new StringMap();

    // Let matcher be the result of calling the GetOption abstract operation with arguments
    // options, "localeMatcher", "string", a List containing the two String values "lookup" and "best fit", and "best fit"
    values.clear();
    values.pushBack(String::fromASCII("lookup"));
    values.pushBack(String::fromASCII("best fit"));
    Value matcher = Intl::getOption(state, options.asObject(), String::fromASCII("localeMatcher"), String::fromASCII("string"), values, String::fromASCII("best fit"));
    // Set opt.[[localeMatcher]] to matcher.
    opt->insert(std::make_pair(String::fromASCII("localeMatcher"), matcher.toString(state)));

    // Table 1 – Collator options settable through both extension keys and options properties
    // Key Property    Type            Values
    // kn  numeric    "boolean"
    // kf  caseFirst  "string"   "upper", "lower", "false"
    std::function<void(String*, Value, String*, ValueVector)> doTable1 = [&](String* keyColumn, Value propertyColumn, String* typeColumn, ValueVector values) {
        // Let key be the name given in the Key column of the row.
        Value key = keyColumn;

        // Let value be the result of calling the GetOption abstract operation, passing as arguments options, the name given in the Property column of the row,
        // the string given in the Type column of the row,
        // a List containing the Strings given in the Values column of the row or undefined if no strings are given, and undefined.
        Value value = Intl::getOption(state, options.asObject(), propertyColumn, typeColumn, values, Value());
        // If the string given in the Type column of the row is "boolean" and value is not undefined, then
        // Let value be ToString(value).
        if (typeColumn->equals("boolean") && !value.isUndefined()) {
            value = value.toString(state);
        }
        // Set opt.[[<key>]] to value.
        opt->insert(std::make_pair(keyColumn, value.toString(state)));
    };

    doTable1(String::fromASCII("kn"), String::fromASCII("numeric"), String::fromASCII("boolean"), ValueVector());
    ValueVector tempV;
    tempV.pushBack(String::fromASCII("upper"));
    tempV.pushBack(String::fromASCII("lower"));
    tempV.pushBack(String::fromASCII("false"));
    doTable1(String::fromASCII("kf"), String::fromASCII("caseFirst"), String::fromASCII("string"), tempV);

    // Let relevantExtensionKeys be the value of the [[relevantExtensionKeys]] internal property of Collator.
    // Let r be the result of calling the ResolveLocale abstract operation (defined in 9.2.5) with the [[availableLocales]]
    // internal property of Collator, requestedLocales, opt, relevantExtensionKeys, and localeData.
    StringMap* r = Intl::resolveLocale(state, state.context()->globalObject()->intlCollatorAvailableLocales(), requestedLocales, opt, intlCollatorRelevantExtensionKeys, intlCollatorRelevantExtensionKeysLength, localeData);

    // Set the [[locale]] internal property of collator to the value of r.[[locale]].
    String* localeInternalString = String::fromASCII("locale");
    collator->internalSlot()->set(state, ObjectPropertyName(state, localeInternalString), r->at(localeInternalString), collator->internalSlot());

    // Let i be 0.
    size_t i = 0;
    // Let len be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument "length".
    size_t len = intlCollatorRelevantExtensionKeysLength;
    // Repeat while i < len:
    while (i < len) {
        // Let key be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument ToString(i).
        const char* key = intlCollatorRelevantExtensionKeys[i];
        // If key is "co", then
        const char* property;
        Value value;
        if (strcmp(key, "co") == 0) {
            // Let property be "collation".
            property = "collation";
            // Let value be the value of r.[[co]].
            auto iter = r->find(String::fromASCII("co"));
            // If value is null, then let value be "default".
            if (r->end() == iter || iter->second->equals("")) {
                value = String::fromASCII("default");
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
            property = "numeric";
            // Let value be the value of r.[[<key>]].
            value = r->at(String::fromASCII("kn"));
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
            value = Value(value.equalsTo(state, String::fromASCII("true")));

        } else if (strcmp(key, "kf") == 0) {
            // Table 1 – Collator options settable through both extension keys and options properties
            // Key Property    Type            Values
            // kn  numeric    "boolean"
            // kf  caseFirst  "string"   "upper", "lower", "false"

            // Else use the row of Table 1 that contains the value of key in the Key column:
            // Let property be the name given in the Property column of the row.
            property = "caseFirst";
            // Let value be the value of r.[[<key>]].
            value = r->at(String::fromASCII("kf"));
            // If the name given in the Type column of the row is "boolean",
            // then let value be the result of comparing value with "true".
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
        // Set the [[<property>]] internal property of collator to value.
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII(property)), value, collator->internalSlot());
        // Increase i by 1.
        i++;
    }
    // Let s be the result of calling the GetOption abstract operation with arguments
    // options, "sensitivity", "string", a List containing the four String values "base", "accent", "case", and "variant", and undefined.
    tempV.clear();
    tempV.pushBack(String::fromASCII("base"));
    tempV.pushBack(String::fromASCII("accent"));
    tempV.pushBack(String::fromASCII("case"));
    tempV.pushBack(String::fromASCII("variant"));
    Value s = Intl::getOption(state, options.asObject(), Value(String::fromASCII("sensitivity")), String::fromASCII("string"), tempV, Value());
    String* sensitivityString = s.toString(state);
    // If s is undefined, then
    // If u is "sort", then let s be "variant".
    // Else
    // Let dataLocale be the value of r.[[dataLocale]].
    // Let dataLocaleData be the result of calling the [[Get]] internal operation of localeData with argument dataLocale.
    // Let s be the result of calling the [[Get]] internal operation of dataLocaleData with argument "sensitivity".
    if (sensitivityString->equals("base")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("base"), collator->internalSlot());
    } else if (sensitivityString->equals("accent")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("accent"), collator->internalSlot());
    } else if (sensitivityString->equals("case")) {
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("case"), collator->internalSlot());
    } else {
        collator->internalSlot()->set(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), String::fromASCII("variant"), collator->internalSlot());
    }

    // Let ip be the result of calling the GetOption abstract operation with arguments options, "ignorePunctuation", "boolean", undefined, and false.
    Value ip = Intl::getOption(state, options.asObject(), String::fromASCII("ignorePunctuation"), String::fromASCII("boolean"), ValueVector(), Value(false));
    // Set the [[ignorePunctuation]] internal property of collator to ip.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation")), ObjectPropertyDescriptor(ip));
    // Set the [[boundCompare]] internal property of collator to undefined.
    // Set the [[initializedCollator]] internal property of collator to true.
    collator->ensureInternalSlot(state)->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")), ObjectPropertyDescriptor(Value(true)));
    // Return collator.

    {
        Object* internalSlot = collator->internalSlot();
        CollatorResolvedOptions opt = resolvedOptions(state, internalSlot);
        UErrorCode status = U_ZERO_ERROR;
        String* locale = opt.locale;
        UCollator* ucollator = ucol_open(locale->toUTF8StringData().data(), &status);
        if (U_FAILURE(status)) {
            return;
        }

        UColAttributeValue strength = UCOL_PRIMARY;
        UColAttributeValue caseLevel = UCOL_OFF;
        UColAttributeValue caseFirst = UCOL_OFF;

        String* sensitivity = opt.sensitivity;
        if (sensitivity->equals("accent")) {
            strength = UCOL_SECONDARY;
        } else if (sensitivity->equals("case")) {
            caseLevel = UCOL_ON;
        } else if (sensitivity->equals("variant")) {
            strength = UCOL_TERTIARY;
        } else if (!sensitivity->equals("base")) {
            ASSERT_NOT_REACHED();
        }

        String* caseFirstString = opt.caseFirst;
        if (caseFirstString->equals("upper")) {
            caseFirst = UCOL_UPPER_FIRST;
        } else if (caseFirstString->equals("lower")) {
            caseFirst = UCOL_LOWER_FIRST;
        }

        ucol_setAttribute(ucollator, UCOL_STRENGTH, strength, &status);
        ucol_setAttribute(ucollator, UCOL_CASE_LEVEL, caseLevel, &status);
        ucol_setAttribute(ucollator, UCOL_CASE_FIRST, caseFirst, &status);

        bool numeric = opt.numeric;
        ucol_setAttribute(ucollator, UCOL_NUMERIC_COLLATION, numeric ? UCOL_ON : UCOL_OFF, &status);

        // FIXME: Setting UCOL_ALTERNATE_HANDLING to UCOL_SHIFTED causes punctuation and whitespace to be
        // ignored. There is currently no way to ignore only punctuation.
        bool ignorePunctuation = opt.ignorePunctuation;
        ucol_setAttribute(ucollator, UCOL_ALTERNATE_HANDLING, ignorePunctuation ? UCOL_SHIFTED : UCOL_DEFAULT, &status);

        // "The method is required to return 0 when comparing Strings that are considered canonically
        // equivalent by the Unicode standard."
        ucol_setAttribute(ucollator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
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
    ASSERT(a != nullptr);
    ASSERT(b != nullptr);

    UCollator* ucol = (UCollator*)collator->internalSlot()->extraData();

    auto utf16A = a->toUTF16StringData();
    auto utf16B = b->toUTF16StringData();

    UCharIterator iterA;
    UCharIterator iterB;

    uiter_setString(&iterA, (const UChar*)utf16A.data(), utf16A.length());
    uiter_setString(&iterB, (const UChar*)utf16B.data(), utf16B.length());

    UErrorCode status = U_ZERO_ERROR;
    auto result = ucol_strcollIter(ucol, &iterA, &iterB, &status);
    if (U_FAILURE(status)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to compare string a and b");
        return result;
    }
    return result;
}
} // namespace Escargot
#endif
