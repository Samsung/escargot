#if defined(ESCARGOT_ENABLE_TEMPORAL)
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
#include "runtime/VMInstance.h"

namespace Escargot {
TemporalObject::TemporalObject(ExecutionState& state)
    : TemporalObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalObject::TemporalObject(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalObject::toISODateTime(ExecutionState& state, DateObject& d)
{
    auto result = std::to_string(d.getFullYear(state))
        + "-"
        + (d.getMonth(state) + 1 < 10 ? "0" : "")
        + std::to_string(d.getMonth(state) + 1)
        + "-"
        + (d.getDate(state) < 10 ? "0" : "")
        + std::to_string(d.getDate(state))
        + "T"
        + (d.getHours(state) < 10 ? "0" : "")
        + std::to_string(d.getHours(state))
        + ":"
        + (d.getMinutes(state) < 10 ? "0" : "")
        + std::to_string(d.getMinutes(state))
        + ":"
        + (d.getSeconds(state) < 10 ? "0" : "")
        + std::to_string(d.getSeconds(state)) + "+00:00";
    return Value(new ASCIIString(result.data(), result.length()));
}

Value TemporalObject::toISODate(ExecutionState& state, DateObject& d)
{
    auto result = std::to_string(d.getFullYear(state)) + "-" + (d.getMonth(state) + 1 < 10 ? "0" : "") + std::to_string(d.getMonth(state) + 1) + "-" + (d.getDate(state) < 10 ? "0" : "") + std::to_string(d.getDate(state));
    return Value(new ASCIIString(result.data(), result.length()));
}

Value TemporalObject::toISOTime(ExecutionState& state, DateObject& d)
{
    auto result = (d.getHours(state) < 10 ? "0" : "") + std::to_string(d.getHours(state)) + ":" + (d.getMinutes(state) < 10 ? "0" : "") + std::to_string(d.getMinutes(state)) + ":" + (d.getSeconds(state) < 10 ? "0" : "") + std::to_string(d.getSeconds(state)) + "." + std::to_string(d.getMilliseconds(state));
    return Value(new ASCIIString(result.data(), result.length()));
}

std::map<std::string, std::string> TemporalObject::parseValidIso8601String(ExecutionState& state, const Value& str)
{
    ASSERT(str.isString());
    size_t pos = str.asString()->toNonGCUTF8StringData().find('[');
    std::string isoDate = str.asString()->toNonGCUTF8StringData().substr(0, pos);
    std::string extra;

    if (pos != std::string::npos) {
        extra = str.asString()->toNonGCUTF8StringData().substr(pos);
    }

    pos = isoDate.find('T');

    if (pos != std::string::npos) {
        std::replace(isoDate.begin(), isoDate.end(), 'T', ' ');
    }

    pos = isoDate.find(' ');
    std::string date = str.asString()->toNonGCUTF8StringData().substr(0, pos);
    std::string time = str.asString()->toNonGCUTF8StringData().substr(pos + 1);
    std::string year;
    std::string month;
    std::string week;
    std::string day;
    pos = date.find('-');

    if (pos != std::string::npos) {
        if (pos == 0 && date.length() > 4) {
            if (date.substr(pos + 1).find('-') == 0) {
                date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
                if (date.length() == 4) {
                    month = date.substr(1, 2);
                    day = date.substr(3, 2);
                    year = "";
                }
            } else {
                year = date;
            }
        } else if (date.length() > 4) {
            year = date.substr(0, pos);
            date = date.substr(pos + 1);
            if (date.find('W') != std::string::npos && date.length() == 5) {
                week = date.substr(0, 3);
                day = date.substr(4);
            } else if (date.length() > 2) {
                month = date.substr(0, 2);
                if (date[2] == '-' && date.length() == 5) {
                    day = date.substr(3, 2);
                }
            }
        }
    } else if (date.length() > 3) {
        year = date.substr(0, 4);
        if (date.find('W') != std::string::npos && date.length() > 6) {
            week = date.substr(4, 3);
            if (date.length() > 7) {
                day = date[8];
            }
        } else if (date.length() > 7) {
            month = date.substr(4, 2);
            day = date.substr(6, 2);
        }
    }

    std::string hour;
    std::string minute;
    std::string second;
    std::string millisecond = "0";
    std::string microSecond = "0";
    std::string nanoSecond = "0";
    std::string timeZone;
    std::string fraction;

    if (!time.empty()) {
        size_t hasTimeZone = (time.find('Z') == std::string::npos ? (time.find('+') == std::string::npos ? time.find('-') : time.find('+')) : time.find('Z'));
        if (time.find(',')) {
            std::replace(isoDate.begin(), isoDate.end(), ',', '.');
        }
        if (time.find(':') != std::string::npos && time.length() > 4) {
            hour = time.substr(0, 2);
            minute = time.substr(2, 2);
            if (time.length() > 6) {
                second = time.substr(6, 2);
            }
        } else if (time.length() > 1) {
            hour = time.substr(0, 2);
            minute = time.substr(2, 2);
            second = time.substr(4, 2);
        }
        pos = time.find('.');
        if (pos != std::string::npos) {
            fraction = time.substr(pos + 1, hasTimeZone);
        }
        if (hasTimeZone != std::string::npos) {
            timeZone = time.substr(hasTimeZone);
        }
    }

    std::string timeZoneExtension;
    std::string calendar;

    if (!extra.empty()) {
        size_t n = std::count(extra.begin(), extra.end(), '[');
        if (n == 2) {
            pos = extra.find("[]");
            timeZoneExtension = extra.substr(0, pos);
            calendar = extra.substr(pos + 2, extra.length() - 1);
        } else if (n == 1) {
            timeZoneExtension = extra.substr(1, extra.length() - 1);
        }
    }

    if (year == "-000000") {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid year");
    }

    if (month.empty()) {
        month = "1";
    }

    if (day.empty()) {
        day = "1";
    }

    if (second == "60") {
        second = "59";
    }

    if (!fraction.empty()) {
        fraction += "000000000";
        millisecond = fraction.substr(0, 3);
        microSecond = fraction.substr(3, 3);
        nanoSecond = fraction.substr(6, 3);
    }

    if (!TemporalPlainDate::isValidISODate(state, Value(year.c_str()), Value(month.c_str()), Value(day.c_str()))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO Date");
    }

    if (!TemporalPlainTime::isValidTime(state, Value(hour.c_str()), Value(minute.c_str()), Value(second.c_str()), Value(millisecond.c_str()), Value(microSecond.c_str()), Value(nanoSecond.c_str()))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid time");
    }

    return std::map<std::string, std::string>{ { "Year", year }, { "Month", month }, { "Day", day }, { "Hour", hour }, { "Minute", minute }, { "Second", second }, { "Millisecond", millisecond }, { "Microsecond", microSecond }, { "Nanosecond", nanoSecond }, { "Calendar", calendar } };
}

TemporalPlainTime::TemporalPlainTime(ExecutionState& state)
    : TemporalPlainTime(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainTime::TemporalPlainTime(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalPlainTime::createTemporalTime(ExecutionState& state, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value hour, min, sec, ms, qs, ns;

    if (argc > 5) {
        hour = argv[0];
        min = argv[1];
        sec = argv[2];
        ms = argv[3];
        qs = argv[4];
        ns = argv[5];
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Too few arguments");
    }

    ASSERT(hour.isInteger(state) && min.isInteger(state) && sec.isInteger(state) && ms.isInteger(state) && qs.isInteger(state) && ns.isInteger(state));

    time64_t timeInMs = hour.asInt32() * 3600000 + min.asInt32() * 60000 + sec.asInt32() * 1000 + ms.asInt32();

    if (!IS_IN_TIME_RANGE(timeInMs)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    TemporalPlainTime* temporalPlainTime;

    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }

    temporalPlainTime = new TemporalPlainTime(state, newTarget->asObject());

    temporalPlainTime->setTime(hour.asInt32(), min.asInt32(), sec.asInt32(), ms.asInt32(), qs.asInt32(), ns.asInt32());
    temporalPlainTime->setCalendar(state, new ASCIIString("iso8601"), newTarget);
    return temporalPlainTime;
}

void TemporalPlainTime::setTime(char h, char m, char s, short ms, short qs, short ns)
{
    this->m_hour = h;
    this->m_minute = m;
    this->m_second = s;
    this->m_millisecond = ms;
    this->m_microsecond = qs;
    this->m_nanosecond = ns;
}

void TemporalPlainTime::setCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget)
{
    this->calendar = TemporalCalendar::createTemporalCalendar(state, id, newTarget);
}

bool TemporalPlainTime::isValidTime(ExecutionState& state, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond)
{
    ASSERT(hour.isInteger(state) && minute.isInteger(state) && second.isInteger(state) && millisecond.isInteger(state) && microsecond.isInteger(state) && nanosecond.isInteger(state));

    int h = hour.asInt32();
    int m = minute.asInt32();
    int s = second.asInt32();
    int ms = millisecond.asInt32();
    int us = microsecond.asInt32();
    int ns = nanosecond.asInt32();

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59 || ms < 0 || ms > 999 || us < 0 || us > 999 || ns < 0 || ns > 999) {
        return false;
    }

    return true;
}

bool TemporalCalendar::isBuiltinCalendar(String* id)
{
    return id->equals("iso8601");
}

TemporalCalendar::TemporalCalendar(ExecutionState& state)
    : TemporalCalendar(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalCalendar::TemporalCalendar(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

TemporalCalendar* TemporalCalendar::createTemporalCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget)
{
    ASSERT(TemporalCalendar::isBuiltinCalendar(id));

    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->aggregateErrorPrototype();
    });

    auto* O = new TemporalCalendar(state, proto);
    O->setIdentifier(id);

    return O;
}

String* TemporalCalendar::getIdentifier() const
{
    return m_identifier;
}

void TemporalCalendar::setIdentifier(String* id)
{
    TemporalCalendar::m_identifier = id;
}

Value TemporalCalendar::getBuiltinCalendar(ExecutionState& state, String* id)
{
    if (!TemporalCalendar::isBuiltinCalendar(id)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::IsNotDefined);
    }
    Value argv[1] = { Value(id) };
    return Object::construct(state, state.context()->globalObject()->temporalCalendar(), 1, argv).toObject(state);
}

