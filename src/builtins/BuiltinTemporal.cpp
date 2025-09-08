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
#include "runtime/TemporalDurationObject.h"
#include "runtime/TemporalInstantObject.h"
#include "runtime/TemporalNowObject.h"
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"

namespace Escargot {

#if defined(ENABLE_TEMPORAL)

static Value builtinTemporalNowTimeZoneId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return TemporalNowObject::timeZoneId(state);
}

static Value builtinTemporalNowInstant(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let ns be SystemUTCEpochNanoseconds().
    auto ns = Temporal::systemUTCEpochNanoseconds();
    // Return ! CreateTemporalInstant(ns).
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), ns);
}

static Value builtinTemporalDurationConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
        return Value();
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalDurationPrototype();
    });

    return new TemporalDurationObject(state, proto, argc >= 1 ? argv[0] : Value(), argc >= 2 ? argv[1] : Value(),
                                      argc >= 3 ? argv[2] : Value(), argc >= 4 ? argv[3] : Value(),
                                      argc >= 5 ? argv[4] : Value(), argc >= 6 ? argv[5] : Value(),
                                      argc >= 7 ? argv[6] : Value(), argc >= 8 ? argv[7] : Value(),
                                      argc >= 9 ? argv[8] : Value(), argc >= 10 ? argv[9] : Value());
}

static Value builtinTemporalDurationFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalDuration(state, argv[0]);
}

#define RESOLVE_THIS_BINDING_TO_DURATION(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalDurationObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalDuration().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                            \
    TemporalDurationObject* NAME = thisValue.asObject()->asTemporalDurationObject();

#define RESOLVE_THIS_BINDING_TO_DURATION2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalDurationObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalDuration().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                    \
    TemporalDurationObject* NAME = thisValue.asObject()->asTemporalDurationObject();

static Value builtinTemporalDurationToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, toString);
    return duration->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalDurationToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, toJSON);
    return duration->toString(state, Value());
}

static Value builtinTemporalDurationToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, toLocaleString);
    return duration->toString(state, Value());
}

static Value builtinTemporalDurationValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "can't convert Duration to primitive type");
    return Value();
}

static Value builtinTemporalDurationNegated(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION(duration, Negated);
    return new TemporalDurationObject(state, TemporalDurationObject::createNegatedTemporalDuration(duration->duration()));
}

static Value builtinTemporalDurationAbs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, abs);

    auto newDuration = duration->duration();
    for (auto& n : newDuration) {
        n = std::abs(n);
    }

    return new TemporalDurationObject(state, newDuration);
}

static Value builtinTemporalDurationWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, with);
    return duration->with(state, argv[0]);
}

static Value builtinTemporalDurationAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION2(duration, add);
    return new TemporalDurationObject(state, duration->addDurations(state, TemporalDurationObject::AddDurationsOperation::Add, argv[0]));
}

static Value builtinTemporalDurationSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_DURATION(duration, Subtract);
    return new TemporalDurationObject(state, duration->addDurations(state, TemporalDurationObject::AddDurationsOperation::Subtract, argv[0]));
}

static Value builtinTemporalInstantConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
        return Value();
    }

    // Let epochNanoseconds be ? ToBigInt(epochNanoseconds).
    BigInt* epochNanoseconds = argv[0].toBigInt(state);

    auto mayInt128 = epochNanoseconds->toInt128();
    // If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    if (!mayInt128 || !Temporal::isValidEpochNanoseconds(mayInt128.value())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid epoch value");
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalInstantPrototype();
    });

    // Return ? CreateTemporalInstant(epochNanoseconds, NewTarget).
    return new TemporalInstantObject(state, proto, mayInt128.value());
}

static Value builtinTemporalInstantFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalInstant(state, argv[0]);
}

static Value builtinTemporalInstantCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Set one to ? ToTemporalInstant(one).
    auto one = Temporal::toTemporalInstant(state, argv[0]);
    // Set two to ? ToTemporalInstant(two).
    auto two = Temporal::toTemporalInstant(state, argv[1]);
    // Return 𝔽(CompareEpochNanoseconds(one.[[EpochNanoseconds]], two.[[EpochNanoseconds]])).
    if (one->epochNanoseconds() > two->epochNanoseconds()) {
        return Value(1);
    } else if (one->epochNanoseconds() < two->epochNanoseconds()) {
        return Value(-1);
    }
    return Value(0);
}

