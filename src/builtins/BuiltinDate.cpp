/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/DateObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/NativeFunctionObject.h"

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#include "intl/IntlDateTimeFormat.h"
#endif

namespace Escargot {

#define FOR_EACH_DATE_VALUES(F)                     \
    /* Name, DateSetterTypr, Setter length, UTC? */ \
    F(Milliseconds, Time, 1, false)                 \
    F(UTCMilliseconds, Time, 1, true)               \
    F(Seconds, Time, 2, false)                      \
    F(UTCSeconds, Time, 2, true)                    \
    F(Minutes, Time, 3, false)                      \
    F(UTCMinutes, Time, 3, true)                    \
    F(Hours, Time, 4, false)                        \
    F(UTCHours, Time, 4, true)                      \
    F(Date, Day, 1, false)                          \
    F(UTCDate, Day, 1, true)                        \
    F(Month, Day, 2, false)                         \
    F(UTCMonth, Day, 2, true)                       \
    F(FullYear, Day, 3, false)                      \
    F(UTCFullYear, Day, 3, true)


bool isInValidRange(double year, double month, double date, double hour, double minute, double second, double millisecond)
{
    if (std::isnan(year) || std::isnan(month) || std::isnan(date) || std::isnan(hour) || std::isnan(minute) || std::isnan(second) || std::isnan(millisecond)) {
        return false;
    }
    if (std::isinf(year) || std::isinf(month) || std::isinf(date) || std::isinf(hour) || std::isinf(minute) || std::isinf(second) || std::isinf(millisecond)) {
        return false;
    }

    int32_t max32 = std::numeric_limits<int32_t>::max();
    int64_t max64 = std::numeric_limits<int64_t>::max();
    if (std::abs(year) > max32 || std::abs(month) > max32 || std::abs(date) > max32
        || std::abs(hour) > max32 || std::abs(minute) > max32 || std::abs(second) > max64 || std::abs(millisecond) > max64) {
        return false;
    }
    return true;
}

static Value builtinDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        DateObject d(state);
        d.setTimeValue(DateObject::currentTime());
        return d.toFullString(state);
    } else {
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
            return constructorRealm->globalObject()->datePrototype();
        });
        DateObject* thisObject = new DateObject(state, proto);

        if (argc == 0) {
            thisObject->setTimeValue(DateObject::currentTime());
        } else if (argc == 1) {
            // https://www.ecma-international.org/ecma-262/6.0/#sec-date-value
            Value v = argv[0];
            // If Type(value) is Object and value has a [[DateValue]] internal slot, then
            if (v.isObject() && v.asObject()->isDateObject()) {
                // Let tv be thisTimeValue(value).
                thisObject->setTimeValue(v.asObject()->asDateObject()->primitiveValue());
                // Else
            } else {
                // Let v be ToPrimitive(value).
                v = v.toPrimitive(state);
                // If Type(v) is String, then
                if (v.isString()) {
                    thisObject->setTimeValue(state, v);
                } else {
                    // Let tv be ToNumber(v).
                    double V = v.toNumber(state);
                    thisObject->setTimeValue(DateObject::timeClipToTime64(state, V));
                }
            }
        } else {
            // https://www.ecma-international.org/ecma-262/6.0/#sec-date-year-month-date-hours-minutes-seconds-ms
            // Assert: numberOfArgs ≥ 2.
            ASSERT(argc > 1);
            double args[7] = { 0, 0, 1, 0, 0, 0, 0 }; // default value of year, month, date, hour, minute, second, millisecond
            argc = (argc > 7) ? 7 : argc; // trim arguments so that they don't corrupt stack
            for (size_t i = 0; i < argc; i++) {
                args[i] = argv[i].toNumber(state);
            }
            double year = args[0];
            double month = args[1];
            double date = args[2];
            double hour = args[3];
            double minute = args[4];
            double second = args[5];
            double millisecond = args[6];
            if ((int)year >= 0 && (int)year <= 99) {
                year += 1900;
            }
            if (UNLIKELY(!isInValidRange(year, month, date, hour, minute, second, millisecond))) {
                thisObject->setTimeValueAsNaN();
            } else {
                thisObject->setTimeValue(state, (int)year, (int)month, (int)date, (int)hour, (int)minute, second, millisecond);
            }
        }
        return thisObject;
    }
}

