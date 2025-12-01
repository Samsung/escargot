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
#include "util/ISO8601.h"

namespace Escargot {

// https://tc39.es/ecma402/#sec-todurationrecord
using DurationRecord = ISO8601::Duration;

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
        ISO8601::DateTimeUnit m_unit;
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
