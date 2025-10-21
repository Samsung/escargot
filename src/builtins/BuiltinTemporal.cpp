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
#include "runtime/TemporalPlainTimeObject.h"
#include "runtime/TemporalPlainDateObject.h"
#include "runtime/TemporalPlainDateTimeObject.h"
#include "runtime/TemporalPlainMonthDayObject.h"
#include "runtime/TemporalPlainYearMonthObject.h"
#include "runtime/TemporalNowObject.h"
#include "runtime/DateObject.h"
#include "runtime/ArrayObject.h"

namespace Escargot {

#if defined(ENABLE_TEMPORAL)

static Value builtinTemporalAnyInstanceValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "can't convert Temporal object to primitive type");
    return Value();
}

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
    if (!mayInt128 || !ISO8601::isValidEpochNanoseconds(mayInt128.value())) {
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

static Value builtinTemporalPlainTimeConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }
    int64_t hour = 0;
    int64_t minute = 0;
    int64_t second = 0;
    int64_t millisecond = 0;
    int64_t microsecond = 0;
    int64_t nanosecond = 0;
    // If hour is undefined, set hour to 0; else set hour to ? ToIntegerWithTruncation(hour).
    if (argc >= 1 && !argv[0].isUndefined()) {
        hour = argv[0].toIntegerWithTruncation(state);
    }
    // If minute is undefined, set minute to 0; else set minute to ? ToIntegerWithTruncation(minute).
    if (argc >= 2 && !argv[1].isUndefined()) {
        minute = argv[1].toIntegerWithTruncation(state);
    }
    // If second is undefined, set second to 0; else set second to ? ToIntegerWithTruncation(second).
    if (argc >= 3 && !argv[2].isUndefined()) {
        second = argv[2].toIntegerWithTruncation(state);
    }
    // If millisecond is undefined, set millisecond to 0; else set millisecond to ? ToIntegerWithTruncation(millisecond).
    if (argc >= 4 && !argv[3].isUndefined()) {
        millisecond = argv[3].toIntegerWithTruncation(state);
    }
    // If microsecond is undefined, set microsecond to 0; else set microsecond to ? ToIntegerWithTruncation(microsecond).
    if (argc >= 5 && !argv[4].isUndefined()) {
        microsecond = argv[4].toIntegerWithTruncation(state);
    }
    // If nanosecond is undefined, set nanosecond to 0; else set nanosecond to ? ToIntegerWithTruncation(nanosecond).
    if (argc >= 6 && !argv[5].isUndefined()) {
        nanosecond = argv[5].toIntegerWithTruncation(state);
    }
    // If IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond) is false, throw a RangeError exception.
    if (!Temporal::isValidTime(hour, minute, second, millisecond, microsecond, nanosecond)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid time");
    }
    // Let time be CreateTimeRecord(hour, minute, second, millisecond, microsecond, nanosecond).
    // Return ? CreateTemporalTime(time, NewTarget).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainTimePrototype();
    });
    return new TemporalPlainTimeObject(state, proto, ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond));
}

static Value builtinTemporalPlainTimeFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalTime(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainTimeCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(TemporalPlainTimeObject::compare(state, argv[0], argv[1]));
}

#define RESOLVE_THIS_BINDING_TO_PLAINTIME(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainTimeObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainTime().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                             \
    TemporalPlainTimeObject* NAME = thisValue.asObject()->asTemporalPlainTimeObject();

#define RESOLVE_THIS_BINDING_TO_PLAINTIME2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainTimeObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainTime().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                     \
    TemporalPlainTimeObject* NAME = thisValue.asObject()->asTemporalPlainTimeObject();

static Value builtinTemporalPlainTimeToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, toString);
    return plainTime->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalPlainTimeToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, toJSON);
    return plainTime->toString(state, Value());
}

static Value builtinTemporalPlainTimeToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, toLocaleString);
    return plainTime->toString(state, Value());
}

static Value builtinTemporalPlainTimeWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, with);
    return plainTime->with(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainTimeAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, add);
    return plainTime->addDurationToTime(state, TemporalPlainTimeObject::AddDurationToTimeOperation::Add, argv[0]);
}

static Value builtinTemporalPlainTimeSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME(plainTime, Subtract);
    return plainTime->addDurationToTime(state, TemporalPlainTimeObject::AddDurationToTimeOperation::Subtract, argv[0]);
}

