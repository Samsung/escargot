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

#ifndef __EscargotWrappedFunctionObject__
#define __EscargotWrappedFunctionObject__

#include "parser/CodeBlock.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/FunctionObject.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#if defined(ESCARGOT_ENABLE_SHADOWREALM)

class WrappedFunctionObject : public DerivedObject {
    friend class Object;
    friend class ObjectGetResult;
    friend class GlobalObject;
    friend class Script;

public:
    WrappedFunctionObject(ExecutionState& state, Object* wrappedTargetFunction, Context* realm, const Value& length, const Value& name);

    virtual bool isWrappedFunctionObject() const override
    {
        return true;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        return Value(m_wrappedTargetFunction).isConstructor();
    }

    virtual Context* getFunctionRealm(ExecutionState& state) override
    {
        return m_realm;
    }

private:
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t calledArgc, Value* calledArgv) override;

    Value ordinaryWrappedFunctionCall(ExecutionState& state, const Value& thisValue, const size_t calledArgc, Value* calledArgv);

    Object* m_wrappedTargetFunction;
    Context* m_realm;
};

#endif

} // namespace Escargot
#endif