Value TemporalCalendar::getISO8601Calendar(ExecutionState& state)
{
    return TemporalCalendar::getBuiltinCalendar(state, new ASCIIString("iso8601"));
}

Value TemporalCalendar::toTemporalCalendar(ExecutionState& state, const Value& calendar)
{
    if (calendar.isObject()) {
        auto* calendarObject = calendar.asObject()->asTemporalCalendarObject();

        if (calendarObject->internalSlot()->isTemporalObject()) {
            return Value(calendarObject->getIdentifier());
        }
        if (!calendarObject->hasProperty(state, AtomicString(state, "calendar"))) {
            return calendar;
        }
        calendar.asObject()->get(state, AtomicString(state, "calendar"), calendar);
        if (!calendarObject->hasProperty(state, AtomicString(state, "calendar"))) {
            return calendar;
        }
    }
    String* identifier = calendar.asString();
    if (!TemporalCalendar::isBuiltinCalendar(identifier)) {
        identifier = TemporalCalendar::parseTemporalCalendarString(state, identifier).asString();
        if (!TemporalCalendar::isBuiltinCalendar(identifier)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::IsNotDefined);
        }
    }
    return Value(TemporalCalendar::createTemporalCalendar(state, identifier));
}

Value TemporalCalendar::parseTemporalCalendarString(ExecutionState& state, const Value& isoString)
{
    ASSERT(isoString.isString());

    return TemporalObject::parseValidIso8601String(state, isoString).at("Calendar").empty() ? Value("iso8601") : Value(TemporalObject::parseValidIso8601String(state, isoString).at("Calendar").c_str());
}

