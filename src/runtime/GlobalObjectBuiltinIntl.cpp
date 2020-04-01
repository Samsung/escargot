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
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"
#include "ArrayObject.h"
#include "VMInstance.h"
#include "NativeFunctionObject.h"
#include "DateObject.h"
#include "Intl.h"
#include "IntlCollator.h"
#include "IntlNumberFormat.h"
#include "IntlDateTimeFormat.h"

namespace Escargot {

static Value builtinIntlCollatorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (newTarget.hasValue()) {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.3.1
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // IntlCollator::initializeCollator(state, collator, locales, options);
        return IntlCollator::create(state, locales, options);
    } else {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.2
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.Collator(locales, options),
        // where Intl.Collator is the standard built-in constructor defined in 10.1.3.
        if (thisValue.isUndefined() || (thisValue.isObject() && thisValue.asObject() == state.context()->globalObject()->intl())) {
            return IntlCollator::create(state, locales, options);
        } else {
            // Let obj be the result of calling ToObject passing the this value as the argument.
            Object* collator = thisValue.toObject(state);
            // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
            if (!collator->isExtensible(state)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.Collator function must be extensible");
            }
            IntlCollator::initialize(state, collator, locales, options);
            return collator;
        }
    }
}

static Value builtinIntlCollatorCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    FunctionObject* callee = state.resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* colllator = callee->asObject();

    String* x = argv[0].toString(state);
    String* y = argv[1].toString(state);

    return Value(IntlCollator::compare(state, colllator, x, y));
}

static Value builtinIntlCollatorCompareGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* compareFunctionString = String::fromASCII("compareFunction");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, compareFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new NativeFunctionObject(state, NativeFunctionInfo(strings->compare, builtinIntlCollatorCompare, 2, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, compareFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static Value builtinIntlCollatorResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedCollator")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    auto r = IntlCollator::resolvedOptions(state, internalSlot);
    Object* result = new Object(state);
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("caseFirst")), ObjectPropertyDescriptor(Value(r.caseFirst), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("collation")), ObjectPropertyDescriptor(r.collation, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("ignorePunctuation")), ObjectPropertyDescriptor(Value(r.ignorePunctuation), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("locale")), ObjectPropertyDescriptor(r.locale, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("numeric")), ObjectPropertyDescriptor(Value(r.numeric), ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("sensitivity")), ObjectPropertyDescriptor(r.sensitivity, ObjectPropertyDescriptor::AllPresent));
    result->defineOwnProperty(state, ObjectPropertyName(state, String::fromASCII("usage")), ObjectPropertyDescriptor(r.usage, ObjectPropertyDescriptor::AllPresent));
    return result;
}

static Value builtinIntlCollatorSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.Collator.
    const auto& availableLocales = state.context()->globalObject()->intlCollatorAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return Intl::supportedLocales(state, availableLocales, requestedLocales, options);
}


static Value builtinIntlDateTimeFormatConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        Value locales, options;
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.DateTimeFormat(locales, options), where Intl.DateTimeFormat is the standard built-in constructor defined in 12.1.3.
        if (thisValue.isUndefined() || thisValue.equalsTo(state, state.context()->globalObject()->intl())) {
            return IntlDateTimeFormat::create(state, locales, options);
        }
        // Let obj be the result of calling ToObject passing the this value as the argument.
        Object* obj = thisValue.toObject(state);
        // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
        if (!obj->isExtensible(state)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.DateTimeFormat function must be extensible");
        }
        // Call the InitializeDateTimeFormat abstract operation (defined in 12.1.1.1) with arguments obj, locales, and options.
        IntlDateTimeFormat::initialize(state, obj, locales, options);
        return obj;
    } else {
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        Value locales, options;
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        return IntlDateTimeFormat::create(state, locales, options);
    }
}

static Value builtinIntlDateTimeFormatFormat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    FunctionObject* callee = state.resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* dateTimeFormat = callee->asObject();
    double value;
    if (argc == 0 || argv[0].isUndefined()) {
        value = DateObject::currentTime();
    } else {
        value = argv[0].toNumber(state);
    }
    auto result = IntlDateTimeFormat::format(state, dateTimeFormat, value);

    return Value(new UTF16String(result.data(), result.length()));
}

static Value builtinIntlDateTimeFormatFormatGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* formatFunctionString = String::fromASCII("format");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, formatFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new NativeFunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlDateTimeFormatFormat, 0, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, formatFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static void setFormatOpt(ExecutionState& state, Object* internalSlot, Object* result, const char* pName)
{
    String* prop = String::fromASCII(pName);

    ObjectGetResult r;
    r = internalSlot->get(state, ObjectPropertyName(state, prop));

    if (r.hasValue()) {
        result->defineOwnProperty(state, ObjectPropertyName(state, prop), ObjectPropertyDescriptor(Value(r.value(state, internalSlot)), ObjectPropertyDescriptor::AllPresent));
    }
}

static Value builtinIntlDateTimeFormatResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedDateTimeFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }
    Object* internalSlot = thisValue.asObject()->internalSlot();

    Object* result = new Object(state);

    setFormatOpt(state, internalSlot, result, "locale");
    setFormatOpt(state, internalSlot, result, "calendar");
    setFormatOpt(state, internalSlot, result, "numberingSystem");
    setFormatOpt(state, internalSlot, result, "timeZone");
    setFormatOpt(state, internalSlot, result, "hour12");
    setFormatOpt(state, internalSlot, result, "era");
    setFormatOpt(state, internalSlot, result, "year");
    setFormatOpt(state, internalSlot, result, "month");
    setFormatOpt(state, internalSlot, result, "weekday");
    setFormatOpt(state, internalSlot, result, "day");
    setFormatOpt(state, internalSlot, result, "hour");
    setFormatOpt(state, internalSlot, result, "minute");
    setFormatOpt(state, internalSlot, result, "second");
    setFormatOpt(state, internalSlot, result, "timeZoneName");

    return Value(result);
}

static Value builtinIntlDateTimeFormatSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.DateTimeFormat.
    const auto& availableLocales = state.context()->globalObject()->intlDateTimeFormatAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return Intl::supportedLocales(state, availableLocales, requestedLocales, options);
}

static Value builtinIntlNumberFormatFormat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    FunctionObject* callee = state.resolveCallee();
    if (!callee->hasInternalSlot() || !callee->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* numberFormat = callee->asObject();

    double number = argv[0].toNumber(state);
    auto result = IntlNumberFormat::format(state, numberFormat, number);

    return new UTF16String(result.data(), result.length());
}

static Value builtinIntlNumberFormatFormatGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    String* formatFunctionString = String::fromASCII("format");
    FunctionObject* fn;
    auto g = internalSlot->get(state, ObjectPropertyName(state, formatFunctionString));
    if (g.hasValue()) {
        fn = g.value(state, internalSlot).asFunction();
    } else {
        const StaticStrings* strings = &state.context()->staticStrings();
        fn = new NativeFunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlNumberFormatFormat, 1, NativeFunctionInfo::Strict));
        internalSlot->set(state, ObjectPropertyName(state, formatFunctionString), Value(fn), internalSlot);
        fn->setInternalSlot(internalSlot);
    }

    return fn;
}

static Value builtinIntlNumberFormatConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (newTarget.hasValue()) {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.3.1
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        return IntlNumberFormat::create(state, locales, options);
    } else {
        // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-10.1.2
        Value locales, options;
        // If locales is not provided, then let locales be undefined.
        // If options is not provided, then let options be undefined.
        if (argc >= 1) {
            locales = argv[0];
        }
        if (argc >= 2) {
            options = argv[1];
        }
        // If this is the standard built-in Intl object defined in 8 or undefined, then
        // Return the result of creating a new object as if by the expression new Intl.Collator(locales, options),
        // where Intl.Collator is the standard built-in constructor defined in 10.1.3.
        if (thisValue.isUndefined() || (thisValue.isObject() && thisValue.asObject() == state.context()->globalObject()->intl())) {
            Value callArgv[] = { locales, options };
            return IntlNumberFormat::create(state, locales, options);
        } else {
            // Let obj be the result of calling ToObject passing the this value as the argument.
            Object* obj = thisValue.toObject(state);
            // If the [[Extensible]] internal property of obj is false, throw a TypeError exception.
            if (!obj->isExtensible(state)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "This value of Intl.NumberFormat function must be extensible");
            }
            IntlNumberFormat::initialize(state, obj, locales, options);
            return obj;
        }
    }
}

static Value builtinIntlNumberFormatResolvedOptions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->hasInternalSlot() || !thisValue.asObject()->internalSlot()->hasOwnProperty(state, ObjectPropertyName(state, String::fromASCII("initializedNumberFormat")))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Method called on incompatible receiver");
    }

    Object* internalSlot = thisValue.asObject()->internalSlot();
    Object* result = new Object(state);

    setFormatOpt(state, internalSlot, result, "locale");
    setFormatOpt(state, internalSlot, result, "numberingSystem");
    setFormatOpt(state, internalSlot, result, "style");
    setFormatOpt(state, internalSlot, result, "currency");
    setFormatOpt(state, internalSlot, result, "currencyDisplay");
    setFormatOpt(state, internalSlot, result, "minimumIntegerDigits");
    setFormatOpt(state, internalSlot, result, "minimumFractionDigits");
    setFormatOpt(state, internalSlot, result, "maximumFractionDigits");
    setFormatOpt(state, internalSlot, result, "minimumSignificantDigits");
    setFormatOpt(state, internalSlot, result, "maximumSignificantDigits");
    setFormatOpt(state, internalSlot, result, "useGrouping");
    return result;
}

