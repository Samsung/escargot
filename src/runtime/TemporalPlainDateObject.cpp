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
#include "TemporalPlainDateObject.h"
#include "intl/Intl.h"
#include "util/ISO8601.h"

namespace Escargot {

#define CHECK_ICU()                                                                                           \
    if (U_FAILURE(status)) {                                                                                  \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar"); \
    }

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDate(new(PointerFreeGC) ISO8601::PlainDate(plainDate))
    , m_calendarID(calendar)
{
    m_icuCalendar = calendar.createICUCalendar(state);
    if (calendar.shouldUseICUExtendedYear()) {
        ucal_set(m_icuCalendar, UCAL_EXTENDED_YEAR, plainDate.year());
    } else {
        ucal_set(m_icuCalendar, UCAL_YEAR, plainDate.year());
    }
    ucal_set(m_icuCalendar, UCAL_MONTH, plainDate.month() - 1);
    ucal_set(m_icuCalendar, UCAL_DAY_OF_MONTH, plainDate.day());

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateObject* self = (TemporalPlainDateObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, UCalendar* icuCalendar, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDate(new(PointerFreeGC) ISO8601::PlainDate())
    , m_calendarID(calendar)
    , m_icuCalendar(icuCalendar)
{
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

    *m_plainDate = ISO8601::PlainDate(y, m, d);

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalPlainDateObject* self = (TemporalPlainDateObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

// https://tc39.es/proposal-temporal/#sec-temporal-temporaldatetostring
static String* temporalDateToString(ISO8601::PlainDate plainDate, Calendar calendar, TemporalShowCalendarNameOption showCalendar)
{
    StringBuilder builder;
    int32_t year = plainDate.year();
    int32_t month = plainDate.month();
    int32_t day = plainDate.day();

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
    builder.appendChar('-');
    {
        auto s = pad('0', 2, std::to_string(day));
        builder.appendString(String::fromASCII(s.data(), s.length()));
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-formatcalendarannotation
    // FormatCalendarAnnotation steps
    if (showCalendar == TemporalShowCalendarNameOption::Never) {
    } else if (showCalendar == TemporalShowCalendarNameOption::Auto && calendar == Calendar()) {
    } else {
        builder.appendChar('[');
        if (showCalendar == TemporalShowCalendarNameOption::Critical) {
            builder.appendChar('!');
        }
        builder.appendString("u-ca=");
        builder.appendString(calendar.toString());
        builder.appendChar(']');
    }

    return builder.finalize();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
String* TemporalPlainDateObject::toString(ExecutionState& state, Value options)
{
    // Let plainDate be the this value.
    // Perform ? RequireInternalSlot(plainDate, [[InitializedTemporalDate]]).
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    // Return TemporalDateToString(plainDate, showCalendar).
    auto pd = plainDate();
    if (!m_calendarID.isISO8601() && m_icuCalendar) {
        UErrorCode status = U_ZERO_ERROR;
        auto epochTime = ucal_getMillis(m_icuCalendar, &status);
        CHECK_ICU()
        DateObject::DateTimeInfo timeInfo;
        DateObject::computeTimeInfoFromEpoch(epochTime, timeInfo);
        pd = ISO8601::PlainDate(timeInfo.year, timeInfo.month + 1, timeInfo.mday);
    }

    return temporalDateToString(pd, m_calendarID, showCalendar);
}

Value TemporalPlainDateObject::era(ExecutionState& state)
{
    if (m_calendarID.isEraRelated()) {
        UErrorCode status = U_ZERO_ERROR;
        auto ucalEra = ucal_get(m_icuCalendar, UCAL_ERA, &status);
        CHECK_ICU()

        std::string result;
        m_calendarID.lookupICUEra(state, [&](size_t idx, const std::string& s) -> bool {
            if (size_t(ucalEra) == idx) {
                result = s;
                return true;
            }
            return false;
        });
        return String::fromASCII(result.data(), result.length());
    }
    return Value();
}

Value TemporalPlainDateObject::eraYear(ExecutionState& state)
{
    if (m_calendarID.isEraRelated()) {
        UErrorCode status = U_ZERO_ERROR;
        auto s = ucal_get(m_icuCalendar, UCAL_YEAR, &status);
        CHECK_ICU()
        return Value(s);
    }
    return Value();
}

Value TemporalPlainDateObject::dayOfWeek(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    auto s = ucal_get(m_icuCalendar, UCAL_DAY_OF_WEEK, &status);
    CHECK_ICU()
    return Value(s - 1);
}

Value TemporalPlainDateObject::dayOfYear(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    auto s = ucal_get(m_icuCalendar, UCAL_DAY_OF_YEAR, &status);
    CHECK_ICU()
    return Value(s);
}

Value TemporalPlainDateObject::weekOfYear(ExecutionState& state)
{
    if (m_calendarID.isISO8601()) {
        UErrorCode status = U_ZERO_ERROR;
        auto s = ucal_get(m_icuCalendar, UCAL_WEEK_OF_YEAR, &status);
        CHECK_ICU()
        return Value(s);
    }
    return Value();
}

Value TemporalPlainDateObject::yearOfWeek(ExecutionState& state)
{
    if (m_calendarID.isISO8601()) {
        UErrorCode status = U_ZERO_ERROR;
        auto s = ucal_get(m_icuCalendar, UCAL_YEAR_WOY, &status);
        CHECK_ICU()
        return Value(s);
    }
    return Value();
}

Value TemporalPlainDateObject::daysInWeek(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> newCal(ucal_clone(m_icuCalendar, &status), [](UCalendar* r) {
        ucal_close(r);
    });
    CHECK_ICU()

    int count = 0;
    for (int i = UCAL_SUNDAY; i <= UCAL_SATURDAY; i++) {
        ucal_set(newCal.get(), UCAL_DAY_OF_MONTH, plainDate().day());
        ucal_set(newCal.get(), UCAL_DAY_OF_WEEK, i);
        if (ucal_get(newCal.get(), UCAL_DAY_OF_WEEK, &status) == i) {
            count++;
        }
    }

    return Value(count);
}

Value TemporalPlainDateObject::daysInMonth(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> newCal(ucal_clone(m_icuCalendar, &status), [](UCalendar* r) {
        ucal_close(r);
    });
    CHECK_ICU()

    int dayCount = 20;
    ucal_set(newCal.get(), UCAL_DAY_OF_MONTH, dayCount);
    for (;; dayCount++) {
        ucal_set(newCal.get(), UCAL_DAY_OF_MONTH, dayCount);
        if (ucal_get(newCal.get(), UCAL_DAY_OF_MONTH, &status) != dayCount) {
            CHECK_ICU()
            break;
        }
    }

    return Value(dayCount - 1);
}

Value TemporalPlainDateObject::daysInYear(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> newCal(ucal_clone(m_icuCalendar, &status), [](UCalendar* r) {
        ucal_close(r);
    });
    CHECK_ICU()

    int dayCount = 353;
    ucal_set(newCal.get(), UCAL_DAY_OF_YEAR, dayCount);
    for (;; dayCount++) {
        ucal_set(newCal.get(), UCAL_DAY_OF_YEAR, dayCount);
        if (ucal_get(newCal.get(), UCAL_DAY_OF_YEAR, &status) != dayCount) {
            CHECK_ICU()
            break;
        }
    }

    return Value(dayCount - 1);
}

Value TemporalPlainDateObject::monthsInYear(ExecutionState& state)
{
    UErrorCode status = U_ZERO_ERROR;
    LocalResourcePointer<UCalendar> newCal(ucal_clone(m_icuCalendar, &status), [](UCalendar* r) {
        ucal_close(r);
    });
    CHECK_ICU()

    ucal_set(newCal.get(), UCAL_MONTH, 0);
    ucal_set(newCal.get(), UCAL_DAY_OF_MONTH, 1);
    int monthCount;
    for (monthCount = 0;; monthCount++) {
        ucal_set(newCal.get(), UCAL_MONTH, monthCount);
        if (ucal_get(newCal.get(), UCAL_MONTH, &status) != monthCount) {
            CHECK_ICU()
            break;
        }
    }

    return Value(monthCount);
}

Value TemporalPlainDateObject::inLeapYear(ExecutionState& state)
{
    if (m_calendarID.isISO8601()) {
        auto year = m_plainDate->year();
        return Value((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)));
    }

    bool ret = false;
    switch (m_calendarID.id()) {
    case Calendar::ID::Islamic:
    case Calendar::ID::IslamicCivil:
    case Calendar::ID::IslamicRGSA:
    case Calendar::ID::IslamicTabular:
    case Calendar::ID::IslamicUmmAlQura: {
        int32_t days = daysInYear(state).asInt32();
        ret = days == 355;
        break;
    }

    case Calendar::ID::Chinese:
    case Calendar::ID::Dangi:
    case Calendar::ID::Hebrew: {
        int32_t months = monthsInYear(state).asInt32();
        ret = months == 13;
        break;
    }
    default: {
        int32_t days = daysInYear(state).asInt32();
        ret = days == 366;
        break;
    }
    }

    return Value(ret);
}

} // namespace Escargot

#endif
