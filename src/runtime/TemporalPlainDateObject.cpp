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

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, ISO8601::PlainDate plainDate, Calendar calendar)
    : DerivedObject(state, proto)
    , m_plainDate(new(PointerFreeGC) ISO8601::PlainDate(plainDate))
    , m_calendarID(calendar)
{
    m_icuCalendar = calendar.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(m_icuCalendar, ISO8601::ExactTime::fromISOPartsAndOffset(plainDate.year(), plainDate.month(), plainDate.day(), 0, 0, 0, 0, 0, 0, 0).epochMilliseconds(), &status);
    CHECK_ICU()

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

ISO8601::PlainDate TemporalPlainDateObject::computeISODate(ExecutionState& state)
{
    if (!m_calendarID.isISO8601()) {
        return Temporal::computeISODate(state, m_icuCalendar);
    }
    return plainDate();
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

    Temporal::formatCalendarAnnotation(builder, calendar, showCalendar);

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
    return temporalDateToString(computeISODate(state), m_calendarID, showCalendar);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
bool TemporalPlainDateObject::equals(ExecutionState& state, Value other)
{
    auto otherDate = Temporal::toTemporalDate(state, other, Value());
    int c = compareISODate(state, this, otherDate);
    if (c == 0) {
        if (calendarID() == otherDate->calendarID()) {
            return true;
        }
    }
    return false;
}

TemporalPlainDateObject* TemporalPlainDateObject::addDurationToDate(ExecutionState& state, AddDurationToDateOperation operation, Value temporalDurationLike, Value options)
{
    // Let calendar be temporalDate.[[Calendar]].
    // Let duration be ? ToTemporalDuration(temporalDurationLike).
    ISO8601::Duration duration = Temporal::toTemporalDuration(state, temporalDurationLike)->duration();
    // If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if (operation == AddDurationToDateOperation::Subtract) {
        duration = TemporalDurationObject::createNegatedTemporalDuration(duration);
    }
    // Let dateDuration be ToDateDurationRecordWithoutTime(duration).
    auto dateDuration = TemporalDurationObject::toDateDurationRecordWithoutTime(state, duration);
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let result be ? CalendarDateAdd(calendar, temporalDate.[[ISODate]], dateDuration, overflow).
    auto result = Temporal::calendarDateAdd(state, calendarID(), computeISODate(state), m_icuCalendar, dateDuration, overflow);
    // Return ! CreateTemporalDate(result, calendar).
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(), result, m_calendarID);
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
    switch (s) {
    case UCAL_SUNDAY:
        return Value(7);
    case UCAL_MONDAY:
        return Value(1);
    case UCAL_TUESDAY:
        return Value(2);
    case UCAL_WEDNESDAY:
        return Value(3);
    case UCAL_THURSDAY:
        return Value(4);
    case UCAL_FRIDAY:
        return Value(5);
    case UCAL_SATURDAY:
        return Value(6);
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "failed to get value from ICU calendar");
    return Value(0);
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
    case Calendar::ID::IslamicCivilLegacy:
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

Value TemporalPlainDateObject::monthCode(ExecutionState& state)
{
    StringBuilder sb;
    sb.appendChar('M');

    if (plainDate().month() < 10) {
        sb.appendChar('0');
    } else {
        sb.appendChar('1');
    }

    sb.appendChar(static_cast<char>('0' + (plainDate().month() % 10)));

    UErrorCode status = U_ZERO_ERROR;
    auto s = ucal_get(m_icuCalendar, UCAL_IS_LEAP_MONTH, &status);
    CHECK_ICU()
    if (s) {
        sb.appendChar('L');
    }

    return sb.finalize();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.compare
int TemporalPlainDateObject::compare(ExecutionState& state, Value one, Value two)
{
    auto oneDate = Temporal::toTemporalDate(state, one, Value());
    auto twoDate = Temporal::toTemporalDate(state, two, Value());
    return compareISODate(state, oneDate, twoDate);
}

TemporalPlainDateObject* TemporalPlainDateObject::with(ExecutionState& state, Value temporalDateLike, Value options)
{
    // Let plainDate be the this value.
    // Perform ? RequireInternalSlot(plainDate, [[InitializedTemporalDate]]).
    // If ? IsPartialTemporalObject(temporalDateLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalDateLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalDateLike");
    }
    // Let calendar be plainDate.[[Calendar]].
    auto calendar = m_calendarID;
    // Let fields be ISODateToFields(calendar, plainDate.[[ISODate]], date).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    MonthCode mc;
    mc.monthNumber = isoDate.month();
    fields.monthCode = mc;
    fields.day = isoDate.day();
    // Let partialDate be ? PrepareCalendarFields(calendar, temporalDateLike, « year, month, month-code, day », « », partial).
    CalendarField fs[4] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode, CalendarField::Day };
    auto partialDate = Temporal::prepareCalendarFields(state, calendar, temporalDateLike.asObject(), fs, 4, nullptr, 0, nullptr, SIZE_MAX);
    // Set fields to CalendarMergeFields(calendar, fields, partialDate).
    fields = Temporal::calendarMergeFields(state, calendar, fields, partialDate);
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let isoDate be ? CalendarDateFromFields(calendar, fields, overflow).
    auto icuDate = Temporal::calendarDateFromFields(state, calendar, fields, overflow);
    // Return ! CreateTemporalDate(isoDate, calendar).
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                       icuDate, calendar);
}