static Value builtinDateNow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(DateObject::currentTime());
}

static Value builtinDateParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value str = argv[0].toPrimitive(state, Value::PreferString);
    if (str.isString()) {
        DateObject d(state);
        d.setTimeValue(state, str);
        return Value(Value::DoubleToIntConvertibleTestNeeds, d.primitiveValue());
    }
    return Value(Value::NanInit);
}

static Value builtinDateUTC(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    DateObject d(state);
    double args[7] = { std::numeric_limits<double>::quiet_NaN(), 0, 1, 0, 0, 0, 0 }; // default value of year, month, date, hour, minute, second, millisecond
    argc = (argc > 7) ? 7 : argc; // trim arguments so that they don't corrupt stack
    for (size_t i = 0; i < argc; i++) {
        args[i] = argv[i].toNumber(state);
    }
    double year = args[0];
    double month = args[1];
    double date = args[2];
    double hour = args[3];
    double minute = args[4];
    double second = args[5];
    double millisecond = args[6];

    if (!std::isnan(year)) {
        int yi = (int)year;
        if (yi >= 0 && yi <= 99) {
            yi += 1900;
            year = yi;
        }
    }

    if (UNLIKELY(!isInValidRange(year, month, date, hour, minute, second, millisecond))) {
        d.setTimeValueAsNaN();
    } else {
        d.setTimeValue(state, year, month, date, hour, minute, second, millisecond, false);
    }
    return Value(Value::DoubleToIntConvertibleTestNeeds, d.primitiveValue());
}

#define RESOLVE_THIS_BINDING_TO_DATE(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                            \
    if (!thisValue.isObject() || !thisValue.asObject()->isDateObject()) {                                                                                                                                                                   \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_ThisNotDateObject); \
    }                                                                                                                                                                                                                                       \
    DateObject* NAME = thisValue.asObject()->asDateObject();

static Value builtinDateGetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, getTime);
    return Value(Value::DoubleToIntConvertibleTestNeeds, thisObject->primitiveValue());
}

static Value builtinDateValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, valueOf);
    double val = thisObject->primitiveValue();
    return Value(Value::DoubleToIntConvertibleTestNeeds, val);
}

static Value builtinDateToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
    return thisObject->toFullString(state);
}

static Value builtinDateToDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
    return thisObject->toDateString(state);
}

static Value builtinDateToTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
    return thisObject->toTimeString(state);
}

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#define INTL_DATE_TIME_FORMAT_FORMAT(REQUIRED, DEFUALT)                                                                                         \
    double x = thisObject->primitiveValue();                                                                                                    \
    if (std::isnan(x)) {                                                                                                                        \
        return new ASCIIStringFromExternalMemory("Invalid Date");                                                                               \
    }                                                                                                                                           \
    Value locales, options;                                                                                                                     \
    if (argc >= 1) {                                                                                                                            \
        locales = argv[0];                                                                                                                      \
    }                                                                                                                                           \
    if (argc >= 2) {                                                                                                                            \
        options = argv[1];                                                                                                                      \
    }                                                                                                                                           \
    auto dateTimeOption = IntlDateTimeFormatObject::toDateTimeOptions(state, options, String::fromASCII(REQUIRED), String::fromASCII(DEFUALT)); \
    IntlDateTimeFormatObject* dateFormat = new IntlDateTimeFormatObject(state, locales, dateTimeOption);                                        \
    auto result = dateFormat->format(state, x);                                                                                                 \
    return new UTF16String(result.data(), result.length());
#endif

