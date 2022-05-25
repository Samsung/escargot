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
#include "runtime/ArrayObject.h"

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

    if (!TemporalPlainDate::isValidISODate(state, Value(year.c_str()).asInt32(), Value(month.c_str()).asInt32(), Value(day.c_str()).asInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO Date");
    }

    if (!TemporalPlainTime::isValidTime(state, Value(hour.c_str()).asInt32(), Value(minute.c_str()).asInt32(), Value(second.c_str()).asInt32(), Value(millisecond.c_str()).asInt32(), Value(microSecond.c_str()).asInt32(), Value(nanoSecond.c_str()).asInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid time");
    }

    return std::map<std::string, std::string>{ { "Year", year }, { "Month", month }, { "Day", day }, { "Hour", hour }, { "Minute", minute }, { "Second", second }, { "Millisecond", millisecond }, { "Microsecond", microSecond }, { "Nanosecond", nanoSecond }, { "Calendar", calendar } };
}

std::map<std::string, std::string> TemporalObject::parseTemporalDateString(ExecutionState& state, std::string isoString)
{
    size_t pos = isoString.find('[');
    std::string isoDate = isoString.substr(0, pos);
    std::string extra;

    if (pos != std::string::npos) {
        extra = isoString.substr(pos);
    }

    pos = isoDate.find('T');

    if (pos != std::string::npos) {
        std::replace(isoDate.begin(), isoDate.end(), 'T', ' ');
    }

    pos = isoDate.find(' ');
    std::string date = isoString.substr(0, pos);
    std::string time = isoString.substr(pos + 1);
    std::string year;
    std::string month;
    std::string week;
    std::string day;
    std::string timeZoneExtension;
    std::string calendar;
    pos = date.find('-');

    if (pos != std::string::npos) {
        if (pos == 0 && date.length() > 4) {
            if (date.substr(pos + 1).find('-') == 0) {
                date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
                if (date.length() == 4) {
                    goto good;
                }
            } else {
                goto good;
            }
        } else if (date.length() > 4) {
            if (date.find('W') != std::string::npos && date.length() == 5) {
                goto good;
            } else if (date.length() > 2) {
                if (date[2] == '-' && date.length() == 5) {
                    goto good;
                }
            }
        }
    } else if (date.length() > 3) {
        if (date.find('W') != std::string::npos && date.length() > 6) {
            if (date.length() > 7) {
                goto good;
            }
        } else if (date.length() > 7) {
            goto good;
        }
    }

    if (!extra.empty()) {
        size_t n = std::count(extra.begin(), extra.end(), '[');
        if (n == 1 || n == 2) {
            goto good;
        }
    }


    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO Date");

good:
    return TemporalObject::parseValidIso8601String(state, Value(new ASCIIString(isoString.c_str())));
}

std::map<std::string, std::string> TemporalObject::parseTemporalDateTimeString(ExecutionState& state, String* isoString)
{
    if (isoString->contains(new ASCIIString("z")) || isoString->contains(new ASCIIString("Z"))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "isoString contains UTCDesignator");
    }
    return TemporalObject::parseValidIso8601String(state, Value(isoString));
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

bool TemporalPlainTime::isValidTime(ExecutionState& state, const int h, const int m, const int s, const int ms, const int us, const int ns)
{
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59 || ms < 0 || ms > 999 || us < 0 || us > 999 || ns < 0 || ns > 999) {
        return false;
    }

    return true;
}

