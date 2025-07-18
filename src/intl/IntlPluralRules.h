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

    String* resolvePlural(ExecutionState& state, double number);
    String* resolvePluralRange(ExecutionState& state, double x, double y);

    String* locale() const
    {
        return m_locale;
    }

    String* type() const
    {
        return m_type;
    }

    String* notation() const
    {
        return m_notation;
    }

    double minimumIntegerDigits() const
    {
        return m_minimumIntegerDigits;
    }

    double roundingIncrement() const
    {
        return m_roundingIncrement;
    }

    Value minimumFractionDigits() const
    {
        return m_minimumFractionDigits;
    }

    Value maximumFractionDigits() const
    {
        return m_maximumFractionDigits;
    }

    Value minimumSignificantDigits() const
    {
        return m_minimumSignificantDigits;
    }

    Value maximumSignificantDigits() const
    {
        return m_maximumSignificantDigits;
    }

    String* roundingMode() const
    {
        return m_roundingMode;
    }

    String* trailingZeroDisplay() const
    {
        return m_trailingZeroDisplay;
    }

    Intl::RoundingType roundingType() const
    {
        return m_roundingType;
    }

    Intl::RoundingPriority computedRoundingPriority() const
    {
        return m_computedRoundingPriority;
    }

    UPluralRules* icuPluralRules() const
    {
        return m_icuPluralRules;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* m_locale;
    String* m_type;
    String* m_notation;
    double m_minimumIntegerDigits;
    double m_roundingIncrement;
    Value m_minimumFractionDigits; // double or undefined
    Value m_maximumFractionDigits; // double or undefined
    Value m_minimumSignificantDigits; // double or undefined
    Value m_maximumSignificantDigits; // double or undefined
    String* m_roundingMode;
    String* m_trailingZeroDisplay;
    Intl::RoundingType m_roundingType;
    Intl::RoundingPriority m_computedRoundingPriority;

    UPluralRules* m_icuPluralRules;
    UNumberFormatter* m_icuNumberFormat;
    Optional<UNumberRangeFormatter*> m_icuNumberRangeFormat;
};

} // namespace Escargot
#endif
#endif
