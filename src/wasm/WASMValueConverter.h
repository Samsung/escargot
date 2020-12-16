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

#ifndef __EscargotWASMValueConverter__
#define __EscargotWASMValueConverter__

namespace Escargot {

class WASMValueConverter {
public:
    static Value wasmToJSValue(ExecutionState& state, const wasm_val_t& value);
    static wasm_val_t wasmToWebAssemblyValue(ExecutionState& state, const Value& value, wasm_valkind_t type);
    static wasm_val_t wasmDefaultValue(wasm_valkind_t type);
};
}
#endif // __EscargotWASMValueConverter__
#endif // ENABLE_WASM
