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

#include <utility>
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

std::map<TemporalObject::DateTimeUnits, int> TemporalObject::getSeconds(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    int counter = index + 1;
    while (std::isdigit(isoString[counter++]))
        ;
    counter -= (index + 2);

    if (counter > 9) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
    }

    std::string tmp = isoString.substr(index + 1, counter);

    index += counter;

    for (int i = counter; i < 9; ++i) {
        tmp += '0';
    }

    return { { MILLISECOND_UNIT, std::stoi(tmp.substr(0, 3)) },
             { MICROSECOND_UNIT, std::stoi(tmp.substr(3, 3)) },
             { NANOSECOND_UNIT, std::stoi(tmp.substr(6, 3)) } };
}

std::string TemporalObject::offset(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    std::string result;

    if (isoString.rfind("−", index) != std::string::npos) {
        result += "-";
        index += 2;
    } else if (isoString[index] == '-') {
        result += '-';
    } else {
        result += "+";
    }

    ++index;
    if (!(((isoString[index] == '0' || isoString[index] == '1') && isoString[index + 1] >= '0' && isoString[index + 1] <= '9') || (isoString[index] == '2' && isoString[index + 1] >= 0 && isoString[index + 1] <= '3'))) {
        return "";
    }

    result += isoString.substr(index, 2);

    index += 2;
    int counter = 0;
    while (index < isoString.length() && isoString[index] != '.') {
        if (isoString[index] == ':') {
            index++;
        }
        result += ':';
        if (isoString[index] >= '0' && isoString[index] <= '5' && std::isdigit(isoString[index + 1])) {
            result += isoString.substr(index, 2);
            index += 2;
            counter++;
            if (counter == 3) {
                return "";
            }
        } else if (isoString[index] == '[') {
            result = result.substr(0, result.length() - 1);
            break;
        } else {
            return "";
        }
    }

    if (isoString[index] == '.' || isoString[index] == ',') {
        if (counter != 2) {
            return "";
        }
        unsigned int start = index;
        TemporalObject::getSeconds(state, isoString, index);
        result += isoString.substr(start, index - start + 1);
        for (unsigned int i = 0; i < 9 - (index - start); i++) {
            result += '0';
        }
    }

    if (result.length() == 3) {
        result += ":00";
    }

    return result;
}

std::string TemporalObject::tzComponent(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    std::string timeZoneID;
    unsigned int i = 0;
    index++;
    if (isoString[index] == '.') {
        for (i = index + 2; i < index + 14; ++i) {
            if (i > isoString.length() || !(isoString[i] == '-' || isoString[i] == '_' || isoString[i] == '.' || std::isalpha(isoString[i]))) {
                break;
            }
        }

        if (!(isoString[index + 1] != '.' && (isoString[index + 1] == '-' || isoString[index + 1] == '_' || std::isalpha(isoString[index + 1]))) || (isoString[index + 1] == '.' && i == 2)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid TimeZone");
        }
    } else if (isoString[index] == '_' || std::isalpha(isoString[index])) {
        for (i = index + 1; i < index + 13; ++i) {
            if (!(isoString[i] == '-' || isoString[i] == '_' || isoString[i] == '.' || std::isalpha(isoString[i]))) {
                break;
            }
        }
    }
    timeZoneID = isoString.substr(index, i - index);

    if (i != 0) {
        index = i;
    }

    return timeZoneID;
}

TemporalObject::DateTime TemporalObject::parseValidIso8601String(ExecutionState& state, std::string isoString, const bool parseTimeZone = false)
{
    DateTime dateTime = { 0, 1, 1, 0, 0, 0, 0, 0, 0, new ASCIIString(), new TimeZone(false, String::emptyString, String::emptyString) };

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
        } else if (!end && !(isoString[index] == '+' || isoString[index] == '-' || isoString.rfind("−", index) == 0)) {
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
                dateTime.second = std::stoi(TemporalObject::getNNumberFromString(isoString, 2, index));
            }

            if (TemporalObject::isNumber(isoString.substr(index, 2))) {
                dateTime.second = std::stoi(TemporalObject::getNNumberFromString(isoString, 2, index));
            }

            if (isoString[index] == '.' || isoString[index] == ',') {
                std::map<TemporalObject::DateTimeUnits, int> seconds;
                seconds = TemporalObject::getSeconds(state, isoString, index);
                dateTime.millisecond = seconds[MILLISECOND_UNIT];
                dateTime.microsecond = seconds[MICROSECOND_UNIT];
                dateTime.nanosecond = seconds[NANOSECOND_UNIT];
            }
        }

        auto timeZoneOffset = TemporalObject::parseTimeZoneOffset(state, isoString, index);

        if (parseTimeZone && timeZoneOffset.offsetString && timeZoneOffset.name) {
            dateTime.tz = new TimeZone{ timeZoneOffset.z, timeZoneOffset.offsetString, timeZoneOffset.name };
        } else if (!(timeZoneOffset.offsetString && timeZoneOffset.name)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
        }

        // Calendar
        if (isoString.rfind("[u-ca=", index) != std::string::npos) {
            index += 6;
            std::string calendar;
            for (int i = 0; isoString[index] != ']'; ++i) {
                // Check if current char is a number or
                if (std::isalpha(isoString[index]) || std::isdigit(isoString[index])) {
                    calendar += isoString[index];
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
                dateTime.calendar = new ASCIIString(calendar.c_str());
                index++;
            }
        }
    }
    if (isoString.length() != index) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO string");
    }

    if (!TemporalPlainDateObject::isValidISODate(state, dateTime.year, dateTime.month, dateTime.day)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO date");
    }

    if (!TemporalPlainTimeObject::isValidTime(state, dateTime.hour, dateTime.minute, dateTime.second, dateTime.millisecond, dateTime.microsecond, dateTime.nanosecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid ISO time");
    }

    if (parseTimeZone) {
        if (dateTime.tz->z) {
            dateTime.tz->offsetString = String::emptyString;
        }
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

std::map<TemporalObject::DateTimeUnits, int> TemporalObject::parseTemporalDurationString(ExecutionState& state, const std::string& isoString)
{
    int sign = 1;
    unsigned int index = 0;

    int years = -1;
    int months = -1;
    int weeks = -1;
    int days = -1;
    int hours = -1;
    int minutes = -1;
    int seconds = -1;
    int fHours = -1;
    int fMinutes = -1;
    int fSeconds = -1;

    if (isoString.length() < 3 || isoString[index + 1] != 'P') {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
    }

    if (isoString.rfind("−", index) == 0) {
        sign = -1;
        index += 4;

        if (isoString.length() < 5) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
        }
    } else {
        index += 2;
    }

    while (isoString[index] == '/') {
        index++;
        std::string value;
        unsigned int start = index;
        while (std::isdigit(isoString[index])) {
            index++;
        }

        value = isoString.substr(start, index - start);

        switch (isoString[index]) {
        case 'Y':
            years = std::stoi(value);
            break;
        case 'M':
            months = std::stoi(value);
            break;
        case 'W':
            weeks = std::stoi(value);
            break;
        case 'D':
            days = std::stoi(value);
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
        }
        index++;
    }

    if (isoString[index] == 'T') {
        std::string value;
        std::string fvalue;
        unsigned int start = index;
        while (std::isdigit(isoString[index])) {
            index++;
        }

        value = isoString.substr(start, index - start);

        if (isoString[index] == '.' || isoString[index] == ',') {
            start = index;
            while (std::isdigit(isoString[index])) {
                index++;
                if (index - start == 10) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
                }
            }
            fvalue = isoString.substr(start, index - start);
        }

        switch (isoString[index]) {
        case 'H':
            hours = std::stoi(value);
            if (!fvalue.empty()) {
                fHours = std::stoi(fvalue);
            }
            break;
        case 'M':
            minutes = std::stoi(value);
            if (!fvalue.empty()) {
                fMinutes = std::stoi(fvalue);
            }
            break;
        case 'S':
            seconds = std::stoi(value);
            if (!fvalue.empty()) {
                fSeconds = std::stoi(fvalue);
            }
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
        }
        index++;
    }

    int minutesMV;
    int secondsMV;
    int milliSecondsMV;

    if (fHours != -1) {
        if (minutes != -1 || fMinutes != -1 || seconds != -1 || fSeconds != -1) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
        }
        minutesMV = fHours / pow(10, trunc(log10(fHours)) + 1) * 60;
    } else {
        minutesMV = minutes;
    }

    if (fMinutes != -1) {
        if (seconds != -1 || fSeconds != -1) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration string");
        }
        secondsMV = fMinutes / pow(10, trunc(log10(fMinutes)) + 1) * 60;
    } else if (seconds != -1) {
        secondsMV = seconds;
    } else {
        secondsMV = (minutesMV % 1) * 60;
    }

    if (fSeconds != -1) {
        milliSecondsMV = fSeconds / pow(10, trunc(log10(fSeconds)) + 1) * 60;
    } else {
        milliSecondsMV = (secondsMV % 1) * 1000;
    }

    int microSecondsMV = (milliSecondsMV % 1) * 1000;
    int nanoSecondsMV = (microSecondsMV % 1) * 1000;

    return TemporalDurationObject::createDurationRecord(state, years * sign, months * sign, weeks * sign, days * sign, hours * sign, std::floor(minutesMV) * sign, std::floor(secondsMV) * sign, std::floor(milliSecondsMV) * sign, std::floor(microSecondsMV) * sign, std::floor(nanoSecondsMV) * sign);
}
TemporalObject::DateTime TemporalObject::parseTemporalYearMonthString(ExecutionState& state, const std::string& isoString)
{
    return TemporalObject::parseTemporalDateTimeString(state, isoString);
}

TemporalObject::DateTime TemporalObject::parseTemporalInstantString(ExecutionState& state, const std::string& isoString)
{
    auto result = parseValidIso8601String(state, isoString, true);

    if (result.tz->z) {
        result.tz->offsetString = new ASCIIString("+00:00");
    }

    if (!result.tz->offsetString || result.tz->offsetString->length() == 0) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "undefined offsetString");
    }

    return result;
}

