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

#ifndef __EscargotBoundFunctionObject__
#define __EscargotBoundFunctionObject__

#include "runtime/Object.h"

namespace Escargot {

class BoundFunctionObject : public Object {
public:
    BoundFunctionObject(ExecutionState& state, Object* targetFunction, Value& boundThis, size_t boundArgc, Value* boundArgv, const Value& length, const Value& name);

    virtual bool isBoundFunctionObject() const override
    {
        return true;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        return Value(m_boundTargetFunction).isConstructor();
    }

    Value targetFunction()
    {
        return m_boundTargetFunction;
    }

    virtual Context* getFunctionRealm(ExecutionState& state) override
    {
        return m_boundTargetFunction->getFunctionRealm(state);
    }

private:
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t calledArgc, Value* calledArgv) override;
    virtual Object* construct(ExecutionState& state, const size_t calledArgc, Value* calledArgv, Object* newTarget) override;

    Object* m_boundTargetFunction;
    EncodedValue m_boundThis;
    EncodedValueVector m_boundArguments;
};
}

#endif
