/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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
#include "runtime/NativeFunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/BigIntObject.h"
#include "runtime/TemporalObject.h"
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"

namespace Escargot {
#if defined(ESCARGOT_ENABLE_TEMPORAL)
static Value builtinTemporalNowTimeZone(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    state.context()->vmInstance()->ensureTimezone();
    return Value(String::fromUTF8(state.context()->vmInstance()->timezoneID().c_str(), state.context()->vmInstance()->timezoneID().length(), true));
}

static Value builtinTemporalNowPlainDateISO(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    DateObject d(state);
    d.setTimeValue(DateObject::currentTime());
    return TemporalObject::toISODate(state, d);
}

static Value builtinTemporalNowPlainTimeISO(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    DateObject d(state);
    d.setTimeValue(DateObject::currentTime());
    return TemporalObject::toISOTime(state, d);
}

static Value builtinTemporalNowPlainDateTimeISO(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    DateObject d(state);
    d.setTimeValue(DateObject::currentTime());
    return TemporalObject::toISODateTime(state, d);
}

static Value builtinTemporalPlainDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::New_Target_Is_Undefined);
    }

    ASSERT(argc > 2);

    if (!(argv[0].isInteger(state) || argv[1].isInteger(state) || argv[2].isInteger(state))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid type");
    }

    Value calendar = TemporalCalendar::toTemporalCalendarWithISODefault(state, argc > 4 ? argv[3] : Value());
    return TemporalPlainDate::createTemporalDate(state, argv[0].asInt32(), argv[1].asInt32(), argv[2].asInt32(), calendar, newTarget);
}

static Value builtinTemporalPlainDateFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc > 1 && !argv[1].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "options must be object");
    }

    if (argv[0].isObject() && argv[0].asObject()->isTemporalPlainDateObject()) {
        TemporalObject::toTemporalOverflow(state, argc > 1 ? Value(argv[1].asObject()) : Value());
        TemporalPlainDate* plainDate = argv[0].asObject()->asTemporalPlainDateObject();
        return TemporalPlainDate::createTemporalDate(state, plainDate->year(), plainDate->month(), plainDate->day(), plainDate->calendar());
    }
    return TemporalPlainDate::toTemporalDate(state, argv[0], argc > 1 ? argv[1].asObject() : nullptr);
}

static Value builtinTemporalPlainDateTimeConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::New_Target_Is_Undefined);
    }

    ASSERT(argc > 2);

    if (!(argv[0].isInteger(state) || argv[1].isInteger(state) || argv[2].isInteger(state)) || !(argc > 3 && argv[3].isInteger(state)) || !(argc > 4 && argv[4].isInteger(state)) || !(argc > 5 && argv[5].isInteger(state)) || !(argc > 6 && argv[6].isInteger(state)) || !(argc > 7 && argv[7].isInteger(state)) || !(argc > 8 && argv[8].isInteger(state))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid type");
    }

    Value calendar = TemporalCalendar::toTemporalCalendarWithISODefault(state, argc > 8 ? argv[9] : Value());
    return TemporalPlainDateTime::createTemporalDateTime(state, argv[0].asInt32(), argv[1].asInt32(), argv[2].asInt32(), argc > 3 ? argv[3].asInt32() : 0, argc > 4 ? argv[4].asInt32() : 0, argc > 5 ? argv[5].asInt32() : 0, argc > 6 ? argv[6].asInt32() : 0, argc > 7 ? argv[7].asInt32() : 0, argc > 8 ? argv[8].asInt32() : 0, calendar, new Object(state));
}

static Value builtinTemporalPlainDateTimeFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc > 1 && !argv[1].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "options must be object");
    }

    if (argv[0].isObject() && argv[0].asObject()->isTemporalPlainDateTimeObject()) {
        TemporalObject::toTemporalOverflow(state, argc > 1 ? Value(argv[1].asObject()) : Value());
        TemporalPlainDateTime* plainDateTime = argv[0].asObject()->asTemporalPlainDateTimeObject();
        return TemporalPlainDateTime::createTemporalDateTime(state, plainDateTime->getYear(), plainDateTime->getMonth(), plainDateTime->getDay(), plainDateTime->getHour(), plainDateTime->getMinute(), plainDateTime->getSecond(), plainDateTime->getMillisecond(), plainDateTime->getMicrosecond(), plainDateTime->getNanosecond(), plainDateTime->getCalendar(), new Object(state));
    }
    return TemporalPlainDateTime::toTemporalDateTime(state, argv[0], argc > 1 ? argv[1].asObject() : nullptr);
}

static Value builtinTemporalCalendarConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::New_Target_Is_Undefined);
    }
    String* id = argv[0].asString();
    if (!TemporalCalendar::isBuiltinCalendar(id)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_RangeError);
    }
    return TemporalCalendar::createTemporalCalendar(state, id, newTarget);
}

