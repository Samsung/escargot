#if defined(ENABLE_ICU) && defined(ENABLE_INTL) && defined(ENABLE_INTL_NUMBERFORMAT)
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


#ifndef __EscargotIntlNumberFormat__
#define __EscargotIntlNumberFormat__

#include "runtime/Object.h"
#include "intl/Intl.h"

namespace Escargot {

class IntlNumberFormat {
public:
    static Object* create(ExecutionState& state, Context* realm, Value locales, Value options);
    static void initialize(ExecutionState& state, Object* numberFormat, Value locales, Value options);
    static UTF16StringDataNonGCStd format(ExecutionState& state, Object* numberFormat, double x);
    static UTF16StringDataNonGCStd format(ExecutionState& state, Object* numberFormat, String* str);
    static String* formatRange(ExecutionState& state, Object* numberFormat, double x, double y);
    static String* formatRange(ExecutionState& state, Object* numberFormat, String* x, String* y);
    static ArrayObject* formatToParts(ExecutionState& state, Object* numberFormat, double x);
    static ArrayObject* formatRangeToParts(ExecutionState& state, Object* numberFormat, String* x, String* y);

private:
    static void initNumberFormatSkeleton(ExecutionState& state, const Intl::SetNumberFormatDigitOptionsResult& formatResult,
                                         const Value& style, const Value& currency, const Value& currencyDisplay, const Value& currencySign, const Value& unit, const Value& unitDisplay, const Value& compactDisplay, const Value& signDisplay, const Value& useGrouping, const Value& notation, UTF16StringDataNonGCStd& skeleton);
};

} // namespace Escargot
#endif
#endif
