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

#if defined(ENABLE_WASM)

#ifndef __EscargotExportedFunctionObject__
#define __EscargotExportedFunctionObject__

#include "runtime/NativeFunctionObject.h"

struct wasm_func_t;

namespace Escargot {

class ExportedFunctionObject : public NativeFunctionObject {
public:
    explicit ExportedFunctionObject(ExecutionState& state, NativeFunctionInfo info, wasm_func_t* func);

    virtual bool isExportedFunctionObject() const override
    {
        return true;
    }

    // Exported Functions do not have a [[Construct]] method and thus it is not possible to call one with the new operator.

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static ExportedFunctionObject* createExportedFunction(ExecutionState& state, wasm_func_t* func, uint32_t index);

    wasm_func_t* function() const
    {
        ASSERT(!!m_function);
        return m_function;
    }

private:
    wasm_func_t* m_function;
};
} // namespace Escargot
#endif // __EscargotExportedFunctionObject__
#endif // ENABLE_WASM
