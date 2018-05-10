/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "VMInstance.h"
#include "DateObject.h"
#include "ErrorObject.h"

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

static Value builtinDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DateObject* thisObject;
    if (isNewExpression) {
        thisObject = thisValue.asObject()->asDateObject();

        if (argc == 0) {
            thisObject->setTimeValue(DateObject::currentTime());
        } else if (argc == 1) {
            Value v = argv[0].toPrimitive(state);
            if (v.isString()) {
                thisObject->setTimeValue(state, v);
            } else {
                double V = v.toNumber(state);
                thisObject->setTimeValue(DateObject::timeClip(state, V));
            }
        } else {
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
                return new ASCIIString("Invalid Date");
            }
            thisObject->setTimeValue(state, (int)year, (int)month, (int)date, (int)hour, (int)minute, second, millisecond);
        }
        return thisObject->toFullString(state);
    } else {
        DateObject d(state);
        d.setTimeValue(DateObject::currentTime());
        return d.toFullString(state);
    }
}

static Value builtinDateNow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value(DateObject::currentTime());
}

static Value builtinDateParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value str = argv[0].toPrimitive(state, Value::PreferString);
    if (str.isString()) {
        DateObject d(state);
        d.setTimeValue(state, str);
        return Value(d.primitiveValue());
    }
    return Value(std::numeric_limits<double>::quiet_NaN());
}

static Value builtinDateUTC(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DateObject d(state);
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

    if (!std::isnan(year) && (int)year >= 0 && (int)year <= 99) {
        year += 1900;
    }

    if (argc < 2) {
        d.setTimeValueAsNaN();
    } else if (UNLIKELY(!isInValidRange(year, month, date, hour, minute, second, millisecond))) {
        d.setTimeValueAsNaN();
    } else {
        d.setTimeValue(state, year, month, date, hour, minute, second, millisecond, false);
    }
    return Value(d.primitiveValue());
}

static Value builtinDateGetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getTime);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getTime.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return Value(thisObject->asDateObject()->primitiveValue());
}

static Value builtinDateValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, valueOf);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().valueOf.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    double val = thisObject->asDateObject()->primitiveValue();
    return Value(val);
}

static Value builtinDateToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toFullString(state);
}

static Value builtinDateToDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toDateString(state);
}

static Value builtinDateToTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    return thisObject->asDateObject()->toTimeString(state);
}

static Value builtinDateToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    return thisObject->asDateObject()->toLocaleFullString(state);
}

static Value builtinDateToLocaleDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    return thisObject->asDateObject()->toLocaleDateString(state);
}

static Value builtinDateToLocaleTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    return thisObject->asDateObject()->toLocaleTimeString(state);
}

static Value builtinDateToUTCString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toUTCString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toUTCString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toUTCString(state, state.context()->staticStrings().toUTCString.string());
}

static Value builtinDateToISOString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toISOString);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toISOString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toISOString(state);
}

static Value builtinDateToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toJSON);

    Value tv = thisValue.toPrimitive(state, Value::PreferNumber);
    if (tv.isNumber() && (std::isnan(tv.asNumber()) || std::isinf(tv.asNumber()))) {
        return Value(Value::Null);
    }

    ObjectGetResult isoFuncGetResult = thisObject->get(state, ObjectPropertyName(state.context()->staticStrings().toISOString));

    if (!(isoFuncGetResult.hasValue())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toJSON.string(), errorMessage_GlobalObject_ToISOStringNotCallable);
    }

    Value isoFunc = isoFuncGetResult.value(state, thisObject);

    if (!(isoFunc.isFunction())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toJSON.string(), errorMessage_GlobalObject_ToISOStringNotCallable);
    }

    return FunctionObject::call(state, isoFunc, thisObject, 0, nullptr);
}

