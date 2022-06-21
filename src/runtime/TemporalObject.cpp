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


std::string TemporalObject::getNNumberFromString(std::string& isoString, const int n, unsigned int& index)
{
    std::string retVal;
    if (!TemporalObject::isNumber(isoString.substr(index, n))) {
        return "";
    }
    retVal = isoString.substr(index, n);
    index += n;
    return retVal;
}

std::map<std::string, int> TemporalObject::getSeconds(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    int counter = 1;
    while (std::isdigit(isoString[counter++]))
        ;
    if (counter == 1 || counter > 9) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
    }

    std::string tmp = isoString.substr(1, counter - 1);

    index += counter;

    for (int i = counter; i <= 9; ++i) {
        tmp += '0';
    }

    return { { "milliSecond", std::stoi(tmp.substr(0, 3)) },
             { "microSecond", std::stoi(tmp.substr(3, 3)) },
             { "nanoSecond", std::stoi(tmp.substr(6, 3)) } };
}

void TemporalObject::offset(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    ++index;
    if (!(((isoString[index] == '0' || isoString[index] == '1') && isoString[index + 1] >= '0' && isoString[index + 1] <= '9') || (isoString[index] == '2' && isoString[index + 1] >= 0 && isoString[index + 1] <= '3'))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
    }

    index += 2;
    int counter = 0;
    while (isoString[index] == ':') {
        if (isoString[index + 1] >= '0' && isoString[index + 1] <= '5' && std::isdigit(isoString[index + 2])) {
            index += 3;
            counter++;
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
        }
    }

    if (isoString[index] == '.' || isoString[index] == ',') {
        if (counter != 2) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
        }
        TemporalObject::getSeconds(state, isoString, index);
    }
}

std::string TemporalObject::tzComponent(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    std::string timeZoneID;
    unsigned int i = 0;
    if (isoString[index] == '\\') {
        index++;
        if (isoString[index] == '.') {
            for (i = index + 2; i < 14; ++i) {
                if (!(isoString[i] == '-' || isoString[i] == '_' || isoString[i] == '.' || std::isalpha(isoString[i]))) {
                    break;
                }
            }
            if (!(isoString[index + 1] != '.' && (isoString[index + 1] == '-' || isoString[index + 1] == '_' || std::isalpha(isoString[index + 1]))) || (isoString[index + 1] == '.' && i == 2)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
            }
        } else if (isoString[index] == '_' || std::isalpha(isoString[index])) {
            for (i = index + 1; i < 13; ++i) {
                if (!(isoString[i] == '-' || isoString[i] == '_' || isoString[i] == '.' || std::isalpha(isoString[i]))) {
                    break;
                }
            }
        }
    }

    timeZoneID = isoString.substr(index, i - index);

    if (i != 0) {
        index = i;
    }

    return timeZoneID;
}

