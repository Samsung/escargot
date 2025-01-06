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

#if defined(ENABLE_TEMPORAL)

// abstract functions (helper)
static int toIntegerWithTruncation(ExecutionState& state, const Value& arg)
{
    double num = arg.toNumber(state);
    if (std::isnan(num) || (num == std::numeric_limits<double>::infinity()) || (num == -std::numeric_limits<double>::infinity())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
    }

    return static_cast<int>(trunc(num));
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

    // Temporal.PlainDate
    m_temporalPlainDate = new NativeFunctionObject(state, NativeFunctionInfo(strings->lazyPlainDate(), builtinTemporalPlainDateConstructor, 3), NativeFunctionObject::__ForBuiltinConstructor__);
    m_temporalPlainDate->setGlobalIntrinsicObject(state);

    m_temporalPlainDatePrototype = new PrototypeObject(state);
    m_temporalPlainDatePrototype->setGlobalIntrinsicObject(state, true);

    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->from), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->from, builtinTemporalPlainDateFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDate->directDefineOwnProperty(state, ObjectPropertyName(strings->compare), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->compare, builtinTemporalPlainDateCompare, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_temporalPlainDatePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                          ObjectPropertyDescriptor(Value(strings->lazyTemporalDotPlainDate().string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    m_temporalPlainDate->setFunctionPrototype(state, m_temporalPlainDatePrototype);


    m_temporal = new TemporalObject(state);
    m_temporal->setGlobalIntrinsicObject(state);

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                        ObjectPropertyDescriptor(Value(strings->lazyTemporal().string()), (ObjectPropertyDescriptor::PresentAttribute)ObjectPropertyDescriptor::ConfigurablePresent));

    m_temporal->directDefineOwnProperty(state, ObjectPropertyName(strings->lazyPlainDate()),
                                        ObjectPropertyDescriptor(m_temporalPlainDate, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
