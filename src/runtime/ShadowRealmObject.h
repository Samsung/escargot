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

class ShadowRealmObject : public DerivedObject {
public:
    explicit ShadowRealmObject(ExecutionState& state);
    explicit ShadowRealmObject(ExecutionState& state, Object* proto);

    Context* realmContext()
    {
        return m_realmContext;
    }

    void setRealmContext(Context* context)
    {
        m_realmContext = context;
    }

    virtual bool isShadowRealmObject() const override
    {
        return true;
    }

    static Value getWrappedValue(ExecutionState& state, Context* callerRealm, const Value& value);
    static Value performShadowRealmEval(ExecutionState& state, Value& sourceText, Context* callerRealm, Context* evalRealm);

private:
    Context* m_realmContext;
};

#endif // defined(ENABLE_SHADOWREALM)

} // namespace Escargot
#endif
