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

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.compare
int TemporalPlainYearMonthObject::compare(ExecutionState& state, Value one, Value two)
{
    // Set one to ? ToTemporalYearMonth(one).
    auto oneObj = Temporal::toTemporalYearMonth(state, one, Value());
    // Set two to ? ToTemporalYearMonth(two).
    auto twoObj = Temporal::toTemporalYearMonth(state, two, Value());
    // Return 𝔽(CompareISODate(one.[[ISODate]], two.[[ISODate]])).
    UErrorCode status = U_ZERO_ERROR;
    auto epochTime1 = ucal_getMillis(oneObj->m_icuCalendar, &status);
    CHECK_ICU()
    auto epochTime2 = ucal_getMillis(twoObj->m_icuCalendar, &status);
    CHECK_ICU()

    if (epochTime1 > epochTime2) {
        return 1;
    } else if (epochTime1 < epochTime2) {
        return -1;
    }
    return 0;
}

TemporalPlainYearMonthObject* TemporalPlainYearMonthObject::with(ExecutionState& state, Value temporalYearMonthLike, Value options)
{
    // If ? IsPartialTemporalObject(temporalYearMonthLike) is false, throw a TypeError exception.
    if (!Temporal::isPartialTemporalObject(state, temporalYearMonthLike)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid temporalYearMonthLike");
    }
    // Let calendar be plainYearMonth.[[Calendar]].
    const auto& calendar = m_calendarID;
    // Let fields be ISODateToFields(calendar, plainYearMonth.[[ISODate]], year-month).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    fields.day = isoDate.day();
    // Let partialYearMonth be ? PrepareCalendarFields(calendar, temporalYearMonthLike, « year, month, month-code », « », partial).
    CalendarField fs[3] = { CalendarField::Year, CalendarField::Month, CalendarField::MonthCode };
    auto partialYearMonth = Temporal::prepareCalendarFields(state, calendar, temporalYearMonthLike.asObject(), fs, 3, nullptr, 0, nullptr, SIZE_MAX);
    // Set fields to CalendarMergeFields(calendar, fields, partialYearMonth).
    fields = Temporal::calendarMergeFields(state, calendar, fields, partialYearMonth);

    // built-ins/Temporal/PlainYearMonth/prototype/with/options-undefined.js
    if (calendar.isISO8601() && fields.monthCode && fields.month && fields.monthCode.value().monthNumber == fields.month.value()) {
        fields.monthCode = NullOption;
    }

    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let isoDate be ? CalendarYearMonthFromFields(calendar, fields, overflow).
    // Return ! CreateTemporalYearMonth(isoDate, calendar).
    auto icuDate = Temporal::calendarResolveFields(state, calendar, fields, overflow, Temporal::CalendarDateFromFieldsMode::YearMonth);
    return new TemporalPlainYearMonthObject(state, state.context()->globalObject()->temporalPlainYearMonthPrototype(),
                                            icuDate, calendar);
}

TemporalPlainDateObject* TemporalPlainYearMonthObject::toPlainDate(ExecutionState& state, Value item)
{
    // If item is not an Object, then
    if (!item.isObject()) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid item");
    }

    // Let calendar be plainYearMonth.[[Calendar]].
    const auto& calendar = m_calendarID;
    // Let fields be ISODateToFields(calendar, plainYearMonth.[[ISODate]], year-month).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    fields.day = isoDate.day();
    // Let inputFields be ? PrepareCalendarFields(calendar, item, « day », « », « »).
    CalendarField fs[1] = { CalendarField::Day };
    auto inputFields = Temporal::prepareCalendarFields(state, calendar, item.asObject(), fs, 1, nullptr, 0, nullptr, SIZE_MAX);
    // Let mergedFields be CalendarMergeFields(calendar, fields, inputFields).
    auto mergedFields = Temporal::calendarMergeFields(state, calendar, fields, inputFields);
    // Let isoDate be ? CalendarDateFromFields(calendar, mergedFields, constrain).
    // Return ! CreateTemporalDate(isoDate, calendar).
    auto icuDate = Temporal::calendarDateFromFields(state, calendar, mergedFields, TemporalOverflowOption::Constrain);
    if (calendar.isISO8601() && !ISO8601::isDateTimeWithinLimits(mergedFields.year.value(), mergedFields.monthCode ? mergedFields.monthCode.value().monthNumber : mergedFields.month.value(), mergedFields.day.value(), 12, 0, 0, 0, 0, 0)) {
        ucal_close(icuDate);
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }
    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                       icuDate, calendar);
}

