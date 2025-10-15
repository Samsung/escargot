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

#ifndef __EscargotTemporalPlainYearMonthObject__
#define __EscargotTemporalPlainYearMonthObject__

#include "runtime/TemporalPlainDateObject.h"

namespace Escargot {

class TemporalPlainYearMonthObject : public TemporalPlainDateObject {
public:
    TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainYearMonth, Calendar calendar);
    TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar);

    virtual bool isTemporalPlainDateObject() const override
    {
        return false;
    }

    virtual bool isTemporalPlainYearMonthObject() const override
    {
        return true;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tostring
    String* toString(ExecutionState& state, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.compare
    static int compare(ExecutionState& state, Value one, Value two);

    // https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
    TemporalPlainYearMonthObject* with(ExecutionState& state, Value temporalYearMonthLike, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
    TemporalPlainDateObject* toPlainDate(ExecutionState& state, Value item);

    // https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
    enum class AddDurationToYearMonthOperation {
        Add,
        Subtract
    };
    TemporalPlainYearMonthObject* addDurationToYearMonth(ExecutionState& state, AddDurationToYearMonthOperation operation, Value temporalDurationLike, Value options);

    TemporalDurationObject* since(ExecutionState& state, Value other, Value options);
    TemporalDurationObject* until(ExecutionState& state, Value other, Value options);

    bool equals(ExecutionState& state, Value other);

private:
    // https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
    enum class DifferenceTemporalYearMonth {
        Until,
        Since
    };
    ISO8601::Duration differenceTemporalPlainYearMonth(ExecutionState& state, DifferenceTemporalYearMonth operation, Value other, Value options);
};

} // namespace Escargot

#endif
#endif
