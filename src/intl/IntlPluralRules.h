#if defined(ENABLE_ICU) && defined(ENABLE_INTL) && defined(ENABLE_INTL_PLURALRULES)
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


#ifndef __EscargotIntlPluralRules__
#define __EscargotIntlPluralRules__

#include "runtime/Object.h"

namespace Escargot {

class IntlPluralRulesObject : public DerivedObject {
public:
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-intl-pluralrules-constructor
    IntlPluralRulesObject(ExecutionState& state, Object* proto, Value locales, Value options);

    virtual bool isIntlPluralRulesObject() const override
    {
        return true;
    }

    String* resolvePlural(double number);

    String* locale() const
    {
        return m_locale;
    }

    String* type() const
    {
        return m_type;
    }

    double minimumIntegerDigits() const
    {
        return m_minimumIntegerDigits;
    }

    double minimumFractionDigits() const
    {
        return m_minimumFractionDigits;
    }

    double maximumFractionDigits() const
    {
        return m_maximumFractionDigits;
    }

    Optional<double> minimumSignificantDigits() const
    {
        return m_minimumSignificantDigits;
    }

    Optional<double> maximumSignificantDigits() const
    {
        return m_maximumSignificantDigits;
    }

    UPluralRules* icuPluralRules() const
    {
        return m_icuPluralRules;
    }

    UNumberFormat* icuNumberFormat() const
    {
        return m_icuNumberFormat;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* m_locale;
    String* m_type;
    double m_minimumIntegerDigits;
    double m_minimumFractionDigits;
    double m_maximumFractionDigits;
    Optional<double> m_minimumSignificantDigits;
    Optional<double> m_maximumSignificantDigits;

    UPluralRules* m_icuPluralRules;
    UNumberFormat* m_icuNumberFormat;
};

} // namespace Escargot
#endif
#endif