TemporalPlainYearMonthObject* TemporalPlainYearMonthObject::addDurationToYearMonth(ExecutionState& state, AddDurationToYearMonthOperation operation, Value temporalDurationLike, Value options)
{
    // Let duration be ? ToTemporalDuration(temporalDurationLike).
    auto duration = Temporal::toTemporalDuration(state, temporalDurationLike)->duration();
    // If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
    if (operation == AddDurationToYearMonthOperation::Subtract) {
        duration = TemporalDurationObject::createNegatedTemporalDuration(duration);
    }

    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    auto overflow = Temporal::getTemporalOverflowOption(state, resolvedOptions);
    // Let sign be DurationSign(duration).
    auto sign = duration.sign();
    // Let calendar be yearMonth.[[Calendar]].
    auto calendar = m_calendarID;
    // Let fields be ISODateToFields(calendar, yearMonth.[[ISODate]], year-month).
    CalendarFieldsRecord fields;
    auto isoDate = computeISODate(state);
    fields.year = isoDate.year();
    fields.month = isoDate.month();
    // Set fields.[[Day]] to 1.
    fields.day = 1;
    // Let intermediateDate be ? CalendarDateFromFields(calendar, fields, constrain).
    LocalResourcePointer<UCalendar> intermediateDate(Temporal::calendarDateFromFields(state, calendar, fields, TemporalOverflowOption::Constrain), [](UCalendar* cal) {
        ucal_close(cal);
    });
    if (calendar.isISO8601() && !ISO8601::isDateTimeWithinLimits(fields.year.value(), fields.monthCode ? fields.monthCode.value().monthNumber : fields.month.value(), fields.day.value(), 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "date is out of range");
    }

    LocalResourcePointer<UCalendar> date(nullptr, [](UCalendar* cal) {
        ucal_close(cal);
    });
    // If sign < 0, then
    if (sign < 0) {
        // Let oneMonthDuration be ! CreateDateDurationRecord(0, 1, 0, 0).
        ISO8601::Duration oneMonthDuration({ 0, 1 });
        // Let nextMonth be ? CalendarDateAdd(calendar, intermediateDate, oneMonthDuration, constrain).
        auto nextMonth = Temporal::calendarDateAdd(state, calendar, Temporal::computeISODate(state, intermediateDate.get()), intermediateDate.get(), oneMonthDuration, TemporalOverflowOption::Constrain);
        // Let date be BalanceISODate(nextMonth.[[Year]], nextMonth.[[Month]], nextMonth.[[Day]] - 1).
        UErrorCode status = U_ZERO_ERROR;
        auto year = ucal_get(nextMonth, UCAL_YEAR, &status);
        CHECK_ICU();
        auto month = ucal_get(nextMonth, UCAL_MONTH, &status) + 1;
        CHECK_ICU();
        auto day = ucal_get(nextMonth, UCAL_DAY_OF_MONTH, &status);
        CHECK_ICU();
        auto balancedDate = Temporal::balanceISODate(state, year, month, day - 1);
        // Assert: ISODateWithinLimits(date) is true.
        ucal_set(nextMonth, UCAL_YEAR, balancedDate.year());
        ucal_set(nextMonth, UCAL_MONTH, balancedDate.month() - 1);
        ucal_set(nextMonth, UCAL_DAY_OF_MONTH, balancedDate.day());

        date.reset(nextMonth);
    } else {
        // Else,
        // Let date be intermediateDate.
        std::swap(date, intermediateDate);
    }

    // Let durationToAdd be ToDateDurationRecordWithoutTime(duration).
    auto durationToAdd = TemporalDurationObject::toDateDurationRecordWithoutTime(state, duration);
    // Let addedDate be ? CalendarDateAdd(calendar, date, durationToAdd, overflow).
    LocalResourcePointer<UCalendar> addedDate(Temporal::calendarDateAdd(state, calendar, Temporal::computeISODate(state, date.get()), date.get(), durationToAdd, overflow),
                                              [](UCalendar* cal) {
                                                  ucal_close(cal);
                                              });
    // Let addedDateFields be ISODateToFields(calendar, addedDate, year-month).
    CalendarFieldsRecord addedDateFields;
    isoDate = Temporal::computeISODate(state, addedDate.get());
    addedDateFields.year = isoDate.year();
    addedDateFields.month = isoDate.month();
    addedDateFields.day = isoDate.day();
    // Let isoDate be ? CalendarYearMonthFromFields(calendar, addedDateFields, overflow).
    // Return ! CreateTemporalYearMonth(isoDate, calendar).
    auto icuDate = Temporal::calendarDateFromFields(state, calendar, addedDateFields, overflow, Temporal::CalendarDateFromFieldsMode::YearMonth);
    return new TemporalPlainYearMonthObject(state, state.context()->globalObject()->temporalPlainYearMonthPrototype(),
                                            icuDate, calendar);
}