static Value builtinDateToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    INTL_DATE_TIME_FORMAT_FORMAT("any", "all")
#else
    return thisObject->toLocaleFullString(state);
#endif
}

static Value builtinDateToLocaleDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    INTL_DATE_TIME_FORMAT_FORMAT("date", "date")
#else
    return thisObject->toLocaleDateString(state);
#endif
}

static Value builtinDateToLocaleTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toString);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    INTL_DATE_TIME_FORMAT_FORMAT("time", "time")
#else
    return thisObject->toLocaleTimeString(state);
#endif
}

static Value builtinDateToUTCString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toUTCString);
    return thisObject->toUTCString(state, state.context()->staticStrings().toUTCString.string());
}

static Value builtinDateToISOString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, toISOString);
    return thisObject->toISOString(state);
}

static Value builtinDateToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);

    Value tv = Value(thisObject).toPrimitive(state, Value::PreferNumber);
    if (tv.isNumber() && (std::isnan(tv.asNumber()) || std::isinf(tv.asNumber()))) {
        return Value(Value::Null);
    }

    Value isoFunc = thisObject->get(state, ObjectPropertyName(state.context()->staticStrings().toISOString)).value(state, thisObject);
    return Object::call(state, isoFunc, thisObject, 0, nullptr);
}

#define DECLARE_STATIC_DATE_GETTER(Name, unused1, unused2, unused3)                                                                  \
    static Value builtinDateGet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                \
        RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, get##Name);                                                                   \
        if (!(thisObject->isValid()))                                                                                                \
            return Value(UnconvertibleDoubleToInt32(std::numeric_limits<double>::quiet_NaN()));                                      \
        return Value(thisObject->get##Name(state));                                                                                  \
    }

FOR_EACH_DATE_VALUES(DECLARE_STATIC_DATE_GETTER);
DECLARE_STATIC_DATE_GETTER(Day, -, -, -);
DECLARE_STATIC_DATE_GETTER(UTCDay, -, -, -);

enum DateSetterType : unsigned {
    Time,
    Day
};

static Value builtinDateSetHelper(ExecutionState& state, DateSetterType setterType, size_t length, bool utc, Value thisValue, size_t argc, Value* argv, const AtomicString& name)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, name);
    DateObject* d = thisObject;

    // Read the current [[DateValue]] first (before any ToNumber conversions)
    // To keep a record of the original state
    double originalDateValue = d->primitiveValue();
    bool isOriginalDateValid = d->isValid();

    if (setterType == DateSetterType::Day && length == 3) {
        // setFullYear, setUTCFullYear case
        if (!isOriginalDateValid) {
            d->setTimeValue(DateObject::timeClipToTime64(state, 0));
            d->setTimeValue(d->getTimezoneOffset(state) * TimeConstant::MsPerMinute);
            originalDateValue = d->primitiveValue();
            isOriginalDateValid = true;
        }
    }

    if (argc < 1) {
        d->setTimeValueAsNaN();
        return Value(Value::NanInit);
    }

    // Read date components from original state (before ToNumber calls)
    double year = 0, month = 0, date = 0, hour = 0, minute = 0, second = 0, millisecond = 0;

    if (isOriginalDateValid) {
        if (!utc) {
            year = d->getFullYear(state);
            month = d->getMonth(state);
            date = d->getDate(state);

            hour = d->getHours(state);
            minute = d->getMinutes(state);
            second = d->getSeconds(state);
            millisecond = d->getMilliseconds(state);
        } else {
            year = d->getUTCFullYear(state);
            month = d->getUTCMonth(state);
            date = d->getUTCDate(state);

            hour = d->getUTCHours(state);
            minute = d->getUTCMinutes(state);
            second = d->getUTCSeconds(state);
            millisecond = d->getUTCMilliseconds(state);
        }
    }

    // Convert arguments to numbers (this may cause side effects)
    switch (setterType) {
    case DateSetterType::Day:
        if ((length >= 3) && (argc > length - 3))
            year = argv[length - 3].toNumber(state);
        if ((length >= 2) && (argc > length - 2))
            month = argv[length - 2].toNumber(state);
        if ((length >= 1) && (argc > length - 1))
            date = argv[length - 1].toNumber(state);
        break;
    case DateSetterType::Time:
        if ((length >= 4) && (argc > length - 4))
            hour = argv[length - 4].toNumber(state);
        if ((length >= 3) && (argc > length - 3))
            minute = argv[length - 3].toNumber(state);
        if ((length >= 2) && (argc > length - 2))
            second = argv[length - 2].toNumber(state);
        if ((length >= 1) && (argc > length - 1))
            millisecond = argv[length - 1].toNumber(state);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Check if original date value was NaN
    if (std::isnan(originalDateValue)) {
        return Value(Value::NanInit);
    }

    bool convertToUTC = !utc;

    if (UNLIKELY(!isInValidRange(year, month, date, hour, minute, second, millisecond))) {
        d->setTimeValueAsNaN();
    } else {
        d->setTimeValue(state, year, month, date, hour, minute, second, millisecond, convertToUTC);
    }

    return Value(Value::DoubleToIntConvertibleTestNeeds, d->primitiveValue());
}