static Value builtinTemporalCalendarFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return TemporalCalendar::toTemporalCalendar(state, argv[0]);
}

static Value builtinTemporalCalendarPrototypeId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    return calendar.asObject()->asTemporalCalendarObject()->getIdentifier();
}

static Value builtinTemporalCalendarPrototypeDateFromFields(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    if (argc > 1 && !argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "fields is not an object");
    }

    Object* options;

    if (argc > 1) {
        options = argv[1].asObject();
    } else {
        options = new Object(state, Object::PrototypeIsNull);
    }

    // TODO ISODateFromFields https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.datefromfields
    return TemporalPlainDate::createTemporalDate(state, 0, 0, 0, calendar, newTarget);
}

static Value builtinTemporalCalendarPrototypeYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    Value temporalDateLike = argv[0];
    if (!(temporalDateLike.isObject() && (temporalDateLike.asObject()->isTemporalPlainDateObject() || temporalDateLike.asObject()->isTemporalPlainDateTimeObject() || temporalDateLike.asObject()->isTemporalYearMonthObject()))) {
        temporalDateLike = TemporalPlainDate::toTemporalDate(state, temporalDateLike);
    }

    return Value(TemporalCalendar::ISOYear(state, temporalDateLike));
}

static Value builtinTemporalCalendarPrototypeMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    Value temporalDateLike = argv[0];
    if (!(temporalDateLike.isObject() && (temporalDateLike.asObject()->isTemporalPlainDateObject() || temporalDateLike.asObject()->isTemporalPlainDateTimeObject() || temporalDateLike.asObject()->isTemporalYearMonthObject()))) {
        temporalDateLike = TemporalPlainDate::toTemporalDate(state, temporalDateLike);
    }

    return Value(TemporalCalendar::ISOMonth(state, temporalDateLike));
}

static Value builtinTemporalCalendarPrototypeMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    Value temporalDateLike = argv[0];
    if (!(temporalDateLike.isObject() && (temporalDateLike.asObject()->isTemporalPlainDateObject() || temporalDateLike.asObject()->isTemporalPlainDateTimeObject() || temporalDateLike.asObject()->isTemporalYearMonthObject()))) {
        temporalDateLike = TemporalPlainDate::toTemporalDate(state, temporalDateLike);
    }

    return Value(new ASCIIString(TemporalCalendar::ISOMonthCode(state, temporalDateLike).c_str()));
}

static Value builtinTemporalCalendarPrototypeDay(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    Value temporalDateLike = argv[0];
    if (!(temporalDateLike.isObject() && (temporalDateLike.asObject()->isTemporalPlainDateObject() || temporalDateLike.asObject()->isTemporalPlainDateTimeObject()))) {
        temporalDateLike = TemporalPlainDate::toTemporalDate(state, temporalDateLike);
    }

    return Value(TemporalCalendar::ISODay(state, temporalDateLike));
}

static Value builtinTemporalCalendarInLeapYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    Value temporalDateLike = argv[0];
    if (!temporalDateLike.isObject() || !(temporalDateLike.asObject()->isTemporalPlainDateObject() || temporalDateLike.asObject()->isTemporalPlainDateTimeObject() || temporalDateLike.asObject()->isTemporalYearMonthObject())) {
        temporalDateLike = TemporalPlainDate::toTemporalDate(state, temporalDateLike);
    }

    return Value(TemporalCalendar::isIsoLeapYear(state, temporalDateLike.asObject()->asTemporalPlainDateObject()->year()));
}

static Value builtinTemporalCalendarFields(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value calendar = thisValue;
    ASSERT(calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, argv[0]);
    ValueVector fieldNames;
    Optional<Object*> next;

    while (true) {
        next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            break;
        }
        Value nextValue = IteratorObject::iteratorValue(state, next->asObject());
        if (!nextValue.isString()) {
            Value throwCompletion = ErrorObject::createError(state, ErrorObject::TypeError, new ASCIIString("Value is not a string"));
            return IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true);
        }
        for (unsigned int i = 0; i < fieldNames.size(); ++i) {
            if (fieldNames[i].equalsTo(state, nextValue)) {
                Value throwCompletion = ErrorObject::createError(state, ErrorObject::RangeError, new ASCIIString("Duplicated keys"));
                return IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true);
            }
        }
        if (!(nextValue.asString()->equals("year")
              || nextValue.asString()->equals("month")
              || nextValue.asString()->equals("monthCode")
              || nextValue.asString()->equals("day")
              || nextValue.asString()->equals("hour")
              || nextValue.asString()->equals("second")
              || nextValue.asString()->equals("millisecond")
              || nextValue.asString()->equals("microsecond")
              || nextValue.asString()->equals("nanosecond"))) {
            Value throwCompletion = ErrorObject::createError(state, ErrorObject::RangeError, new ASCIIString("Invalid key"));
            return IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true);
        }
        fieldNames.pushBack(nextValue);
    }
    return Object::createArrayFromList(state, fieldNames);
}

