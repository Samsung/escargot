#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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


#ifndef __EscargotIntlDateTimeFormat__
#define __EscargotIntlDateTimeFormat__

#include "runtime/Object.h"

namespace Escargot {

class IntlDateTimeFormatObject : public DerivedObject {
public:
    IntlDateTimeFormatObject(ExecutionState& state, Value locales, Value options, Optional<String*> toLocaleStringTimeZone = NullOption);
    IntlDateTimeFormatObject(ExecutionState& state, Object* proto, Value locales, Value options, Optional<String*> toLocaleStringTimeZone = NullOption);

    virtual bool isIntlDateTimeFormatObject() const override
    {
        return true;
    }

    UTF16StringDataNonGCStd format(ExecutionState& state, Value x, bool allowZonedDateTime = false);
    UTF16StringDataNonGCStd format(ExecutionState& state, double x);
    ArrayObject* formatToParts(ExecutionState& state, double x);
    UTF16StringDataNonGCStd formatRange(ExecutionState& state, double startDate, double endDate);
    ArrayObject* formatRangeToParts(ExecutionState& state, double startDate, double endDate);
    static std::pair<Value, bool> toDateTimeOptions(ExecutionState& state, Value options, Value required, Value defaults);
    static std::string readHourCycleFromPattern(const UTF16StringDataNonGCStd& patternString);
    String* locale() const
    {
        return m_locale;
    }

    String* calendar() const
    {
        return m_calendar;
    }

    String* numberingSystem() const
    {
        return m_numberingSystem;
    }

    String* timeZone() const
    {
        return m_timeZone;
    }

    Value hour12() const
    {
        return m_hour12;
    }

    Value era() const
    {
        return m_era;
    }

    Value year() const
    {
        return m_year;
    }

    Value month() const
    {
        return m_month;
    }

    Value weekday() const
    {
        return m_weekday;
    }

    Value day() const
    {
        return m_day;
    }

    Value dayPeriod() const
    {
        return m_dayPeriod;
    }

    Value hour() const
    {
        return m_hour;
    }

    Value hourCycle() const
    {
        return m_hourCycle;
    }

    Value minute() const
    {
        return m_minute;
    }

    Value second() const
    {
        return m_second;
    }

    Value timeZoneName() const
    {
        return m_timeZoneName;
    }

    Value fractionalSecondDigits() const
    {
        return m_fractionalSecondDigits;
    }

    Value dateStyle() const
    {
        return m_dateStyle;
    }

    Value timeStyle() const
    {
        return m_timeStyle;
    }

    UDateFormat* icuDateFormat()
    {
        return m_icuDateFormat;
    }

    bool allOptionsUndefined();

protected:
    static String* initDateTimeFormatMainHelper(ExecutionState& state, StringMap& opt, Object* options, const Value& hour12, StringBuilder& skeletonBuilder);
    struct DateTimeFormatOtherHelperResult {
        Optional<UDateFormat*> icuDateFormat;
        Value newHourCycle;
        Optional<String*> timeZoneICU;
    };
    static DateTimeFormatOtherHelperResult initDateTimeFormatOtherHelper(ExecutionState& state, Optional<IntlDateTimeFormatObject*> dateObject, const Value& dataLocale, String* timeZone, const Value& dateStyle, const Value& timeStyle, const Value& computedHourCycle, const Value& hourCycle, const Value& hour12, String* hour, const StringMap& opt, StringBuilder& skeletonBuilder);
    void setDateFromPattern(ExecutionState& state, UTF16StringDataNonGCStd& patternBuffer, bool hasHourOption);
    void initICUIntervalFormatIfNecessary(ExecutionState& state);
    UTF16StringDataNonGCStd format(ExecutionState& state, UDateFormat* dateFormat, double x);

    bool m_wasThereNoFormatOption;

    String* m_locale;
    String* m_dataLocale;
    String* m_calendar;
    String* m_numberingSystem;
    String* m_timeZone;
    String* m_timeZoneICU;

    EncodedValue m_hour12;
    EncodedValue m_era;
    EncodedValue m_year;
    EncodedValue m_month;
    EncodedValue m_weekday;
    EncodedValue m_day;
    EncodedValue m_dayPeriod;
    EncodedValue m_hour;
    EncodedValue m_hourCycle;
    EncodedValue m_minute;
    EncodedValue m_second;
    EncodedValue m_timeZoneName;
    EncodedValue m_fractionalSecondDigits;
    EncodedValue m_dateStyle;
    EncodedValue m_timeStyle;

    UDateFormat* m_icuDateFormat;
    Optional<UDateIntervalFormat*> m_icuDateIntervalFormat;
};

} // namespace Escargot
#endif
#endif
