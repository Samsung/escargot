#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
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


static Value builtinDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DateObject* thisObject;
    if (isNewExpression) {
        thisObject = thisValue.asObject()->asDateObject();

        if (argc == 0) {
            thisObject->setTime(DateObject::currentTime());
        } else if (argc == 1) {
            Value v = argv[0].toPrimitive(state);
            if (v.isString()) {
                thisObject->setTimeValue(state, v);
            } else {
                double V = v.toNumber(state);
                thisObject->setTime(DateObject::timeClip(state, V));
            }
        } else {
            double args[7] = { 0, 0, 1, 0, 0, 0, 0 }; // default value of year, month, date, hour, minute, second, millisecond
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
            if (std::isnan(year) || std::isnan(month) || std::isnan(date) || std::isnan(hour) || std::isnan(minute) || std::isnan(second) || std::isnan(millisecond)) {
                thisObject->setTimeValueAsNaN();
                return new ASCIIString("Invalid Date");
            }
            thisObject->setTimeValue(state, (int)year, (int)month, (int)date, (int)hour, (int)minute, second, millisecond);
        }
        return thisObject->toFullString(state);
    } else {
        DateObject d(state);
        d.setTime(DateObject::currentTime());
        return d.toFullString(state);
    }
}

static Value builtinDateNow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value(DateObject::currentTime());
}

static Value builtinDateParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinDateUTC(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinDateGetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getTime);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getTime.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return Value(thisObject->asDateObject()->primitiveValue());
}

static Value builtinDateValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, valueOf);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().valueOf.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    double val = thisObject->asDateObject()->primitiveValue();
    return Value(val);
}

static Value builtinDateToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toFullString(state);
}

static Value builtinDateToDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toDateString(state);
}

static Value builtinDateToTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toTimeString(state);
}

static Value builtinDateToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toFullString(state);
}

static Value builtinDateToLocaleDateString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toDateString(state);
}

static Value builtinDateToLocaleTimeString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toTimeString(state);
}

static Value builtinDateToUTCString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toFullString(state);
}

static Value builtinDateToISOString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toString);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    return thisObject->asDateObject()->toISOString(state);
}

