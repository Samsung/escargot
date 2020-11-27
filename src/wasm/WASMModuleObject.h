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

#ifndef __EscargotWASMModuleObject__
#define __EscargotWASMModuleObject__

struct wasm_module_t;

namespace Escargot {

class WASMModuleObject : public Object {
public:
    enum ImportExportKind {
        Function,
        Table,
        Memory,
        Global
    };

    explicit WASMModuleObject(ExecutionState& state, wasm_module_t* module);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_module_t* module() const { return m_module; }
    ImportExportKind kind() const { return m_kind; }

private:
    wasm_module_t* m_module;
    ImportExportKind m_kind;
};
}
#endif // __EscargotWASMModuleObject__

#endif // ENABLE_WASM