static Value builtinTemporalCalendarPrototypeMergeFields(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(thisValue.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));
    if (argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Too few arguments");
    }
    Value fields = argv[0];
    Value additionalFields = argv[1];
    return TemporalCalendar::defaultMergeFields(state, fields, additionalFields);
}

static Value builtinTemporalCalendarToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(thisValue.asObject()->asTemporalCalendarObject()->getIdentifier());
}

static Value builtinTemporalCalendarToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(thisValue.asObject()->asTemporalCalendarObject()->getIdentifier());
}

void GlobalObject::initializeTemporal(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(
        true, false, true,
        [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
            ASSERT(self->isGlobalObject());
            return self->asGlobalObject()->temporal();
        },
        nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyTemporal()), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installTemporal(ExecutionState& state)
{
    StaticStrings* strings = &state.context()->staticStrings();

    m_temporal = new Object(state);
    m_temporal->setGlobalIntrinsicObject(state);

    m_temporalNow = new Object(state);
    m_temporalNow->setGlobalIntrinsicObject(state);

    m_temporalPlainDate = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyDate(), builtinTemporalPlainDateConstructor, 4), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDate->setGlobalIntrinsicObject(state);

    m_temporalPlainDatePrototype = new PrototypeObject(state);
    m_temporalPlainDatePrototype->setGlobalIntrinsicObject(state, true);

    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalPlainDate->setFunctionPrototype(state, m_temporalPlainDatePrototype);

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalPlainDateTime = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyDateTime(), builtinTemporalPlainDateTimeConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDateTime->setGlobalIntrinsicObject(state);

    m_temporalPlainDateTimePrototype = new PrototypeObject(state);
    m_temporalPlainDateTimePrototype->setGlobalIntrinsicObject(state, true);

    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalPlainDateTime, (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalPlainDateTime->setFunctionPrototype(state, m_temporalPlainDateTimePrototype);

    m_temporalPlainDateTime->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateTimeFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendar = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCalendar(), builtinTemporalCalendarConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalCalendar->setGlobalIntrinsicObject(state);

    m_temporalCalendarPrototype = new PrototypeObject(state);
    m_temporalCalendarPrototype->setGlobalIntrinsicObject(state, true);

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalCalendar, (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));


    m_temporalCalendar->setFunctionPrototype(state, m_temporalCalendarPrototype);

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                        ObjectPropertyDescriptor(Value(strings->lazyTemporal().string()), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                           ObjectPropertyDescriptor(strings->temporalDotNow.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazytimeZone()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazytimeZone(), builtinTemporalNowTimeZone, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyplainDateISO()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyplainDateISO(), builtinTemporalNowPlainDateISO, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyplainTimeISO()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyplainTimeISO(), builtinTemporalNowPlainTimeISO, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyplainDateTimeISO()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyplainDateTimeISO(), builtinTemporalNowPlainDateTimeISO, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                 ObjectPropertyDescriptor(strings->temporalDotPlainDate.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendar->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalCalendarFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyid()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyid(), builtinTemporalCalendarPrototypeId, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydateFromFields()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydateFromFields(), builtinTemporalCalendarPrototypeDateFromFields, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyYear(), builtinTemporalCalendarPrototypeYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonth()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonth(), builtinTemporalCalendarPrototypeMonth, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthCode()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthCode(), builtinTemporalCalendarPrototypeMonthCode, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyday()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyday(), builtinTemporalCalendarPrototypeDay, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyinLeapYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyinLeapYear(), builtinTemporalCalendarInLeapYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymergeFields()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymergeFields(), builtinTemporalCalendarPrototypeMergeFields, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyfields()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyfields(), builtinTemporalCalendarFields, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));


    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalCalendarToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalCalendarToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyNow()),
                                        ObjectPropertyDescriptor(m_temporalNow, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyPlainDate()),
                                        ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyPlainDateTime()),
                                        ObjectPropertyDescriptor(m_temporalPlainDateTime, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendar()),
                                        ObjectPropertyDescriptor(m_temporalCalendar, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    redefineOwnProperty(state, ObjectPropertyName(strings->lazyTemporal()),
                        ObjectPropertyDescriptor(m_temporal, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

#else

void GlobalObject::initializeTemporal(ExecutionState& state)
{
    // dummy initialize function
}

#endif

} // namespace Escargot