static Value builtinTemporalInstantFromEpochMilliseconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Set epochMilliseconds to ? ToNumber(epochMilliseconds).
    // Set epochMilliseconds to ? NumberToBigInt(epochMilliseconds).
    double n = argv[0].toNumber(state);
    Int128 epoch = static_cast<int64_t>(n);
    // Let epochNanoseconds be epochMilliseconds × ℤ(10**6).
    epoch *= 1000000;
    // If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    if (!isIntegralNumber(n) || !ISO8601::ExactTime(epoch).isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid epoch value");
    }
    // Return ! CreateTemporalInstant(epochNanoseconds).
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), epoch);
}

static Value builtinTemporalInstantFromEpochNanoseconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
    BigInt* epoch = argv[0].toBigInt(state);
    // If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    auto mayInt128 = epoch->toInt128();
    if (!mayInt128 || !ISO8601::ExactTime(mayInt128.value()).isValid()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid epoch value");
    }
    // Return ! CreateTemporalInstant(epochNanoseconds).
    return new TemporalInstantObject(state, state.context()->globalObject()->temporalInstantPrototype(), mayInt128.value());
}

#define RESOLVE_THIS_BINDING_TO_INSTANT(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalInstantObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalInstant().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                           \
    TemporalInstantObject* NAME = thisValue.asObject()->asTemporalInstantObject();

#define RESOLVE_THIS_BINDING_TO_INSTANT2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalInstantObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalInstant().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                   \
    TemporalInstantObject* NAME = thisValue.asObject()->asTemporalInstantObject();

static Value builtinTemporalInstantGetEpochMilliseconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, GetEpochMilliseconds);
    return instant->epochMilliseconds();
}

static Value builtinTemporalInstantGetEpochNanoseconds(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, GetEpochNanoseconds);
    auto s = std::to_string(instant->epochNanoseconds());
    return BigInt::parseString(s.data(), s.length()).value();
}

static Value builtinTemporalInstantValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "can't convert Instant to primitive type");
    return Value();
}

static Value builtinTemporalInstantEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let instant be the this value.
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, GetEpochNanoseconds);
    // Perform ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
    // Set other to ? ToTemporalInstant(other).
    auto other = Temporal::toTemporalInstant(state, argv[0]);
    // If instant.[[EpochNanoseconds]] ≠ other.[[EpochNanoseconds]], return false.
    // Return true.
    return Value(instant->epochNanoseconds() == other->epochNanoseconds());
}

static Value builtinTemporalInstantToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT2(instant, toJSON);
    return instant->toString(state, Value());
}

static Value builtinTemporalInstantToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT2(instant, toLocaleString);
    return instant->toString(state, Value());
}

static Value builtinTemporalInstantToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT2(instant, toString);
    return instant->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalInstantRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT2(instant, round);
    return instant->round(state, argv[0]);
}

static Value builtinTemporalInstantUntil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, Until);
    return instant->differenceTemporalInstant(state, TemporalInstantObject::DifferenceTemporalInstantOperation::Until, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalInstantSince(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, Since);
    return instant->differenceTemporalInstant(state, TemporalInstantObject::DifferenceTemporalInstantOperation::Since, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalInstantAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT2(instant, add);
    return instant->addDurationToInstant(state, TemporalInstantObject::AddDurationOperation::Add, argv[0]);
}

static Value builtinTemporalInstantSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_INSTANT(instant, Subtract);
    return instant->addDurationToInstant(state, TemporalInstantObject::AddDurationOperation::Subtract, argv[0]);
}

#define RESOLVE_THIS_BINDING_TO_PLAINDATE(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDate().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                             \
    TemporalPlainDateObject* NAME = thisValue.asObject()->asTemporalPlainDateObject();


#define FOR_EACH_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER(F) \
    F(Era, eraValue())                                      \
    F(EraYear, eraYearValue())                              \
    F(Year, year)                                           \
    F(Month, month)                                         \
    F(MonthCode, monthCode)                                 \
    F(Day, day)                                             \
    F(DayOfWeek, dayOfWeek)                                 \
    F(DayOfYear, dayOfYear)                                 \
    F(WeekOfYear, weekOfYear.weekValue())                   \
    F(YearOfWeek, weekOfYear.yearValue())                   \
    F(DaysInWeek, daysInWeek)                               \
    F(DaysInMonth, daysInMonth)                             \
    F(DaysInYear, daysInYear)                               \
    F(MonthsInYear, monthsInYear)                           \
    F(InLeapYear, inLeapYear)


// abstract functions (helper)
static int toIntegerWithTruncation(ExecutionState& state, const Value& arg)
{
    double num = arg.toNumber(state);
    if (std::isnan(num) || (num == std::numeric_limits<double>::infinity()) || (num == -std::numeric_limits<double>::infinity())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
    }

    return static_cast<int>(trunc(num));
}