TemporalObject::TimeZone TemporalObject::parseTemporalTimeZoneString(ExecutionState& state, const std::string& isoString)
{
    auto result = TimeZone(false, String::emptyString, String::emptyString);

    if (isoString == "UTC") {
        result.z = true;
        return result;
    }

    unsigned int index = 0;
    result = TemporalObject::parseTimeZoneOffset(state, const_cast<std::string&>(isoString), index);

    if (result.offsetString->length() == 0 && result.name->length() == 0 && !result.z) {
        result = *TemporalObject::parseValidIso8601String(state, isoString, true).tz;
    }

    if (result.z) {
        result.offsetString = String::emptyString;
    }

    if (!(result.offsetString && result.name) && result.z) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "bare date-time string is not a time zone");
    }

    return result;
}

TemporalObject::TimeZone TemporalObject::parseTimeZoneOffset(ExecutionState& state, std::string& isoString, unsigned int& index)
{
    // Offset
    auto tz = TimeZone(false, String::emptyString, String::emptyString);

    if (isoString[index] == 'z' || isoString[index] == 'Z') {
        tz.z = true;
        ++index;
    } else if (isoString[index] == '+' || isoString[index] == '-' || isoString.rfind("−", index) == index) {
        tz.offsetString = new ASCIIString(TemporalObject::offset(state, isoString, index).c_str());
        if (tz.offsetString->length() == 0) {
            return TemporalObject::TimeZone{ false, nullptr, nullptr };
        }
    }

    // TimeZone
    if (isoString[index] == '[' && isoString.rfind("[u-ca=", index) != index) {
        std::string tmp = TemporalObject::tzComponent(state, isoString, index);
        while (tmp.length() != 0) {
            tz.name = new ASCIIString(std::string(tz.name->toNonGCUTF8StringData()).append(tmp).c_str());
            if (isoString[index] == '/') {
                tmp = TemporalObject::tzComponent(state, isoString, index);
            }
            if (isoString[index] == ']') {
                tz.name = new ASCIIString(std::string(tz.name->toNonGCUTF8StringData()).append("/" + tmp).c_str());
                index++;
                break;
            }
        }
    } else if (isoString.rfind("Etc/GMT", index) == 0) {
        index += 7;
        if (isoString[index] == '+' || isoString[index] == '-') {
            if (!std::isdigit(isoString[index + 1])) {
                return TemporalObject::TimeZone{ false, nullptr, nullptr };
            }

            if (isNumber(isoString.substr(index + 1, 2))) {
                index += 3;
            }
            index += 2;
        }
    } else if (isoString[index] == '+' || isoString[index] == '-' || (index < isoString.length() && isoString.rfind("−", index) == 0)) {
        TemporalObject::offset(state, isoString, index);
    }

    return tz;
}

TemporalObject::DateTime TemporalObject::parseTemporalMonthDayString(ExecutionState& state, const std::string& isoString)
{
    return parseTemporalDateTimeString(state, isoString);
}

Value TemporalPlainTimeObject::createTemporalTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, Optional<Object*> newTarget)
{
    auto temporalPlainTime = new TemporalPlainTimeObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainTimePrototype());

    temporalPlainTime->setTime(hour, minute, second, millisecond, microsecond, nanosecond);
    temporalPlainTime->setCalendar(state, state.context()->staticStrings().lazyISO8601().string());
    return temporalPlainTime;
}

TemporalPlainTimeObject::TemporalPlainTimeObject(ExecutionState& state)
    : TemporalPlainTimeObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainTimeObject::TemporalPlainTimeObject(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

void TemporalPlainTimeObject::setTime(char h, char m, char s, short ms, short qs, short ns)
{
    this->m_hour = h;
    this->m_minute = m;
    this->m_second = s;
    this->m_millisecond = ms;
    this->m_microsecond = qs;
    this->m_nanosecond = ns;
}

void TemporalPlainTimeObject::setCalendar(ExecutionState& state, String* id)
{
    ASSERT(TemporalCalendarObject::isBuiltinCalendar(id));

    this->m_calendar = new TemporalCalendarObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalCalendarPrototype(), id);
}

bool TemporalPlainTimeObject::isValidTime(ExecutionState& state, const int h, const int m, const int s, const int ms, const int us, const int ns)
{
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59 || ms < 0 || ms > 999 || us < 0 || us > 999 || ns < 0 || ns > 999) {
        return false;
    }

    return true;
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainTimeObject::balanceTime(ExecutionState& state, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    std::map<TemporalObject::DateTimeUnits, int> result;

    result[TemporalObject::MICROSECOND_UNIT] = microsecond + (int)std::floor(nanosecond / 1000);
    result[TemporalObject::NANOSECOND_UNIT] = nanosecond % 1000;
    result[TemporalObject::MILLISECOND_UNIT] = millisecond + (int)std::floor(result[TemporalObject::MICROSECOND_UNIT] / 1000);
    result[TemporalObject::MICROSECOND_UNIT] %= 1000;
    result[TemporalObject::SECOND_UNIT] = second + (int)std::floor(result[TemporalObject::MILLISECOND_UNIT] / 1000);
    result[TemporalObject::MILLISECOND_UNIT] %= 1000;
    result[TemporalObject::MINUTE_UNIT] = minute + (int)std::floor(result[TemporalObject::SECOND_UNIT] / 60);
    result[TemporalObject::SECOND_UNIT] %= 60;
    result[TemporalObject::HOUR_UNIT] = hour + (int)std::floor(result[TemporalObject::MINUTE_UNIT] / 60);
    result[TemporalObject::MINUTE_UNIT] %= 60;
    result[TemporalObject::DAY_UNIT] = std::floor(result[TemporalObject::HOUR_UNIT] / 24);
    result[TemporalObject::HOUR_UNIT] %= 24;

    return result;
}

Value TemporalPlainTimeObject::toTemporalTime(ExecutionState& state, const Value& item, Value options)
{
    if (options.isUndefined()) {
        options = Value(state.context()->staticStrings().lazyConstrain().string());
    }

    ASSERT(options.asString()->equals(state.context()->staticStrings().lazyConstrain().string()) || options.asString()->equals(state.context()->staticStrings().lazyRejected().string()));

    std::map<TemporalObject::DateTimeUnits, int> result;

    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainTimeObject()) {
            return item;
        }

        if (item.asObject()->isTemporalZonedDateTimeObject()) {
            Value instant = TemporalInstantObject::createTemporalInstant(state, item.asObject()->asTemporalZonedDateTimeObject()->getNanoseconds());
            TemporalPlainDateTimeObject* plainDateTime = TemporalTimeZoneObject::builtinTimeZoneGetPlainDateTimeFor(state, item.asObject()->asTemporalZonedDateTimeObject()->getTimeZone(), instant, item.asObject()->asTemporalZonedDateTimeObject()->getCalendar()).asObject()->asTemporalPlainDateTimeObject();
            return TemporalPlainTimeObject::createTemporalTime(state, plainDateTime->getHour(), plainDateTime->getMinute(),
                                                               plainDateTime->getSecond(), plainDateTime->getMillisecond(),
                                                               plainDateTime->getMicrosecond(),
                                                               plainDateTime->getNanosecond(), nullptr);
        }

        if (item.asObject()->isTemporalPlainDateTimeObject()) {
            TemporalPlainDateTimeObject* plainDateTime = item.asObject()->asTemporalPlainDateTimeObject();
            return TemporalPlainTimeObject::createTemporalTime(state, plainDateTime->getHour(), plainDateTime->getMinute(),
                                                               plainDateTime->getSecond(), plainDateTime->getMillisecond(),
                                                               plainDateTime->getMicrosecond(),
                                                               plainDateTime->getNanosecond(), nullptr);
        }

        Value calendar = TemporalCalendarObject::getTemporalCalendarWithISODefault(state, item);

        if (!calendar.asObject()->asTemporalCalendarObject()->getIdentifier()->equals(state.context()->staticStrings().lazyISO8601().string())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Calendar is not ISO8601");
        }
        result = TemporalPlainTimeObject::toTemporalTimeRecord(state, item);
        result = TemporalPlainTimeObject::regulateTime(state, result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT], options);
        return TemporalPlainTimeObject::createTemporalTime(state, result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT], nullptr);
    }

    Temporal::toTemporalOverflow(state, options);
    auto tmp = TemporalObject::parseTemporalDateTimeString(state, item.asString()->toNonGCUTF8StringData());
    ASSERT(TemporalPlainTimeObject::isValidTime(state, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond));
    if (tmp.calendar->length() != 0 && !tmp.calendar->equals("iso8601")) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Calendar is not ISO8601");
    }

    return TemporalPlainTimeObject::createTemporalTime(state, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond, nullptr);
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainTimeObject::toTemporalTimeRecord(ExecutionState& state, const Value& temporalTimeLike)
{
    ASSERT(temporalTimeLike.isObject());
    std::map<TemporalObject::DateTimeUnits, int> result = { { TemporalObject::HOUR_UNIT, 0 }, { TemporalObject::MINUTE_UNIT, 0 }, { TemporalObject::SECOND_UNIT, 0 }, { TemporalObject::MILLISECOND_UNIT, 0 }, { TemporalObject::MICROSECOND_UNIT, 0 }, { TemporalObject::NANOSECOND_UNIT, 0 } };
    TemporalObject::DateTimeUnits temporalTimeLikeProp[6] = { TemporalObject::HOUR_UNIT, TemporalObject::MICROSECOND_UNIT, TemporalObject::MILLISECOND_UNIT, TemporalObject::MINUTE_UNIT, TemporalObject::NANOSECOND_UNIT, TemporalObject::SECOND_UNIT };

    bool any = false;
    for (auto i : temporalTimeLikeProp) {
        Value value = temporalTimeLike.asObject()->get(state, ObjectPropertyName(state, new ASCIIString(dateTimeUnitStrings[i]))).value(state, temporalTimeLike);
        any = !value.isUndefined();
        result[i] = value.asInt32();
    }
    if (!any) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "any is false");
    }

    return result;
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainTimeObject::regulateTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, const Value& overflow)
{
    ASSERT(overflow.asString()->equals(state.context()->staticStrings().lazyConstrain().string()) || overflow.asString()->equals(state.context()->staticStrings().lazyRejected().string()));

    if (overflow.asString()->equals(state.context()->staticStrings().lazyConstrain().string())) {
        return TemporalPlainTimeObject::constrainTime(state, hour, minute, second, millisecond, microsecond, nanosecond);
    }

    if (TemporalPlainTimeObject::isValidTime(state, hour, minute, second, millisecond, microsecond, nanosecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid time");
    }

    return { { TemporalObject::HOUR_UNIT, hour }, { TemporalObject::MINUTE_UNIT, minute }, { TemporalObject::SECOND_UNIT, second }, { TemporalObject::MILLISECOND_UNIT, millisecond }, { TemporalObject::MICROSECOND_UNIT, microsecond }, { TemporalObject::NANOSECOND_UNIT, nanosecond } };
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainTimeObject::constrainTime(ExecutionState& state, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond)
{
    return { { TemporalObject::HOUR_UNIT, std::max(0, std::min(hour, 23)) }, { TemporalObject::MINUTE_UNIT, std::max(0, std::min(minute, 59)) }, { TemporalObject::SECOND_UNIT, std::max(0, std::min(second, 59)) }, { TemporalObject::MILLISECOND_UNIT, std::max(0, std::min(millisecond, 999)) }, { TemporalObject::MICROSECOND_UNIT, std::max(0, std::min(microsecond, 999)) }, { TemporalObject::NANOSECOND_UNIT, std::max(0, std::min(nanosecond, 999)) } };
}

int TemporalPlainTimeObject::compareTemporalTime(ExecutionState& state, const int time1[6], const int time2[6])
{
    for (int i = 0; i < 6; ++i) {
        if (time1[i] > time2[i]) {
            return 1;
        }

        if (time1[i] < time2[i]) {
            return -1;
        }
    }
    return 0;
}
std::map<TemporalObject::DateTimeUnits, Value> TemporalPlainTimeObject::toPartialTime(ExecutionState& state, const Value& temporalTimeLike)
{
    ASSERT(temporalTimeLike.isObject());
    std::map<TemporalObject::DateTimeUnits, Value> result = { { TemporalObject::HOUR_UNIT, Value() }, { TemporalObject::MINUTE_UNIT, Value() }, { TemporalObject::SECOND_UNIT, Value() }, { TemporalObject::MILLISECOND_UNIT, Value() }, { TemporalObject::MICROSECOND_UNIT, Value() }, { TemporalObject::NANOSECOND_UNIT, Value() } };

    TemporalObject::DateTimeUnits temporalTimeLikeProp[6] = { TemporalObject::HOUR_UNIT, TemporalObject::MICROSECOND_UNIT, TemporalObject::MILLISECOND_UNIT, TemporalObject::MINUTE_UNIT, TemporalObject::NANOSECOND_UNIT, TemporalObject::SECOND_UNIT };

    bool any = false;
    for (auto i : temporalTimeLikeProp) {
        Value value = temporalTimeLike.asObject()->get(state, ObjectPropertyName(state, new ASCIIString(dateTimeUnitStrings[i]))).value(state, temporalTimeLike);
        if (!value.isUndefined()) {
            any = true;
            result[i] = value;
        }
    }

    if (!any) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "any is false");
    }

    return result;
}

