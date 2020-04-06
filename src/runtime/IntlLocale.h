#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
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


#ifndef __EscargotIntlLocale__
#define __EscargotIntlLocale__

#include "runtime/Object.h"

namespace Escargot {

class IntlLocaleObject : public Object {
public:
    IntlLocaleObject(ExecutionState& state, String* tag, Optional<Object*> options);
    IntlLocaleObject(ExecutionState& state, Object* proto, String* tag, Optional<Object*> options);

    String* locale();
    String* language()
    {
        return m_language;
    }

    String* baseName()
    {
        return m_baseName;
    }

    Optional<String*> calendar()
    {
        return m_calendar;
    }

    Optional<String*> caseFirst()
    {
        return m_caseFirst;
    }

    Optional<String*> collation()
    {
        return m_collation;
    }

    Optional<String*> hourCycle()
    {
        return m_hourCycle;
    }

    Optional<String*> numeric()
    {
        return m_numeric;
    }

    Optional<String*> numberingSystem()
    {
        return m_numberingSystem;
    }

    virtual bool isIntlLocaleObject() const override
    {
        return true;
    }

protected:
    String* m_language;
    String* m_baseName;
    Optional<String*> m_calendar; // ca
    Optional<String*> m_caseFirst; // kf
    Optional<String*> m_collation; // co
    Optional<String*> m_hourCycle; // hc
    Optional<String*> m_numeric; // kn
    Optional<String*> m_numberingSystem; // nu
};

} // namespace Escargot
#endif
#endif
