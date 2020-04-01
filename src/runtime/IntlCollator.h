#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
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


#ifndef __EscargotIntlCollator__
#define __EscargotIntlCollator__

#include "runtime/Object.h"

namespace Escargot {

class IntlCollator {
public:
    struct CollatorResolvedOptions {
        String* locale;
        String* sensitivity;
        String* usage;
        String* collation;
        bool numeric;
        bool ignorePunctuation;
        String* caseFirst;
    };

    static Object* create(ExecutionState& state, Context* realm, Value locales, Value options);
    static CollatorResolvedOptions resolvedOptions(ExecutionState& state, Object* internalSlot);
    static void initialize(ExecutionState& state, Object* collator, Context* realm, Value locales, Value options);
    static int compare(ExecutionState& state, Object* collator, String* a, String* b);
};

} // namespace Escargot
#endif
#endif