#define DECLARE_STATIC_DATE_SETTER(Name, setterType, length, utc)                                                                                              \
    static Value builtinDateSet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)                           \
    {                                                                                                                                                          \
        return Value(builtinDateSetHelper(state, DateSetterType::setterType, length, utc, thisValue, argc, argv, state.context()->staticStrings().set##Name)); \
    }

FOR_EACH_DATE_VALUES(DECLARE_STATIC_DATE_SETTER);

static Value builtinDateSetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, setTime);
    if (argc > 0) {
        thisObject->setTimeValue(DateObject::timeClipToTime64(state, argv[0].toNumber(state)));
        return Value(Value::DoubleToIntConvertibleTestNeeds, thisObject->primitiveValue());
    } else {
        thisObject->setTimeValueAsNaN();
        return Value(Value::NanInit);
    }
}

static Value builtinDateGetYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, getYear);
    if (!(thisObject->isValid())) {
        return Value(Value::NanInit);
    }
    int ret = thisObject->getFullYear(state) - 1900;
    return Value(ret);
}

static Value builtinDateSetYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, setYear);
    DateObject* d = thisObject;

    if (!(d->isValid())) {
        d->setTimeValue(DateObject::timeClipToTime64(state, 0));
        d->setTimeValue(d->getTimezoneOffset(state) * TimeConstant::MsPerMinute);
    }
    ASSERT(d->isValid());

    if (argc < 1) {
        d->setTimeValueAsNaN();
        return Value(Value::NanInit);
    }

    double y;
    int month, date, hour, minute, second, millisecond;

    // Let y be ToNumber(year).
    y = argv[0].toNumber(state);
    // If y is NaN, set the [[DateValue]] internal slot of this Date object to NaN and return NaN.
    if (std::isnan(y)) {
        d->setTimeValueAsNaN();
        return Value(Value::NanInit);
    }

    month = d->getMonth(state);
    date = d->getDate(state);
    hour = d->getHours(state);
    minute = d->getMinutes(state);
    second = d->getSeconds(state);
    millisecond = d->getMilliseconds(state);

    double yyyy;
    double yAsInteger = Value(Value::DoubleToIntConvertibleTestNeeds, y).toInteger(state);
    // If y is not NaN and 0 ≤ ToInteger(y) ≤ 99, let yyyy be ToInteger(y) + 1900.
    if (0 <= yAsInteger && yAsInteger <= 99) {
        yyyy = 1900 + yAsInteger;
    } else {
        // Else, let yyyy be y.
        yyyy = y;
    }

    if (d->isValid()) {
        d->setTimeValue(state, yyyy, month, date, hour, minute, second, millisecond);
    }

    return Value(Value::DoubleToIntConvertibleTestNeeds, d->primitiveValue());
}

