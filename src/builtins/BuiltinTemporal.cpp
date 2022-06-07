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

static void checkTemporalCalendar(ExecutionState& state, const Value& thisValue, unsigned long argc)
{
    if (!(thisValue.isObject() && thisValue.asObject()->isTemporalCalendarObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ThisNotObject);
    }

    ASSERT(thisValue.asObject()->asTemporalCalendarObject()->getIdentifier()->equals("iso8601"));

    if (argc == 0) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid type");
    }
}

static TemporalPlainDate* getTemporalPlainDate(ExecutionState& state, const Value& thisValue, unsigned long argc, Value* argv)
{
    checkTemporalCalendar(state, thisValue, argc);
    return TemporalPlainDate::toTemporalDate(state, argv[0]).asObject()->asTemporalPlainDateObject();
}

void checkTemporalPlainDate(ExecutionState& state, const Value& value)
{
    if (!(value.isObject() && value.asObject()->isTemporalPlainDateObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid type");
    }
}

void checkTemporalPlainDateTime(ExecutionState& state, const Value& value)
{
    if (!(value.isObject() && value.asObject()->isTemporalPlainDateTimeObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid type");
    }
}

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

static Value builtinTemporalPlainDateCalendar(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return thisValue.asObject()->asTemporalPlainDateObject()->calendar();
}

static Value builtinTemporalPlainDateYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarMonth(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarMonthCode(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDay(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDay(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDayOfWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDayOfWeek(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDayOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDayOfYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateWeekOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarWeekOfYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDaysInWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDaysInWeek(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDaysInMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDaysInMonth(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateDaysInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarDaysInYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateMonthsInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarMonthsInYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateInLeapYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDate(state, thisValue);
    return TemporalCalendar::calendarInLeapYear(state, thisValue.asObject()->asTemporalPlainDateObject()->calendar().asObject(), thisValue);
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

static Value builtinTemporalPlainDateTimeCalendar(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar();
}

static Value builtinTemporalPlainDateTimeYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarMonth(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarMonthCode(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDay(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDay(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDayOfWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDayOfWeek(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDayOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDayOfYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeWeekOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarWeekOfYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDaysInWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDaysInWeek(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDaysInMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDaysInMonth(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeDaysInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarDaysInYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeMonthsInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarMonthsInYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
}

static Value builtinTemporalPlainDateTimeInLeapYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalPlainDateTime(state, thisValue);
    return TemporalCalendar::calendarInLeapYear(state, thisValue.asObject()->asTemporalPlainDateTimeObject()->getCalendar().asObject(), thisValue);
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

static Value builtinTemporalCalendarPrototypeDayOfWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TemporalPlainDate* temporalDate = getTemporalPlainDate(state, thisValue, argc, argv);
    Value epochDays = DateObject::makeDay(state, Value(temporalDate->year()), Value(temporalDate->month() - 1), Value(temporalDate->day()));
    ASSERT(std::isfinite(epochDays.asNumber()));
    int dayOfWeek = DateObject::weekDay(DateObject::makeDate(state, epochDays, Value(0)).toUint32(state));
    return Value(dayOfWeek == 0 ? 7 : dayOfWeek);
}

static Value builtinTemporalCalendarPrototypeDayOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TemporalPlainDate* temporalDate = getTemporalPlainDate(state, thisValue, argc, argv);
    Value epochDays = DateObject::makeDay(state, Value(temporalDate->year()), Value(temporalDate->month() - 1), Value(temporalDate->day()));
    ASSERT(std::isfinite(epochDays.asNumber()));

    return Value(TemporalCalendar::dayOfYear(state, epochDays));
}

static Value builtinTemporalCalendarPrototypeWeekOfYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    TemporalPlainDate* temporalDate = getTemporalPlainDate(state, thisValue, argc, argv);
    return Value(TemporalCalendar::toISOWeekOfYear(state, temporalDate->year(), temporalDate->month(), temporalDate->day()));
}

static Value builtinTemporalCalendarPrototypeDaysInWeek(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalCalendar(state, thisValue, argc);
    return Value(7);
}

static Value builtinTemporalCalendarPrototypeDaysInMonth(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    getTemporalPlainDate(state, thisValue, argc, argv);

    TemporalPlainDate* temporalDate;

    if (!argv[0].isObject() || !(argv[0].asObject()->isTemporalPlainDateObject() || argv[0].asObject()->isTemporalPlainDateTimeObject() || argv[0].asObject()->isTemporalYearMonthObject())) {
        temporalDate = TemporalPlainDate::toTemporalDate(state, argv[0]).asObject()->asTemporalPlainDateObject();
    } else {
        temporalDate = argv[0].asObject()->asTemporalPlainDateObject();
    }

    return TemporalCalendar::ISODaysInMonth(state, temporalDate->year(), temporalDate->month());
}

static Value builtinTemporalCalendarPrototypeDaysInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalCalendar(state, thisValue, argc);

    TemporalPlainDate* temporalDate;

    if (!argv[0].isObject() || !(argv[0].asObject()->isTemporalPlainDateObject() || argv[0].asObject()->isTemporalPlainDateTimeObject() || argv[0].asObject()->isTemporalYearMonthObject())) {
        temporalDate = TemporalPlainDate::toTemporalDate(state, argv[0]).asObject()->asTemporalPlainDateObject();
    } else {
        temporalDate = argv[0].asObject()->asTemporalPlainDateObject();
    }

    return Value(DateObject::daysInYear(temporalDate->year()));
}

static Value builtinTemporalCalendarPrototypeMonthsInYear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    checkTemporalCalendar(state, thisValue, argc);

    if (!argv[0].isObject() || !(argv[0].asObject()->isTemporalPlainDateObject() || argv[0].asObject()->isTemporalPlainDateTimeObject() || argv[0].asObject()->isTemporalYearMonthObject())) {
        TemporalPlainDate::toTemporalDate(state, argv[0]);
    }

    return Value(12);
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

    JSGetterSetter dateCalendarGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazycalendar(), builtinTemporalPlainDateCalendar, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateCalendarDesc(dateCalendarGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazycalendar()), dateCalendarDesc);

    JSGetterSetter dateYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyYear(), builtinTemporalPlainDateYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateYearDesc(dateYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyYear()), dateYearDesc);

    JSGetterSetter dateMonthGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyMonth(), builtinTemporalPlainDateMonth, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateMonthDesc(dateMonthGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonth()), dateMonthDesc);

    JSGetterSetter dateMonthCodeGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthCode(), builtinTemporalPlainDateMonthCode, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateMonthCodeDesc(dateMonthCodeGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthCode()), dateMonthCodeDesc);

    JSGetterSetter dateDayGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyDay(), builtinTemporalPlainDateDay, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDayDesc(dateDayGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyDay()), dateDayDesc);

    JSGetterSetter dateDayOfWeekGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfWeek(), builtinTemporalPlainDateDayOfWeek, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDayOfWeekDesc(dateDayOfWeekGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfWeek()), dateDayOfWeekDesc);

    JSGetterSetter dateDayOfYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfYear(), builtinTemporalPlainDateDayOfYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDayOfYearDesc(dateDayOfYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfYear()), dateDayOfYearDesc);

    JSGetterSetter dateWeekOfYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyweekOfYear(), builtinTemporalPlainDateWeekOfYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateWeekOfYearDesc(dateWeekOfYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyweekOfYear()), dateWeekOfYearDesc);

    JSGetterSetter dateDaysInWeekGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInWeek(), builtinTemporalPlainDateDaysInWeek, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDaysInWeekDesc(dateDaysInWeekGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInWeek()), dateDaysInWeekDesc);

    JSGetterSetter dateDaysInMonthGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInMonth(), builtinTemporalPlainDateDaysInMonth, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDaysInMonthDesc(dateDaysInMonthGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInMonth()), dateDaysInMonthDesc);

    JSGetterSetter dateDaysInYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInYear(), builtinTemporalPlainDateDaysInYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateDaysInYearDesc(dateDaysInYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInYear()), dateDaysInYearDesc);

    JSGetterSetter dateMonthsInYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthsInYear(), builtinTemporalPlainDateMonthsInYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateMonthsInYearDesc(dateMonthsInYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthsInYear()), dateMonthsInYearDesc);

    JSGetterSetter dateInLeapYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyinLeapYear(), builtinTemporalPlainDateInLeapYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateInLeapYearDesc(dateInLeapYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyinLeapYear()), dateInLeapYearDesc);

    m_temporalPlainDate->setFunctionPrototype(state, m_temporalPlainDatePrototype);

    m_temporalPlainDate->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->from),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalPlainDateTime = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyDateTime(), builtinTemporalPlainDateTimeConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDateTime->setGlobalIntrinsicObject(state);

    m_temporalPlainDateTimePrototype = new PrototypeObject(state);
    m_temporalPlainDateTimePrototype->setGlobalIntrinsicObject(state, true);

    m_temporalPlainDateTimePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalPlainDateTime, (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    JSGetterSetter dateTimeCalendarGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazycalendar(), builtinTemporalPlainDateTimeCalendar, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeCalendarDesc(dateTimeCalendarGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazycalendar()), dateTimeCalendarDesc);

    JSGetterSetter dateTimeYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyYear(), builtinTemporalPlainDateTimeYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeYearDesc(dateTimeYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyYear()), dateTimeYearDesc);

    JSGetterSetter dateTimeMonthGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyMonth(), builtinTemporalPlainDateTimeMonth, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeMonthDesc(dateTimeMonthGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonth()), dateTimeMonthDesc);

    JSGetterSetter dateTimeMonthCodeGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthCode(), builtinTemporalPlainDateTimeMonthCode, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeMonthCodeDesc(dateTimeMonthCodeGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthCode()), dateTimeMonthCodeDesc);

    JSGetterSetter dateTimeDayGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyDay(), builtinTemporalPlainDateTimeDay, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDayDesc(dateTimeDayGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyDay()), dateTimeDayDesc);

    JSGetterSetter dateTimeDayOfWeekGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfWeek(), builtinTemporalPlainDateTimeDayOfWeek, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDayOfWeekDesc(dateTimeDayOfWeekGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfWeek()), dateTimeDayOfWeekDesc);

    JSGetterSetter dateTimeDayOfYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfYear(), builtinTemporalPlainDateTimeDayOfYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDayOfYearDesc(dateTimeDayOfYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfYear()), dateTimeDayOfYearDesc);

    JSGetterSetter dateTimeWeekOfYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyweekOfYear(), builtinTemporalPlainDateTimeWeekOfYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeWeekOfYearDesc(dateTimeWeekOfYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyweekOfYear()), dateTimeWeekOfYearDesc);

    JSGetterSetter dateTimeDaysInWeekGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInWeek(), builtinTemporalPlainDateTimeDaysInWeek, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDaysInWeekDesc(dateTimeDaysInWeekGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInWeek()), dateTimeDaysInWeekDesc);

    JSGetterSetter dateTimeDaysInMonthGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInMonth(), builtinTemporalPlainDateTimeDaysInMonth, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDaysInMonthDesc(dateTimeDaysInMonthGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInMonth()), dateTimeDaysInMonthDesc);

    JSGetterSetter dateTimeDaysInYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInYear(), builtinTemporalPlainDateTimeDaysInYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeDaysInYearDesc(dateTimeDaysInYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInYear()), dateTimeDaysInYearDesc);

    JSGetterSetter dateTimeMonthsInYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthsInYear(), builtinTemporalPlainDateTimeMonthsInYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeMonthsInYearDesc(dateTimeMonthsInYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthsInYear()), dateTimeMonthsInYearDesc);

    JSGetterSetter dateTimeInLeapYearGS(
        new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyinLeapYear(), builtinTemporalPlainDateTimeInLeapYear, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor dateTimeInLeapYearDesc(dateTimeInLeapYearGS, ObjectPropertyDescriptor::ConfigurablePresent);
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyinLeapYear()), dateTimeInLeapYearDesc);

    m_temporalPlainDateTime->setFunctionPrototype(state, m_temporalPlainDateTimePrototype);

    m_temporalPlainDateTime->defineOwnProperty(state, ObjectPropertyName(strings->from),
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

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfWeek()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfWeek(), builtinTemporalCalendarPrototypeDayOfWeek, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydayOfYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydayOfYear(), builtinTemporalCalendarPrototypeDayOfYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyweekOfYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyweekOfYear(), builtinTemporalCalendarPrototypeWeekOfYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInWeek()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInWeek(), builtinTemporalCalendarPrototypeDaysInWeek, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInMonth()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInMonth(), builtinTemporalCalendarPrototypeDaysInMonth, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazydaysInYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazydaysInYear(), builtinTemporalCalendarPrototypeDaysInYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalCalendarPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazymonthsInYear()),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazymonthsInYear(), builtinTemporalCalendarPrototypeMonthsInYear, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