Value TemporalCalendar::ISODaysInMonth(ExecutionState& state, const Value& year, const Value& month)
{
    ASSERT(year.isInteger(state));
    ASSERT(month.isInteger(state) && month.asInt32() > 0 && month.asInt32() < 13);
    int m = month.asInt32();

    if (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12) {
        return Value(31);
    }
    if (m == 4 || m == 6 || m == 9 || m == 11) {
        return Value(30);
    }
    if (TemporalCalendar::isIsoLeapYear(state, year)) {
        return Value(29);
    }
    return Value(28);
}

bool TemporalCalendar::isIsoLeapYear(ExecutionState& state, const Value& year)
{
    ASSERT(year.isInteger(state));
    int y = year.asInt32();
    if (y % 4 != 0) {
        return false;
    }
    if (y % 400 == 0) {
        return true;
    }
    if (y % 100 == 0) {
        return false;
    }
    return true;
}

/*12.1.6 toTemporalCalendarWithISODefault*/
Value TemporalCalendar::toTemporalCalendarWithISODefault(ExecutionState& state, const Value& calendar)
{
    if (calendar.isUndefined()) {
        return TemporalCalendar::getISO8601Calendar(state);
    }
    return TemporalCalendar::toTemporalCalendar(state, calendar);
}

Value TemporalCalendar::defaultMergeFields(ExecutionState& state, const Value& fields, const Value& additionalFields)
{
    Object* merged = Object::getPrototypeFromConstructor(state, state.resolveCallee(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->objectPrototype();
    });

    auto originalKeys = Object::enumerableOwnProperties(state, fields.asObject(), EnumerableOwnPropertiesType::Key);

    for (auto nextKey : originalKeys) {
        if (!nextKey.asString()->equals("month") || !nextKey.asString()->equals("monthCode")) {
            Value propValue;
            fields.asObject()->get(state, AtomicString(state, nextKey.asString()), propValue);
            if (!propValue.isUndefined()) {
                merged->defineOwnPropertyThrowsException(state, AtomicString(state, nextKey.asString()), ObjectPropertyDescriptor(propValue, ObjectPropertyDescriptor::AllPresent));
            }
        }
    }
    auto newKeys = Object::enumerableOwnProperties(state, additionalFields.asObject(), EnumerableOwnPropertiesType::Key);
    bool containsMonth = false;

    for (unsigned int i = 0; i < newKeys.size(); ++i) {
        Value nextKey = originalKeys[i];
        if (!nextKey.asString()->equals("month") || !nextKey.asString()->equals("monthCode")) {
            containsMonth = true;
        }
        Value propValue;
        fields.asObject()->get(state, AtomicString(state, nextKey.asString())).value(state, propValue);
        if (!propValue.isUndefined()) {
            merged->defineOwnPropertyThrowsException(state, AtomicString(state, nextKey.asString()), ObjectPropertyDescriptor(propValue, ObjectPropertyDescriptor::AllPresent));
        }
    }
    if (!containsMonth) {
        Value month;
        fields.asObject()->get(state, AtomicString(state, "month"), month);
        if (!month.isUndefined()) {
            merged->defineOwnPropertyThrowsException(state, AtomicString(state, "month"), ObjectPropertyDescriptor(month, ObjectPropertyDescriptor::AllPresent));
        }
        Value monthCode;
        fields.asObject()->get(state, AtomicString(state, "monthCode"), monthCode);
        if (!monthCode.isUndefined()) {
            merged->defineOwnPropertyThrowsException(state, AtomicString(state, "monthCode"), ObjectPropertyDescriptor(monthCode, ObjectPropertyDescriptor::AllPresent));
        }
    }
    return Value(merged);
}

