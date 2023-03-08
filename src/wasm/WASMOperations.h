/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotWASMOperations__
#define __EscargotWASMOperations__

struct wasm_module_t;
struct wasm_instance_t;
struct wasm_extern_vec_t;

namespace Escargot {

class WASMOperations {
public:
    static Value copyStableBufferBytes(ExecutionState& state, Value source);
    static Value compileModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    static Object* asyncCompileModule(ExecutionState& state, Value source);

    static Object* createExportsObject(ExecutionState& state, wasm_module_t* module, wasm_instance_t* instance);
    static void readImportsOfModule(ExecutionState& state, wasm_module_t* module, const Value& importObj, wasm_extern_vec_t* imports);
    static Value instantiateCoreModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    static Object* instantiatePromiseOfModuleWithImportObject(ExecutionState& state, PromiseObject* promiseOfModule, Value importObj);
};

} // namespace Escargot
#endif // __EscargotWASMOperations__
#endif // ENABLE_WASM