TemporalPlainDateObject* TemporalPlainDateObject::withCalendar(ExecutionState& state, Value calendarLike)
{
    auto calendar = Temporal::toTemporalCalendarIdentifier(state, calendarLike);
    auto icuCalendar = calendar.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(icuCalendar, ucal_getMillis(m_icuCalendar, &status), &status);

    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(), icuCalendar, calendar);
}

TemporalDurationObject* TemporalPlainDateObject::since(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainDate(state, DifferenceTemporalPlainDate::Since, other, options));
}

TemporalDurationObject* TemporalPlainDateObject::until(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainDate(state, DifferenceTemporalPlainDate::Until, other, options));
}

int TemporalPlainDateObject::compareISODate(ExecutionState& state, TemporalPlainDateObject* one, TemporalPlainDateObject* two)
{
    UErrorCode status = U_ZERO_ERROR;
    auto epochTime1 = ucal_getMillis(one->m_icuCalendar, &status);
    CHECK_ICU()
    auto epochTime2 = ucal_getMillis(two->m_icuCalendar, &status);
    CHECK_ICU()

    if (epochTime1 > epochTime2) {
        return 1;
    } else if (epochTime1 < epochTime2) {
        return -1;
    }
    return 0;
}

ISO8601::Duration TemporalPlainDateObject::differenceTemporalPlainDate(ExecutionState& state, DifferenceTemporalPlainDate operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalDate(other).
    auto other = Temporal::toTemporalDate(state, otherInput, Value());
    // If CalendarEquals(temporalDate.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
    if (other->calendarID() != calendarID()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "other calendar is not same");
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, date, « », day, day).
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalPlainDate::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::Date, nullptr, 0, TemporalUnit::Day, TemporalUnit::Day);
    // If CompareISODate(temporalDate.[[ISODate]], other.[[ISODate]]) = 0, then
    if (computeISODate(state) == other->computeISODate(state)) {
        // Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
        return {};
    }

    // Let dateDifference be CalendarDateUntil(temporalDate.[[Calendar]], temporalDate.[[ISODate]], other.[[ISODate]], settings.[[LargestUnit]]).
    auto dateDifference = Temporal::calendarDateUntil(m_calendarID, computeISODate(state), other->computeISODate(state), toTemporalUnit(settings.largestUnit));

    // Let duration be CombineDateAndTimeDuration(dateDifference, 0).
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(dateDifference, {});
    // If settings.[[SmallestUnit]] is not day or settings.[[RoundingIncrement]] ≠ 1, then
    if (settings.smallestUnit != ISO8601::DateTimeUnit::Day || settings.roundingIncrement != 1) {
        // Let isoDateTime be CombineISODateAndTimeRecord(temporalDate.[[ISODate]], MidnightTimeRecord()).
        auto isoDateTime = computeISODate(state);
        // Let isoDateTimeOther be CombineISODateAndTimeRecord(other.[[ISODate]], MidnightTimeRecord()).
        auto isoDateTimeOther = other->computeISODate(state);
        // Let destEpochNs be GetUTCEpochNanoseconds(isoDateTimeOther).
        auto destEpochNs = ISO8601::ExactTime::fromISOPartsAndOffset(isoDateTimeOther.year(), isoDateTimeOther.month(), isoDateTimeOther.day(), 0, 0, 0, 0, 0, 0, 0).epochNanoseconds();
        duration = Temporal::roundRelativeDuration(state, duration, destEpochNs, ISO8601::PlainDateTime(isoDateTime, ISO8601::PlainTime()), NullOption, calendarID(), toTemporalUnit(settings.largestUnit), settings.roundingIncrement, toTemporalUnit(settings.smallestUnit), settings.roundingMode);
    }

    // Let result be ! TemporalDurationFromInternal(duration, day).
    auto result = TemporalDurationObject::temporalDurationFromInternal(state, duration, ISO8601::DateTimeUnit::Day);
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalPlainDate::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }

    // Return result.
    return result;
}

} // namespace Escargot

#endif
