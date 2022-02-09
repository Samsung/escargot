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

#include "Escargot.h"
#include "wasm.h"
#include "runtime/Context.h"
#include "runtime/Object.h"
#include "runtime/ArrayObject.h"
#include "runtime/NativeFunctionObject.h"
#include "wasm/WASMObject.h"
#include "wasm/WASMValueConverter.h"
#include "wasm/ExportedFunctionObject.h"
#include "wasm/WASMOperations.h"

// represent ownership of each object
// object marked with 'own' should be deleted in the current context
#define own

namespace Escargot {

ExportedFunctionObject::ExportedFunctionObject(ExecutionState& state, NativeFunctionInfo info, wasm_func_t* func)
    : NativeFunctionObject(state, info)
    , m_function(func)
{
    ASSERT(!!m_function);

    addFinalizer([](Object* obj, void* data) {
        ExportedFunctionObject* self = (ExportedFunctionObject*)obj;
        wasm_func_delete(self->function());
    },
                 nullptr);
}

void* ExportedFunctionObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ExportedFunctionObject)] = { 0 };
        FunctionObject::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ExportedFunctionObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

static Value callExportedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExportedFunctionObject* callee = state.resolveCallee()->asExportedFunctionObject();
    wasm_func_t* funcaddr = callee->function();

    // Let functype be func_type(store, funcaddr).
    own wasm_functype_t* functype = wasm_func_type(funcaddr);

    // Let [parameters] ? [results] be functype.
    const wasm_valtype_vec_t* parameters = wasm_functype_params(functype);
    const wasm_valtype_vec_t* results = wasm_functype_results(functype);

    // Let args be << >>
    wasm_val_t* argsBuffer = ALLOCA(parameters->size * sizeof(wasm_val_t), wasm_val_t, state);
    wasm_val_vec_t args = { parameters->size, argsBuffer };

    // For each t of parameters,
    for (size_t i = 0; i < parameters->size; i++) {
        // If argValues?s size > i, let arg be argValues[i].
        // Otherwise, let arg be undefined.
        Value arg = (argc > i) ? argv[i] : Value();

        // Append ToWebAssemblyValue(arg, t) to args.
        args.data[i] = WASMValueConverter::wasmToWebAssemblyValue(state, arg, wasm_valtype_kind(parameters->data[i]));
    }

    wasm_val_t* retBuffer = ALLOCA(results->size * sizeof(wasm_val_t), wasm_val_t, state);
    wasm_val_vec_t ret = { results->size, retBuffer };

    wasm_functype_delete(functype);

    // Let (store, ret) be the result of func_invoke(store, funcaddr, args).
    own wasm_trap_t* trap = wasm_func_call(funcaddr, args.data, ret.data);

    // If ret is error, throw an exception. This exception should be a WebAssembly RuntimeError exception, unless otherwise indicated by the WebAssembly error mapping.
    if (trap) {
        own wasm_name_t message;
        wasm_trap_message(trap, &message);
        ESCARGOT_LOG_ERROR("[WASM Message] %s\n", message.data);
        wasm_name_delete(&message);
        wasm_trap_delete(trap);
        ErrorObject::throwBuiltinError(state, ErrorObject::WASMRuntimeError, ErrorObject::Messages::WASM_FuncCallError);
        return Value();
    }

    // Let outArity be the size of ret.
    size_t outArity = ret.size;
    if (outArity == 0) {
        // If outArity is 0, return undefined.
        return Value();
    } else if (outArity == 1) {
        // Otherwise, if outArity is 1, return ToJSValue(ret[0]).
        return WASMValueConverter::wasmToJSValue(state, ret.data[0]);
    }

    // Otherwise,
    // Let values be << >>.
    ValueVector values;
    values.resizeWithUninitializedValues(ret.size);

    // For each r of ret,
    // Append ToJSValue(r) to values.
    for (size_t i = 0; i < ret.size; i++) {
        values[i] = WASMValueConverter::wasmToJSValue(state, ret.data[i]);
    }

    // Return CreateArrayFromList(values).
    return Object::createArrayFromList(state, values);
}

ExportedFunctionObject* ExportedFunctionObject::createExportedFunction(ExecutionState& state, wasm_func_t* funcaddr, uint32_t index)
{
    ASSERT(!!funcaddr);

    wasm_ref_t* funcref = wasm_func_as_ref(funcaddr);

    // Let map be the surrounding agent's associated Exported Function cache.
    // If map[funcaddr] exists, Return map[funcaddr].
    WASMFunctionMap& map = state.context()->wasmCache()->functionMap;
    for (auto iter = map.begin(); iter != map.end(); iter++) {
        wasm_ref_t* ref = iter->first;
        if (wasm_ref_same(funcref, ref)) {
            return iter->second;
        }
    }

    // Let steps be "call the Exported Function funcaddr with arguments."
    // Let realm be the current Realm.
    // Let function be CreateBuiltinFunction(realm, steps, %FunctionPrototype%, ? [[FunctionAddress]] ?).
    // Set function.[[FunctionAddress]] to funcaddr.

    // Let functype be func_type(store, funcaddr).
    // Let [paramTypes] -> [resultTypes] be functype.
    // Let arity be paramTypes?s size.
    size_t arity = wasm_func_param_arity(funcaddr);

    // Let name be the name of the WebAssembly function funcaddr.
    // Perform ! SetFunctionName(function, name).
    AtomicString name = (index < ESCARGOT_STRINGS_NUMBERS_MAX) ? state.context()->staticStrings().numbers[index] : AtomicString(state.context(), String::fromDouble(index));

    // Perform ! SetFunctionLength(function, arity).
    // Let steps be "call the Exported Function funcaddr with arguments."
    // Let realm be the current Realm.
    // Let function be CreateBuiltinFunction(realm, steps, %FunctionPrototype%, ? [[FunctionAddress]] ?).
    // Set function.[[FunctionAddress]] to funcaddr.
    ExportedFunctionObject* function = new ExportedFunctionObject(state, NativeFunctionInfo(name, callExportedFunction, arity, NativeFunctionInfo::Strict), funcaddr);

    // Set map[funcaddr] to function.
    map.pushBack(std::make_pair(funcref, function));

    // Return function.
    return function;
}
} // namespace Escargot

#endif // ENABLE_WASM