#define DECLARE_STATIC_DATE_GETTER(Name, unused1, unused2, unused3)                                                                                                                                                                \
    static Value builtinDateGet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)                                                                                                      \
    {                                                                                                                                                                                                                              \
        RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, get##Name);                                                                                                                                                               \
        if (!(thisObject->isDateObject())) {                                                                                                                                                                                       \
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().get##Name.string(), errorMessage_GlobalObject_ThisNotDateObject); \
        }                                                                                                                                                                                                                          \
        if (!(thisObject->asDateObject()->isValid()))                                                                                                                                                                              \
            return Value(std::numeric_limits<double>::quiet_NaN());                                                                                                                                                                \
        return Value(thisObject->asDateObject()->get##Name(state));                                                                                                                                                                \
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
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, name);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, name.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    DateObject* d = thisObject->asDateObject();

    if (setterType == DateSetterType::Day && length == 3) {
        // setFullYear, setUTCFullYear case
        if (!(d->isValid())) {
            d->setTimeValue(DateObject::timeClip(state, 0));
            d->setTimeValue(d->getTimezoneOffset(state) * const_Date_msPerMinute);
        }
        ASSERT(d->isValid());
    }

    if (argc < 1) {
        d->setTimeValueAsNaN();
        return Value(d->primitiveValue());
    }

    double year = 0, month = 0, date = 0, hour = 0, minute = 0, second = 0, millisecond = 0;

    if (d->isValid()) {
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

    bool convertToUTC = !utc;

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

    if (UNLIKELY(!isInValidRange(year, month, date, hour, minute, second, millisecond))) {
        d->setTimeValueAsNaN();
    } else if (d->isValid()) {
        d->setTimeValue(state, year, month, date, hour, minute, second, millisecond, convertToUTC);
    }

    return Value(d->primitiveValue());
}

#define DECLARE_STATIC_DATE_SETTER(Name, setterType, length, utc)                                                                                              \
    static Value builtinDateSet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)                                  \
    {                                                                                                                                                          \
        return Value(builtinDateSetHelper(state, DateSetterType::setterType, length, utc, thisValue, argc, argv, state.context()->staticStrings().set##Name)); \
    }

FOR_EACH_DATE_VALUES(DECLARE_STATIC_DATE_SETTER);

static Value builtinDateSetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, setTime);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().setTime.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (argc > 0) {
        thisObject->asDateObject()->setTimeValue(DateObject::timeClip(state, argv[0].toNumber(state)));
        return Value(thisObject->asDateObject()->primitiveValue());
    } else {
        double value = std::numeric_limits<double>::quiet_NaN();
        thisObject->asDateObject()->setTimeValueAsNaN();
        return Value(value);
    }
}

static Value builtinDateGetYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getYear);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getYear.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!(thisObject->asDateObject()->isValid())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    int ret = thisObject->asDateObject()->getFullYear(state) - 1900;
    return Value(ret);
}

static Value builtinDateSetYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, setYear);
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().setYear.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    DateObject* d = thisObject->asDateObject();

    if (!(d->isValid())) {
        d->setTimeValue(DateObject::timeClip(state, 0));
        d->setTimeValue(d->getTimezoneOffset(state) * const_Date_msPerMinute);
    }
    ASSERT(d->isValid());

    if (argc < 1) {
        d->setTimeValueAsNaN();
        return Value(d->primitiveValue());
    }

    double year;
    int month, date, hour, minute, second, millisecond;

    year = argv[0].toNumber(state);
    month = d->getMonth(state);
    date = d->getDate(state);
    hour = d->getHours(state);
    minute = d->getMinutes(state);
    second = d->getSeconds(state);
    millisecond = d->getMilliseconds(state);

    if (0 <= year && year <= 99) {
        year += 1900;
    }

    if (std::isnan(year)) {
        d->setTimeValueAsNaN();
    } else if (d->isValid()) {
        d->setTimeValue(state, year, month, date, hour, minute, second, millisecond);
    }

    return Value(d->primitiveValue());
}