std::map<std::string, int> TemporalPlainTime::balanceTime(ExecutionState& state, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    std::map<std::string, int> result;

    result["Microsecond"] = microsecond + (int)std::floor(nanosecond / 1000);
    result["Nanosecond"] = nanosecond % 1000;
    result["Millisecond"] = millisecond + (int)std::floor(result["Microsecond"] / 1000);
    result["Microsecond"] %= 1000;
    result["Second"] = second + (int)std::floor(result["Millisecond"] / 1000);
    result["Millisecond"] %= 1000;
    result["Minute"] = minute + (int)std::floor(result["Second"] / 60);
    result["Second"] %= 60;
    result["Hour"] = hour + (int)std::floor(result["minute"] / 60);
    result["Minute"] %= 60;
    result["Days"] = std::floor(result["Hour"] / 24);
    result["hour"] %= 24;

    return result;
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

Value TemporalCalendar::ISODaysInMonth(ExecutionState& state, const int year, const int m)
{
    ASSERT(m > 0 && m < 13);

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

bool TemporalCalendar::isIsoLeapYear(ExecutionState& state, const int year)
{
    if (year % 4 != 0) {
        return false;
    }
    if (year % 400 == 0) {
        return true;
    }
    if (year % 100 == 0) {
        return false;
    }
    return true;
}

std::string TemporalCalendar::buildISOMonthCode(ExecutionState& state, const int month)
{
    return "M" + std::string((month < 10 ? "0" : "") + char(48 + month));
}

int TemporalCalendar::ISOYear(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->year());
    return temporalObject.asObject()->asTemporalPlainDateObject()->year();
}

int TemporalCalendar::ISOMonth(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->month());
    return temporalObject.asObject()->asTemporalPlainDateObject()->month();
}
std::string TemporalCalendar::ISOMonthCode(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->month());
    return TemporalCalendar::buildISOMonthCode(state, temporalObject.asObject()->asTemporalPlainDateObject()->month());
}
int TemporalCalendar::ISODay(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->day());
    return temporalObject.asObject()->asTemporalPlainDateObject()->day();
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
        fields.asObject()->get(state, AtomicString(state, nextKey.asString()), propValue);
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

Value TemporalCalendar::getTemporalCalendarWithISODefault(ExecutionState& state, const Value& item)
{
    Value calendarLike;
    item.asObject()->asTemporalObject()->Object::get(state, ObjectPropertyName(state.context()->staticStrings().calendar), calendarLike);
    return TemporalCalendar::toTemporalCalendarWithISODefault(state, calendarLike);
}

ValueVector TemporalCalendar::calendarFields(ExecutionState& state, const Value& calendar, const ValueVector& fieldNames)
{
    Value fields = Object::getMethod(state, calendar.asObject(), ObjectPropertyName(state.context()->staticStrings().lazyfields()));
    Value fieldsArray = Object::createArrayFromList(state, fieldNames);
    if (!fields.isUndefined()) {
        fieldsArray = Object::call(state, fields, calendar, 1, &fieldsArray);
    }

    return IteratorObject::iterableToListOfType(state, fieldsArray, new ASCIIString("String"));
}

Value TemporalCalendar::dateFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    ASSERT(calendar.isObject());
    ASSERT(fields.isObject());
    Value dateFromFields;
    calendar.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazydateFromFields()), dateFromFields);
    Value argv[2] = { fields, options };
    return Object::call(state, dateFromFields, calendar, 2, argv);
}

Value TemporalCalendar::ISODaysInYear(ExecutionState& state, const int year)
{
    return Value(TemporalCalendar::isIsoLeapYear(state, year) ? 366 : 355);
}

bool TemporalPlainDate::isValidISODate(ExecutionState& state, const int year, const int month, const int day)
{
    if (month < 1 || month > 12) {
        return false;
    }
    Value daysInMonth = TemporalCalendar::ISODaysInMonth(state, year, month);
    if (day < 1 || day > daysInMonth.asInt32()) {
        return false;
    }
    return true;
}