bool TemporalCalendarObject::isBuiltinCalendar(String* id)
{
    return id->equals("iso8601");
}

TemporalCalendarObject::TemporalCalendarObject(ExecutionState& state)
    : TemporalCalendarObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalCalendarObject::TemporalCalendarObject(ExecutionState& state, Object* proto, String* identifier)
    : Temporal(state, proto)
    , m_identifier(identifier)
{
}

TemporalCalendarObject* TemporalCalendarObject::createTemporalCalendar(ExecutionState& state, String* id, Optional<Object*> newTarget)
{
    ASSERT(TemporalCalendarObject::isBuiltinCalendar(id));

    return new TemporalCalendarObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalCalendarPrototype(), id);
}

Value TemporalCalendarObject::getBuiltinCalendar(ExecutionState& state, String* id)
{
    if (!TemporalCalendarObject::isBuiltinCalendar(id)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::IsNotDefined);
    }
    Value argv[1] = { Value(id) };
    return Object::construct(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalCalendar(), 1, argv).toObject(state);
}

Value TemporalCalendarObject::getISO8601Calendar(ExecutionState& state)
{
    return TemporalCalendarObject::getBuiltinCalendar(state, state.context()->staticStrings().lazyISO8601().string());
}

Value TemporalCalendarObject::toTemporalCalendar(ExecutionState& state, const Value& calendar)
{
    if (calendar.isObject()) {
        auto* calendarObject = calendar.asObject()->asTemporalCalendarObject();

        if (calendarObject->internalSlot()->isTemporalObject()) {
            return Value(calendarObject->getIdentifier());
        }
        if (!calendarObject->hasProperty(state, state.context()->staticStrings().calendar)) {
            return calendar;
        }
    }

    String* identifier = calendar.asString();
    if (!TemporalCalendarObject::isBuiltinCalendar(identifier)) {
        identifier = TemporalCalendarObject::parseTemporalCalendarString(state, identifier).asString();
        if (!TemporalCalendarObject::isBuiltinCalendar(identifier)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::IsNotDefined);
        }
    }
    return Value(TemporalCalendarObject::createTemporalCalendar(state, identifier, nullptr));
}

Value TemporalCalendarObject::parseTemporalCalendarString(ExecutionState& state, const Value& isoString)
{
    ASSERT(isoString.isString());
    TemporalObject::DateTime time = TemporalObject::parseValidIso8601String(state, isoString.asString()->toNonGCUTF8StringData());
    return time.calendar->length() == 0 ? Value(state.context()->staticStrings().lazyISO8601().string()) : Value(time.calendar);
}

Value TemporalCalendarObject::ISODaysInMonth(ExecutionState& state, const int year, const int m)
{
    ASSERT(m > 0 && m < 13);

    if (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12) {
        return Value(31);
    }
    if (m == 4 || m == 6 || m == 9 || m == 11) {
        return Value(30);
    }
    if (TemporalCalendarObject::isIsoLeapYear(state, year)) {
        return Value(29);
    }
    return Value(28);
}

bool TemporalCalendarObject::isIsoLeapYear(ExecutionState& state, const int year)
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

std::string TemporalCalendarObject::buildISOMonthCode(ExecutionState& state, const int month)
{
    return "M" + std::string((month < 10 ? "0" : "") + char(48 + month));
}

int TemporalCalendarObject::ISOYear(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->year());
    return temporalObject.asObject()->asTemporalPlainDateObject()->year();
}

int TemporalCalendarObject::ISOMonth(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->month());
    return temporalObject.asObject()->asTemporalPlainDateObject()->month();
}

std::string TemporalCalendarObject::ISOMonthCode(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->month());
    return TemporalCalendarObject::buildISOMonthCode(state, temporalObject.asObject()->asTemporalPlainDateObject()->month());
}

int TemporalCalendarObject::ISODay(ExecutionState& state, const Value& temporalObject)
{
    ASSERT(temporalObject.asObject()->asTemporalPlainDateObject()->day());
    return temporalObject.asObject()->asTemporalPlainDateObject()->day();
}

/*12.1.6 toTemporalCalendarWithISODefault*/
Value TemporalCalendarObject::toTemporalCalendarWithISODefault(ExecutionState& state, const Value& calendar)
{
    if (calendar.isUndefined()) {
        return TemporalCalendarObject::getISO8601Calendar(state);
    }
    return TemporalCalendarObject::toTemporalCalendar(state, calendar);
}

