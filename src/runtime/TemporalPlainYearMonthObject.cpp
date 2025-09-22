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
#include "TemporalPlainYearMonthObject.h"
#include "TemporalDurationObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

TemporalPlainYearMonthObject::TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar)
    : TemporalPlainDateObject(state, proto, plainDate, calendar)
{
}

TemporalPlainYearMonthObject::TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar)
    : TemporalPlainDateObject(state, proto, icuCalendar, calendar)
{
}

String* TemporalPlainYearMonthObject::toString(ExecutionState& state, Value options)
{
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    auto isoDate = computeISODate(state);
    StringBuilder builder;
    int32_t year = isoDate.year();
    int32_t month = isoDate.month();
    int32_t day = isoDate.day();

    if (year > 9999 || year < 0) {
        builder.appendChar(year < 0 ? '-' : '+');
        auto s = pad('0', 6, std::to_string(std::abs(year)));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    } else {
        auto s = pad('0', 4, std::to_string(std::abs(year)));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    builder.appendChar('-');
    {
        auto s = pad('0', 2, std::to_string(month));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }


    if (showCalendar == TemporalShowCalendarNameOption::Always || showCalendar == TemporalShowCalendarNameOption::Critical || !calendarID().isISO8601()) {
        builder.appendChar('-');
        {
            auto s = pad('0', 2, std::to_string(day));
            builder.appendString(String::fromASCII(s.data(), s.length()));
        }
    }

    Temporal::formatCalendarAnnotation(builder, m_calendarID, showCalendar);

    return builder.finalize();
}

} // namespace Escargot

#endif
