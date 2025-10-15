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
#include "TemporalPlainMonthDayObject.h"
#include "TemporalPlainDateObject.h"
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

TemporalPlainMonthDayObject::TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar)
    : TemporalPlainDateObject(state, proto, plainDate, calendar)
{
}

TemporalPlainMonthDayObject::TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar)
    : TemporalPlainDateObject(state, proto, icuCalendar, calendar)
{
}

String* TemporalPlainMonthDayObject::toString(ExecutionState& state, Value options)
{
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    auto isoDate = computeISODate(state);
    StringBuilder builder;
    int32_t year = isoDate.year();
    int32_t month = isoDate.month();
    int32_t day = isoDate.day();

    if (showCalendar == TemporalShowCalendarNameOption::Always || showCalendar == TemporalShowCalendarNameOption::Critical || !calendarID().isISO8601()) {
        if (year > 9999 || year < 0) {
            builder.appendChar(year < 0 ? '-' : '+');
            auto s = pad('0', 6, std::to_string(std::abs(year)));
            builder.appendString(String::fromASCII(s.data(), s.length()));
        } else {
            auto s = pad('0', 4, std::to_string(std::abs(year)));
            builder.appendString(String::fromASCII(s.data(), s.length()));
        }

        builder.appendChar('-');
    }

    {
        auto s = pad('0', 2, std::to_string(month));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    builder.appendChar('-');

    {
        auto s = pad('0', 2, std::to_string(day));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    Temporal::formatCalendarAnnotation(builder, m_calendarID, showCalendar);

    return builder.finalize();
}

TemporalPlainMonthDayObject* TemporalPlainMonthDayObject::with(ExecutionState& state, Value temporalMonthDayLike, Value options)
{
    // If ? IsPartialTemporalObject(temporalMonthDayLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalMonthDayLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalMonthDayLike");
    }
    // Let calendar be plainMonthDay.[[Calendar]].
    auto calendar = calendarID();
    // Let fields be ISODateToFields(calendar, plainMonthDay.[[ISODate]], month-day).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    MonthCode mc;
    mc.monthNumber = isoDate.month();
    fields.monthCode = mc;
    fields.day = isoDate.day();
    // Let partialMonthDay be ? PrepareCalendarFields(calendar, temporalMonthDayLike, « year, month, month-code, day », « », partial).
    CalendarField f[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
    auto partialMonthDay = Temporal::prepareCalendarFields(state, calendar, temporalMonthDayLike.asObject(), f, 4, nullptr, 0, nullptr, SIZE_MAX);

    // intl402/Temporal/PlainMonthDay/prototype/with/fields-missing-properties
    if (!calendar.isISO8601()) {
        bool missing = false;
        if (partialMonthDay.month && !partialMonthDay.day) {
            missing = true;
        } else if (partialMonthDay.monthCode && !partialMonthDay.day) {
            missing = true;
        } else if (partialMonthDay.day && !partialMonthDay.month && !partialMonthDay.monthCode) {
            missing = true;
        }
        if (missing) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalMonthDayLike");
        }
    }

    // Set fields to CalendarMergeFields(calendar, fields, partialMonthDay).
    fields = Temporal::calendarMergeFields(state, calendar, fields, partialMonthDay);
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let isoDate be ? CalendarMonthDayFromFields(calendar, fields, overflow).
    auto icuDate = Temporal::calendarDateFromFields(state, calendar, fields, overflow, Temporal::CalendarDateFromFieldsMode::MonthDay);
    // Return ! CreateTemporalMonthDay(isoDate, calendar).
    return new TemporalPlainMonthDayObject(state, state.context()->globalObject()->temporalPlainMonthDayPrototype(),
                                           icuDate, calendar);
}

bool TemporalPlainMonthDayObject::equals(ExecutionState& state, Value other)
{
    auto otherDate = Temporal::toTemporalMonthDay(state, other, Value());
    int c = compareISODate(state, this, otherDate);
    if (c == 0) {
        if (calendarID() == otherDate->calendarID()) {
            return true;
        }
    }
    return false;
}

TemporalPlainDateObject* TemporalPlainMonthDayObject::toPlainDate(ExecutionState& state, Value item)
{
    // If item is not an Object, then
    if (!item.isObject()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid item");
    }

    // Let calendar be plainMonthDay.[[Calendar]].
    auto calendar = calendarID();
    // Let fields be ISODateToFields(calendar, plainMonthDay.[[ISODate]], month-day).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    MonthCode mc;
    mc.monthNumber = isoDate.month();
    fields.monthCode = mc;
    fields.day = isoDate.day();
    // Let inputFields be ? PrepareCalendarFields(calendar, item, « year », « », « »).
    CalendarField f[1] = { CalendarField::Year };
    auto inputFields = Temporal::prepareCalendarFields(state, calendar, item.asObject(), f, 1, nullptr, 0, nullptr, SIZE_MAX);
    // Let mergedFields be CalendarMergeFields(calendar, fields, inputFields).
    auto mergedFields = Temporal::calendarMergeFields(state, calendar, fields, inputFields);
    // Let isoDate be ? CalendarDateFromFields(calendar, mergedFields, constrain).
    auto icuDate = Temporal::calendarDateFromFields(state, calendar, mergedFields, TemporalOverflowOption::Constrain);
    // Return ! CreateTemporalDate(isoDate, calendar).
    if (calendar.isISO8601() && !ISO8601::isDateTimeWithinLimits(mergedFields.year.value(), mergedFields.monthCode ? mergedFields.monthCode.value().monthNumber : mergedFields.month.value(), mergedFields.day.value(), 12, 0, 0, 0, 0, 0)) {
        ucal_close(icuDate);
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                       icuDate, calendar);
}

} // namespace Escargot

#endif
