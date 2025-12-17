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

#ifndef __EscargotTemporalPlainMonthDayObject__
#define __EscargotTemporalPlainMonthDayObject__

#include "runtime/TemporalPlainDateObject.h"

namespace Escargot {

class TemporalPlainMonthDayObject : public TemporalPlainDateObject {
public:
    TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, ISO8601::PlainDate isoYearMonth, Calendar calendar);
    TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar);

    virtual bool isTemporalPlainDateObject() const override
    {
        return false;
    }

    virtual bool isTemporalPlainMonthDayObject() const override
    {
        return true;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tostring
    String* toString(ExecutionState& state, Value options);

    // https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
    TemporalPlainMonthDayObject* with(ExecutionState& state, Value temporalMonthDayLike, Value options);

    bool equals(ExecutionState& state, Value other);

    TemporalPlainDateObject* toPlainDate(ExecutionState& state, Value item);

private:
};

} // namespace Escargot

#endif
#endif
