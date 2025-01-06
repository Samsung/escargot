#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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
#include "TemporalObject.h"
#include "DateObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/BigInt.h"

namespace Escargot {

//////////////////
// helper function
//////////////////
static int epochDayNumberForYear(const int year)
{
    return static_cast<int>(365 * (year - 1970) + std::floor((year - 1969) / 4) - std::floor((year - 1901) / 100) + std::floor((year - 1601) / 400));
}

static int epochTimeForYear(const int year)
{
    return TimeConstant::MsPerDay * epochDayNumberForYear(year);
}

static int epochTimeToEpochYear(const int time)
{
    return DateObject::yearFromTime(time);
}

static int mathematicalDaysInYear(const int year)
{
    if ((year % 4) != 0) {
        return 365;
    }

    if ((year % 100) != 0) {
        return 366;
    }

    if ((year % 400) != 0) {
        return 365;
    }

    return 366;
}

static int mathematicalInLeapYear(const int time)
{
    int daysInYear = mathematicalDaysInYear(epochTimeToEpochYear(time));

    if (daysInYear == 365) {
        return 0;
    } else {
        ASSERT(daysInYear == 366);
        return 1;
    }
}

static Value getOption(ExecutionState& state, Object* options, Value property, bool isBool, Value* values, size_t valuesLength, const Value& defaultValue)
{
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    if (value.isUndefined()) {
        // TODO handle REQUIRED in default value
        return defaultValue;
    }
    if (isBool) {
        value = Value(value.toBoolean());
    } else {
        value = Value(value.toString(state));
    }

    if (valuesLength) {
        bool contains = false;
        for (size_t i = 0; i < valuesLength; i++) {
            if (values[i].equalsTo(state, value)) {
                contains = true;
            }
        }
        if (!contains) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
        }
    }

    return value;
}

static Object* getOptionsObject(ExecutionState& state, Value options)
{
    if (options.isUndefined()) {
        return new Object(state, Object::PrototypeIsNull);
    }
    if (options.isObject()) {
        return options.asObject();
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::TemporalError);
    return nullptr;
}

static bool getTemporalOverflowOptions(ExecutionState& state, Object* options)
{
    StaticStrings* strings = &state.context()->staticStrings();
    Value stringConstrain = Value(strings->lazyConstrain().string());

    Value values[2] = { stringConstrain, strings->reject.string() };
    Value stringValue = getOption(state, options, Value(strings->lazyOverflow().string()), false, values, 2, stringConstrain);
    if (stringValue.equalsTo(state, stringConstrain)) {
        // return CONSTRAIN
        return true;
    }

    // return REJECT
    return false;
}

static int64_t countModZerosInRange(int64_t mod, int64_t start, int64_t end)
{
    int64_t divEnd = end - 1;
    int negative = divEnd < 0;
    int64_t endPos = (divEnd + negative) / mod - negative;

    divEnd = start - 1;
    negative = divEnd < 0;
    int64_t startPos = (divEnd + negative) / mod - negative;

    return endPos - startPos;
}

static int countDaysOfYear(int year, int month, int day)
{
    if (month < 1 || month > 12) {
        return 0;
    }

    static const int seekTable[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int daysOfYear = seekTable[month - 1] + day - 1;

    if ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) && month >= 3) {
        daysOfYear++;
    }

    return daysOfYear;
}

