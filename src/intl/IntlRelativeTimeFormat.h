#if defined(ENABLE_ICU) && defined(ENABLE_INTL) && defined(ENABLE_INTL_RELATIVETIMEFORMAT)
/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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


#ifndef __EscargotIntlRelativeTimeFormat__
#define __EscargotIntlRelativeTimeFormat__

#include "runtime/Object.h"

namespace Escargot {

class IntlRelativeTimeFormatObject : public DerivedObject {
public:
    IntlRelativeTimeFormatObject(ExecutionState& state, Value locales, Value options);
    IntlRelativeTimeFormatObject(ExecutionState& state, Object* proto, Value locales, Value options);

    virtual bool isIntlRelativeTimeFormatObject() const override
    {
        return true;
    }

    String* locale() const
    {
        return m_locale;
    }

    String* dataLocale() const
    {
        return m_dataLocale;
    }

    String* numberingSystem() const
    {
        return m_numberingSystem;
    }

    String* style() const
    {
        return m_style;
    }

    String* numeric() const
    {
        return m_numeric;
    }

    String* format(ExecutionState& state, double value, String* unit);
    ArrayObject* formatToParts(ExecutionState& state, double value, String* unit);

protected:
    String* m_locale;
    String* m_dataLocale;
    String* m_numberingSystem;
    String* m_style;
    String* m_numeric;

    URelativeDateTimeFormatter* m_icuRelativeDateTimeFormatter;
};

} // namespace Escargot
#endif
#endif
