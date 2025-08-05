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
    IntlLocaleObject(ExecutionState& state, String* tag, Object* options);
    IntlLocaleObject(ExecutionState& state, Object* proto, String* tag, Object* options);

    String* locale() const;
    String* language() const;
    String* region() const;
    String* script() const;
    String* variants() const;
    String* baseName() const;
    Optional<String*> calendar() const;
    Optional<String*> caseFirst() const;
    Optional<String*> collation() const;
    Optional<String*> firstDayOfWeek() const;
    Optional<String*> hourCycle() const;
    Optional<String*> numeric() const;
    Optional<String*> numberingSystem() const;

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
    String* m_localeID;
};

} // namespace Escargot
#endif
#endif
