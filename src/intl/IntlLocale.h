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

class IntlLocaleObject : public DerivedObject {
public:
    IntlLocaleObject(ExecutionState& state, String* tag, Optional<Object*> options);
    IntlLocaleObject(ExecutionState& state, Object* proto, String* tag, Optional<Object*> options);

    String* locale() const
    {
        return m_locale;
    }

    String* language() const
    {
        return m_language;
    }

    Value region() const
    {
        return m_region->length() ? Value(m_region) : Value();
    }

    Value script() const
    {
        return m_script->length() ? Value(m_script) : Value();
    }

    String* baseName() const
    {
        return m_baseName;
    }

    Optional<String*> calendar() const
    {
        return m_calendar;
    }

    Optional<String*> caseFirst() const
    {
        return m_caseFirst;
    }

    Optional<String*> collation() const
    {
        return m_collation;
    }

    Optional<String*> hourCycle() const
    {
        return m_hourCycle;
    }

    Optional<String*> numeric() const
    {
        return m_numeric;
    }

    Optional<String*> numberingSystem() const
    {
        return m_numberingSystem;
    }

    virtual bool isIntlLocaleObject() const override
    {
        return true;
    }

    Value calendars(ExecutionState& state);
    Value collations(ExecutionState& state);
    Value hourCycles(ExecutionState& state);
    Value numberingSystems(ExecutionState& state);
    Value textInfo(ExecutionState& state);
    Value weekInfo(ExecutionState& state);
    Value timeZones(ExecutionState& state);

protected:
    String* m_language;
    String* m_script;
    String* m_region;
    String* m_baseName;
    String* m_locale;
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
