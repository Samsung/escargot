#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
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
#include "TemporalPlainDateTimeObject.h"
#include "TemporalPlainDateObject.h"
#include "TemporalPlainTimeObject.h"
#include "TemporalDurationObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, ISO8601::PlainTime plainTime, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime(plainDate, plainTime))
    , m_calendarID(calendar)
{
    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(),
                                         plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.microsecond(), plainTime.microsecond(), plainTime.nanosecond())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid date-time");
    }

    m_icuCalendar = calendar.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(m_icuCalendar, ISO8601::ExactTime::fromISOPartsAndOffset(plainDate.year(), plainDate.month(), plainDate.day(), plainTime.hour(), plainTime.minute(), plainTime.second(), plainTime.millisecond(), plainTime.microsecond(), plainTime.microsecond(), 0).epochMilliseconds(), &status);
    CHECK_ICU()

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateTimeObject* self = (TemporalPlainDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, unsigned underMicrosecondValue, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_calendarID(calendar)
    , m_icuCalendar(icuCalendar)
{
    ASSERT(underMicrosecondValue < 1000 * 1000);
    UErrorCode status = U_ZERO_ERROR;

    int32_t y;
    if (calendar.shouldUseICUExtendedYear()) {
        y = ucal_get(m_icuCalendar, UCAL_EXTENDED_YEAR, &status);
    } else {
        y = ucal_get(m_icuCalendar, UCAL_YEAR, &status);
    }
    CHECK_ICU()
    auto m = ucal_get(m_icuCalendar, UCAL_MONTH, &status) + 1;
    CHECK_ICU()
    auto d = ucal_get(m_icuCalendar, UCAL_DAY_OF_MONTH, &status);
    CHECK_ICU()

    auto h = ucal_get(m_icuCalendar, UCAL_HOUR_OF_DAY, &status);
    CHECK_ICU()
    auto mm = ucal_get(m_icuCalendar, UCAL_MINUTE, &status);
    CHECK_ICU()
    auto s = ucal_get(m_icuCalendar, UCAL_SECOND, &status);
    CHECK_ICU()
    auto ms = ucal_get(m_icuCalendar, UCAL_MILLISECOND, &status);
    CHECK_ICU()

    *m_plainDateTime = ISO8601::PlainDateTime(ISO8601::PlainDate(y, m, d), ISO8601::PlainTime(h, mm, s, ms, underMicrosecondValue / 1000, underMicrosecondValue % 1000));

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateTimeObject* self = (TemporalPlainDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

static void incrementDay(ISO8601::Duration& duration)
{
    double year = duration.years();
    double month = duration.months();
    double day = duration.days();

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (day < daysInMonth) {
        duration.setDays(day + 1);
        return;
    }

    duration.setDays(1);
    if (month < 12) {
        duration.setMonths(month + 1);
        return;
    }

    duration.setMonths(1);
    duration.setYears(year + 1);
}

String* TemporalPlainDateTimeObject::toString(ExecutionState& state, Value options)
{
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetTemporalShowCalendarNameOption reads "calendarName", GetTemporalFractionalSecondDigitsOption reads "fractionalSecondDigits", and GetRoundingModeOption reads "roundingMode").
    // Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);
    // Let result be RoundISODateTime(plainDateTime.[[ISODateTime]], precision.[[Increment]], precision.[[Unit]], roundingMode).
    // If ISODateTimeWithinLimits(result) is false, throw a RangeError exception.
    auto roundedResult = TemporalPlainTimeObject::roundTime(state, m_plainDateTime->plainTime(), precision.increment, precision.unit, roundingMode);
    auto plainTime = TemporalPlainTimeObject::toPlainTime(state, roundedResult);
    double extraDays = roundedResult.days();
    roundedResult.setYears(m_plainDateTime->plainDate().year());
    roundedResult.setMonths(m_plainDateTime->plainDate().month());
    roundedResult.setDays(m_plainDateTime->plainDate().day());
    if (extraDays) {
        ASSERT(extraDays == 1);
        incrementDay(roundedResult);
    }
    auto plainDate = TemporalPlainDateObject::toPlainDate(state, roundedResult);
    // Return ISODateTimeToString(result, plainDateTime.[[Calendar]], precision.[[Precision]], showCalendar).
    StringBuilder sb;
    sb.appendString(TemporalPlainDateObject::temporalDateToString(plainDate, m_calendarID, showCalendar));
    sb.appendChar('T');
    sb.appendString(TemporalPlainTimeObject::temporalTimeToString(plainTime, precision.precision));
    return sb.finalize();
}

} // namespace Escargot

#endif
