#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "DateObject.h"
#include "ErrorObject.h"

namespace Escargot {

static Value builtinDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DateObject* thisObject;
    if (isNewExpression) {
        thisObject = thisValue.asObject()->asDateObject();

        if (argc == 0) {
            thisObject->setTimeValueAsCurrentTime();
        } else if (argc == 1) {
            Value v = argv[0].toPrimitive(state);
            if (v.isString()) {
                thisObject->setTimeValue(state, v);
            } else {
                double V = v.toNumber(state);
                thisObject->setTimeValue(state, DateObject::timeClip(state, V));
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
        d.setTimeValueAsCurrentTime();
        return d.toFullString(state);
    }
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

static Value builtinDateGetDate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getDate);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getDate.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getDate(state));
}

static Value builtinDateGetDay(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getDay);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getDay.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getDay(state));
}

static Value builtinDateGetFullYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getFullYear);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getFullYear.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getFullYear(state));
}

static Value builtinDateGetHours(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getHours);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getHours.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getHours(state));
}

static Value builtinDateGetMilliseconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getMilliseconds);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getMilliseconds.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getMilliseconds(state));
}

static Value builtinDateGetMinutes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getMinutes);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getMinutes.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getMinutes(state));
}

static Value builtinDateGetMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getMonth);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getMonth.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getMonth(state));
}

static Value builtinDateGetSeconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getSeconds);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getSeconds.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getSeconds(state));
}

static Value builtinDateGetTimezoneOffset(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getTimezoneOffset);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getTimezoneOffset.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    if (!thisObject->asDateObject()->isValid())
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(thisObject->asDateObject()->getTimezoneOffset(state));
}

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


void GlobalObject::installDate(ExecutionState& state)
{
    m_date = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Date, builtinDateConstructor, 7, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                    return new DateObject(state);
                                }),
                                FunctionObject::__ForBuiltin__);
    m_date->markThisObjectDontNeedStructureTransitionTable(state);
    m_date->setPrototype(state, m_functionPrototype);
    m_datePrototype = m_objectPrototype;
    m_datePrototype = new Object(state);
    m_datePrototype->setPrototype(state, m_objectPrototype);

    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTime),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTime, builtinDateGetTime, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().valueOf),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().valueOf, builtinDateValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinDateToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setTime),
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setTime, builtinDateSetTime, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


#define DATE_DEFINE_GETTER(dname)                                                                                                                                                                                          \
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get##dname),                                                                                                             \
                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get##dname, builtinDateGet##dname, 0, nullptr, NativeFunctionInfo::Strict)), \
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    DATE_DEFINE_GETTER(Date);
    DATE_DEFINE_GETTER(Day);
    DATE_DEFINE_GETTER(FullYear);
    DATE_DEFINE_GETTER(Hours);
    DATE_DEFINE_GETTER(Milliseconds);
    DATE_DEFINE_GETTER(Minutes);
    DATE_DEFINE_GETTER(Month);
    DATE_DEFINE_GETTER(Seconds);
    DATE_DEFINE_GETTER(TimezoneOffset);


    m_date->setFunctionPrototype(state, m_datePrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Date),
                      ObjectPropertyDescriptor(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