Value TemporalCalendarObject::defaultMergeFields(ExecutionState& state, const Value& fields, const Value& additionalFields)
{
    auto& strings = state.context()->staticStrings();

    Object* merged = new Object(state);

    auto originalKeys = Object::enumerableOwnProperties(state, fields.asObject(), EnumerableOwnPropertiesType::Key);

    for (auto nextKey : originalKeys) {
        if (!nextKey.asString()->equals(strings.lazyMonth().string()) || !nextKey.asString()->equals(strings.lazymonthCode().string())) {
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
        if (!nextKey.asString()->equals(strings.lazyMonth().string()) || !nextKey.asString()->equals(strings.lazymonthCode().string())) {
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

Value TemporalCalendarObject::getTemporalCalendarWithISODefault(ExecutionState& state, const Value& item)
{
    Value calendarLike;
    item.asObject()->asTemporalObject()->Object::get(state, ObjectPropertyName(state.context()->staticStrings().calendar), calendarLike);
    return TemporalCalendarObject::toTemporalCalendarWithISODefault(state, calendarLike);
}

ValueVector TemporalCalendarObject::calendarFields(ExecutionState& state, const Value& calendar, const ValueVector& fieldNames)
{
    Value fields = Object::getMethod(state, calendar.asObject(), ObjectPropertyName(state.context()->staticStrings().lazyfields()));
    Value fieldsArray = Object::createArrayFromList(state, fieldNames);
    if (!fields.isUndefined()) {
        fieldsArray = Object::call(state, fields, calendar, 1, &fieldsArray);
    }

    return IteratorObject::iterableToListOfType(state, fieldsArray, state.context()->staticStrings().String.string());
}

Value TemporalCalendarObject::getterHelper(ExecutionState& state, const Value& callee, Object* thisValue, Value* argv)
{
    Value result = Object::call(state, callee, thisValue, 1, argv);

    if (result.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Undefined");
    }

    return result;
}

Value TemporalCalendarObject::calendarYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendarObject::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyYear()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarMonth(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendarObject::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyMonth()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarMonthCode(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendarObject::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazymonthCode()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDay(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    return TemporalCalendarObject::getterHelper(state, calendar->get(state, state.context()->staticStrings().lazyDay()).value(state, calendar), calendar, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDayOfWeek(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value dayOfWeek = calendar->get(state, state.context()->staticStrings().lazydayOfWeek()).value(state, calendar);
    return Object::call(state, dayOfWeek, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDayOfYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value dayOfYear = calendar->get(state, state.context()->staticStrings().lazydayOfYear()).value(state, calendar);
    return Object::call(state, dayOfYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarWeekOfYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value weekOfYear = calendar->get(state, state.context()->staticStrings().lazyweekOfYear()).value(state, calendar);
    return Object::call(state, weekOfYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDaysInWeek(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInWeek = calendar->get(state, state.context()->staticStrings().lazydaysInWeek()).value(state, calendar);
    return Object::call(state, daysInWeek, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDaysInMonth(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInMonth = calendar->get(state, state.context()->staticStrings().lazydaysInMonth()).value(state, calendar);
    return Object::call(state, daysInMonth, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarDaysInYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value daysInYear = calendar->get(state, state.context()->staticStrings().lazydaysInYear()).value(state, calendar);
    return Object::call(state, daysInYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarMonthsInYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value monthsInYear = calendar->get(state, state.context()->staticStrings().lazymonthsInYear()).value(state, calendar);
    return Object::call(state, monthsInYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::calendarInLeapYear(ExecutionState& state, Object* calendar, const Value& dateLike)
{
    Value inLeapYear = calendar->get(state, state.context()->staticStrings().lazyinLeapYear()).value(state, calendar);
    return Object::call(state, inLeapYear, calendar, 1, const_cast<Value*>(&dateLike));
}

Value TemporalCalendarObject::dateFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    ASSERT(calendar.isObject());
    ASSERT(fields.isObject());
    Value dateFromFields;
    calendar.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazydateFromFields()), dateFromFields);
    Value argv[2] = { fields, options };
    return Object::call(state, dateFromFields, calendar, 2, argv);
}

Value TemporalCalendarObject::ISODaysInYear(ExecutionState& state, const int year)
{
    return Value(TemporalCalendarObject::isIsoLeapYear(state, year) ? 366 : 355);
}

int TemporalCalendarObject::toISOWeekOfYear(ExecutionState& state, const int year, const int month, const int day)
{
    Value epochDays = DateObject::makeDay(state, Value(year), Value(month - 1), Value(day));
    int dayOfYear = TemporalCalendarObject::dayOfYear(state, epochDays);
    int dayOfWeek = DateObject::weekDay(DateObject::makeDate(state, epochDays, Value(0)).toUint32(state));
    int dayOfWeekOfJanFirst = DateObject::weekDay(DateObject::makeDate(state, DateObject::makeDay(state, Value(year), Value(0), Value(1)), Value(0)).toUint32(state));
    int weekNum = (dayOfYear + 6) / 7;
    if (dayOfWeek < dayOfWeekOfJanFirst) {
        ++weekNum;
    }

    return weekNum;
}

Value TemporalCalendarObject::calendarYearMonthFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    Value argv[] = { fields, options };
    return Object::call(state, Object::getMethod(state, calendar, ObjectPropertyName(state.context()->staticStrings().lazyyearMonthFromFields())), calendar, 2, argv);
}

bool TemporalCalendarObject::calendarEquals(const TemporalCalendarObject& firstCalendar, const TemporalCalendarObject& secondCalendar)
{
    return &firstCalendar == &secondCalendar || firstCalendar.getIdentifier() == secondCalendar.getIdentifier();
}

bool TemporalCalendarObject::operator==(const TemporalCalendarObject& rhs) const
{
    return calendarEquals(*this, rhs);
}

bool TemporalCalendarObject::operator!=(const TemporalCalendarObject& rhs) const
{
    return !(rhs == *this);
}


Value TemporalCalendarObject::calendarMonthDayFromFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    Value argv[] = { fields, options };
    return Object::call(state, Object::getMethod(state, calendar, ObjectPropertyName(state.context()->staticStrings().lazymonthDayFromFields())), calendar, 2, argv);
}

bool TemporalPlainDateObject::isValidISODate(ExecutionState& state, const int year, const int month, const int day)
{
    if (month < 1 || month > 12) {
        return false;
    }
    Value daysInMonth = TemporalCalendarObject::ISODaysInMonth(state, year, month);
    if (day < 1 || day > daysInMonth.asInt32()) {
        return false;
    }
    return true;
}

Value TemporalPlainDateObject::createTemporalDate(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const Value& calendar, Optional<Object*> newTarget)
{
    ASSERT(calendar.isObject());
    if (!TemporalPlainDateObject::isValidISODate(state, isoYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Not valid ISOdate");
    }

    if (!TemporalPlainDateTimeObject::ISODateTimeWithinLimits(state, isoYear, isoMonth, isoDay, 12, 0, 0, 0, 0, 0)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Not valid ISOdate");
    }

    return new TemporalPlainDateObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainDatePrototype(), isoYear, isoMonth, isoDay, calendar);
}

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, int isoYear, int isoMonth, int isoDay, Optional<Value> calendarLike)
    : TemporalPlainDateObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainDatePrototype(), isoYear, isoMonth, isoDay, calendarLike)
{
}

TemporalPlainDateObject::TemporalPlainDateObject(ExecutionState& state, Object* proto, short isoYear, char isoMonth, char isoDay, Optional<Value> calendarLike)
    : Temporal(state, proto)
    , TemporalDate(isoYear, isoMonth, isoDay)
    , m_calendar(calendarLike.value().asObject()->asTemporalCalendarObject())
{
}

Value TemporalPlainDateObject::toTemporalDate(ExecutionState& state, const Value& item, Optional<Object*> options)
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
            Value instant = TemporalInstantObject::createTemporalInstant(state, tItem->asTemporalZonedDateTimeObject()->getNanoseconds());
            Value plainDateTime = TemporalTimeZoneObject::builtinTimeZoneGetPlainDateTimeFor(state, tItem->asTemporalZonedDateTimeObject()->getTimeZone(), instant, tItem->asTemporalZonedDateTimeObject()->getCalendar());
            return TemporalPlainDateObject::createTemporalDate(state, plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getYear(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getMonth(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getDay(), plainDateTime.asObject()->asTemporalPlainDateTimeObject()->getCalendar());
        }

        if (tItem->isTemporalPlainDateTimeObject()) {
            TemporalPlainDateTimeObject* tmp = tItem->asTemporalPlainDateTimeObject();
            return TemporalPlainDateObject::createTemporalDate(state, tmp->getYear(), tmp->getMonth(), tmp->getDay(), Value(state.context()->staticStrings().lazyISO8601().string()),
                                                               nullptr);
        }

        Value calendar = TemporalCalendarObject::getTemporalCalendarWithISODefault(state, item);
        ValueVector requiredFields{
            Value(state.context()->staticStrings().lazyDay().string()),
            Value(state.context()->staticStrings().lazyMonth().string()),
            Value(state.context()->staticStrings().lazymonthCode().string()),
            Value(state.context()->staticStrings().lazyYear().string())
        };
        ValueVector fieldNames = TemporalCalendarObject::calendarFields(state, calendar, ValueVector());
        Value fields = Temporal::prepareTemporalFields(state, item, fieldNames, ValueVector());
        return TemporalCalendarObject::dateFromFields(state, calendar, fields, options.value());
    }

    Temporal::toTemporalOverflow(state, options.value());
    auto result = TemporalObject::parseTemporalDateString(state, item.toString(state)->toNonGCUTF8StringData());
    ASSERT(TemporalPlainDateObject::isValidISODate(state, result.year, result.month, result.day));

    return TemporalPlainDateObject::createTemporalDate(state, result.year, result.month, result.day,
                                                       TemporalCalendarObject::toTemporalCalendarWithISODefault(state, Value(result.calendar)),
                                                       nullptr);
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainDateObject::balanceISODate(ExecutionState& state, int year, int month, int day)
{
    Value epochDays = DateObject::makeDay(state, Value(year), Value(month), Value(day));

    ASSERT(std::isfinite(epochDays.asNumber()));

    time64_t ms = DateObject::makeDate(state, epochDays, Value(0)).asUInt32();

    return TemporalPlainDateObject::createISODateRecord(state, DateObject::yearFromTime(ms), DateObject::monthFromTime(ms) + 1, DateObject::dateFromTime(ms));
}
std::map<TemporalObject::DateTimeUnits, int> TemporalPlainDateObject::createISODateRecord(ExecutionState& state, const int year, const int month, const int day)
{
    ASSERT(TemporalPlainDateObject::isValidISODate(state, year, month, day));
    return { { TemporalObject::YEAR_UNIT, year }, { TemporalObject::MONTH_UNIT, month }, { TemporalObject::DAY_UNIT, day } };
}

int TemporalPlainDateObject::compareISODate(int firstYear, int firstMonth, int firstDay, int secondYear, int secondMonth, int secondDay)
{
    if (firstYear > secondYear) {
        return 1;
    } else if (firstYear < secondYear) {
        return -1;
    }

    if (firstMonth > secondMonth) {
        return 1;
    } else if (firstMonth < secondMonth) {
        return -1;
    }

    if (firstDay > secondDay) {
        return 1;
    } else if (firstDay < secondDay) {
        return -1;
    }
    return 0;
}

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state)
    : TemporalPlainDateTimeObject(state, state.context()->globalObject()->objectPrototype(), 0, 0, 0, 0, 0, 0, 0, 0, 0)
{
}

TemporalPlainDateTimeObject::TemporalPlainDateTimeObject(ExecutionState& state, Object* proto, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
    : Temporal(state, proto)
    , TemporalDate(year, month, day)
    , TemporalTime(hour, minute, second, microsecond, millisecond, nanosecond)
{
}

uint64_t TemporalPlainDateTimeObject::getEpochFromISOParts(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    ASSERT(TemporalPlainDateObject::isValidISODate(state, year, month, day));
    ASSERT(TemporalPlainTimeObject::isValidTime(state, hour, minute, second, millisecond, microsecond, nanosecond));

    Value date = DateObject::makeDay(state, Value(year), Value(month), Value(day));
    Value time = DateObject::makeTime(state, Value(hour), Value(minute), Value(second), Value(millisecond));
    Value ms = DateObject::makeDate(state, date, time);
    ASSERT(std::isfinite(ms.asNumber()));

    return ms.toLength(state);
}

bool TemporalPlainDateTimeObject::ISODateTimeWithinLimits(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    time64_t ms = TemporalPlainDateTimeObject::getEpochFromISOParts(state, year, month, day, hour, minute, second, millisecond, microsecond, nanosecond);
    return IS_IN_TIME_RANGE(ms);
}

std::map<TemporalObject::DateTimeUnits, int> TemporalPlainDateTimeObject::balanceISODateTime(ExecutionState& state, const int year, const int month, const int day, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond)
{
    auto balancedTime = TemporalPlainTimeObject::balanceTime(state, hour, minute, second, millisecond, microsecond, nanosecond);
    auto balancedDate = TemporalPlainDateObject::balanceISODate(state, year, month, day + balancedTime[TemporalObject::DAY_UNIT]);
    balancedDate.insert(balancedTime.begin(), balancedTime.end());
    return balancedDate;
}

Value TemporalPlainDateTimeObject::createTemporalDateTime(ExecutionState& state, const int isoYear, const int isoMonth, const int isoDay, const int hour, const int minute, const int second, const int millisecond, const int microsecond, const int nanosecond, const Value& calendar, Optional<Object*> newTarget)
{
    ASSERT(calendar.isObject());
    if (!TemporalPlainDateObject::isValidISODate(state, isoYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid Date");
    }

    if (!TemporalPlainTimeObject::isValidTime(state, hour, minute, second, millisecond, microsecond, millisecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid Time");
    }

    if (!TemporalPlainDateTimeObject::ISODateTimeWithinLimits(state, isoYear, isoMonth, isoDay, hour, minute, second, millisecond, microsecond, millisecond)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "ISODateTime is out of limits");
    }

    return new TemporalPlainDateTimeObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainDateTimePrototype(), isoYear, isoMonth, isoDay, hour, minute, second, millisecond, microsecond, millisecond);
}

std::map<TemporalObject::DateTimeUnits, std::string> TemporalPlainDateTimeObject::interpretTemporalDateTimeFields(ExecutionState& state, const Value& calendar, const Value& fields, const Value& options)
{
    return {};
}

Value TemporalPlainDateTimeObject::toTemporalDateTime(ExecutionState& state, const Value& item, const Value& options)
{
    ASSERT(options.isObject() || options.isUndefined());

    Value calendar;
    std::map<TemporalObject::DateTimeUnits, std::string> result = {};

    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainDateTimeObject()) {
            return item;
        } else if (item.asObject()->isTemporalZonedDateTimeObject()) {
            Value instant = TemporalInstantObject::createTemporalInstant(state, item.asObject()->asTemporalZonedDateTimeObject()->getNanoseconds());
            return TemporalTimeZoneObject::builtinTimeZoneGetPlainDateTimeFor(state, item.asObject()->asTemporalZonedDateTimeObject()->getTimeZone(), instant, item.asObject()->asTemporalZonedDateTimeObject()->getCalendar());
        } else if (item.asObject()->isTemporalPlainDateObject()) {
            return TemporalPlainDateTimeObject::createTemporalDateTime(state, item.asObject()->asTemporalPlainDateObject()->year(), item.asObject()->asTemporalPlainDateObject()->month(), item.asObject()->asTemporalPlainDateObject()->day(), 0, 0, 0, 0, 0, 0, item.asObject()->asTemporalPlainDateObject()->calendar(), new Object(state));
        }

        calendar = TemporalCalendarObject::getTemporalCalendarWithISODefault(state, item);
        ValueVector values = { Value(state.context()->staticStrings().lazyDay().string()),
                               Value(state.context()->staticStrings().lazyHour().string()),
                               Value(state.context()->staticStrings().lazymicrosecond().string()),
                               Value(state.context()->staticStrings().lazymillisecond().string()),
                               Value(state.context()->staticStrings().lazyMinute().string()),
                               Value(state.context()->staticStrings().lazyMonth().string()),
                               Value(state.context()->staticStrings().lazymonthCode().string()),
                               Value(state.context()->staticStrings().lazynanosecond().string()),
                               Value(state.context()->staticStrings().lazySecond().string()),
                               Value(state.context()->staticStrings().lazyYear().string()) };
        ValueVector fieldNames = TemporalCalendarObject::calendarFields(state, calendar, values);
        Value fields = Temporal::prepareTemporalFields(state, item, fieldNames, ValueVector());
        result = TemporalPlainDateTimeObject::interpretTemporalDateTimeFields(state, calendar, fields, options);
        return TemporalPlainDateTimeObject::createTemporalDateTime(state, Value(result[TemporalObject::YEAR_UNIT].c_str()).asInt32(), Value(result[TemporalObject::MONTH_UNIT].c_str()).asInt32(), Value(result[TemporalObject::DAY_UNIT].c_str()).asInt32(), Value(result[TemporalObject::HOUR_UNIT].c_str()).asInt32(), Value(result[TemporalObject::MINUTE_UNIT].c_str()).asInt32(), Value(result[TemporalObject::SECOND_UNIT].c_str()).asInt32(), Value(result[TemporalObject::MILLISECOND_UNIT].c_str()).asInt32(), Value(result[TemporalObject::MICROSECOND_UNIT].c_str()).asInt32(), Value(result[TemporalObject::NANOSECOND_UNIT].c_str()).asInt32(), calendar, new Object(state));
    }
    Temporal::toTemporalOverflow(state, options);
    auto tmp = TemporalObject::parseTemporalDateTimeString(state, item.toString(state)->toNonGCUTF8StringData());
    ASSERT(TemporalPlainDateObject::isValidISODate(state, tmp.year, tmp.month, tmp.day));
    ASSERT(TemporalPlainTimeObject::isValidTime(state, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond));

    return TemporalPlainDateTimeObject::createTemporalDateTime(state, tmp.year, tmp.month, tmp.day, tmp.hour, tmp.minute, tmp.second, tmp.millisecond, tmp.microsecond, tmp.nanosecond, TemporalCalendarObject::toTemporalCalendarWithISODefault(state, Value(tmp.calendar)), new Object(state));
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state)
    : TemporalZonedDateTimeObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalZonedDateTimeObject::TemporalZonedDateTimeObject(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

TemporalInstantObject::TemporalInstantObject(ExecutionState& state)
    : TemporalInstantObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalInstantObject::TemporalInstantObject(ExecutionState& state, Object* proto)
    : Temporal(state, proto)
{
}

Value TemporalInstantObject::createTemporalInstant(ExecutionState& state, const Value& epochNanoseconds, Optional<Object*> newTarget)
{
    ASSERT(epochNanoseconds.isBigInt());
    ASSERT(TemporalInstantObject::isValidEpochNanoseconds(epochNanoseconds));

    auto* object = new TemporalInstantObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalInstantPrototype());
    object->setNanoseconds(epochNanoseconds.asBigInt());

    return object;
}

bool TemporalInstantObject::isValidEpochNanoseconds(const Value& epochNanoseconds)
{
    ASSERT(epochNanoseconds.isBigInt());
    /* Maximum possible and minimum possible Nanoseconds */
    const char min[] = "-8640000000000000000000";
    const char max[] = "8640000000000000000000";
    BigInt* minEpoch = BigInt::parseString(min, sizeof(min) - 1).value();
    BigInt* maxEpoch = BigInt::parseString(max, sizeof(max) - 1).value();
    return epochNanoseconds.asBigInt()->greaterThanEqual(minEpoch) && epochNanoseconds.asBigInt()->lessThanEqual(maxEpoch);
}

Value TemporalInstantObject::toTemporalInstant(ExecutionState& state, const Value& item)
{
    if (item.isObject()) {
        if (item.asObject()->isTemporalInstantObject()) {
            return item;
        }

        if (item.asObject()->isTemporalZonedDateTimeObject()) {
            return TemporalInstantObject::createTemporalInstant(state, item.asObject()->asTemporalZonedDateTimeObject()->getNanoseconds());
        }
    }

    return TemporalInstantObject::createTemporalInstant(state, parseTemporalInstant(state, std::string(item.asString()->toNonGCUTF8StringData())));
}

Value TemporalInstantObject::parseTemporalInstant(ExecutionState& state, const std::string& isoString)
{
    auto result = TemporalObject::parseTemporalInstantString(state, isoString);
    int64_t offsetNanoseconds = offsetStringToNanoseconds(state, result.tz->offsetString);
    time64_t utc = TemporalPlainDateTimeObject::getEpochFromISOParts(state, result.year, result.month, result.day, result.hour, result.minute, result.second, result.millisecond, result.microsecond, result.nanosecond);
    auto utcWithOffset = new BigInt(utc - offsetNanoseconds);
    if (!TemporalInstantObject::isValidEpochNanoseconds(utcWithOffset)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "epochNanosecond is out of limits");
    }

    return utcWithOffset;
}

int64_t TemporalInstantObject::offsetStringToNanoseconds(ExecutionState& state, String* offset)
{
    UTF8StringData utf8StringData = offset->toUTF8StringData();
    std::string offsetString(utf8StringData.data(), utf8StringData.length());
    int64_t result = std::stoi(offsetString.substr(1, 2)) * TemporalInstantObject::HourToNanosecond;
    if (offsetString[3] == ':') {
        result += std::stoi(offsetString.substr(4, 2)) * TemporalInstantObject::MinuteToNanosecond;
        if (offsetString[6] == ':') {
            result += std::stoi(offsetString.substr(7, 2)) * TemporalInstantObject::SecondToNanosecond;
            if (offsetString[9] == '.') {
                result += std::stoi(offsetString.substr(10, 3)) * TemporalInstantObject::MillisecondToNanosecond;
                result += std::stoi(offsetString.substr(13, 3)) * TemporalInstantObject::MicrosecondToNanosecond;
                result += std::stoi(offsetString.substr(16, 3));
            }
        }
    }

    if (offsetString[0] == '-') {
        result *= -1;
    }

    return result;
}

int TemporalInstantObject::compareEpochNanoseconds(ExecutionState& state, const BigInt& firstNanoSeconds, const BigInt& secondNanoSeconds)
{
    if (firstNanoSeconds.greaterThan(&secondNanoSeconds)) {
        return 1;
    } else if (firstNanoSeconds.lessThan(&secondNanoSeconds)) {
        return -1;
    }

    return 0;
}

TemporalTimeZoneObject::TemporalTimeZoneObject(ExecutionState& state)
    : TemporalTimeZoneObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalTimeZoneObject::TemporalTimeZoneObject(ExecutionState& state, Object* proto, ASCIIString* identifier, const Value& offsetNanoseconds)
    : Temporal(state, proto)
    , m_identifier(identifier)
    , m_offsetNanoseconds(offsetNanoseconds)
{
}

Value TemporalTimeZoneObject::builtinTimeZoneGetPlainDateTimeFor(ExecutionState& state, const Value& timeZone, const Value& instant, const Value& calendar)
{
    ASSERT(instant.asObject()->isTemporalInstantObject());
    int64_t offsetNanoseconds = TemporalTimeZoneObject::getOffsetNanosecondsFor(state, timeZone, instant);
    auto result = TemporalTimeZoneObject::getISOPartsFromEpoch(state, Value(instant.asObject()->asTemporalInstantObject()->getNanoseconds()));
    result = TemporalPlainDateTimeObject::balanceISODateTime(state, result[TemporalObject::YEAR_UNIT], result[TemporalObject::MONTH_UNIT], result[TemporalObject::DAY_UNIT], result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT]);

    return TemporalPlainDateTimeObject::createTemporalDateTime(state, result[TemporalObject::YEAR_UNIT], result[TemporalObject::MONTH_UNIT], result[TemporalObject::DAY_UNIT], result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT], calendar, new Object(state));
}

int64_t TemporalTimeZoneObject::getOffsetNanosecondsFor(ExecutionState& state, const Value& timeZone, const Value& instant)
{
    Value getOffsetNanosecondsFor = Object::getMethod(state, timeZone, ObjectPropertyName(state.context()->staticStrings().lazygetOffsetNanosecondsFor()));
    Value offsetNanoseconds = Object::call(state, getOffsetNanosecondsFor, Value(), 1, const_cast<Value*>(&instant));
    if (!offsetNanoseconds.isNumber()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "offsetNanoseconds is not a Number");
    }

    if (!offsetNanoseconds.isInteger(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "offsetNanoseconds is not an Integer");
    }

    int64_t nanoseconds = offsetNanoseconds.asDouble();

    if (std::abs(nanoseconds) > TemporalInstantObject::dayToNanosecond) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "offsetNanoseconds is out of range");
    }

    return nanoseconds;
}

std::map<TemporalObject::DateTimeUnits, int> TemporalTimeZoneObject::getISOPartsFromEpoch(ExecutionState& state, const Value& epochNanoseconds)
{
    ASSERT(TemporalInstantObject::isValidEpochNanoseconds(epochNanoseconds));
    BigInt ns = BigInt((uint64_t)1000000);
    time64_t remainderNs = epochNanoseconds.asBigInt()->remainder(state, &ns)->toInt64();
    time64_t epochMilliseconds = epochNanoseconds.asBigInt()->subtraction(state, new BigInt(remainderNs))->remainder(state, &ns)->toInt64();
    std::map<TemporalObject::DateTimeUnits, int> result;

    result[TemporalObject::YEAR_UNIT] = DateObject::yearFromTime(epochMilliseconds);
    result[TemporalObject::MONTH_UNIT] = DateObject::monthFromTime(epochMilliseconds);
    result[TemporalObject::DAY_UNIT] = DateObject::dateFromTime(epochMilliseconds);
    result[TemporalObject::HOUR_UNIT] = DateObject::hourFromTime(epochMilliseconds);
    result[TemporalObject::MINUTE_UNIT] = DateObject::minFromTime(epochMilliseconds);
    result[TemporalObject::SECOND_UNIT] = DateObject::secFromTime(epochMilliseconds);
    result[TemporalObject::MILLISECOND_UNIT] = DateObject::msFromTime(epochMilliseconds);
    result[TemporalObject::MICROSECOND_UNIT] = (int)std::floor(remainderNs / 1000) % 1000;
    result[TemporalObject::NANOSECOND_UNIT] = remainderNs % 1000;

    return result;
}

bool TemporalTimeZoneObject::isValidTimeZoneName(const std::string& timeZone)
{
    return timeZone == "UTC";
}

std::string TemporalTimeZoneObject::canonicalizeTimeZoneName(const std::string& timeZone)
{
    return "UTC";
}

Value TemporalTimeZoneObject::createTemporalTimeZone(ExecutionState& state, const std::string& identifier, Optional<Object*> newTarget)
{
    if (TemporalTimeZoneObject::isValidTimeZoneName(identifier)) {
        ASSERT(TemporalTimeZoneObject::canonicalizeTimeZoneName(identifier) == identifier);
        return new TemporalTimeZoneObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalTimeZonePrototype(), new ASCIIString(identifier.c_str()), Value());
    }

    int64_t offsetNanoseconds = TemporalInstantObject::offsetStringToNanoseconds(state, new ASCIIString(identifier.c_str()));

    return new TemporalTimeZoneObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalTimeZonePrototype(), new ASCIIString(TemporalTimeZoneObject::formatTimeZoneOffsetString(offsetNanoseconds).c_str()), Value(offsetNanoseconds));
}

std::string TemporalTimeZoneObject::formatTimeZoneOffsetString(long long offsetNanoseconds)
{
    std::string result = offsetNanoseconds >= 0 ? "+" : "-";
    offsetNanoseconds = std::abs(offsetNanoseconds);
    int nanoseconds = offsetNanoseconds % TemporalInstantObject::SecondToNanosecond;
    int seconds = int(std::floor(offsetNanoseconds / TemporalInstantObject::SecondToNanosecond)) % 60;
    int minutes = int(std::floor(offsetNanoseconds / TemporalInstantObject::MinuteToNanosecond)) % 60;
    int hours = int(std::floor(offsetNanoseconds / TemporalInstantObject::HourToNanosecond));

    result += (hours == 0 ? "00" : ((hours < 10 ? "0" : "") + std::to_string(hours))) + ":" + (minutes < 10 ? "0" : "") + std::to_string(minutes);

    if (nanoseconds != 0) {
        std::string second;
        int num = nanoseconds;
        unsigned int digits = 9;

        while (num != 0 && digits != 0) {
            num /= 10;
            digits--;
        }

        for (unsigned int i = 0; i < digits; ++i) {
            second += '0';
        }

        second += std::to_string(nanoseconds);
        int index = second.size() - 1;

        for (int i = second.size() - 1; i >= 0; i--) {
            if (second[i] == '0') {
                index--;
            } else {
                break;
            }
        }

        result += ":";
        result += (hours < 10 ? "0" : "") + std::to_string(seconds) + "." + second.substr(0, index + 1);
    } else if (seconds != 0) {
        result += ":";
        result += (hours < 10 ? "0" : "") + std::to_string(seconds);
    }

    return result;
}

Value TemporalTimeZoneObject::toTemporalTimeZone(ExecutionState& state, const Value& temporalTimeZoneLike)
{
    auto timeZoneLike = Value();

    if (temporalTimeZoneLike.isObject()) {
        auto item = temporalTimeZoneLike.asObject();
        if (item->isTemporalZonedDateTimeObject()) {
            return item->asTemporalZonedDateTimeObject()->getTimeZone();
        }

        if (!item->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyTimeZone()))) {
            return temporalTimeZoneLike;
        }

        timeZoneLike = item->get(state, ObjectPropertyName(state.context()->staticStrings().lazyTimeZone())).value(state, item);

        if (timeZoneLike.isObject() && !timeZoneLike.asObject()->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyTimeZone()))) {
            return timeZoneLike;
        }
    }

    TemporalObject::TimeZone parseResult = TemporalObject::parseTemporalTimeZoneString(state, timeZoneLike.isUndefined() ? temporalTimeZoneLike.asString()->toNonGCUTF8StringData() : timeZoneLike.asString()->toNonGCUTF8StringData());

    if (parseResult.name->length() != 0) {
        unsigned int index = 0;
        std::string name = std::string(parseResult.name->toNonGCUTF8StringData());
        if (TemporalObject::offset(state, name, index).empty()) {
            if (!TemporalTimeZoneObject::isValidTimeZoneName(name)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid TimeZone identifier");
            }

            parseResult.name = new ASCIIString(TemporalTimeZoneObject::canonicalizeTimeZoneName(name).c_str());
        }
        return TemporalTimeZoneObject::createTemporalTimeZone(state, name);
    }

    if (parseResult.z) {
        return TemporalTimeZoneObject::createTemporalTimeZone(state, "UTC");
    }

    return TemporalTimeZoneObject::createTemporalTimeZone(state, parseResult.offsetString->toNonGCUTF8StringData());
}