static Value builtinDateGetTimezoneOffset(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DATE(thisObject, Date, getTimezoneOffset);
    if (!(thisObject->isValid()))
        return Value(Value::NanInit);
    return Value(thisObject->getTimezoneOffset(state));
}

static Value builtinDateToPrimitive(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    Value O = thisValue;
    // If Type(O) is not Object, throw a TypeError exception.
    if (!O.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    bool tryFirstIsString = false;
    // If hint is the String value "string" or the String value "default", then
    if (argv[0].isString() && (argv[0].asString()->equals("string") || argv[0].asString()->equals("default"))) {
        // Let tryFirst be "string".
        tryFirstIsString = true;
    } else if (argv[0].isString() && argv[0].asString()->equals("number")) {
        // Else if hint is the String value "number", then
        // Let tryFirst be "number".
        tryFirstIsString = false;
    } else {
        // Else, throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    // Return ? OrdinaryToPrimitive(O, tryFirst).
    if (tryFirstIsString) {
        return O.ordinaryToPrimitive(state, Value::PreferString);
    } else {
        return O.ordinaryToPrimitive(state, Value::PreferNumber);
    }
}

void GlobalObject::initializeDate(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->date(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Date), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installDate(ExecutionState& state)
{
    m_date = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Date, builtinDateConstructor, 7), NativeFunctionObject::__ForBuiltinConstructor__);
    m_date->setGlobalIntrinsicObject(state);

    m_datePrototype = new PrototypeObject(state);
    m_datePrototype->setGlobalIntrinsicObject(state, true);

    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_date->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().now),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().now, builtinDateNow, 0, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_date->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().parse),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().parse, builtinDateParse, 1, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_date->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().UTC),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().UTC, builtinDateUTC, 7, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTime),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTime, builtinDateGetTime, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setTime),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setTime, builtinDateSetTime, 1, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().valueOf),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().valueOf, builtinDateValueOf, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinDateToString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toDateString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toDateString, builtinDateToDateString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toTimeString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toTimeString, builtinDateToTimeString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleString, builtinDateToLocaleString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleDateString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleDateString, builtinDateToLocaleDateString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleTimeString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleTimeString, builtinDateToLocaleTimeString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toISOString),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toISOString, builtinDateToISOString, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toJSON),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toJSON, builtinDateToJSON, 1, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    FunctionObject* toUTCString = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toUTCString, builtinDateToUTCString, 0, NativeFunctionInfo::Strict));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toUTCString),
                                             ObjectPropertyDescriptor(toUTCString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toGMTString),
                                             ObjectPropertyDescriptor(toUTCString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getYear),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getYear, builtinDateGetYear, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setYear),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setYear, builtinDateSetYear, 1, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTimezoneOffset),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTimezoneOffset, builtinDateGetTimezoneOffset, 0, NativeFunctionInfo::Strict)),
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toPrimitive)),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.toPrimitive]")), builtinDateToPrimitive, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
#define DATE_DEFINE_GETTER(dname, unused1, unused2, unused3)                                                                                                                                                                  \
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get##dname),                                                                                                          \
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get##dname, builtinDateGet##dname, 0, NativeFunctionInfo::Strict)), \
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FOR_EACH_DATE_VALUES(DATE_DEFINE_GETTER);
    DATE_DEFINE_GETTER(Day, -, -, -);
    DATE_DEFINE_GETTER(UTCDay, -, -, -);

#define DATE_DEFINE_SETTER(dname, unused1, length, unused3)                                                                                                                                                                        \
    m_datePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set##dname),                                                                                                               \
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set##dname, builtinDateSet##dname, length, NativeFunctionInfo::Strict)), \
                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FOR_EACH_DATE_VALUES(DATE_DEFINE_SETTER);

    m_date->setFunctionPrototype(state, m_datePrototype);

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Date),
                        ObjectPropertyDescriptor(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
