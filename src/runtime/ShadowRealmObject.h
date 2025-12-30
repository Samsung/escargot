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

#ifndef __EscargotShadowRealmObject__
#define __EscargotShadowRealmObject__

#include "runtime/Object.h"
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/WrappedFunctionObject.h"

namespace Escargot {

#if defined(ENABLE_SHADOWREALM)

class Script;
class ShadowRealmObject : public DerivedObject {
public:
    explicit ShadowRealmObject(ExecutionState& state, Object* proto, Context* realmContext, Script* referrer);

    Context* realmContext()
    {
        return m_realmContext;
    }

    virtual bool isShadowRealmObject() const override
    {
        return true;
    }

    // https://tc39.es/proposal-shadowrealm/#sec-getwrappedvalue
    static Value wrappedValue(ExecutionState& state, Context* callerRealm, const Value& value);
    // https://tc39.es/proposal-shadowrealm/#sec-performshadowrealmeval
    Value eval(ExecutionState& state, String* sourceText, Context* callerRealm);
    // https://tc39.es/proposal-shadowrealm/#sec-shadowrealmimportvalue
    Value importValue(ExecutionState& state, String* specifierString, String* exportNameString, Context* callerRealm);

private:
    Context* m_realmContext;
    Script* m_referrer;
};

#endif // defined(ENABLE_SHADOWREALM)

} // namespace Escargot
#endif