static double makeDay(double year, double month, double date)
{
    if (!std::isfinite(year) || !std::isfinite(month) || !std::isfinite(date)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double ym = year + std::floor(month / TimeConstant::MonthsPerYear);
    if (!std::isfinite(ym)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mn = ((int)month % TimeConstant::MonthsPerYear);
    if ((ym > std::numeric_limits<int>::max()) || (ym < std::numeric_limits<int>::min())) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if ((mn + 1 > std::numeric_limits<int>::max()) || (mn + 1 < std::numeric_limits<int>::min())) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    int beginYear, endYear, leapSign;
    if (ym < 1970) {
        beginYear = static_cast<int>(ym);
        endYear = 1970;
        leapSign = -1;
    } else {
        beginYear = 1970;
        endYear = static_cast<int>(ym);
        leapSign = 1;
    }
    int64_t days = 365 * (static_cast<int64_t>(ym) - 1970);
    int64_t extraLeapDays = 0;
    extraLeapDays += countModZerosInRange(4, beginYear, endYear);
    extraLeapDays += countModZerosInRange(100, beginYear, endYear);
    extraLeapDays += countModZerosInRange(400, beginYear, endYear);
    int64_t yearsToDays = days + extraLeapDays * leapSign;

    int64_t daysOfYear = countDaysOfYear(static_cast<int>(ym), static_cast<int>(mn) + 1, 1);

    int64_t t = (yearsToDays + daysOfYear) * TimeConstant::MsPerDay;
    return (std::floor(t / TimeConstant::MsPerDay) + date - 1);
}

static double makeTime(double hour, double minute, double sec, double ms)
{
    if (!std::isfinite(hour) || !std::isfinite(minute) || !std::isfinite(sec) || !std::isfinite(ms)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return (hour * TimeConstant::MsPerHour + minute * TimeConstant::MsPerMinute + sec * TimeConstant::MsPerSecond + ms);
}

static double makeDate(double day, double time)
{
    if (!std::isfinite(day) || !std::isfinite(time)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double tv = day * TimeConstant::MsPerDay + time;
    if (!std::isfinite(tv)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return tv;
}

////////////////////////////
// Temporal helper functions
////////////////////////////
bool Temporal::isValidISODate(ExecutionState& state, const int year, const int month, const int day)
{
    if (month < 1 || month > 12) {
        return false;
    }

    int daysInMonth = Temporal::ISODaysInMonth(state, year, month);
    if (day < 1 || day > daysInMonth) {
        return false;
    }

    return true;
}

int Temporal::ISODaysInMonth(ExecutionState& state, const int year, const int month)
{
    if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
        return 31;
    }

    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }

    ASSERT(month == 2);

    // Return 28 + MathematicalInLeapYear(EpochTimeForYear(year)).
    return 28 + mathematicalInLeapYear(epochTimeForYear(year));
}

bool Temporal::ISODateWithinLimits(ExecutionState& state, const ISODate& date)
{
    // NoonTimeRecord
    // { [[Days]]: 0, [[Hour]]: 12, [[Minute]]: 0, [[Second]]: 0, [[Millisecond]]: 0, [[Microsecond]]: 0, [[Nanosecond]]: 0  }
    TimeRecord record = TimeRecord::noonTimeRecord();
    ISODateTime dateTime = ISODateTime(date, record);

    return Temporal::ISODateTimeWithinLimits(state, dateTime);
}

bool Temporal::ISODateTimeWithinLimits(ExecutionState& state, const ISODateTime& dateTime)
{
    double date = makeDay(dateTime.date.year, dateTime.date.month - 1, dateTime.date.day);
    if (std::abs(date) > 100000001) {
        return false;
    }

    double time = makeTime(dateTime.time.hour, dateTime.time.minute, dateTime.time.second, dateTime.time.millisecond);
    double ms = makeDate(date, time);

    // Z(R(ms) × 10**6 + isoDateTime.[[Time]].[[Microsecond]] × 10**3 + isoDateTime.[[Time]].[[Nanosecond]]).
    int64_t ns = static_cast<int64_t>(ms) * 1000000 + dateTime.time.microsecond * 1000 + dateTime.time.nanosecond;

    BigIntData nsData(ns);

    if (nsData.lessThanEqual(Temporal::nsMinConstant())) {
        return false;
    }
    if (nsData.greaterThanEqual(Temporal::nsMaxConstant())) {
        return false;
    }

    return true;
}

BigInt* Temporal::nsMaxInstant()
{
    static MAY_THREAD_LOCAL BigInt* NSMaxInstant = nullptr;
    if (!NSMaxInstant) {
        const char* maxStr = "8640000000000000000000";
        BigIntData dat(maxStr, strlen(maxStr), 10);
        ASSERT(!dat.isNaN());
        NSMaxInstant = new (GC_MALLOC_UNCOLLECTABLE(sizeof(BigInt))) BigInt(std::move(dat));
    }

    return NSMaxInstant;
}

BigInt* Temporal::nsMinInstant()
{
    static MAY_THREAD_LOCAL BigInt* NSMinInstant = nullptr;
    if (!NSMinInstant) {
        const char* minStr = "-8640000000000000000000";
        BigIntData dat(minStr, strlen(minStr), 10);
        ASSERT(!dat.isNaN());
        NSMinInstant = new (GC_MALLOC_UNCOLLECTABLE(sizeof(BigInt))) BigInt(std::move(dat));
    }

    return NSMinInstant;
}

BigInt* Temporal::nsMaxConstant()
{
    static MAY_THREAD_LOCAL BigInt* NSMaxConstant = nullptr;
    if (!NSMaxConstant) {
        const char* maxStr = "8640000086400000000000";
        BigIntData dat(maxStr, strlen(maxStr), 10);
        ASSERT(!dat.isNaN());
        NSMaxConstant = new (GC_MALLOC_UNCOLLECTABLE(sizeof(BigInt))) BigInt(std::move(dat));
    }

    return NSMaxConstant;
}

BigInt* Temporal::nsMinConstant()
{
    static MAY_THREAD_LOCAL BigInt* NSMinConstant = nullptr;
    if (!NSMinConstant) {
        const char* minStr = "-8640000086400000000000";
        BigIntData dat(minStr, strlen(minStr), 10);
        ASSERT(!dat.isNaN());
        NSMinConstant = new (GC_MALLOC_UNCOLLECTABLE(sizeof(BigInt))) BigInt(std::move(dat));
    }

    return NSMinConstant;
}

int64_t Temporal::nsPerDay()
{
    static MAY_THREAD_LOCAL int64_t NSPerDay = 86400000000000;
    return NSPerDay;
}

Value Temporal::createTemporalDate(ExecutionState& state, const ISODate& isoDate, String* calendar, Optional<Object*> newTarget)
{
    if (!Temporal::ISODateWithinLimits(state, isoDate)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::TemporalError);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.hasValue() ? newTarget.value() : state.context()->globalObject()->temporalPlainDate(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainDatePrototype();
    });

    return new TemporalPlainDateObject(state, proto, isoDate, calendar);
}

Value Temporal::toTemporalDate(ExecutionState& state, Value item, Value options)
{
    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainDateObject()) {
            Object* resolvedOptions = getOptionsObject(state, options);
            getTemporalOverflowOptions(state, resolvedOptions);
            return Temporal::createTemporalDate(state, item.asObject()->asTemporalPlainDateObject()->date(), item.asObject()->asTemporalPlainDateObject()->calendar(), nullptr);
        }

        // TODO
        return Value();
    }

    if (!item.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::TemporalError);
    }

    // TODO ParseISODateTime
    return Value();
}

TemporalObject::TemporalObject(ExecutionState& state)
    : TemporalObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalObject::TemporalObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, const ISODate& date, String* calendar)
    : TemporalObject(state, proto)
    , m_calendar(calendar)
    , m_date(date)
{
}

void* TemporalPlainDateObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(TemporalPlainDateObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(TemporalPlainDateObject, m_calendar));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(TemporalPlainDateObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

} // namespace Escargot

#endif
