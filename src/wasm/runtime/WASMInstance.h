/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_WASM_INTERPRETER)

#ifndef __EscargotWASMInstance__
#define __EscargotWASMInstance__

#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

class WASMModule;
class WASMFunction;

class WASMInstance : public gc {
    friend class WASMModule;

public:
    WASMInstance(WASMModule* module)
        : m_module(module)
    {
    }

    WASMModule* module() const
    {
        return m_module;
    }

    WASMFunction* function(uint32_t index) const
    {
        return m_function[index];
    }

private:
    WASMModule* m_module;
    TightVector<WASMFunction*, GCUtil::gc_malloc_allocator<WASMFunction*>> m_function;
};

} // namespace Escargot

#endif // __EscargotWASMFunction__
#endif // ENABLE_WASM_INTERPRETER