static Value builtinIntlNumberFormatSupportedLocalesOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If options is not provided, then let options be undefined.
    Value locales = argv[0];
    Value options;
    if (argc >= 2) {
        options = argv[1];
    }
    // Let availableLocales be the value of the [[availableLocales]] internal property of the standard built-in object that is the initial value of Intl.NumberFormat.
    const auto& availableLocales = state.context()->globalObject()->intlNumberFormatAvailableLocales();
    // Let requestedLocales be the result of calling the CanonicalizeLocaleList abstract operation (defined in 9.2.1) with argument locales.
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // Return the result of calling the SupportedLocales abstract operation (defined in 9.2.8) with arguments availableLocales, requestedLocales, and options.
    return Intl::supportedLocales(state, availableLocales, requestedLocales, options);
}

static Value builtinIntlGetCanonicalLocales(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argv != nullptr);
    // Let ll be ? CanonicalizeLocaleList(locales).
    ValueVector ll = Intl::canonicalizeLocaleList(state, argv[0]);
    // Return CreateArrayFromList(ll);
    return Object::createArrayFromList(state, ll);
}

void GlobalObject::installIntl(ExecutionState& state)
{
    m_intl = new Object(state);
    m_intl->setGlobalIntrinsicObject(state);

    const StaticStrings* strings = &state.context()->staticStrings();
    defineOwnProperty(state, ObjectPropertyName(strings->Intl),
                      ObjectPropertyDescriptor(m_intl, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlCollator = new NativeFunctionObject(state, NativeFunctionInfo(strings->Collator, builtinIntlCollatorConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_intlCollator->setGlobalIntrinsicObject(state);

    FunctionObject* compareFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->compare, builtinIntlCollatorCompareGetter, 0, NativeFunctionInfo::Strict));
    m_intlCollator->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().compare,
                                                                              ObjectPropertyDescriptor(JSGetterSetter(compareFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlCollator->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlCollatorResolvedOptions, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlCollator->getFunctionPrototype(state).asObject()->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                                              ObjectPropertyDescriptor(strings->Object.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlCollator->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlCollatorSupportedLocalesOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlDateTimeFormat = new NativeFunctionObject(state, NativeFunctionInfo(strings->DateTimeFormat, builtinIntlDateTimeFormatConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_intlDateTimeFormat->setGlobalIntrinsicObject(state);

    FunctionObject* formatFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlDateTimeFormatFormatGetter, 0, NativeFunctionInfo::Strict));
    m_intlDateTimeFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().format,
                                                                                    ObjectPropertyDescriptor(JSGetterSetter(formatFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlDateTimeFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlDateTimeFormatResolvedOptions, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlDateTimeFormat->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlDateTimeFormatSupportedLocalesOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlNumberFormat = new NativeFunctionObject(state, NativeFunctionInfo(strings->NumberFormat, builtinIntlNumberFormatConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_intlNumberFormat->setGlobalIntrinsicObject(state);

    formatFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->format, builtinIntlNumberFormatFormatGetter, 0, NativeFunctionInfo::Strict));
    m_intlNumberFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().format,
                                                                                  ObjectPropertyDescriptor(JSGetterSetter(formatFunction, Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intlNumberFormat->getFunctionPrototype(state).asObject()->defineOwnProperty(state, state.context()->staticStrings().resolvedOptions,
                                                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resolvedOptions, builtinIntlNumberFormatResolvedOptions, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));

    m_intlNumberFormat->defineOwnProperty(state, state.context()->staticStrings().supportedLocalesOf,
                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->supportedLocalesOf, builtinIntlNumberFormatSupportedLocalesOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));


    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->Collator),
                              ObjectPropertyDescriptor(m_intlCollator, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->DateTimeFormat),
                              ObjectPropertyDescriptor(m_intlDateTimeFormat, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->NumberFormat),
                              ObjectPropertyDescriptor(m_intlNumberFormat, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* getCanonicalLocales = new NativeFunctionObject(state, NativeFunctionInfo(strings->getCanonicalLocales, builtinIntlGetCanonicalLocales, 0, NativeFunctionInfo::Strict));
    m_intl->defineOwnProperty(state, ObjectPropertyName(strings->getCanonicalLocales),
                              ObjectPropertyDescriptor(getCanonicalLocales, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot

#endif // ENABLE_ICU