bool TemporalPlainDate::isValidISODate(ExecutionState& state, const Value& year, const Value& month, const Value& day)
{
    ASSERT(year.isInteger(state) && month.isInteger(state) && day.isInteger(state));
    if (month.asInt32() < 1 || month.asInt32() > 12) {
        return false;
    }
    Value daysInMonth = TemporalCalendar::ISODaysInMonth(state, year, month);
    if (day.asInt32() < 1 || day.asInt32() > daysInMonth.asInt32()) {
        return false;
    }
    return true;
}

Value TemporalPlainDate::createTemporalDate(ExecutionState& state, const Value& isoYear, const Value& isoMonth, const Value& isoDay, const Value& calendar, Optional<Object*> newTarget)
{
    ASSERT(isoYear.isInteger(state) && isoMonth.isInteger(state) && isoDay.isInteger(state));
    ASSERT(calendar.isObject());
    if (!TemporalPlainDate::isValidISODate(state, isoYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Not valid ISOdate");
    }

    if (!TemporalPlainDateTime::ISODateTimeWithinLimits(state, isoYear, isoMonth, isoDay, Value(12), Value(0), Value(0), Value(0), Value(0), Value(0))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Not valid ISOdate");
    }

    Object* newTargetVariable;
    if (!newTarget.hasValue()) {
        newTargetVariable = state.resolveCallee();
    } else {
        newTargetVariable = newTarget.value();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTargetVariable, [](ExecutionState& state, Context* realm) -> Object* {
        return realm->globalObject()->temporalPlainDatePrototype();
    });
    Object* object = new TemporalPlainDate(state, proto, isoYear, isoMonth, isoDay, calendar);

    return Value(object);
}

TemporalPlainDate::TemporalPlainDate(ExecutionState& state, Value isoYear, Value isoMonth, Value isoDay, Optional<Value> calendarLike)
    : TemporalPlainDate(state, state.context()->globalObject()->temporalPlainDatePrototype(), isoYear, isoMonth, isoDay, calendarLike)
{
}

TemporalPlainDate::TemporalPlainDate(ExecutionState& state, Object* proto, Value isoYear, Value isoMonth, Value isoDay, Optional<Value> calendarLike)
    : Temporal(state, proto)
    , m_y(isoYear.asInt32())
    , m_m(isoMonth.asInt32())
    , m_d(isoDay.asInt32())
    , m_calendar(calendarLike.value())
{
}

TemporalPlainDateTime::TemporalPlainDateTime(ExecutionState& state)
    : TemporalPlainDateTime(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainDateTime::TemporalPlainDateTime(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalPlainDateTime::getEpochFromISOParts(ExecutionState& state, const Value& year, const Value& month, const Value& day, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond)
{
    ASSERT(year.isInteger(state) && month.isInteger(state) && day.isInteger(state) && hour.isInteger(state) && minute.isInteger(state) && second.isInteger(state) && millisecond.isInteger(state) && microsecond.isInteger(state) && nanosecond.isInteger(state));
    ASSERT(TemporalPlainDate::isValidISODate(state, year, month, day));
    ASSERT(TemporalPlainTime::isValidTime(state, hour, minute, second, millisecond, microsecond, nanosecond));

    Value date = DateObject::makeDay(state, year, month, day);
    Value time = DateObject::makeTime(state, hour, minute, second, millisecond);
    Value ms = DateObject::makeDate(state, date, time);
    ASSERT(std::isfinite(ms.asNumber()));

    return Value(ms.toLength(state));
}

bool TemporalPlainDateTime::ISODateTimeWithinLimits(ExecutionState& state, const Value& year, const Value& month, const Value& day, const Value& hour, const Value& minute, const Value& second, const Value& millisecond, const Value& microsecond, const Value& nanosecond)
{
    ASSERT(year.isInteger(state) && month.isInteger(state) && day.isInteger(state) && hour.isInteger(state) && minute.isInteger(state) && second.isInteger(state) && millisecond.isInteger(state) && microsecond.isInteger(state) && nanosecond.isInteger(state));

    time64_t ms = TemporalPlainDateTime::getEpochFromISOParts(state, year, month, day, hour, minute, second, millisecond, microsecond, nanosecond).toLength(state);
    return IS_IN_TIME_RANGE(ms);
}

} // namespace Escargot

#endif