static Value builtinTemporalPlainTimeSince(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME(plainTime, Since);
    return plainTime->since(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainTimeUntil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME(plainTime, Until);
    return plainTime->until(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainTimeRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME2(plainTime, round);
    return plainTime->round(state, argv[0]);
}

static Value builtinTemporalPlainTimeEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINTIME(plainTime, Equals);
    return Value(plainTime->equals(state, argv[0]));
}

#define RESOLVE_THIS_BINDING_TO_PLAINDATE(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDate().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                             \
    TemporalPlainDateObject* NAME = thisValue.asObject()->asTemporalPlainDateObject();

#define RESOLVE_THIS_BINDING_TO_PLAINDATE2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDate().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                     \
    TemporalPlainDateObject* NAME = thisValue.asObject()->asTemporalPlainDateObject();


static Value builtinTemporalPlainDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainDatePrototype();
    });

    // Let y be ? ToIntegerWithTruncation(isoYear).
    auto y = argv[0].toIntegerWithTruncation(state);
    // Let m be ? ToIntegerWithTruncation(isoMonth).
    auto m = argv[1].toIntegerWithTruncation(state);
    // Let d be ? ToIntegerWithTruncation(isoDay).
    auto d = argv[2].toIntegerWithTruncation(state);
    // If calendar is undefined, set calendar to "iso8601".
    Value calendar = argc > 3 ? argv[3] : Value();
    if (calendar.isUndefined()) {
        calendar = state.context()->staticStrings().lazyISO8601().string();
    }
    // If calendar is not a String, throw a TypeError exception.
    if (!calendar.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "calendar must be String");
    }
    // Set calendar to ? CanonicalizeCalendar(calendar).
    auto mayCalendar = Calendar::fromString(calendar.asString());
    if (!mayCalendar) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
    }
    // If IsValidISODate(y, m, d) is false, throw a RangeError exception.
    if (m < 1 || m > 12 || d < 1 || d > ISO8601::daysInMonth(y, m) || !ISO8601::isDateTimeWithinLimits(y, m, d, 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    // Let isoDate be CreateISODateRecord(y, m, d).
    // Return ? CreateTemporalDate(isoDate, calendar, NewTarget).
    return new TemporalPlainDateObject(state, proto, ISO8601::PlainDate(y, m, d), mayCalendar.value());
}

static Value builtinTemporalPlainDateFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalDate(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(TemporalPlainDateObject::compare(state, argv[0], argv[1]));
}

static Value builtinTemporalPlainDateToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE2(plainDate, toJSON);
    return plainDate->toString(state, Value());
}

static Value builtinTemporalPlainDateToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE2(plainDate, toLocaleString);
    return plainDate->toString(state, Value());
}

static Value builtinTemporalPlainDateToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE2(plainDate, toString);
    return plainDate->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalPlainDateEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, Equals);
    return Value(plainDate->equals(state, argv[0]));
}

static Value builtinTemporalPlainDateCalendarId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(temporalDate, CalendarId);
    if (temporalDate->calendarID() == Calendar(Calendar::ID::ISO8601)) {
        return state.context()->staticStrings().lazyISO8601().string();
    } else {
        return temporalDate->calendarID().toString();
    }
}

static Value builtinTemporalPlainDateMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(temporalDate, MonthCode);
    return temporalDate->monthCode(state);
}

static Value builtinTemporalPlainDateAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE2(plainDate, add);
    return plainDate->addDurationToDate(state, TemporalPlainDateObject::AddDurationToDateOperation::Add, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, Subtract);
    return plainDate->addDurationToDate(state, TemporalPlainDateObject::AddDurationToDateOperation::Subtract, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE2(plainDate, with);
    return plainDate->with(state, argv[0], argc > 1 ? argv[1] : argv[0]);
}

static Value builtinTemporalPlainDateWithCalendar(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, WithCalendar);
    return plainDate->withCalendar(state, argv[0]);
}

static Value builtinTemporalPlainDateSince(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, Since);
    return plainDate->since(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateUntil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATE(plainDate, Until);
    return plainDate->until(state, argv[0], argc > 1 ? argv[1] : Value());
}

#define RESOLVE_THIS_BINDING_TO_PLAINDATETIME(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateTimeObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDateTime().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                                 \
    TemporalPlainDateTimeObject* NAME = thisValue.asObject()->asTemporalPlainDateTimeObject();

#define RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateTimeObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDateTime().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                         \
    TemporalPlainDateTimeObject* NAME = thisValue.asObject()->asTemporalPlainDateTimeObject();

static Value builtinTemporalPlainDateTimeConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainDateTimePrototype();
    });

    // Let isoYear be ? ToIntegerWithTruncation(isoYear).
    auto y = argv[0].toIntegerWithTruncation(state);
    // Let isoMonth be ? ToIntegerWithTruncation(isoMonth).
    auto m = argv[1].toIntegerWithTruncation(state);
    // Let isoDay be ? ToIntegerWithTruncation(isoDay).
    auto d = argv[2].toIntegerWithTruncation(state);

#define GET_TIME(index, name)                              \
    int64_t name = 0;                                      \
    if (argc > index && !argv[index].isUndefined()) {      \
        name = argv[index].toIntegerWithTruncation(state); \
    }
    // If hour is undefined, set hour to 0; else set hour to ? ToIntegerWithTruncation(hour).
    GET_TIME(3, hour)
    // If minute is undefined, set minute to 0; else set minute to ? ToIntegerWithTruncation(minute).
    GET_TIME(4, minute)
    // If second is undefined, set second to 0; else set second to ? ToIntegerWithTruncation(second).
    GET_TIME(5, second)
    // If millisecond is undefined, set millisecond to 0; else set millisecond to ? ToIntegerWithTruncation(millisecond).
    GET_TIME(6, millisecond)
    // If microsecond is undefined, set microsecond to 0; else set microsecond to ? ToIntegerWithTruncation(microsecond).
    GET_TIME(7, microsecond)
    // If nanosecond is undefined, set nanosecond to 0; else set nanosecond to ? ToIntegerWithTruncation(nanosecond).
    GET_TIME(8, nanosecond)

    // If calendar is undefined, set calendar to "iso8601".
    Value calendar = argc > 9 ? argv[9] : Value();
    if (calendar.isUndefined()) {
        calendar = state.context()->staticStrings().lazyISO8601().string();
    }
    // If calendar is not a String, throw a TypeError exception.
    if (!calendar.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "calendar must be String");
    }
    // Set calendar to ? CanonicalizeCalendar(calendar).
    auto mayCalendar = Calendar::fromString(calendar.asString());
    if (!mayCalendar) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
    }

    // If IsValidISODate(isoYear, isoMonth, isoDay) is false, throw a RangeError exception.
    if (m < 1 || m > 12 || d < 1 || d > ISO8601::daysInMonth(y, m) || !ISO8601::isDateTimeWithinLimits(y, m, d, 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    // Let isoDate be CreateISODateRecord(y, m, d).

    // If IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond) is false, throw a RangeError exception.
    if (!Temporal::isValidTime(hour, minute, second, millisecond, microsecond, nanosecond)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid time");
    }
    // Let time be CreateTimeRecord(hour, minute, second, millisecond, microsecond, nanosecond).
    // Let isoDateTime be CombineISODateAndTimeRecord(isoDate, time).
    // Return ? CreateTemporalDateTime(isoDateTime, calendar, NewTarget).
    return new TemporalPlainDateTimeObject(state, proto, ISO8601::PlainDate(y, m, d), ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond), mayCalendar.value());
}

static Value builtinTemporalPlainDateTimeFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalDateTime(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateTimeToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, toJSON);
    return plainDateTime->toString(state, Value());
}

static Value builtinTemporalPlainDateTimeToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, toLocaleString);
    return plainDateTime->toString(state, Value());
}

static Value builtinTemporalPlainDateTimeToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, toString);
    return plainDateTime->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalPlainDateTimeCalendarId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, CalendarId);
    if (plainDateTime->calendarID() == Calendar(Calendar::ID::ISO8601)) {
        return state.context()->staticStrings().lazyISO8601().string();
    } else {
        return plainDateTime->calendarID().toString();
    }
}

static Value builtinTemporalPlainDateTimeMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, MonthCode);
    return plainDateTime->monthCode(state);
}

static Value builtinTemporalPlainDateTimeWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, with);
    return plainDateTime->with(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateTimeWithPlainTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, WithPlainTime);
    return plainDateTime->withPlainTime(state, argc > 0 ? argv[0] : Value());
}

static Value builtinTemporalPlainDateTimeWithCalendar(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, Calendar);
    return plainDateTime->withCalendar(state, argv[0]);
}

static Value builtinTemporalPlainDateTimeAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, add);
    return plainDateTime->addDurationToDateTime(state, TemporalPlainDateTimeObject::AddDurationToDateTimeOperation::Add, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateTimeSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, Subtract);
    return plainDateTime->addDurationToDateTime(state, TemporalPlainDateTimeObject::AddDurationToDateTimeOperation::Subtract, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainDateTimeSince(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, Since);
    return new TemporalDurationObject(state, plainDateTime->differenceTemporalPlainDateTime(state, TemporalPlainDateTimeObject::DifferenceTemporalPlainDateTime::Since, argv[0], argc > 1 ? argv[1] : Value()));
}

static Value builtinTemporalPlainDateTimeUntil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, Until);
    return new TemporalDurationObject(state, plainDateTime->differenceTemporalPlainDateTime(state, TemporalPlainDateTimeObject::DifferenceTemporalPlainDateTime::Until, argv[0], argc > 1 ? argv[1] : Value()));
}

static Value builtinTemporalPlainDateTimeRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME2(plainDateTime, round);
    return plainDateTime->round(state, argv[0]);
}

static Value builtinTemporalPlainDateTimeEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINDATETIME(plainDateTime, Equals);
    return Value(plainDateTime->equals(state, argv[0]));
}

static Value builtinTemporalPlainYearMonthConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainYearMonthPrototype();
    });

    Value referenceISODay = argc > 3 ? argv[3] : Value();
    // If referenceISODay is undefined, then
    if (referenceISODay.isUndefined()) {
        // Set referenceISODay to 1𝔽
        referenceISODay = Value(1);
    }

    // Let y be ? ToIntegerWithTruncation(isoYear).
    auto y = argv[0].toIntegerWithTruncation(state);
    // Let m be ? ToIntegerWithTruncation(isoMonth).
    auto m = argv[1].toIntegerWithTruncation(state);
    // If calendar is undefined, set calendar to "iso8601".
    Value calendar = argc >= 3 ? argv[2] : Value();
    if (calendar.isUndefined()) {
        calendar = state.context()->staticStrings().lazyISO8601().string();
    }
    // If calendar is not a String, throw a TypeError exception.
    if (!calendar.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "calendar must be String");
    }
    // Set calendar to ? CanonicalizeCalendar(calendar).
    auto mayCalendar = Calendar::fromString(calendar.asString());
    if (!mayCalendar) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
    }
    // Let ref be ? ToIntegerWithTruncation(referenceISODay).
    auto ref = referenceISODay.toIntegerWithTruncation(state);
    // If IsValidISODate(y, m, ref) is false, throw a RangeError exception.
    // If IsValidISODate(y, m, d) is false, throw a RangeError exception.
    if (m < 1 || m > 12 || ref < 1 || ref > ISO8601::daysInMonth(y, m) || !Temporal::isoYearMonthWithinLimits(ISO8601::PlainDate(y, m, ref))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    // Let isoDate be CreateISODateRecord(y, m, ref).
    // Return ? CreateTemporalYearMonth(isoDate, calendar, NewTarget).
    return new TemporalPlainYearMonthObject(state, proto, ISO8601::PlainDate(y, m, ref), mayCalendar.value());
}

static Value builtinTemporalPlainYearMonthFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalYearMonth(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainYearMonthCompare(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(TemporalPlainYearMonthObject::compare(state, argv[0], argv[1]));
}

#define RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainYearMonthObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainYearMonth().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                                  \
    TemporalPlainYearMonthObject* NAME = thisValue.asObject()->asTemporalPlainYearMonthObject();

#define RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainYearMonthObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainYearMonth().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                          \
    TemporalPlainYearMonthObject* NAME = thisValue.asObject()->asTemporalPlainYearMonthObject();

static Value builtinTemporalPlainYearMonthCalendarId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(temporalYearMonth, CalendarId);
    if (temporalYearMonth->calendarID() == Calendar(Calendar::ID::ISO8601)) {
        return state.context()->staticStrings().lazyISO8601().string();
    } else {
        return temporalYearMonth->calendarID().toString();
    }
}

static Value builtinTemporalPlainYearMonthMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(temporalYearMonth, MonthCode);
    return temporalYearMonth->monthCode(state);
}

static Value builtinTemporalPlainYearMonthToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(plainYearMonth, toJSON);
    return plainYearMonth->toString(state, Value());
}

static Value builtinTemporalPlainYearMonthToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(plainYearMonth, toLocaleString);
    return plainYearMonth->toString(state, Value());
}

static Value builtinTemporalPlainYearMonthToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(plainYearMonth, toString);
    return plainYearMonth->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalPlainYearMonthWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(plainYearMonth, with);
    return plainYearMonth->with(state, argv[0], argc > 1 ? argv[1] : argv[0]);
}