TemporalObject::DateTime TemporalObject::parseValidIso8601String(ExecutionState& state, std::string isoString)
{
    DateTime dateTime = { 0, 1, 1, 0, 0, 0, 0, 0, 0, "" };

    unsigned int index = 0;

    bool monthDay = false;

    bool end = false;

    if (isoString[index] != 'T') {
        // Date
        if (isoString.rfind("−", index) == 0 && TemporalObject::isNumber(isoString.substr(3 + index, 4))) {
            dateTime.year = std::stoi("-" + isoString.substr(3 + index, 4));
            index += 7;
        } else if (TemporalObject::isNumber(isoString.substr(index, 4))) {
            dateTime.year = std::stoi(isoString.substr(0, 4));
            index += 4;
        } else if (isoString.rfind("--", 0) == 0) {
            monthDay = true;
            index += 2;
        }

        if (isoString[index] == '-') {
            ++index;
        }

        if ((isoString[index] == '0' && std::isdigit(isoString[index + 1])) || (isoString[index] == '1' && isoString[index + 1] >= '0' && isoString[index + 1] <= '2')) {
            dateTime.month = std::stoi(isoString.substr(index, 2));
            index += 2;
            end = index == isoString.length();
        }

        if (isoString[index] != '-' && monthDay) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
        } else if (isoString[index] == '-') {
            ++index;
        }

        if ((isoString[index] >= '0' && isoString[index] <= '2' && std::isdigit(isoString[index + 1])) || (isoString[index] == '3' && (isoString[index + 1] == '0' || isoString[index + 1] == '1'))) {
            dateTime.day = std::stoi(isoString.substr(index, 2));
            index += 2;
            if (monthDay) {
                if (index != isoString.length()) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
                }
                end = true;
            }
        } else if (!end) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
        }
    }

    if (!end) {
        // Time
        if (isoString[index] == 'T' || isoString.rfind("\\s", index) == 0) {
            ++index;
            for (; isoString[index] == 's'; ++index)
                ;

            dateTime.hour = std::stoi(TemporalObject::getNNumberFromString(isoString, 2, index));

            if (isoString[index] == ':') {
                ++index;
            }

            dateTime.minute = std::stoi(TemporalObject::getNNumberFromString(isoString, 2, index));

            if (isoString[index] == ':') {
                ++index;
            }

            dateTime.second = std::stoi(TemporalObject::getNNumberFromString(isoString, 2, index));

            if (isoString[index] == '.' || isoString[index] == ',') {
                std::map<std::string, int> seconds;
                seconds = TemporalObject::getSeconds(state, isoString, index);
                dateTime.millisecond = seconds["milliSecond"];
                dateTime.microsecond = seconds["microSecond"];
                dateTime.nanosecond = seconds["NanoSecond"];
            }
        }

        // Offset
        if (isoString[index] == 'z' || isoString[index] == 'Z') {
            ++index;
        } else if (isoString[index] == '+' || isoString[index] == '-' || isoString.rfind("−", index) == 0) {
            TemporalObject::offset(state, isoString, index);
        }

        // TimeZone
        if (isoString[index] != 'E') {
            while (TemporalObject::tzComponent(state, isoString, index).length() != 0)
                ;
        } else if (isoString.rfind("Etc/GMT", index) == 0) {
            isoString = isoString.substr(index + 7);
            if (isoString[index] == '+' || isoString[index] == '-') {
                if (!std::isdigit(isoString[index + 1])) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
                }

                if (isNumber(isoString.substr(index + 1, 2))) {
                    index += 3;
                }
                index += 2;
            }
        } else if (isoString[index] == '+' || isoString[index] == '-' || isoString.rfind("−", index) == 0) {
            TemporalObject::offset(state, isoString, index);
        }

        // Calendar
        if (isoString.rfind("[u-ca=") == 0) {
            index += 6;
            for (int i = 0; isoString[index] != ']'; ++i) {
                // Check if current char is a number or
                if (std::isalpha(isoString[index]) || std::isdigit(isoString[index])) {
                    dateTime.calendar += isoString[index];
                    ++index;
                }

                // A calendar can't be longer than 8 character
                if (i == 8) {
                    if (isoString[index] != '-' && isoString[index] != ']') {
                        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
                    }
                    i = 0;
                }

                // A calendar must be at least 3 character
                if (i < 3 && (isoString[index] == '-' || isoString[index] == ']')) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
                }
            }
            if (isoString[index] == ']') {
                index++;
            }
        }
    }
    if (isoString.length() != index) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
    }

    if (!TemporalPlainDate::isValidISODate(state, dateTime.year, dateTime.month, dateTime.day)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO date");
    }

    if (!TemporalPlainTime::isValidTime(state, dateTime.hour, dateTime.minute, dateTime.second, dateTime.millisecond, dateTime.microsecond, dateTime.nanosecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO time");
    }

    return dateTime;
}

TemporalObject::DateTime TemporalObject::parseTemporalDateString(ExecutionState& state, const std::string& isoString)
{
    return TemporalObject::parseTemporalDateTimeString(state, isoString);
}

TemporalObject::DateTime TemporalObject::parseTemporalDateTimeString(ExecutionState& state, const std::string& isoString)
{
    if (isoString.find('z') != std::string::npos || isoString.find('Z') != std::string::npos) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "isoString contains UTCDesignator");
    }
    return TemporalObject::parseValidIso8601String(state, isoString);
}