Value TemporalTimeZoneObject::builtinTimeZoneGetOffsetStringFor(ExecutionState& state, const Value& timeZone, const Value& instant)
{
    return Value(new ASCIIString(TemporalTimeZoneObject::formatTimeZoneOffsetString(TemporalTimeZoneObject::getOffsetNanosecondsFor(state, timeZone, instant)).c_str()));
}

Value TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(ExecutionState& state, const Value& epochNanoseconds, const std::string& timeZoneIdentifier)
{
    auto u16 = utf8StringToUTF16String(timeZoneIdentifier.data(), timeZoneIdentifier.size());
    return epochNanoseconds.asBigInt()->addition(state, new BigInt((int64_t)vzone_getRawOffset(vzone_openID(u16.data(), u16.size()))));
}

std::map<TemporalObject::DateTimeUnits, int> TemporalTimeZoneObject::getIANATimeZoneDateTimeParts(ExecutionState& state, const Value& epochNanoseconds)
{
    auto result = TemporalTimeZoneObject::getISOPartsFromEpoch(state, epochNanoseconds);
    return TemporalPlainDateTimeObject::balanceISODateTime(state, result[TemporalObject::YEAR_UNIT], result[TemporalObject::MONTH_UNIT], result[TemporalObject::DAY_UNIT], result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT]);
}