static Value builtinTemporalPlainYearMonthToPlainDate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(plainYearMonth, ToPlainDate);
    return plainYearMonth->toPlainDate(state, argv[0]);
}

static Value builtinTemporalPlainYearMonthAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH2(plainYearMonth, add);
    return plainYearMonth->addDurationToYearMonth(state, TemporalPlainYearMonthObject::AddDurationToYearMonthOperation::Add, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainYearMonthSubtract(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(plainYearMonth, Subtract);
    return plainYearMonth->addDurationToYearMonth(state, TemporalPlainYearMonthObject::AddDurationToYearMonthOperation::Subtract, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainYearMonthUntil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(plainYearMonth, Until);
    return plainYearMonth->until(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainYearMonthSince(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(plainYearMonth, Since);
    return plainYearMonth->since(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainYearMonthEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINYEARMONTH(plainYearMonth, Equals);
    return Value(plainYearMonth->equals(state, argv[0]));
}

static Value builtinTemporalPlainMonthDayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainMonthDayPrototype();
    });

    Value referenceISOYear = argc > 3 ? argv[3] : Value();
    // If referenceISOYear is undefined, then
    if (referenceISOYear.isUndefined()) {
        // Set referenceISOYear to 1972𝔽 (the first ISO 8601 leap year after the epoch).
        referenceISOYear = Value(1972);
    }

    // Let m be ? ToIntegerWithTruncation(isoMonth).
    auto m = argv[0].toIntegerWithTruncation(state);
    // Let d be ? ToIntegerWithTruncation(isoDay).
    auto d = argv[1].toIntegerWithTruncation(state);
    // If calendar is undefined, set calendar to "iso8601".
    Value calendar = argc >= 3 ? argv[2] : Value();
    if (calendar.isUndefined()) {
        calendar = state.context()->staticStrings().lazyISO8601().string();
    }
    // If calendar is not a String, throw a TypeError exception.
    if (!calendar.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "calendar must be String");
    }
    // Set calendar to ? CanonicalizeCalendar(calendar).
    auto mayCalendar = Calendar::fromString(calendar.asString());
    if (!mayCalendar) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid calendar");
    }
    // Let y be ? ToIntegerWithTruncation(referenceISOYear).
    auto y = referenceISOYear.toIntegerWithTruncation(state);
    // If IsValidISODate(y, m, ref) is false, throw a RangeError exception.
    // If IsValidISODate(y, m, d) is false, throw a RangeError exception.
    if (m < 1 || m > 12 || d < 1 || d > ISO8601::daysInMonth(y, m) || !ISO8601::isDateTimeWithinLimits(y, m, d, 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    // Let isoDate be CreateISODateRecord(y, m, d).
    // Return ? CreateTemporalMonthDay(isoDate, calendar, NewTarget).
    return new TemporalPlainMonthDayObject(state, proto, ISO8601::PlainDate(y, m, d), mayCalendar.value());
}

static Value builtinTemporalPlainMonthDayFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Temporal::toTemporalMonthDay(state, argv[0], argc > 1 ? argv[1] : Value());
}

#define RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                                  \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainMonthDayObject()) {                                                                                                                                                                                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainMonthDay().string(), true, state.context()->staticStrings().lazy##BUILT_IN_METHOD().string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                                 \
    TemporalPlainMonthDayObject* NAME = thisValue.asObject()->asTemporalPlainMonthDayObject();

#define RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY2(NAME, BUILT_IN_METHOD)                                                                                                                                                                                                         \
    if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainMonthDayObject()) {                                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainMonthDay().string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                                         \
    TemporalPlainMonthDayObject* NAME = thisValue.asObject()->asTemporalPlainMonthDayObject();

static Value builtinTemporalPlainMonthDayCalendarId(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY(temporalMonthDay, CalendarId);
    if (temporalMonthDay->calendarID() == Calendar(Calendar::ID::ISO8601)) {
        return state.context()->staticStrings().lazyISO8601().string();
    } else {
        return temporalMonthDay->calendarID().toString();
    }
}

static Value builtinTemporalPlainMonthDayMonthCode(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY(temporalMonthDay, MonthCode);
    return temporalMonthDay->monthCode(state);
}

static Value builtinTemporalPlainMonthDayToJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY2(plainMonthDay, toJSON);
    return plainMonthDay->toString(state, Value());
}

static Value builtinTemporalPlainMonthDayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY2(plainMonthDay, toLocaleString);
    return plainMonthDay->toString(state, Value());
}

static Value builtinTemporalPlainMonthDayToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY2(plainMonthDay, toString);
    return plainMonthDay->toString(state, argc ? argv[0] : Value());
}

static Value builtinTemporalPlainMonthDayWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY2(plainMonthDay, with);
    return plainMonthDay->with(state, argv[0], argc > 1 ? argv[1] : Value());
}

static Value builtinTemporalPlainMonthDayEquals(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY(plainMonthDay, Equals);
    return Value(plainMonthDay->equals(state, argv[0]));
}

static Value builtinTemporalPlainMonthDayToPlainDate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_PLAINMONTHDAY(plainMonthDay, ToPlainDate);
    return plainMonthDay->toPlainDate(state, argv[0]);
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
    m_temporalDurationPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
    m_temporalInstantPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
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

    // Temporal.PlainTime
    m_temporalPlainTime = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainTime(), builtinTemporalPlainTimeConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainTime->setGlobalIntrinsicObject(state);

    m_temporalPlainTime->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainTimeFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTime->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCompare()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCompare(), builtinTemporalPlainTimeCompare, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainTimePrototype = m_temporalPlainTime->getFunctionPrototype(state).asObject();
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                          ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainTime().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalPlainTimeToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalPlainTimeToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalPlainTimeToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalPlainTimeWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalPlainTimeAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalPlainTimeSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySince()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySince(), builtinTemporalPlainTimeSince, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyUntil()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyUntil(), builtinTemporalPlainTimeUntil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->round), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->round, builtinTemporalPlainTimeRound, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalPlainTimeEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

#define DEFINE_PLAINTIME_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                      \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainTimeObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainTime().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainTimeObject* s = thisValue.asObject()->asTemporalPlainTimeObject(); \
            return Value(s->plainTime().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                                   \
    }

#define DEFINE_GETTER(name, Name) DEFINE_PLAINTIME_PROTOTYPE_GETTER_PROPERTY(name, #name, Name)
    PLAIN_TIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINTIME_PROTOTYPE_GETTER_PROPERTY

    // Temporal.PlainDate
    m_temporalPlainDate = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainDate(), builtinTemporalPlainDateConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDate->setGlobalIntrinsicObject(state);

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCompare()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCompare(), builtinTemporalPlainDateCompare, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDatePrototype = m_temporalPlainDate->getFunctionPrototype(state).asObject();
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                          ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainDate().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalPlainDateToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalPlainDateToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalPlainDateToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalPlainDateEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalPlainDateAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalPlainDateSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalPlainDateWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyWithCalendar()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyWithCalendar(), builtinTemporalPlainDateWithCalendar, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyUntil()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyUntil(), builtinTemporalPlainDateUntil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySince()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySince(), builtinTemporalPlainDateSince, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        AtomicString name(state.context(), "get calendarId");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainDateCalendarId, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendarId()), desc);
    }

    {
        AtomicString name(state.context(), "get monthCode");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainDateMonthCode, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonthCode()), desc);
    }

#define DEFINE_PLAINDATE_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                      \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDate().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainDateObject* s = thisValue.asObject()->asTemporalPlainDateObject(); \
            return Value(s->plainDate().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                                   \
    }

#define DEFINE_GETTER(name, Name) DEFINE_PLAINDATE_PROTOTYPE_GETTER_PROPERTY(name, #name, Name)
    PLAIN_DATE_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINDATE_PROTOTYPE_GETTER_PROPERTY

#define PLAINDATE_EXTRA_PROPERTY(F) \
    F(era, Era)                     \
    F(eraYear, EraYear)             \
    F(dayOfWeek, DayOfWeek)         \
    F(dayOfYear, DayOfYear)         \
    F(weekOfYear, WeekOfYear)       \
    F(yearOfWeek, YearOfWeek)       \
    F(daysInWeek, DaysInWeek)       \
    F(daysInMonth, DaysInMonth)     \
    F(daysInYear, DaysInYear)       \
    F(monthsInYear, MonthsInYear)   \
    F(inLeapYear, InLeapYear)

#define DEFINE_PLAINDATE_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        AtomicString pName(state.context(), stringName);                                                                                                                                                                        \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDate().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainDateObject* s = thisValue.asObject()->asTemporalPlainDateObject(); \
            return Value(s->name(state)); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                                   \
    }