static Value builtinDateToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinDateToGMTString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, toGMTString);
    if (thisObject->isDateObject()) {
        DateObject* thisDateObject = thisObject->asDateObject();
        static char days[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        char buffer[512];
        if (!std::isnan(thisDateObject->primitiveValue())) {
            snprintf(buffer, 512, "%s, %02d %s %d %02d:%02d:%02d GMT", days[thisDateObject->getUTCDay(state)], thisDateObject->getUTCDate(state), months[thisDateObject->getUTCMonth(state)], thisDateObject->getUTCFullYear(state), thisDateObject->getUTCHours(state), thisDateObject->getUTCMinutes(state), (int)thisDateObject->getUTCSeconds(state));
            return new ASCIIString(buffer);
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toGMTString.string(), errorMessage_GlobalObject_InvalidDate);
        }
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toGMTString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#define DECLARE_STATIC_DATE_GETTER(Name, unused1, unused2, unused3)                                                                                                                                                                \
    static Value builtinDateGet##Name(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)                                                                                                      \
    {                                                                                                                                                                                                                              \
        RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, get##Name);                                                                                                                                                               \
        if (!thisObject->isDateObject()) {                                                                                                                                                                                         \
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().get##Name.string(), errorMessage_GlobalObject_ThisNotDateObject); \
        }                                                                                                                                                                                                                          \
        if (!thisObject->asDateObject()->isValid())                                                                                                                                                                                \
            return Value(std::numeric_limits<double>::quiet_NaN());                                                                                                                                                                \
        return Value(thisObject->asDateObject()->get##Name(state));                                                                                                                                                                \
    }

FOR_EACH_DATE_VALUES(DECLARE_STATIC_DATE_GETTER);
DECLARE_STATIC_DATE_GETTER(Day, -, -, -);
DECLARE_STATIC_DATE_GETTER(UTCDay, -, -, -);

enum DateSetterType {
    Time,
    Day
};

static Value builtinDateSetHelper(ExecutionState& state, DateSetterType setterType, size_t length, bool utc, Value thisValue, size_t argc, Value* argv, const AtomicString& name)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, name);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, name.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    DateObject* d = thisObject->asDateObject();

    if (setterType == DateSetterType::Day && length == 3) {
        // setFullYear, setUTCFullYear case
        if (!d->isValid())
            d->setTime(0);
        ASSERT(d->isValid());
    }
    int year = d->getFullYear(state);
    int month = d->getMonth(state);
    int date = d->getDate(state);

    int hour = d->getHours(state);
    int minute = d->getMinutes(state);
    int64_t second = d->getSeconds(state);
    int64_t millisecond = d->getMilliseconds(state);

    bool convertToUTC = !utc;

    switch (setterType) {
    case DateSetterType::Day:
        if ((length >= 1) && (argc > length - 1))
            date = argv[length - 1].toNumber(state);
        if ((length >= 2) && (argc > length - 2))
            month = argv[length - 2].toNumber(state);
        if ((length >= 3) && (argc > length - 3))
            year = argv[length - 3].toNumber(state);
        break;
    case DateSetterType::Time:
        if ((length >= 1) && (argc > length - 1))
            millisecond = argv[length - 1].toNumber(state);
        if ((length >= 2) && (argc > length - 2))
            second = argv[length - 2].toNumber(state);
        if ((length >= 3) && (argc > length - 3))
            minute = argv[length - 3].toNumber(state);
        if ((length >= 4) && (argc > length - 4))
            hour = argv[length - 4].toNumber(state);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    d->setTimeValue(state, year, month, date, hour, minute, second, millisecond, convertToUTC);
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
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().setTime.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (argc > 0 && argv[0].isNumber()) {
        thisObject->asDateObject()->setTime(argv[0].toNumber(state));
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
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getYear.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    int ret = thisObject->asDateObject()->getFullYear(state) - 1900;
    return Value(ret);
}

static Value builtinDateSetYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getYear);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().setYear.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }

    DateObject* thisDateObject = thisObject->asDateObject();
    double args[1];
    if (argc < 1) {
        thisDateObject->setTimeValueAsNaN();
        return Value(thisDateObject->primitiveValue());
    }
    if (std::isnan(thisDateObject->primitiveValue())) {
        thisDateObject->setTimeValue(state, 1970, 0, 1, 0, 0, 0, 0, true);
    }

    double d = argv[0].toNumber(state);
    if (std::isnan(d)) {
        thisDateObject->setTimeValueAsNaN();
        return Value(thisDateObject->primitiveValue());
    }
    if (0 <= args[0] && args[0] <= 99) {
        args[0] += 1900;
    }
    thisDateObject->setTimeValue(state, (int)args[0], thisDateObject->getMonth(state), thisDateObject->getDate(state), thisDateObject->getHours(state), thisDateObject->getMinutes(state), thisDateObject->getSeconds(state), thisDateObject->getMilliseconds(state));
    return Value(thisDateObject->primitiveValue());
}

static Value builtinDateGetTimezoneOffset(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().getTimezoneOffset.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    Object* thisObject = thisValue.toObject(state);
    ;
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true,
                                       state.context()->staticStrings().getTimezoneOffset.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getTimezoneOffset(state) / 60.0);
}

void GlobalObject::installDate(ExecutionState& state)
{
    m_date = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Date, builtinDateConstructor, 7, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
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
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toUTCString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toUTCString, builtinDateToUTCString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toISOString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toISOString, builtinDateToISOString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toJSON),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toJSON, builtinDateToJSON, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toGMTString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toGMTString, builtinDateToGMTString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getYear),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getYear, builtinDateGetYear, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setYear),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setYear, builtinDateSetYear, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTimezoneOffset),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTimezoneOffset, builtinDateGetTimezoneOffset, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
}
}
