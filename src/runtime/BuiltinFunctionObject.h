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

#ifndef __EscargotBuiltinFunctionObject__
#define __EscargotBuiltinFunctionObject__

#include "runtime/NativeFunctionObject.h"

namespace Escargot {

class BuiltinFunctionObject : public NativeFunctionObject {
public:
    BuiltinFunctionObject(ExecutionState& state, NativeFunctionInfo info);
    BuiltinFunctionObject(ExecutionState& state, CodeBlock* codeBlock);

    enum ForBuiltinProxyConstructor { __ForBuiltinProxyConstructor__ };
    BuiltinFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinProxyConstructor);

    virtual bool isBuiltin() override
    {
        return true;
    }

private:
};
}

#endif