Value TemporalPlainDate::createTemporalDate(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const Value& calendar, Optional<Object*> newTarget)
{
    ASSERT(calendar.isObject());
    if (!TemporalPlainDate::isValidISODate(state, isoYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Not valid ISOdate");
    }

    if (!TemporalPlainDateTime::ISODateTimeWithinLimits(state, isoYear, isoMonth, isoDay, 12, 0, 0, 0, 0, 0)) {
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

TemporalPlainDate::TemporalPlainDate(ExecutionState& state, int isoYear, int isoMonth, int isoDay, Optional<Value> calendarLike)
    : TemporalPlainDate(state, state.context()->globalObject()->temporalPlainDatePrototype(), isoYear, isoMonth, isoDay, calendarLike)
{
}

TemporalPlainDate::TemporalPlainDate(ExecutionState& state, Object* proto, int isoYear, int isoMonth, int isoDay, Optional<Value> calendarLike)
    : Temporal(state, proto)
    , m_y(isoYear)
    , m_m(isoMonth)
    , m_d(isoDay)
    , m_calendar(calendarLike.value())
{
}

Value TemporalPlainDate::toTemporalDate(ExecutionState& state, const Value& item, Optional<Object*> options)
{
    if (!options.hasValue()) {
        options = new Object(state, Object::PrototypeIsNull);
    }

    ASSERT(options->isObject());

    if (item.isObject()) {
        Temporal* tItem = item.asObject()->asTemporalObject();
        if (tItem->isTemporalPlainDateObject()) {
            return item;
        }

        if (tItem->isTemporalZonedDateTimeObject()) {
            Value instant = TemporalInstant::createTemporalInstant(state, tItem->asTemporalZonedDateTimeObject()->getNanoseconds());
            Value plainDateTime = TemporalTimeZone::builtinTimeZoneGetPlainDateTimeFor(state, tItem->asTemporalZonedDateTimeObject()->getTimeZone(), instant, tItem->asTemporalZonedDateTimeObject()->getCalendar());
            return TemporalPlainDate::createTemporalDate(state, plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getYear(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getMonth(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getDay(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getCalendar());
        }

        if (tItem->isTemporalPlainDateTimeObject()) {
            TemporalPlainDateTime* tmp = tItem->asTemporalPlainDateTimeObject();
            return TemporalPlainDate::createTemporalDate(state, tmp->getYear(), tmp->getMonth(), tmp->getDay(), Value("iso8601"), new Object(state));
        }

        Value calendar = TemporalCalendar::getTemporalCalendarWithISODefault(state, item);
        ValueVector requiredFields{
            Value(new ASCIIString("day")),
            Value(new ASCIIString("month")),
            Value(new ASCIIString("monthCode")),
            Value(new ASCIIString("year"))
        };
        ValueVector fieldNames = TemporalCalendar::calendarFields(state, calendar, ValueVector());
        Value fields = Temporal::prepareTemporalFields(state, item, fieldNames, ValueVector());
        return TemporalCalendar::dateFromFields(state, calendar, fields, options.value());
    }

    Temporal::toTemporalOverflow(state, options.value());
    String* string = item.toString(state);
    auto result = TemporalObject::parseTemporalDateString(state, string->toNonGCUTF8StringData());
    ASSERT(TemporalPlainDate::isValidISODate(state, Value(result.at("year").c_str()).asInt32(), Value(result.at("month").c_str()).asInt32(), Value(result.at("day").c_str()).asInt32()));

    Value calendar = TemporalCalendar::toTemporalCalendarWithISODefault(state, Value(result.at("calendar").c_str()));

    return TemporalPlainDate::createTemporalDate(state, Value(result.at("year").c_str()).asInt32(), Value(result.at("month").c_str()).asInt32(), Value(result.at("day").c_str()).asInt32(), Value(result.at("calendar").c_str()), new Object(state));
}

std::map<std::string, int> TemporalPlainDate::balanceISODate(ExecutionState& state, int year, int month, int day)
{
    Value epochDays = DateObject::makeDay(state, Value(year), Value(month), Value(day));

    ASSERT(std::isfinite(epochDays.asNumber()));

    time64_t ms = DateObject::makeDate(state, epochDays, Value(0)).asUInt32();

    return TemporalPlainDate::createISODateRecord(state, DateObject::yearFromTime(ms), DateObject::monthFromTime(ms) + 1, DateObject::dateFromTime(ms));
}
std::map<std::string, int> TemporalPlainDate::createISODateRecord(ExecutionState& state, const int year, const int month, const int day)
{
    ASSERT(TemporalPlainDate::isValidISODate(state, year, month, day));
    return { { "year", year }, { "month", month }, { "day", day } };
}

TemporalPlainDateTime::TemporalPlainDateTime(ExecutionState& state)
    : TemporalPlainDateTime(state, state.context()->globalObject()->objectPrototype(), 0, 0, 0, 0, 0, 0, 0, 0, 0)
{
}

TemporalPlainDateTime::TemporalPlainDateTime(ExecutionState& state, Object* proto, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
    : Temporal(state, proto)
{
    this->m_year = year;
    this->m_month = month;
    this->m_day = day;
    this->m_hour = hour;
    this->m_minute = minute;
    this->m_second = second;
    this->m_millisecond = millisecond;
    this->m_microsecond = microsecond;
    this->m_nanosecond = nanosecond;
}

Value TemporalPlainDateTime::getEpochFromISOParts(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    ASSERT(TemporalPlainDate::isValidISODate(state, year, month, day));
    ASSERT(TemporalPlainTime::isValidTime(state, hour, minute, second, millisecond, microsecond, nanosecond));

    Value date = DateObject::makeDay(state, Value(year), Value(month), Value(day));
    Value time = DateObject::makeTime(state, Value(hour), Value(minute), Value(second), Value(millisecond));
    Value ms = DateObject::makeDate(state, date, time);
    ASSERT(std::isfinite(ms.asNumber()));

    return Value(ms.toLength(state));
}

bool TemporalPlainDateTime::ISODateTimeWithinLimits(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    time64_t ms = TemporalPlainDateTime::getEpochFromISOParts(state, year, month, day, hour, minute, second, millisecond, microsecond, nanosecond).toLength(state);
    return IS_IN_TIME_RANGE(ms);
}

std::map<std::string, int> TemporalPlainDateTime::balanceISODateTime(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    auto balancedTime = TemporalPlainTime::balanceTime(state, hour, minute, second, millisecond, microsecond, nanosecond);
    auto balancedDate = TemporalPlainDate::balanceISODate(state, year, month, day + balancedTime["Days"]);
    balancedDate.insert(balancedTime.begin(), balancedTime.end());
    return balancedDate;
}

Value TemporalPlainDateTime::createTemporalDateTime(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond, const Value& calendar, Optional<Object*> newTarget)
{
    ASSERT(calendar.isObject());
    if (!TemporalPlainDate::isValidISODate(state, isoYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid Date");
    }

    if (!TemporalPlainTime::isValidTime(state, hour, minute, second, millisecond, microsecond, millisecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid Time");
    }

    if (!TemporalPlainDateTime::ISODateTimeWithinLimits(state, isoYear, isoMonth, isoDay, hour, minute, second, millisecond, microsecond, millisecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "ISODateTime is out of limits");
    }

    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->temporalPlainDateTimePrototype();
    });
    Object* object = new TemporalPlainDateTime(state, proto, isoYear, isoMonth, isoDay, hour, minute, second, millisecond, microsecond, millisecond);
    return Value(object);
}

std::map<std::string, std::string> TemporalPlainDateTime::interpretTemporalDateTimeFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    return {};
}

Value TemporalPlainDateTime::toTemporalDateTime(ExecutionState& state, const Value& item, const Value& options)
{
    ASSERT(options.isObject() || options.isUndefined());

    Value calendar;
    std::map<std::string, std::string> result = {};

    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainDateTimeObject()) {
            return item;
        } else if (item.asObject()->isTemporalZonedDateTimeObject()) {
            Value instant = TemporalInstant::createTemporalInstant(state, item.asObject()->asTemporalZonedDateTimeObject()->getNanoseconds());
            return TemporalTimeZone::builtinTimeZoneGetPlainDateTimeFor(state, item.asObject()->asTemporalZonedDateTimeObject()->getTimeZone(), instant, item.asObject()->asTemporalZonedDateTimeObject()->getCalendar());
        } else if (item.asObject()->isTemporalPlainDateObject()) {
            return TemporalPlainDateTime::createTemporalDateTime(state, item.asObject()->asTemporalPlainDateObject()->year(), item.asObject()->asTemporalPlainDateObject()->month(), item.asObject()->asTemporalPlainDateObject()->day(), 0, 0, 0, 0, 0, 0, item.asObject()->asTemporalPlainDateObject()->calendar(), new Object(state));
        }

        calendar = TemporalCalendar::getTemporalCalendarWithISODefault(state, item);
        ValueVector values = { Value(new ASCIIString("day")),
                               Value(new ASCIIString("hour")),
                               Value(new ASCIIString("microsecond")),
                               Value(new ASCIIString("millisecond")),
                               Value(new ASCIIString("minute")),
                               Value(new ASCIIString("month")),
                               Value(new ASCIIString("monthCode")),
                               Value(new ASCIIString("nanosecond")),
                               Value(new ASCIIString("second")),
                               Value(new ASCIIString("year")) };
        ValueVector fieldNames = TemporalCalendar::calendarFields(state, calendar, values);
        Value fields = Temporal::prepareTemporalFields(state, item, fieldNames, ValueVector());
        result = TemporalPlainDateTime::interpretTemporalDateTimeFields(state, calendar, fields, options);
    } else {
        Temporal::toTemporalOverflow(state, options);
        String* string = item.toString(state);
        result = TemporalObject::parseTemporalDateTimeString(state, string);
        ASSERT(TemporalPlainDate::isValidISODate(state, Value(result["Year"].c_str()).asInt32(), Value(result["Month"].c_str()).asInt32(), Value(result["Day"].c_str()).asInt32()));
        ASSERT(TemporalPlainTime::isValidTime(state, Value(result["Hour"].c_str()).asInt32(), Value(result["Minute"].c_str()).asInt32(), Value(result["Second"].c_str()).asInt32(), Value(result["Millisecond"].c_str()).asInt32(), Value(result["Microsecond"].c_str()).asInt32(), Value(result["Nanosecond"].c_str()).asInt32()));
        calendar = TemporalCalendar::toTemporalCalendarWithISODefault(state, Value(result["Calendar"].c_str()));
    }
    return TemporalPlainDateTime::createTemporalDateTime(state, Value(result["Year"].c_str()).asInt32(), Value(result["Month"].c_str()).asInt32(), Value(result["Day"].c_str()).asInt32(), Value(result["Hour"].c_str()).asInt32(), Value(result["Minute"].c_str()).asInt32(), Value(result["Second"].c_str()).asInt32(), Value(result["Millisecond"].c_str()).asInt32(), Value(result["Microsecond"].c_str()).asInt32(), Value(result["Nanosecond"].c_str()).asInt32(), calendar, new Object(state));
}

TemporalZonedDateTime::TemporalZonedDateTime(ExecutionState& state)
    : TemporalZonedDateTime(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalZonedDateTime::TemporalZonedDateTime(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

TemporalInstant::TemporalInstant(ExecutionState& state)
    : TemporalInstant(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalInstant::TemporalInstant(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalInstant::createTemporalInstant(ExecutionState& state, const Value& epochNanoseconds, Optional<Object*> newTarget)
{
    ASSERT(epochNanoseconds.isBigInt());
    ASSERT(TemporalInstant::isValidEpochNanoseconds(epochNanoseconds));

    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->setPrototype();
    });
    auto* object = new TemporalInstant(state, proto);
    object->setNanoseconds(epochNanoseconds.asBigInt());

    return object;
}

bool TemporalInstant::isValidEpochNanoseconds(const Value& epochNanoseconds)
{
    ASSERT(epochNanoseconds.isBigInt());
    /* Maximum possible and minimum possible Nanoseconds */
    BigInt minEpoch = BigInt(new ASCIIString("-8640000000000000000000"));
    BigInt maxEpoch = BigInt(new ASCIIString("8640000000000000000000"));
    return epochNanoseconds.asBigInt()->greaterThanEqual(&minEpoch) && epochNanoseconds.asBigInt()->lessThanEqual(&maxEpoch);
}

TemporalTimeZone::TemporalTimeZone(ExecutionState& state)
    : TemporalTimeZone(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalTimeZone::TemporalTimeZone(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalTimeZone::builtinTimeZoneGetPlainDateTimeFor(ExecutionState& state, const Value& timeZone, const Value& instant, const Value& calendar)
{
    ASSERT(instant.asObject()->isTemporalInstantObject());
    Value offsetNanoseconds = TemporalTimeZone::getOffsetNanosecondsFor(state, timeZone, instant);
    auto result = TemporalTimeZone::getISOPartsFromEpoch(state, Value(instant.asObject()->asTemporalInstantObject()->getNanoseconds()));
    result = TemporalPlainDateTime::balanceISODateTime(state, result["Year"], result["Month"], result["Day"], result["Hour"], result["Minute"], result["Second"], result["Millisecond"], result["Microsecond"], result["Nanosecond"]);

    return TemporalPlainDateTime::createTemporalDateTime(state, result["Year"], result["Month"], result["Day"], result["Hour"], result["Minute"], result["Second"], result["Millisecond"], result["Microsecond"], result["Nanosecond"], calendar, new Object(state));
}

Value TemporalTimeZone::getOffsetNanosecondsFor(ExecutionState& state, const Value& timeZone, const Value& instant)
{
    Value getOffsetNanosecondsFor = Object::getMethod(state, timeZone, ObjectPropertyName(state.context()->staticStrings().lazygetOffsetNanosecondsFor()));
    Value offsetNanoseconds = Object::call(state, getOffsetNanosecondsFor, Value(), 1, const_cast<Value*>(&instant));
    if (!offsetNanoseconds.isNumber()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "offsetNanoseconds is not a Number");
    }

    if (!offsetNanoseconds.isInteger(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "offsetNanoseconds is not an Integer");
    }

    BigInt maxEpoch = BigInt(new ASCIIString("86400000000000"));
    if (offsetNanoseconds.asBigInt()->isNegative() ? offsetNanoseconds.asBigInt()->negativeValue(state)->greaterThan(&maxEpoch) : offsetNanoseconds.asBigInt()->greaterThan(&maxEpoch)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "offsetNanoseconds is out of range");
    }

    return offsetNanoseconds;
}

std::map<std::string, int> TemporalTimeZone::getISOPartsFromEpoch(ExecutionState& state, const Value& epochNanoseconds)
{
    ASSERT(TemporalInstant::isValidEpochNanoseconds(epochNanoseconds));
    BigInt ns = BigInt(new ASCIIString("1000000"));
    time64_t remainderNs = epochNanoseconds.asBigInt()->remainder(state, &ns)->toInt64();
    time64_t epochMilliseconds = epochNanoseconds.asBigInt()->subtraction(state, new BigInt(remainderNs))->remainder(state, &ns)->toInt64();
    std::map<std::string, int> result;

    result["Year"] = DateObject::yearFromTime(epochMilliseconds);
    result["Month"] = DateObject::monthFromTime(epochMilliseconds);
    result["Day"] = DateObject::dateFromTime(epochMilliseconds);
    result["Hour"] = DateObject::hourFromTime(epochMilliseconds);
    result["Minute"] = DateObject::minFromTime(epochMilliseconds);
    result["Second"] = DateObject::secFromTime(epochMilliseconds);
    result["Millisecond"] = DateObject::msFromTime(epochMilliseconds);
    result["Microsecond"] = (int)std::floor(remainderNs / 1000) % 1000;
    result["Nanosecond"] = remainderNs % 1000;

    return result;
}

TemporalPlainYearMonth::TemporalPlainYearMonth(ExecutionState& state)
    : TemporalPlainYearMonth(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainYearMonth::TemporalPlainYearMonth(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

std::map<std::string, int> TemporalPlainYearMonth::balanceISOYearMonth(ExecutionState& state, int year, int month)
{
    std::map<std::string, int> result;
    result["Year"] = year + (int)std::floor((month - 1) / 12);
    result["Month"] = (month - 1) % 13;
    return result;
}
} // namespace Escargot

#endif