ISO8601::Duration TemporalPlainYearMonthObject::differenceTemporalPlainYearMonth(ExecutionState& state, DifferenceTemporalYearMonth operation, Value otherInput, Value options)
{
    // Set other to ? ToTemporalYearMonth(other).
    auto other = Temporal::toTemporalYearMonth(state, otherInput, Value());
    // Let calendar be yearMonth.[[Calendar]].
    auto calendar = calendarID();
    // If CalendarEquals(calendar, other.[[Calendar]]) is false, throw a RangeError exception.
    if (calendar != other->calendarID()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Calendar mismatch");
    }
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // Let settings be ? GetDifferenceSettings(operation, resolvedOptions, date, « week, day », month, year).
    TemporalUnit disallowedUnits[2] = { TemporalUnit::Week, TemporalUnit::Day };
    auto settings = Temporal::getDifferenceSettings(state, operation == DifferenceTemporalYearMonth::Since, resolvedOptions, ISO8601::DateTimeUnitCategory::Date, disallowedUnits, 2, TemporalUnit::Month, TemporalUnit::Year);
    // If CompareISODate(yearMonth.[[ISODate]], other.[[ISODate]]) = 0, then
    if (TemporalPlainDateObject::compareISODate(state, this, other) == 0) {
        // Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
        return ISO8601::Duration();
    }
    // Let thisFields be ISODateToFields(calendar, yearMonth.[[ISODate]], year-month).
    // Set thisFields.[[Day]] to 1.
    CalendarFieldsRecord thisFields;
    auto isoDate = computeISODate(state);
    thisFields.year = isoDate.year();
    thisFields.month = isoDate.month();
    thisFields.day = 1;
    // Let thisDate be ? CalendarDateFromFields(calendar, thisFields, constrain).
    auto thisDate = new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                                Temporal::calendarDateFromFields(state, calendar, thisFields, TemporalOverflowOption::Constrain), calendar);
    // Let otherFields be ISODateToFields(calendar, other.[[ISODate]], year-month).
    // Set otherFields.[[Day]] to 1.
    CalendarFieldsRecord otherFields;
    isoDate = other->computeISODate(state);
    otherFields.year = isoDate.year();
    otherFields.month = isoDate.month();
    otherFields.day = 1;
    // Let otherDate be ? CalendarDateFromFields(calendar, otherFields, constrain).
    auto otherDate = new TemporalPlainDateObject(state, state.context()->globalObject()->temporalPlainDatePrototype(),
                                                 Temporal::calendarDateFromFields(state, calendar, otherFields, TemporalOverflowOption::Constrain), calendar);
    // Let dateDifference be CalendarDateUntil(calendar, thisDate, otherDate, settings.[[LargestUnit]]).
    auto dateDifference = Temporal::calendarDateUntil(calendar, thisDate->computeISODate(state), otherDate->computeISODate(state), toTemporalUnit(settings.largestUnit));

    // Let yearsMonthsDifference be ! AdjustDateDurationRecord(dateDifference, 0, 0).
    auto yearsMonthsDifference = Temporal::adjustDateDurationRecord(state, dateDifference, 0, 0, NullOption);
    // Let duration be CombineDateAndTimeDuration(yearsMonthsDifference, 0).
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(yearsMonthsDifference, 0);

    // If settings.[[SmallestUnit]] is not month or settings.[[RoundingIncrement]] ≠ 1, then
    if (settings.smallestUnit != ISO8601::DateTimeUnit::Day || settings.roundingIncrement != 1) {
        // Let isoDateTime be CombineISODateAndTimeRecord(thisDate, MidnightTimeRecord()).
        auto isoDateTime = computeISODate(state);
        // Let isoDateTimeOther be CombineISODateAndTimeRecord(otherDate, MidnightTimeRecord()).
        auto isoDateTimeOther = other->computeISODate(state);
        // Let destEpochNs be GetUTCEpochNanoseconds(isoDateTimeOther).
        auto destEpochNs = ISO8601::ExactTime::fromISOPartsAndOffset(isoDateTimeOther.year(), isoDateTimeOther.month(), isoDateTimeOther.day(), 0, 0, 0, 0, 0, 0, 0).epochNanoseconds();
        // Set duration to ? RoundRelativeDuration(duration, destEpochNs, isoDateTime, unset, calendar, settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        duration = Temporal::roundRelativeDuration(state, duration, destEpochNs, ISO8601::PlainDateTime(isoDateTime, ISO8601::PlainTime()), NullOption, calendar, toTemporalUnit(settings.largestUnit), settings.roundingIncrement, toTemporalUnit(settings.smallestUnit), settings.roundingMode);
    }
    // Let result be ! TemporalDurationFromInternal(duration, day).
    auto result = TemporalDurationObject::temporalDurationFromInternal(state, duration, ISO8601::DateTimeUnit::Day);
    // If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (operation == DifferenceTemporalYearMonth::Since) {
        result = TemporalDurationObject::createNegatedTemporalDuration(result);
    }
    // Return result.
    return result;
}

TemporalDurationObject* TemporalPlainYearMonthObject::since(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainYearMonth(state, DifferenceTemporalYearMonth::Since, other, options));
}

TemporalDurationObject* TemporalPlainYearMonthObject::until(ExecutionState& state, Value other, Value options)
{
    return new TemporalDurationObject(state, differenceTemporalPlainYearMonth(state, DifferenceTemporalYearMonth::Until, other, options));
}

} // namespace Escargot

#endif
