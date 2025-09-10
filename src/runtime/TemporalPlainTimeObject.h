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

#ifndef __EscargotTemporalPlainTimeObject__
#define __EscargotTemporalPlainTimeObject__

#include "runtime/TemporalObject.h"

namespace Escargot {

class TemporalPlainTimeObject : public DerivedObject {
public:
    TemporalPlainTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainTime plainTime);

    virtual bool isTemporalPlainTimeObject() const override
    {
        return true;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimerecord
    static ISO8601::PartialPlainTime toTemporalTimeRecord(ExecutionState& state, Value temporalTimeLike, Optional<bool> completeness);

    // https://tc39.es/proposal-temporal/#sec-temporal-regulatetime
    static ISO8601::PlainTime regulateTime(ExecutionState& state, int64_t hour, int64_t minute, int64_t second,
                                           int64_t millisecond, int64_t microsecond, int64_t nanosecond, TemporalOverflowOption overflow);

    // https://tc39.es/proposal-temporal/#sec-temporal-roundtime
    static ISO8601::Duration roundTime(ExecutionState& state, ISO8601::PlainTime plainTime, double increment, ISO8601::DateTimeUnit unit, ISO8601::RoundingMode roundingMode);

    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtotime
    enum class AddDurationToTimeOperation {
        Add,
        Subtract
    };
    TemporalPlainTimeObject* addDurationToTime(ExecutionState& state, AddDurationToTimeOperation operation, Value temporalDurationLike);

    ISO8601::PlainTime plainTime() const
    {
        return *m_plainTime;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.with
    TemporalPlainTimeObject* with(ExecutionState& state, Value temporalTimeLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
    String* toString(ExecutionState& state, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.since
    TemporalDurationObject* since(ExecutionState& state, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.until
    TemporalDurationObject* until(ExecutionState& state, Value other, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
    TemporalPlainTimeObject* round(ExecutionState& state, Value roundToInput);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.equals
    bool equals(ExecutionState& state, Value otherInput);

    // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.compare
    static int compare(ExecutionState& state, Value one, Value two);

private:
    // https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaintime
    enum class DifferenceTemporalPlainTime {
        Until,
        Since
    };
    ISO8601::Duration differenceTemporalPlainTime(ExecutionState& state, DifferenceTemporalPlainTime operation, Value other, Value options);

    ISO8601::PlainTime* m_plainTime;
};

} // namespace Escargot

#endif
#endif
