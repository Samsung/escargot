#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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


#ifndef __EscargotIntlDisplayNames__
#define __EscargotIntlDisplayNames__

#include "runtime/Object.h"

namespace Escargot {

class IntlDisplayNamesObject : public Object {
public:
    IntlDisplayNamesObject(ExecutionState& state, Object* proto, Value locales, Value options);

    virtual bool isIntlDisplayNamesObject() const override
    {
        return true;
    }

    String* locale() const
    {
        return m_locale;
    }

    String* style() const
    {
        return m_style;
    }

    String* type() const
    {
        return m_type;
    }

    String* fallback() const
    {
        return m_fallback;
    }

    String* languageDisplay() const
    {
        return m_languageDisplay;
    }

protected:
    String* m_style;
    String* m_type;
    String* m_fallback;
    String* m_locale;
    String* m_languageDisplay;

    ULocaleDisplayNames* m_icuLocaleDisplayNames;
};

} // namespace Escargot
#endif
#endif