TemporalPlainTime::TemporalPlainTime(ExecutionState& state)
    : TemporalPlainTime(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainTime::TemporalPlainTime(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
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
        return constructorRealm->globalObject()->temporalCalendarPrototype();
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
    TemporalObject::DateTime time = TemporalObject::parseValidIso8601String(state, isoString.asString()->toNonGCUTF8StringData());
    return time.calendar.empty() ? Value(new ASCIIString("iso8601")) : Value(time.calendar.c_str());
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

Value TemporalCalendar::getterHelper(ExecutionState& state, const Value& callee, Object* thisValue, Value* argv)
{
    Value result = Object::call(state, callee, thisValue, 1, argv);

    if (result.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Undefined");
    }

    return result;
}

Value TemporalCalendar::calendarYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendar::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyYear()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarMonth(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendar::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyMonth()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarMonthCode(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendar::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazymonthCode()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDay(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendar::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyDay()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDayOfWeek(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value dayOfWeek = calendar->get(state, state.context()->staticStrings().lazydayOfWeek()).value(state, calendar);
    return Object::call(state, dayOfWeek, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDayOfYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value dayOfYear = calendar->get(state, state.context()->staticStrings().lazydayOfYear()).value(state, calendar);
    return Object::call(state, dayOfYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarWeekOfYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value weekOfYear = calendar->get(state, state.context()->staticStrings().lazyweekOfYear()).value(state, calendar);
    return Object::call(state, weekOfYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDaysInWeek(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInWeek = calendar->get(state, state.context()->staticStrings().lazydaysInWeek()).value(state, calendar);
    return Object::call(state, daysInWeek, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDaysInMonth(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInMonth = calendar->get(state, state.context()->staticStrings().lazydaysInMonth()).value(state, calendar);
    return Object::call(state, daysInMonth, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarDaysInYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInYear = calendar->get(state, state.context()->staticStrings().lazydaysInYear()).value(state, calendar);
    return Object::call(state, daysInYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarMonthsInYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value monthsInYear = calendar->get(state, state.context()->staticStrings().lazymonthsInYear()).value(state, calendar);
    return Object::call(state, monthsInYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendar::calendarInLeapYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value inLeapYear = calendar->get(state, state.context()->staticStrings().lazyinLeapYear()).value(state, calendar);
    return Object::call(state, inLeapYear, calendar, 1, const_cast<Value*>(&dateLike));
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

int TemporalCalendar::toISOWeekOfYear(ExecutionState& state, const int year, const int month, const int day)
{
    Value epochDays = DateObject::makeDay(state, Value(year), Value(month - 1), Value(day));
    int dayOfYear = TemporalCalendar::dayOfYear(state, epochDays);
    int dayOfWeek = DateObject::weekDay(DateObject::makeDate(state, epochDays, Value(0)).toUint32(state));
    int dayOfWeekOfJanFirst = DateObject::weekDay(DateObject::makeDate(state, DateObject::makeDay(state, Value(year), Value(0), Value(1)), Value(0)).toUint32(state));
    int weekNum = (dayOfYear + 6) / 7;
    if (dayOfWeek < dayOfWeekOfJanFirst) {
        ++weekNum;
    }

    return weekNum;
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
    auto result = TemporalObject::parseTemporalDateString(state, item.toString(state)->toNonGCUTF8StringData());
    ASSERT(TemporalPlainDate::isValidISODate(state, result.year, result.month, result.day));

    return TemporalPlainDate::createTemporalDate(state, result.year, result.month, result.day, TemporalCalendar::toTemporalCalendarWithISODefault(state, Value(result.calendar.c_str())), new Object(state));
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
        return TemporalPlainDateTime::createTemporalDateTime(state, Value(result["Year"].c_str()).asInt32(), Value(result["Month"].c_str()).asInt32(), Value(result["Day"].c_str()).asInt32(), Value(result["Hour"].c_str()).asInt32(), Value(result["Minute"].c_str()).asInt32(), Value(result["Second"].c_str()).asInt32(), Value(result["Millisecond"].c_str()).asInt32(), Value(result["Microsecond"].c_str()).asInt32(), Value(result["Nanosecond"].c_str()).asInt32(), calendar, new Object(state));
    }
    Temporal::toTemporalOverflow(state, options);
    auto tmp = TemporalObject::parseTemporalDateTimeString(state, item.toString(state)->toNonGCUTF8StringData());
    ASSERT(TemporalPlainDate::isValidISODate(state, tmp.year, tmp.month, tmp.day));
    ASSERT(TemporalPlainTime::isValidTime(state, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond));

    return TemporalPlainDateTime::createTemporalDateTime(state, tmp.year, tmp.month, tmp.day, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond, TemporalCalendar::toTemporalCalendarWithISODefault(state, Value(tmp.calendar.c_str())), new Object(state));
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