ValueVector TemporalTimeZoneObject::getIANATimeZoneEpochValue(ExecutionState& state, const std::string& timeZoneIdentifier, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond)
{
    auto ns = TemporalPlainDateTimeObject::getEpochFromISOParts(state, year, month, day, hour, minute, second, millisecond, microsecond, nanosecond);
    auto nsEarlier = new BigInt(int64_t(ns - TemporalInstantObject::dayToNanosecond));

    if (!TemporalInstantObject::isValidEpochNanoseconds(nsEarlier)) {
        nsEarlier = new BigInt(ns);
    }

    auto nsLater = new BigInt(int64_t(ns + TemporalInstantObject::dayToNanosecond));

    if (!TemporalInstantObject::isValidEpochNanoseconds(nsLater)) {
        nsLater = new BigInt(ns);
    }

    auto earliest = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, nsEarlier, timeZoneIdentifier).asBigInt();
    auto latest = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, nsLater, timeZoneIdentifier).asBigInt();
    BigInt* found[] = { earliest, latest };
    ValueVector retVal = {};

    for (auto& i : found) {
        auto epochNanoseconds = (new BigInt(ns))->subtraction(state, i);
        auto parts = getIANATimeZoneDateTimeParts(state, epochNanoseconds);

        if (year != parts[TemporalObject::YEAR_UNIT] || month != parts[TemporalObject::MONTH_UNIT] || day != parts[TemporalObject::DAY_UNIT] || hour != parts[TemporalObject::HOUR_UNIT] || minute != parts[TemporalObject::MINUTE_UNIT] || second != parts[TemporalObject::SECOND_UNIT] || millisecond != parts[TemporalObject::MILLISECOND_UNIT] || microsecond != parts[TemporalObject::MICROSECOND_UNIT] || nanosecond != parts[TemporalObject::NANOSECOND_UNIT]) {
            retVal.push_back(Value());
            continue;
        }

        retVal.push_back(epochNanoseconds);
    }

    return retVal;
}