#define DEFINE_GETTER(name, Name) DEFINE_PLAINDATE_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, #name, Name)
    PLAINDATE_EXTRA_PROPERTY(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINDATE_PROTOTYPE_EXTRA_GETTER_PROPERTY

    // Temporal.PlainDateTime
    m_temporalPlainDateTime = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainDateTime(), builtinTemporalPlainDateTimeConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDateTime->setGlobalIntrinsicObject(state);

    m_temporalPlainDateTime->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateTimeFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDateTimePrototype = m_temporalPlainDateTime->getFunctionPrototype(state).asObject();
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                              ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainDateTime().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalPlainDateTimeToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalPlainDateTimeToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalPlainDateTimeToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalPlainDateTimeWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyWithPlainTime()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyWithPlainTime(), builtinTemporalPlainDateTimeWithPlainTime, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyWithCalendar()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyWithCalendar(), builtinTemporalPlainDateTimeWithCalendar, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalPlainDateTimeAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalPlainDateTimeSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyUntil()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyUntil(), builtinTemporalPlainDateTimeUntil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySince()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySince(), builtinTemporalPlainDateTimeSince, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->round), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->round, builtinTemporalPlainDateTimeRound, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalPlainDateTimeEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        AtomicString name(state.context(), "get calendarId");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainDateTimeCalendarId, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendarId()), desc);
    }

    {
        AtomicString name(state.context(), "get monthCode");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainDateTimeMonthCode, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonthCode()), desc);
    }

#define DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                  \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateTimeObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDateTime().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainDateTimeObject* s = thisValue.asObject()->asTemporalPlainDateTimeObject(); \
            return Value(s->plainDate().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                               \
    }

#define DEFINE_GETTER(name, Name) DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY(name, #name, Name)
    PLAIN_DATE_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY

#define DEFINE_PLAINDATETIME_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                            \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        AtomicString pName(state.context(), stringName);                                                                                                                                                                        \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateTimeObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDateTime().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainDateTimeObject* s = thisValue.asObject()->asTemporalPlainDateTimeObject(); \
            return Value(s->name(state)); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                               \
    }
#define DEFINE_GETTER(name, Name) DEFINE_PLAINDATETIME_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, #name, Name)
    PLAINDATE_EXTRA_PROPERTY(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINDATE_PROTOTYPE_EXTRA_GETTER_PROPERTY

#define DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                  \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainDateTimeObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainDateTime().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainDateTimeObject* s = thisValue.asObject()->asTemporalPlainDateTimeObject(); \
            return Value(s->plainTime().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainDateTimePrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                               \
    }

#define DEFINE_GETTER(name, Name) DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY(name, #name, Name)
    PLAIN_TIME_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINDATETIME_PROTOTYPE_GETTER_PROPERTY

    // Temporal.PlainYearMonth
    m_temporalPlainYearMonth = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainYearMonth(), builtinTemporalPlainYearMonthConstructor, 2), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainYearMonth->setGlobalIntrinsicObject(state);

    m_temporalPlainYearMonth->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainYearMonthFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonth->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCompare()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCompare(), builtinTemporalPlainYearMonthCompare, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainYearMonthPrototype = m_temporalPlainYearMonth->getFunctionPrototype(state).asObject();
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                               ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainYearMonth().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalPlainYearMonthToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalPlainYearMonthToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalPlainYearMonthToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalPlainYearMonthWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyToPlainDate()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyToPlainDate(), builtinTemporalPlainYearMonthToPlainDate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->add), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->add, builtinTemporalPlainYearMonthAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySubtract()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySubtract(), builtinTemporalPlainYearMonthSubtract, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyUntil()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyUntil(), builtinTemporalPlainYearMonthUntil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazySince()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazySince(), builtinTemporalPlainYearMonthSince, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalPlainYearMonthEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        AtomicString name(state.context(), "get calendarId");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainYearMonthCalendarId, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendarId()), desc);
    }

    {
        AtomicString name(state.context(), "get monthCode");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainYearMonthMonthCode, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonthCode()), desc);
    }

#define DEFINE_PLAINYEARMONTH_PROTOTYPE_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                                 \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainYearMonthObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainYearMonth().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainYearMonthObject* s = thisValue.asObject()->asTemporalPlainYearMonthObject(); \
            return Value(s->plainDate().name()); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                              \
    }

