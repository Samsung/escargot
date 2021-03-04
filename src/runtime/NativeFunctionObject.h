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

#ifndef __EscargotNativeFunctionObject__
#define __EscargotNativeFunctionObject__

#include "runtime/FunctionObject.h"

namespace Escargot {

class NativeFunctionObject : public FunctionObject {
public:
    NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info);

    enum ForGlobalBuiltin { __ForGlobalBuiltin__ };
    NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForGlobalBuiltin);

    enum ForBuiltinConstructor { __ForBuiltinConstructor__ };
    NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForBuiltinConstructor);

    enum ForBuiltinProxyConstructor { __ForBuiltinProxyConstructor__ };
    NativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, ForBuiltinProxyConstructor);

    virtual bool isNativeFunctionObject() const override
    {
        return true;
    }

    virtual bool isConstructor() const override;

    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override;
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;

    NativeCodeBlock* nativeCodeBlock() const
    {
        ASSERT(m_codeBlock->isNativeCodeBlock());
        return (NativeCodeBlock*)m_codeBlock;
    }

protected:
    NativeFunctionObject(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info);

    template <bool isConstruct, bool shouldReturnsObjectOnConstructCall = true>
    ALWAYS_INLINE Value processNativeFunctionCall(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv, Optional<Object*> newTarget);
};
} // namespace Escargot

#endif
