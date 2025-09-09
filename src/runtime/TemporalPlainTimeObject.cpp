#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#include "TemporalPlainTimeObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

TemporalPlainTimeObject::TemporalPlainTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainTime plainTime)
    : DerivedObject(state, proto)
    , m_plainTime(new(PointerFreeGC) ISO8601::PlainTime(plainTime))
{
}

TemporalPlainTimeObject* TemporalPlainTimeObject::toTemporalTime(ExecutionState& state, Value item, Value options)
{
    ISO8601::PlainTime result;
    // If options is not present, set options to undefined.
    // If item is an Object, then
    if (item.isObject()) {
        // If item has an [[InitializedTemporalTime]] internal slot, then
        if (item.asObject()->isTemporalPlainTimeObject()) {
            // Let resolvedOptions be ? GetOptionsObject(options).
            auto resolvedOptions = Intl::getOptionsObject(state, options);
            // Perform ? GetTemporalOverflowOption(resolvedOptions).
            if (resolvedOptions) {
                Temporal::getTemporalOverflowOption(state, resolvedOptions);
            }
            // Return ! CreateTemporalTime(item.[[Time]]).
            return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(),
                                               item.asObject()->asTemporalPlainTimeObject()->plainTime());
        }
        // TODO If item has an [[InitializedTemporalDateTime]] internal slot, then...
        // TODO If item has an [[InitializedTemporalDateTime]] internal slot, then...
        // Let result be ? ToTemporalTimeRecord(item).
        auto resultRecord = toTemporalTimeRecord(state, item, NullOption);
        // Let resolvedOptions be ? GetOptionsObject(options).
        auto resolvedOptions = Intl::getOptionsObject(state, options);
        // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
        // Set result to ? RegulateTime(result.[[Hour]], result.[[Minute]], result.[[Second]], result.[[Millisecond]], result.[[Microsecond]], result.[[Nanosecond]], overflow).
        result = regulateTime(state, resultRecord.hour().value(), resultRecord.minute().value(), resultRecord.second().value(), resultRecord.millisecond().value(),
                              resultRecord.microsecond().value(), resultRecord.nanosecond().value(), overflow);
    } else {
        // Else,
        // If item is not a String, throw a TypeError exception.
        if (!item.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ToTemporalTime needs Object or String");
        }
        // Let parseResult be ? ParseISODateTime(item, « TemporalTimeString »).
        auto parseDateTimeResult = ISO8601::parseCalendarDateTime(item.asString());
        auto parseTimeResult = ISO8601::parseTime(item.asString());
        // If ParseText(StringToCodePoints(item), AmbiguousTemporalTimeString) is a Parse Node, throw a RangeError exception.
        // Assert: parseResult.[[Time]] is not start-of-day.
        if (!parseTimeResult && (!parseDateTimeResult || !std::get<1>(parseDateTimeResult.value()))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalTime needs ISO Time string");
        }
        if ((parseDateTimeResult && std::get<2>(parseDateTimeResult.value()) && std::get<2>(parseDateTimeResult.value()).value().m_z) || (parseTimeResult && std::get<1>(parseTimeResult.value()) && std::get<1>(parseTimeResult.value()).value().m_z)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "ToTemporalTime needs ISO Time string without UTC designator");
        }
        // Set result to parseResult.[[Time]].
        if (parseDateTimeResult && std::get<1>(parseDateTimeResult.value())) {
            result = std::get<1>(parseDateTimeResult.value()).value();
        } else {
            result = std::get<0>(parseTimeResult.value());
        }
        // Let resolvedOptions be ? GetOptionsObject(options).
        auto resolvedOptions = Intl::getOptionsObject(state, options);
        // Perform ? GetTemporalOverflowOption(resolvedOptions).
        Temporal::getTemporalOverflowOption(state, resolvedOptions);
    }
    return new TemporalPlainTimeObject(state, state.context()->globalObject()->temporalPlainTimePrototype(), result);
}

ISO8601::PartialPlainTime TemporalPlainTimeObject::toTemporalTimeRecord(ExecutionState& state, Value temporalTimeLike, Optional<bool> completeness)
{
    // If completeness is not present, set completeness to complete.
    if (!completeness) {
        completeness = true;
    }

    ISO8601::PartialPlainTime result;
    // If completeness is complete, then
    if (completeness.value()) {
        // Let result be a new TemporalTimeLike Record with each field set to 0.
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    result.set##capitalizedName(0);
        PLAIN_TIME_UNITS(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
    } else {
        // Else,
        // Let result be a new TemporalTimeLike Record with each field set to unset.
    }

    // Let any be false.
    bool any = false;

    // Step 5..16
#define DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName)                                                         \
    {                                                                                                                  \
        Value value = Object::getV(state, temporalTimeLike, state.context()->staticStrings().lazy##capitalizedName()); \
        if (!value.isUndefined()) {                                                                                    \
            any = true;                                                                                                \
            result.set##capitalizedName(value.toIntegerWithTruncation(state));                                         \
        }                                                                                                              \
    }
    PLAIN_TIME_UNITS_ALPHABET_ORDER(DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef DEFINE_ISO8601_PLAIN_TIME_FIELD
    // If any is false, throw a TypeError exception.
    if (!any) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "At least one field required for ToTemporalTimeRecord");
    }
    // Return result.
    return result;
}

inline int64_t clamping(int64_t v, int64_t lower, int64_t upper)
{
    if (v < lower) {
        return lower;
    }
    if (v > upper) {
        return upper;
    }
    return v;
}

ISO8601::PlainTime TemporalPlainTimeObject::regulateTime(ExecutionState& state, int64_t hour, int64_t minute, int64_t second,
                                                         int64_t millisecond, int64_t microsecond, int64_t nanosecond, TemporalOverflowOption overflow)
{
    // If overflow is constrain, then
    if (overflow == TemporalOverflowOption::Constrain) {
        // Set hour to the result of clamping hour between 0 and 23.
        hour = clamping(hour, 0, 23);
        // Set minute to the result of clamping minute between 0 and 59.
        minute = clamping(minute, 0, 59);
        // Set second to the result of clamping second between 0 and 59.
        second = clamping(second, 0, 59);
        // Set millisecond to the result of clamping millisecond between 0 and 999.
        millisecond = clamping(millisecond, 0, 999);
        // Set microsecond to the result of clamping microsecond between 0 and 999.
        microsecond = clamping(microsecond, 0, 999);
        // Set nanosecond to the result of clamping nanosecond between 0 and 999.
        nanosecond = clamping(nanosecond, 0, 999);
    } else {
        // Else,
        // Assert: overflow is reject.
        // If IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond) is false, throw a RangeError exception.
        if (!Temporal::isValidTime(hour, minute, second, millisecond, microsecond, nanosecond)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid time");
        }
    }

    // Return CreateTimeRecord(hour, minute, second, millisecond, microsecond, nanosecond).
    return ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

} // namespace Escargot

#endif
