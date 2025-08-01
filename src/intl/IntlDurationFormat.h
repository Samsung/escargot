#if defined(ENABLE_ICU) && defined(ENABLE_INTL) && defined(ENABLE_INTL_DURATIONFORMAT)
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


#ifndef __EscargotIntlDurationFormat__
#define __EscargotIntlDurationFormat__

#include "runtime/Object.h"
#include "runtime/BigInt.h"

namespace Escargot {

// https://tc39.es/ecma402/#sec-todurationrecord
class DurationRecord {
#define FOR_EACH_DURATION_RECORD(F)  \
    F(years, Years, 0)               \
    F(months, Months, 1)             \
    F(weeks, Weeks, 2)               \
    F(days, Days, 3)                 \
    F(hours, Hours, 4)               \
    F(minutes, Minutes, 5)           \
    F(seconds, Seconds, 6)           \
    F(milliseconds, Milliseconds, 7) \
    F(microseconds, Microseconds, 8) \
    F(nanoseconds, Nanoseconds, 9)

    std::array<double, 10> m_data;

public:
    enum class Type : uint8_t {
#define DEFINE_TYPE(name, Name, index) Name,
        FOR_EACH_DURATION_RECORD(DEFINE_TYPE)
#undef DEFINE_TYPE
    };

    DurationRecord()
    {
        m_data.fill(0);
    }

    static String* typeName(ExecutionState& state, Type t);
    BigIntData totalNanoseconds(DurationRecord::Type type) const;

    double operator[](DurationRecord::Type idx) const
    {
        return m_data[static_cast<unsigned>(idx)];
    }
    std::array<double, 10>::const_iterator begin() const { return m_data.begin(); }
    std::array<double, 10>::const_iterator end() const { return m_data.end(); }

#define DEFINE_GETTER(name, Name, index) \
    double name() const { return m_data[index]; }
    FOR_EACH_DURATION_RECORD(DEFINE_GETTER)
#undef DEFINE_GETTER

#define DEFINE_SETTER(name, Name, index) \
    void set##Name(double v) { m_data[index] = v; }
    FOR_EACH_DURATION_RECORD(DEFINE_SETTER)
#undef DEFINE_SETTER
};

class IntlDurationFormatObject : public DerivedObject {
public:
    IntlDurationFormatObject(ExecutionState& state, Value locales, Value options);
    IntlDurationFormatObject(ExecutionState& state, Object* proto, Value locales, Value options);

    virtual bool isIntlDurationFormatObject() const override
    {
        return true;
    }

    std::pair<String*, String*> data(size_t index);

    Object* resolvedOptions(ExecutionState& state);
    String* format(ExecutionState& state, const Value& duration);
    ArrayObject* formatToParts(ExecutionState& state, const Value& duration);

protected:
    enum class ElementType : uint8_t {
        Literal,
        Element,
    };

    struct Element {
        ElementType m_type;
        bool m_valueSignBit;
        DurationRecord::Type m_unit;
        UTF16StringDataNonGCStd m_string;
        LocalResourcePointer<UFormattedNumber> m_formattedNumber;
    };
    static std::vector<Element> collectElements(ExecutionState& state, IntlDurationFormatObject* durationFormat, const DurationRecord& duration);

    String* m_locale;
    String* m_dataLocale;
    String* m_dataLocaleWithExtensions;
    String* m_numberingSystem;
    String* m_style;

    String* m_yearsStyle;
    String* m_yearsDisplay;
    String* m_monthsStyle;
    String* m_monthsDisplay;
    String* m_weeksStyle;
    String* m_weeksDisplay;
    String* m_daysStyle;
    String* m_daysDisplay;
    String* m_hoursStyle;
    String* m_hoursDisplay;
    String* m_minutesStyle;
    String* m_minutesDisplay;
    String* m_secondsStyle;
    String* m_secondsDisplay;
    String* m_millisecondsStyle;
    String* m_millisecondsDisplay;
    String* m_microsecondsStyle;
    String* m_microsecondsDisplay;
    String* m_nanosecondsStyle;
    String* m_nanosecondsDisplay;

    Value m_fractionalDigits;

    UListFormatter* m_icuListFormatter;
};

} // namespace Escargot
#endif
#endif
