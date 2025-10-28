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
#include "TemporalZonedDateTimeObject.h"
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

void TemporalZonedDateTimeObject::init(ExecutionState& state, ComputedTimeZone timeZone)
{
    m_timeZone = timeZone;
    Int128 timezoneAppliedEpochNanoseconds = *m_epochNanoseconds + timeZone.offset();
    int64_t computedEpoch = ISO8601::ExactTime(timezoneAppliedEpochNanoseconds).epochMilliseconds();
    DateObject::DateTimeInfo timeInfo;
    DateObject::computeTimeInfoFromEpoch(computedEpoch, timeInfo);
    auto d = Temporal::balanceTime(0, 0, 0, 0, 0, timezoneAppliedEpochNanoseconds % ISO8601::ExactTime::nsPerDay);
    *m_plainDateTime = ISO8601::PlainDateTime(ISO8601::PlainDate(timeInfo.year, timeInfo.month + 1, timeInfo.mday),
                                              ISO8601::PlainTime(d.hours(), d.minutes(), d.seconds(), d.milliseconds(), d.microseconds(), d.nanoseconds()));

    if (!ISO8601::isoDateTimeWithinLimits(*m_epochNanoseconds)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Out of range date-time");
    }

    m_icuCalendar = m_calendarID.createICUCalendar(state);

    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(m_icuCalendar, ISO8601::ExactTime(*m_epochNanoseconds).epochMilliseconds(), &status);
    CHECK_ICU()

    auto tz = m_timeZone.timeZoneName()->toUTF16StringData();
    ucal_setTimeZone(m_icuCalendar, tz.data(), tz.length(), &status);
    CHECK_ICU()

    addFinalizer([](PointerValue* obj, void* data) {
        TemporalZonedDateTimeObject* self = (TemporalZonedDateTimeObject*)obj;
        ucal_close(self->m_icuCalendar);
    },
                 nullptr);
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, TimeZone timeZone, Calendar calendar)
    : DerivedObject(state, proto)
    , m_epochNanoseconds(new(PointerFreeGC) Int128(epochNanoseconds))
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_timeZone()
    , m_calendarID(calendar)
    , m_icuCalendar(nullptr)
{
    ComputedTimeZone computedTimeZone;
    if (timeZone.hasTimeZoneName()) {
        Int128 timezoneAppliedEpoch = Temporal::getEpochNanosecondsFor(state, timeZone, epochNanoseconds, TemporalDisambiguationOption::Compatible);
        computedTimeZone = ComputedTimeZone(timeZone.timeZoneName(), int64_t(timezoneAppliedEpoch - epochNanoseconds));
    } else {
        StringBuilder tz;
        Temporal::formatOffsetTimeZoneIdentifier(state, int(timeZone.offset() / ISO8601::ExactTime::nsPerMinute), tz);
        computedTimeZone = ComputedTimeZone(tz.finalize(), timeZone.offset());
    }

    init(state, computedTimeZone);
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, ComputedTimeZone timeZone, Calendar calendar)
    : DerivedObject(state, proto)
    , m_epochNanoseconds(new(PointerFreeGC) Int128(epochNanoseconds))
    , m_plainDateTime(new(PointerFreeGC) ISO8601::PlainDateTime({}, {}))
    , m_timeZone()
    , m_calendarID(calendar)
    , m_icuCalendar(nullptr)
{
    init(state, timeZone);
}

String* TemporalZonedDateTimeObject::toString(ExecutionState& state, Value options)
{
    // Let resolvedOptions be ? GetOptionsObject(options).
    auto resolvedOptions = Intl::getOptionsObject(state, options);
    // NOTE: The following steps read options and perform independent validation in alphabetical order (GetTemporalShowCalendarNameOption reads "calendarName", GetTemporalFractionalSecondDigitsOption reads "fractionalSecondDigits", and GetRoundingModeOption reads "roundingMode").
    // Let showCalendar be ? GetTemporalShowCalendarNameOption(resolvedOptions).
    auto showCalendar = Temporal::getTemporalShowCalendarNameOption(state, resolvedOptions);
    // Let digits be ? GetTemporalFractionalSecondDigitsOption(resolvedOptions).
    auto digits = Temporal::getTemporalFractionalSecondDigitsOption(state, resolvedOptions);
    // Let showOffset be ? GetTemporalShowOffsetOption(resolvedOptions).
    auto showOffset = Temporal::getTemporalShowOffsetOption(state, resolvedOptions);
    // Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
    auto roundingMode = Temporal::getRoundingModeOption(state, resolvedOptions, state.context()->staticStrings().trunc.string());
    // Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions, "smallestUnit", unset).
    auto smallestUnit = Temporal::getTemporalUnitValuedOption(state, resolvedOptions, state.context()->staticStrings().lazySmallestUnit().string(), NullOption);
    // Let showTimeZone be ? GetTemporalShowTimeZoneNameOption(resolvedOptions).
    auto showTimeZone = Temporal::getTemporalShowTimeZoneNameOption(state, resolvedOptions);
    // Perform ? ValidateTemporalUnitValue(smallestUnit, time).
    Temporal::validateTemporalUnitValue(state, smallestUnit, ISO8601::DateTimeUnitCategory::Time, nullptr, 0);
    // If smallestUnit is hour, throw a RangeError exception.
    if (smallestUnit == TemporalUnit::Hour) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid smallestUnit");
    }
    // Let precision be ToSecondsStringPrecisionRecord(smallestUnit, digits).
    auto precision = Temporal::toSecondsStringPrecisionRecord(state, toDateTimeUnit(smallestUnit), digits);

    // Return TemporalZonedDateTimeToString(zonedDateTime, precision.[[Precision]], showCalendar, showTimeZone, showOffset, precision.[[Increment]], precision.[[Unit]], roundingMode).
    auto result = Temporal::roundISODateTime(state, ISO8601::PlainDateTime(plainDate(), plainTime()), precision.increment, precision.unit, roundingMode);
    StringBuilder sb;
    sb.appendString(TemporalPlainDateObject::temporalDateToString(result.plainDate(), m_calendarID, TemporalShowCalendarNameOption::Never));
    sb.appendChar('T');
    sb.appendString(TemporalPlainTimeObject::temporalTimeToString(result.plainTime(), precision.precision));

    if (showOffset != TemporalShowOffsetOption::Never) {
        auto offsetNanoseconds = TemporalDurationObject::roundTimeDurationToIncrement(state, m_timeZone.offset(), ISO8601::ExactTime::nsPerMinute, ISO8601::RoundingMode::HalfExpand);
        auto offsetMinutes = int(offsetNanoseconds / ISO8601::ExactTime::nsPerMinute);
        Temporal::formatOffsetTimeZoneIdentifier(state, offsetMinutes, sb);
    }

    if (showTimeZone != TemporalShowTimeZoneNameOption::Never) {
        // If showTimeZone is critical, let flag be "!"; else let flag be the empty String.
        if (showTimeZone == TemporalShowTimeZoneNameOption::Critical) {
            sb.appendChar('!');
        }
        // Let timeZoneString be the string-concatenation of the code unit 0x005B (LEFT SQUARE BRACKET), flag, timeZone, and the code unit 0x005D (RIGHT SQUARE BRACKET).
        sb.appendChar('[');
        sb.appendString(m_timeZone.timeZoneName());
        sb.appendChar(']');
    }

    Temporal::formatCalendarAnnotation(sb, calendarID(), showCalendar);

    return sb.finalize();
}

} // namespace Escargot

#endif
