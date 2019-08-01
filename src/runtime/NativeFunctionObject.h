/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
    NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info);
    NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock);
    enum ForGlobalBuiltin { __ForGlobalBuiltin__ };
    NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin);

    virtual bool isNativeFunctionObject() const override
    {
        return true;
    }

    friend class FunctionObjectProcessCallGenerator;
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget) override;

protected:
    Value processNativeFunctionCall(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv, bool isNewExpression, const Value& newTarget);
    NativeFunctionObject(ExecutionState& state, size_t defaultSpace); // function for derived classes. derived class MUST initlize member variable of FunctionObject.
};
}

#endif