#define DEFINE_GETTER(name, Name) DEFINE_PLAINYEARMONTH_PROTOTYPE_GETTER_PROPERTY(name, #name, Name)
    PLAIN_YEARMONTH_UNITS(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINYEARMONTH_PROTOTYPE_GETTER_PROPERTY

#define PLAINYEARMONTH_EXTRA_PROPERTY(F) \
    F(era, Era)                          \
    F(eraYear, EraYear)                  \
    F(daysInMonth, DaysInMonth)          \
    F(daysInYear, DaysInYear)            \
    F(monthsInYear, MonthsInYear)        \
    F(inLeapYear, InLeapYear)

#define DEFINE_PLAINYEARMONTH_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, stringName, Name)                                                                                                                                           \
    {                                                                                                                                                                                                                           \
        AtomicString name(state.context(), "get " stringName);                                                                                                                                                                  \
        AtomicString pName(state.context(), stringName);                                                                                                                                                                        \
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(name, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value { \
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainYearMonthObject()) { \
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainYearMonth().string(), true, String::fromASCII(stringName), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
            } \
            TemporalPlainYearMonthObject* s = thisValue.asObject()->asTemporalPlainYearMonthObject(); \
            return Value(s->name(state)); }, 0, NativeFunctionInfo::Strict)); \
        JSGetterSetter gs(getter, Value());                                                                                                                                                                                     \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);                                                                                                                                       \
        m_temporalPlainYearMonthPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazy##Name()), desc);                                                                                              \
    }
#define DEFINE_GETTER(name, Name) DEFINE_PLAINYEARMONTH_PROTOTYPE_EXTRA_GETTER_PROPERTY(name, #name, Name)
    PLAINYEARMONTH_EXTRA_PROPERTY(DEFINE_GETTER)
#undef DEFINE_GETTER
#undef DEFINE_PLAINYEARMONTH_PROTOTYPE_EXTRA_GETTER_PROPERTY

    // Temporal.PlainMonthDay
    m_temporalPlainMonthDay = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyCapitalPlainMonthDay(), builtinTemporalPlainMonthDayConstructor, 2), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainMonthDay->setGlobalIntrinsicObject(state);

    m_temporalPlainMonthDay->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainMonthDayFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype = m_temporalPlainMonthDay->getFunctionPrototype(state).asObject();
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                              ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainMonthDay().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinTemporalPlainMonthDayToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toJSON), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toJSON, builtinTemporalPlainMonthDayToJSON, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinTemporalPlainMonthDayToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinTemporalAnyInstanceValueOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->with), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->with, builtinTemporalPlainMonthDayWith, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyEquals()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyEquals(), builtinTemporalPlainMonthDayEquals, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyToPlainDate()), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyToPlainDate(), builtinTemporalPlainMonthDayToPlainDate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        AtomicString name(state.context(), "get calendarId");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainMonthDayCalendarId, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCalendarId()), desc);
    }

    {
        AtomicString name(state.context(), "get monthCode");
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(name, builtinTemporalPlainMonthDayMonthCode, 0, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyMonthCode()), desc);
    }

    {
        AtomicString day(state.context(), "get day");
        auto getter = new NativeFunctionObject(state, NativeFunctionInfo(day, [](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value {
            if (!thisValue.isObject() || !thisValue.asObject()->isTemporalPlainMonthDayObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().lazyCapitalPlainMonthDay().string(), true, String::fromASCII("day"), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
            }
            TemporalPlainMonthDayObject* s = thisValue.asObject()->asTemporalPlainMonthDayObject();
            return Value(s->plainDate().day()); }, 0, NativeFunctionInfo::Strict));
        JSGetterSetter gs(getter, Value());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_temporalPlainMonthDayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, strings->lazyDay()), desc);
    }

    m_temporal = new Object(state);
    m_temporal->setGlobalIntrinsicObject(state);

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                        ObjectPropertyDescriptor(Value(strings->lazyCapitalTemporal().string()), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalNow()),
                                        ObjectPropertyDescriptor(m_temporalNow, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalDuration()),
                                        ObjectPropertyDescriptor(m_temporalDuration, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalInstant()),
                                        ObjectPropertyDescriptor(m_temporalInstant, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainTime()),
                                        ObjectPropertyDescriptor(m_temporalPlainTime, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainDate()),
                                        ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainDateTime()),
                                        ObjectPropertyDescriptor(m_temporalPlainDateTime, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainYearMonth()),
                                        ObjectPropertyDescriptor(m_temporalPlainYearMonth, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyCapitalPlainMonthDay()),
                                        ObjectPropertyDescriptor(m_temporalPlainMonthDay, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