static Value builtinDateGetTimezoneOffset(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().getTimezoneOffset.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    Object* thisObject = thisValue.toObject(state);
    ;
    if (!(thisObject->isDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().getTimezoneOffset.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!(thisObject->asDateObject()->isValid()))
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getTimezoneOffset(state));
}

static Value builtinDateToPrimitive(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the this value.
    Value O = thisValue;
    // If Type(O) is not Object, throw a TypeError exception.
    if (!O.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), errorMessage_GlobalObject_ThisNotObject);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().toPrimitive.string(), errorMessage_GlobalObject_IllegalFirstArgument);
    }
    // Return ? OrdinaryToPrimitive(O, tryFirst).
    if (tryFirstIsString) {
        return O.ordinaryToPrimitive(state, Value::PreferString);
    } else {
        return O.ordinaryToPrimitive(state, Value::PreferNumber);
    }
}

void GlobalObject::installDate(ExecutionState& state)
{
    m_date = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Date, builtinDateConstructor, 7, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                    return new DateObject(state);
                                }),
                                FunctionObject::__ForBuiltin__);
    m_date->markThisObjectDontNeedStructureTransitionTable(state);
    m_date->setPrototype(state, m_functionPrototype);
    m_datePrototype = m_objectPrototype;
    m_datePrototype = new DateObject(state);
    m_datePrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_datePrototype->setPrototype(state, m_objectPrototype);

    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_date->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().now),
                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().now, builtinDateNow, 0, nullptr, NativeFunctionInfo::Strict)),
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_date->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().parse),
                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().parse, builtinDateParse, 1, nullptr, NativeFunctionInfo::Strict)),
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_date->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().UTC),
                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().UTC, builtinDateUTC, 7, nullptr, NativeFunctionInfo::Strict)),
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTime),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTime, builtinDateGetTime, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setTime),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setTime, builtinDateSetTime, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().valueOf),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().valueOf, builtinDateValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinDateToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toDateString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toDateString, builtinDateToDateString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toTimeString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toTimeString, builtinDateToTimeString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleString, builtinDateToLocaleString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleDateString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleDateString, builtinDateToLocaleDateString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleTimeString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleTimeString, builtinDateToLocaleTimeString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toISOString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toISOString, builtinDateToISOString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toJSON),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toJSON, builtinDateToJSON, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    FunctionObject* toUTCString = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toUTCString, builtinDateToUTCString, 0, nullptr, NativeFunctionInfo::Strict));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toUTCString),
                                       ObjectPropertyDescriptor(toUTCString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toGMTString),
                                       ObjectPropertyDescriptor(toUTCString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getYear),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getYear, builtinDateGetYear, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setYear),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setYear, builtinDateSetYear, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTimezoneOffset),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTimezoneOffset, builtinDateGetTimezoneOffset, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toPrimitive)),
                                                      ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.toPrimitive]")), builtinDateToPrimitive, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
#define DATE_DEFINE_GETTER(dname, unused1, unused2, unused3)                                                                                                                                                               \
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get##dname),                                                                                                             \
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get##dname, builtinDateGet##dname, 0, nullptr, NativeFunctionInfo::Strict)), \
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FOR_EACH_DATE_VALUES(DATE_DEFINE_GETTER);
    DATE_DEFINE_GETTER(Day, -, -, -);
    DATE_DEFINE_GETTER(UTCDay, -, -, -);

#define DATE_DEFINE_SETTER(dname, unused1, length, unused3)                                                                                                                                                                     \
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set##dname),                                                                                                                  \
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set##dname, builtinDateSet##dname, length, nullptr, NativeFunctionInfo::Strict)), \
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FOR_EACH_DATE_VALUES(DATE_DEFINE_SETTER);

    m_date->setFunctionPrototype(state, m_datePrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Date),
                      ObjectPropertyDescriptor(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    DateObject::initCachedUTC(state, new DateObject(state));
}
}