Value TemporalTimeZoneObject::getIANATimeZoneNextTransition(ExecutionState& state, const Value& epochNanoseconds, std::string timeZoneIdentifier)
{
    auto upperCap = new BigInt(DateObject::currentTime() + (TemporalInstantObject::dayToNanosecond * 366));
    auto leftNanos = epochNanoseconds.asBigInt();
    auto leftOffsetNs = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, leftNanos, timeZoneIdentifier).asBigInt();
    auto rightNanos = leftNanos;
    auto rightOffsetNs = leftOffsetNs;

    return TemporalTimeZoneObject::getIANATimeZoneTransition(state, rightNanos, rightOffsetNs, leftNanos, leftOffsetNs, upperCap, timeZoneIdentifier);
}

Value TemporalTimeZoneObject::getIANATimeZonePreviousTransition(ExecutionState& state, const Value& epochNanoseconds, std::string timeZoneIdentifier)
{
    auto lowerCap = (new BigInt((int64_t)-388152))->multiply(state, new BigInt((int64_t)1e13)); // 1847-01-01T00:00:00Z

    auto rightNanos = epochNanoseconds.asBigInt()->subtraction(state, new BigInt((int64_t)1));
    auto rightOffsetNs = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, rightNanos, timeZoneIdentifier).asBigInt();
    auto leftNanos = rightNanos;
    auto leftOffsetNs = rightOffsetNs;

    return TemporalTimeZoneObject::getIANATimeZoneTransition(state, rightNanos, rightOffsetNs, leftNanos, leftOffsetNs, lowerCap, timeZoneIdentifier);
}

Value TemporalTimeZoneObject::getIANATimeZoneTransition(ExecutionState& state, BigInt* rightNanos, BigInt* rightOffsetNs, BigInt* leftNanos, BigInt* leftOffsetNs, BigInt* cap, const std::string& timeZoneIdentifier)
{
    while (leftOffsetNs == rightOffsetNs && leftNanos->greaterThan(cap)) {
        leftNanos = leftNanos->subtraction(state, new BigInt(TemporalInstantObject::dayToNanosecond * 2 * 7));
        leftOffsetNs = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, leftNanos, timeZoneIdentifier).asBigInt();

        if (leftOffsetNs->equals(rightOffsetNs)) {
            leftNanos = rightNanos;
        }
    }

    while (rightNanos->subtraction(state, leftNanos)->greaterThan(1)) {
        auto middle = leftNanos->addition(state, rightNanos)->division(state, new BigInt((uint64_t)2));
        auto mState = TemporalTimeZoneObject::getIANATimeZoneOffsetNanoseconds(state, middle, timeZoneIdentifier).asBigInt();

        if (mState->equals(leftOffsetNs)) {
            leftNanos = middle;
            leftOffsetNs = mState;
        } else if (mState->equals(rightOffsetNs)) {
            rightNanos = middle;
            rightOffsetNs = mState;
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "invalid state");
            break;
        }
    }

    return rightNanos;
}

TemporalPlainYearMonthObject::TemporalPlainYearMonthObject(ExecutionState& state)
    : TemporalPlainYearMonthObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainYearMonthObject::TemporalPlainYearMonthObject(ExecutionState& state, Object* proto, int isoYear, int isoMonth, Object* calendar, int referenceISODay)
    : Temporal(state, proto)
    , m_isoYear(isoYear)
    , m_isoMonth(isoMonth)
    , m_calendar(calendar)
    , m_referenceISODay(referenceISODay)
{
}

Value TemporalPlainYearMonthObject::createTemporalYearMonth(ExecutionState& state, int isoYear, int isoMonth, const Value& calendar, int referenceISODay, Optional<Object*> newTarget)
{
    ASSERT(calendar.isObject());

    if (!TemporalPlainDateObject::isValidISODate(state, isoYear, isoMonth, referenceISODay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid date");
    }

    if (!TemporalPlainYearMonthObject::isoYearMonthWithinLimits(isoYear, isoMonth)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid date");
    }

    if (!newTarget.hasValue()) {
        newTarget = state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainYearMonthPrototype();
    }

    return new TemporalPlainYearMonthObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainYearMonthPrototype(), isoYear, isoMonth, calendar.asObject(), referenceISODay);
}

bool TemporalPlainYearMonthObject::isoYearMonthWithinLimits(int isoYear, int isoMonth)
{
    return (isoYear > -271821 && isoYear < 275760) || (isoYear == -271821 && isoMonth < 4) || (isoYear == 275760 && isoMonth < 9);
}

Value TemporalPlainYearMonthObject::toTemporalYearMonth(ExecutionState& state, const Value& item, const Value& options)
{
    ASSERT(options.isObject() || options.isUndefined());

    if (item.isObject()) {
        if (item.asObject()->isTemporalPlainYearMonthObject()) {
            return item;
        }

        auto calendar = TemporalCalendarObject::getTemporalCalendarWithISODefault(state, item);
        auto fieldNames = TemporalCalendarObject::calendarFields(state, calendar, { new ASCIIString("month"), new ASCIIString("monthCode"), new ASCIIString("year") });
        auto fields = Temporal::prepareTemporalFields(state, item, fieldNames, {});
        return TemporalCalendarObject::calendarYearMonthFromFields(state, calendar, fields, options);
    }

    Temporal::toTemporalOverflow(state, options);
    auto result = TemporalObject::parseTemporalYearMonthString(state, item.asString()->toNonGCUTF8StringData());
    auto calendar = TemporalCalendarObject::toTemporalCalendarWithISODefault(state, Value(result.calendar));
    auto retVal = TemporalPlainYearMonthObject::createTemporalYearMonth(state, result.year, result.month, calendar, result.day);

    return TemporalCalendarObject::calendarYearMonthFromFields(state, calendar, retVal, Value());
}

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, int years = 0, int months = 0, int weeks = 0, int days = 0, int hours = 0, int minutes = 0, int seconds = 0, int milliseconds = 0, int microseconds = 0, int nanoseconds = 0)
    : TemporalDurationObject(state, state.context()->globalObject()->objectPrototype(), years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds)
{
}

TemporalDurationObject::TemporalDurationObject(ExecutionState& state, Object* proto, int years = 0, int months = 0, int weeks = 0, int days = 0, int hours = 0, int minutes = 0, int seconds = 0, int milliseconds = 0, int microseconds = 0, int nanoseconds = 0)
    : Temporal(state, proto)
    , TemporalDate(years, months, days)
    , TemporalTime(hours, minutes, seconds, microseconds, milliseconds, nanoseconds)
{
}

bool TemporalDurationObject::isValidDuration(const int fields[])
{
    int sign = TemporalDurationObject::durationSign(fields);
    for (int i = 0; i < 10; i++) {
        if ((fields[i] < 0 && sign > 0) || (fields[i] > 0 && sign < 0)) {
            return false;
        }
    }
    return true;
}

int TemporalDurationObject::durationSign(const int fields[])
{
    for (int i = 0; i < 10; i++) {
        if (fields[i] < 0) {
            return -1;
        } else if (fields[i] > 0) {
            return 1;
        }
    }
    return 0;
}

Value TemporalDurationObject::createTemporalDuration(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds, Optional<Object*> newTarget)
{
    int dateTime[] = { years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds };
    if (isValidDuration(dateTime)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid time");
    }

    return new TemporalDurationObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalDurationPrototype(), years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds);
}
Value TemporalDurationObject::toTemporalDuration(ExecutionState& state, const Value& item)
{
    if (item.isObject() && item.asObject()->asTemporalDurationObject()) {
        return item;
    }

    auto result = TemporalDurationObject::toTemporalDurationRecord(state, item);

    return TemporalDurationObject::createTemporalDuration(state, result[TemporalObject::YEAR_UNIT], result[TemporalObject::MONTH_UNIT], result[TemporalObject::WEEK_UNIT], result[TemporalObject::DAY_UNIT], result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT], nullptr);
}