static CalendarDate calendarISOToDate(ExecutionState& state, const String* calendar, const ISODate& date)
{
    ASSERT(date.month >= 0);
    StaticStrings* strings = &state.context()->staticStrings();

    if (calendar->equals(strings->lazyISO8601().string())) {
        StringBuilder builder;
        builder.appendChar('M');
        String* monthPart = strings->dtoa(date.month);
        if (monthPart->length() == 1) {
            builder.appendChar('0');
            builder.appendString(monthPart);
        } else {
            builder.appendSubString(monthPart, 0, 2);
        }
        String* monthCode = builder.finalize();

        bool inLeapYear = true;
        if (Temporal::mathematicalInLeapYear(Temporal::epochTimeForYear(date.year)) != 1) {
            inLeapYear = false;
        }

        return CalendarDate(date.year, date.month, monthCode, date.day, Temporal::ISODayOfWeek(date), Temporal::ISODayOfYear(date), Temporal::ISOWeekOfYear(date), 7, Temporal::ISODaysInMonth(date.year, date.month), Temporal::mathematicalDaysInYear(date.year), 12, inLeapYear);
    }

    return calendarISOToDate(state, strings->lazyISO8601().string(), date);
}

// Temporal.PlainDate
static Value builtinTemporalPlainDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    int y = toIntegerWithTruncation(state, argv[0]);
    int m = toIntegerWithTruncation(state, argv[1]);
    int d = toIntegerWithTruncation(state, argv[2]);

    Value calendar = argc > 3 ? argv[3] : Value();
    if (calendar.isUndefined()) {
        calendar = state.context()->staticStrings().lazyISO8601().string();
    }
    if (!calendar.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::TemporalError);
    }

    // TODO Set calendar to ? CanonicalizeCalendar(calendar).
    calendar = state.context()->staticStrings().lazyISO8601().string();

    if (!Temporal::isValidISODate(state, y, m, d)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
    }

    ASSERT(calendar.isString());

    ISODate isoDate(y, m, d);
    return Temporal::createTemporalDate(state, isoDate, calendar.asString(), newTarget);
}

static Value builtinTemporalPlainDateFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value options = argc > 1 ? argv[1] : Value();
    return Temporal::toTemporalDate(state, argv[0], options);
}

static Value builtinTemporalPlainDateCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value();
}

static Value builtinTemporalPlainDateCalendarId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(temporalDate, CalendarId);
    return temporalDate->calendar();
}


#define DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER(NAME, PROP)                                                                             \
    static Value builtinTemporalPlainDate##NAME(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) \
    {                                                                                                                                          \
        RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, NAME);                                                                                    \
        return Value(calendarISOToDate(state, plainDate->calendar(), plainDate->date()).PROP);                                                 \
    }

FOR_EACH_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER(DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER)
#undef DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER


void GlobalObject::initializeTemporal(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(
        true, false, true,
        [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
            ASSERT(self->isGlobalObject());
            return self->asGlobalObject()->temporal();
        },
        nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyCapitalTemporal()), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installTemporal(ExecutionState& state)
{
    StaticStrings* strings = &state.context()->staticStrings();

    // Temporal.Now
    m_temporalNow = new Object(state);
    m_temporalNow->setGlobalIntrinsicObject(state, false);

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                           ObjectPropertyDescriptor(Value(strings->lazyTemporalDotNow().string()), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyTimeZoneId()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                             NativeFunctionInfo(strings->lazyTimeZoneId(), builtinTemporalNowTimeZoneId, 0, NativeFunctionInfo::Strict)),
                                                                    (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalNow->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyInstant()),
                                           ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                             NativeFunctionInfo(strings->lazyInstant(), builtinTemporalNowInstant, 0, NativeFunctionInfo::Strict)),
                                                                    (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // Temporal.Duration
    m_temporalDuration = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalDuration(), builtinTemporalDurationConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalDuration->setGlobalIntrinsicObject(state);

    m_temporalDuration->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                  NativeFunctionInfo(strings->from, builtinTemporalDurationFrom, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalDurationPrototype = m_temporalDuration->getFunctionPrototype(state).asObject();
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                         ObjectPropertyDescriptor(Value(strings->lazyTemporalDotDuration().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalDurationToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalDurationToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalDurationToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalDurationValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyNegated()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyNegated(), builtinTemporalDurationNegated, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->abs), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->abs, builtinTemporalDurationAbs, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalDurationWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalDurationAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalDurationSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


#define DEFINE_DURATION_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                       \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalDurationObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalDuration().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalDurationObject* s = thisValue.asObject()->asTemporalDurationObject(); \
            return Value(Value::DoubleToIntConvertibleTestNeeds, s->duration().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                                    \
    }

#define DEFINE_GETTER(name, Name, names, Names, index, category) DEFINE_DURATION_PROTOTYPE_GETTER_PROPERTY(names, #names, Names)
    PLAIN_DATETIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER

    {
        AtomicString name(state.context(), "get sign");
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value {
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalDurationObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalDuration().string(), true, String::fromASCII("get sign"), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
            }
            TemporalDurationObject* s = thisValue.asObject()->asTemporalDurationObject();
            return Value(s->sign()); }, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->sign), desc);
    }

    {
        AtomicString name(state.context(), "get blank");
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value {
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalDurationObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalDuration().string(), true, String::fromASCII("get blank"), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
            }
            TemporalDurationObject* s = thisValue.asObject()->asTemporalDurationObject();
            return Value(s->sign() == 0); }, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazyBlank()), desc);
    }

    // Temporal.Instant
    m_temporalInstant = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalInstant(), builtinTemporalInstantConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalInstant->setGlobalIntrinsicObject(state);

    m_temporalInstant->directDefineOwnProperty(state, ObjectPropertyName(strings->from),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings->from, builtinTemporalInstantFrom, 1, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalInstant->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyFromEpochMilliseconds()),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings->lazyFromEpochMilliseconds(), builtinTemporalInstantFromEpochMilliseconds, 1, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalInstant->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyFromEpochNanoseconds()),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings->lazyFromEpochNanoseconds(), builtinTemporalInstantFromEpochNanoseconds, 1, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalInstant->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCompare()),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings->lazyCompare(), builtinTemporalInstantCompare, 2, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_temporalInstantPrototype = m_temporalInstant->getFunctionPrototype(state).asObject();
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                        ObjectPropertyDescriptor(Value(strings->lazyTemporalDotInstant().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalInstantToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalInstantValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalInstantEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalInstantToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalInstantToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->round), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->round, builtinTemporalInstantRound, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyUntil()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyUntil(), builtinTemporalInstantUntil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySince()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySince(), builtinTemporalInstantSince, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalInstantAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalInstantSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyGetEpochMilliseconds(), builtinTemporalInstantGetEpochMilliseconds, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazyEpochMilliseconds()), desc);
    }

    {
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyGetEpochNanoseconds(), builtinTemporalInstantGetEpochNanoseconds, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazyEpochNanoseconds()), desc);
    }


    // Temporal.PlainDate
    m_temporalPlainDate = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainDate(), builtinTemporalPlainDateConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDate->setGlobalIntrinsicObject(state);

    m_temporalPlainDatePrototype = new PrototypeObject(state);
    m_temporalPlainDatePrototype->setGlobalIntrinsicObject(state, true);

    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCompare()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCompare(), builtinTemporalPlainDateCompare, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                          ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainDate().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), builtinTemporalPlainDateCalendarId, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendarId()), desc);
    }

#define DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER_PROPERTY(NAME, PROP)                                                                     \
    {                                                                                                                                           \
        JSGetterSetter gs(                                                                                                                      \
            new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), builtinTemporalPlainDate##NAME, 0, NativeFunctionInfo::Strict)), \
            Value(Value::EmptyValue));                                                                                                          \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                       \
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazy##NAME()), desc);                          \
    }

    FOR_EACH_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER(DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER_PROPERTY)
#undef DEFINE_PLAINDATE_PROTOTYPE_CALENDARDATE_GETTER_PROPERTY


    m_temporalPlainDate->setFunctionPrototype(state, m_temporalPlainDatePrototype);


    m_temporal = new TemporalObject(state);
    m_temporal->setGlobalIntrinsicObject(state);

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                        ObjectPropertyDescriptor(Value(strings->lazyCapitalTemporal().string()), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalNow()),
                                        ObjectPropertyDescriptor(m_temporalNow, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalDuration()),
                                        ObjectPropertyDescriptor(m_temporalDuration, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalInstant()),
                                        ObjectPropertyDescriptor(m_temporalInstant, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainDate()),
                                        ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalTemporal()),
                        ObjectPropertyDescriptor(m_temporal, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

#else

void GlobalObject::initializeTemporal(ExecutionState& state)
{
    // dummy initialize function
}

#endif

} // namespace Escargot