std::map<TemporalObject::DateTimeUnits, int> TemporalDurationObject::toTemporalDurationRecord(ExecutionState& state, const Value& temporalDurationLike)
{
    if (!temporalDurationLike.isObject()) {
        return TemporalObject::parseTemporalDurationString(state, temporalDurationLike.asString()->toNonGCUTF8StringData());
    } else if (temporalDurationLike.asObject()->isTemporalDurationObject()) {
        auto duration = temporalDurationLike.asObject()->asTemporalDurationObject();
        return TemporalDurationObject::createDurationRecord(state, duration->getYear(), duration->getMonth(), duration->getWeek(), duration->getDay(), duration->getHour(), duration->getMinute(), duration->getSecond(), duration->getMillisecond(), duration->getMicrosecond(), duration->getNanosecond());
    }

    std::map<TemporalObject::DateTimeUnits, int> result = { { TemporalObject::YEAR_UNIT, 0 }, { TemporalObject::MONTH_UNIT, 0 }, { TemporalObject::WEEK_UNIT, 0 }, { TemporalObject::DAY_UNIT, 0 }, { TemporalObject::HOUR_UNIT, 0 }, { TemporalObject::MINUTE_UNIT, 0 }, { TemporalObject::SECOND_UNIT, 0 }, { TemporalObject::MILLISECOND_UNIT, 0 }, { TemporalObject::MICROSECOND_UNIT, 0 }, { TemporalObject::NANOSECOND_UNIT, 0 } };
    auto partial = TemporalDurationObject::toTemporalPartialDurationRecord(state, temporalDurationLike);

    for (auto const& x : partial) {
        if (!x.second.isUndefined()) {
            result[x.first] = x.second.asInt32();
        }
    }

    int dateTime[] = { result[TemporalObject::YEAR_UNIT], result[TemporalObject::MONTH_UNIT], result[TemporalObject::WEEK_UNIT], result[TemporalObject::DAY_UNIT], result[TemporalObject::HOUR_UNIT], result[TemporalObject::MINUTE_UNIT], result[TemporalObject::SECOND_UNIT], result[TemporalObject::MILLISECOND_UNIT], result[TemporalObject::MICROSECOND_UNIT], result[TemporalObject::NANOSECOND_UNIT] };

    if (!TemporalDurationObject::isValidDuration(dateTime)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration");
    }

    return result;
}

std::map<TemporalObject::DateTimeUnits, Value> TemporalDurationObject::toTemporalPartialDurationRecord(ExecutionState& state, const Value& temporalDurationLike)
{
    if (!temporalDurationLike.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Expected object");
    }

    std::map<TemporalObject::DateTimeUnits, Value> result;

    TemporalObject::DateTimeUnits temporalTimeLikeProp[10] = { TemporalObject::YEAR_UNIT, TemporalObject::MONTH_UNIT, TemporalObject::WEEK_UNIT, TemporalObject::DAY_UNIT, TemporalObject::HOUR_UNIT, TemporalObject::MINUTE_UNIT, TemporalObject::SECOND_UNIT, TemporalObject::MILLISECOND_UNIT, TemporalObject::MICROSECOND_UNIT, TemporalObject::NANOSECOND_UNIT };
    bool any = false;
    for (auto i : temporalTimeLikeProp) {
        Value value = temporalDurationLike.asObject()->get(state, ObjectPropertyName(state, new ASCIIString(dateTimeUnitStrings[i]))).value(state, temporalDurationLike);
        if (!value.isUndefined()) {
            any = true;
            result[i] = value;
        }
    }
    if (!any) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "any is false");
    }
    return result;
}

std::map<TemporalObject::DateTimeUnits, int> TemporalDurationObject::createDurationRecord(ExecutionState& state, int years, int months, int weeks, int days, int hours, int minutes, int seconds, int milliseconds, int microseconds, int nanoseconds)
{
    int dateTime[] = { years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds };

    if (!TemporalDurationObject::isValidDuration(dateTime)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid duration");
    }
    return { { TemporalObject::YEAR_UNIT, years }, { TemporalObject::MONTH_UNIT, months }, { TemporalObject::WEEK_UNIT, weeks }, { TemporalObject::DAY_UNIT, days }, { TemporalObject::HOUR_UNIT, hours }, { TemporalObject::MINUTE_UNIT, minutes }, { TemporalObject::SECOND_UNIT, seconds }, { TemporalObject::MILLISECOND_UNIT, milliseconds }, { TemporalObject::MICROSECOND_UNIT, microseconds }, { TemporalObject::NANOSECOND_UNIT, nanoseconds } };
}

Value TemporalDurationObject::createNegatedTemporalDuration(ExecutionState& state, const Value& duration)
{
    auto durationObject = duration.asObject()->asTemporalDurationObject();
    return TemporalDurationObject::createTemporalDuration(state, -durationObject->m_year, -durationObject->m_month, -durationObject->m_week, -durationObject->m_day, -durationObject->m_hour, -durationObject->m_minute, -durationObject->m_second, -durationObject->m_millisecond, -durationObject->m_microsecond, -durationObject->m_nanosecond, nullptr);
}

TemporalDate::TemporalDate(short mYear, char mMonth, char mDay)
    : m_year(mYear)
    , m_month(mMonth)
    , m_day(mDay)
{
}

TemporalTime::TemporalTime(char mHour, char mMinute, char mSecond, short mMillisecond, short mMicrosecond, short mNanosecond)
    : m_hour(mHour)
    , m_minute(mMinute)
    , m_second(mSecond)
    , m_millisecond(mMillisecond)
    , m_microsecond(mMicrosecond)
    , m_nanosecond(mNanosecond)
{
}
TemporalTime::TemporalTime()
    : m_hour(0)
    , m_minute(0)
    , m_second(0)
    , m_millisecond(0)
    , m_microsecond(0)
    , m_nanosecond(0)
{
}

TemporalPlainMonthDayObject::TemporalPlainMonthDayObject(ExecutionState& state)
    : TemporalPlainMonthDayObject(state, state.context()->globalObject()->objectPrototype())
{
}

TemporalPlainMonthDayObject::TemporalPlainMonthDayObject(ExecutionState& state, Object* proto, int isoMonth, int isoDay, TemporalCalendarObject* calendar, int referenceISOYear)
    : Temporal(state, proto)
    , m_isoMonth(isoMonth)
    , m_isoDay(isoDay)
    , m_calendar(calendar)
    , m_referenceISOYear(referenceISOYear)
{
}

Value TemporalPlainMonthDayObject::createTemporalMonthDay(ExecutionState& state, int isoMonth, int isoDay, TemporalCalendarObject* calendar, int referenceISOYear, Optional<Object*> newTarget)
{
    if (!TemporalPlainDateObject::isValidISODate(state, referenceISOYear, isoMonth, isoDay)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Invalid date");
    }

    return new TemporalPlainMonthDayObject(state, state.context()->globalObject()->temporal()->asTemporalObject()->getTemporalPlainMonthDayPrototype(), isoMonth, isoDay, calendar, referenceISOYear);
}

Value TemporalPlainMonthDayObject::toTemporalMonthDay(ExecutionState& state, const Value& item, const Value& options)
{
    ASSERT(options.isObject() || options.isUndefined());
    int referenceISOYear = 1972;

    if (!item.isObject()) {
        Temporal::toTemporalOverflow(state, options);
        auto result = TemporalObject::parseTemporalMonthDayString(state, item.asString()->toNonGCUTF8StringData());
        auto calendar = TemporalCalendarObject::toTemporalCalendarWithISODefault(state, Value(result.calendar));

        if (result.year == 0) {
            return createTemporalMonthDay(state, result.month, result.day, calendar.asObject()->asTemporalCalendarObject(), referenceISOYear);
        }

        return TemporalCalendarObject::calendarMonthDayFromFields(state, calendar, createTemporalMonthDay(state, result.month, result.day, calendar.asObject()->asTemporalCalendarObject(), referenceISOYear), options);
    }

    TemporalCalendarObject* calendar = nullptr;
    bool calendarAbsent = true;
    Object* itemObject = item.asObject();

    if (itemObject->isTemporalPlainMonthDayObject()) {
        return item;
    }

    if (itemObject->isTemporalPlainDateObject() || itemObject->isTemporalPlainDateTimeObject() || itemObject->isTemporalPlainTimeObject() || itemObject->isTemporalPlainYearMonthObject() || itemObject->isTemporalZonedDateTimeObject()) {
        calendarAbsent = false;
    } else {
        Value calendarLike = itemObject->get(state, state.context()->staticStrings().calendar).value(state, itemObject);
        calendarAbsent = !calendarLike.isUndefined();
        calendar = TemporalCalendarObject::toTemporalCalendarWithISODefault(state, calendarLike).asObject()->asTemporalCalendarObject();
    }

    ValueVector fieldNames = TemporalCalendarObject::calendarFields(state, calendar, { new ASCIIString("day"), new ASCIIString("month"), new ASCIIString("monthCode"), new ASCIIString("year") });
    Value fields = Temporal::prepareTemporalFields(state, item, fieldNames, {});
    Value month = fields.asObject()->get(state, state.context()->staticStrings().lazyMonth()).value(state, fields);
    Value monthCode = fields.asObject()->get(state, state.context()->staticStrings().lazymonthCode()).value(state, fields);
    Value year = fields.asObject()->get(state, state.context()->staticStrings().lazyYear()).value(state, fields);

    if (calendarAbsent && month.isUndefined() && monthCode.isUndefined() && year.isUndefined()) {
        fields.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().lazyYear()), ObjectPropertyDescriptor(Value(referenceISOYear), ObjectPropertyDescriptor::AllPresent));
    }

    return TemporalCalendarObject::calendarMonthDayFromFields(state, calendar, fields, options);
}

TemporalObject::DateTime::DateTime(int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, String* calendar, TemporalObject::TimeZone* tz)
    : year(year)
    , month(month)
    , day(day)
    , hour(hour)
    , minute(minute)
    , second(second)
    , millisecond(millisecond)
    , microsecond(microsecond)
    , nanosecond(nanosecond)
    , calendar(calendar)
    , tz(tz)
{
}

TemporalObject::TimeZone::TimeZone(bool z, String* offsetString, String* name)
    : z(z)
    , offsetString(offsetString)
    , name(name)
{
}
} // namespace Escargot

#endif
