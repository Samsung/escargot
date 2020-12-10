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

#ifndef __EscargotWASMOperations__
#define __EscargotWASMOperations__

#include "wasm.h"

// represent ownership of each object
// object marked with 'own' should be deleted in the current context
#define own

namespace Escargot {

static Value wasmGetValueFromMaybeObject(ExecutionState& state, const Value& obj, const AtomicString& property)
{
    Value result = obj;
    if (obj.isObject()) {
        auto desc = obj.asObject()->get(state, property);
        if (desc.hasValue()) {
            Value method = desc.value(state, obj);
            if (method.isCallable()) {
                result = Object::call(state, method, obj, 0, nullptr);
            }
        }
    }

    return result;
}

static std::pair<bool, Value> wasmGetValueFromObjectProperty(ExecutionState& state, const Value& obj, const AtomicString& property, const AtomicString& innerProperty)
{
    if (LIKELY(obj.isObject())) {
        auto desc = obj.asObject()->get(state, property);
        if (desc.hasValue()) {
            Value result = desc.value(state, obj);

            // handle inner property
            if (result.isObject() && innerProperty.string()->length()) {
                desc = result.asObject()->get(state, innerProperty);
                if (desc.hasValue()) {
                    Value method = desc.value(state, result);
                    if (method.isCallable()) {
                        result = Object::call(state, method, result, 0, nullptr);
                    }
                }
            }

            return std::make_pair(true, result);
        }
    }

    return std::make_pair(false, Value());
}

#define WASM_I32_VAL(i)                     \
    {                                       \
        .kind = WASM_I32, .of = {.i32 = i } \
    }
#define WASM_I64_VAL(i)                     \
    {                                       \
        .kind = WASM_I64, .of = {.i64 = i } \
    }
#define WASM_F32_VAL(z)                     \
    {                                       \
        .kind = WASM_F32, .of = {.f32 = z } \
    }
#define WASM_F64_VAL(z)                     \
    {                                       \
        .kind = WASM_F64, .of = {.f64 = z } \
    }
#define WASM_REF_VAL(r)                        \
    {                                          \
        .kind = WASM_ANYREF, .of = {.ref = r } \
    }
#define WASM_INIT_VAL                             \
    {                                             \
        .kind = WASM_ANYREF, .of = {.ref = NULL } \
    }

static wasm_val_t wasmDefaultValue(wasm_valkind_t type)
{
    wasm_val_t result;
    switch (type) {
    case WASM_I32: {
        result = WASM_I32_VAL(0);
        break;
    }
    case WASM_I64: {
        result = WASM_I64_VAL(0);
        break;
    }
    case WASM_F32: {
        result = WASM_F32_VAL(0);
        break;
    }
    case WASM_F64: {
        result = WASM_F64_VAL(0);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

static wasm_val_t wasmToWebAssemblyValue(ExecutionState& state, const Value& value, wasm_valkind_t type)
{
    wasm_val_t result;
    switch (type) {
    case WASM_I32: {
        int32_t val = value.toInt32(state);
        result = WASM_I32_VAL(val);
        break;
    }
    case WASM_F32: {
        // FIXME Let f32 be ? ToNumber(v) rounded to the nearest representable value using IEEE 754-2019 round to nearest, ties to even mode.
        float32_t val = value.toNumber(state);
        result = WASM_F32_VAL(val);
        break;
    }
    case WASM_F64: {
        float64_t val = value.toNumber(state);
        result = WASM_F64_VAL(val);
        break;
    }
    case WASM_I64: {
        int64_t val = value.toBigInt(state)->toInt64();
        result = WASM_I64_VAL(val);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

static String* wasmStringValueOfExternType(ExecutionState& state, wasm_externkind_t kind)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    String* result = nullptr;
    switch (kind) {
    case WASM_EXTERN_FUNC:
        result = strings->function.string();
        break;
    case WASM_EXTERN_TABLE:
        result = strings->table.string();
        break;
    case WASM_EXTERN_MEMORY:
        result = strings->memory.string();
        break;
    case WASM_EXTERN_GLOBAL:
        result = strings->global.string();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return result;
}

static Value wasmCopyStableBufferBytes(ExecutionState& state, Value source)
{
    Value copyBuffer = source;
    if (LIKELY(source.isObject())) {
        // copy only ArrayBuffer or TypedArray
        Object* srcObject = source.asObject();
        if (srcObject->isArrayBufferObject()) {
            ArrayBufferObject* srcBuffer = srcObject->asArrayBufferObject();
            copyBuffer = ArrayBufferObject::cloneArrayBuffer(state, srcBuffer, 0, srcBuffer->byteLength(), state.context()->globalObject()->arrayBuffer());
        } else if (srcObject->isTypedArrayObject()) {
            TypedArrayObject* srcArray = srcObject->asTypedArrayObject();
            ArrayBufferObject* srcBuffer = srcArray->buffer();
            copyBuffer = ArrayBufferObject::cloneArrayBuffer(state, srcBuffer, srcArray->byteOffset(), srcArray->arrayLength() * srcArray->elementSize(), state.context()->globalObject()->arrayBuffer());
        }
    }

    return copyBuffer;
}

static Value wasmCompileModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argc > 0);
    Value source = argv[0];
    // source should be ArrayBufferObject
    if (!source.isPointerValue() || !source.asPointerValue()->isArrayBufferObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().compile.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    ArrayBufferObject* srcBuffer = source.asPointerValue()->asArrayBufferObject();
    ASSERT(!srcBuffer->isDetachedBuffer());
    size_t byteLength = srcBuffer->byteLength();

    wasm_byte_vec_t binary;
    wasm_byte_vec_new_uninitialized(&binary, byteLength);
    memcpy(binary.data, srcBuffer->data(), byteLength);

    wasm_module_t* module = wasm_module_new(state.context()->vmInstance()->wasmStore(), &binary);
    wasm_byte_vec_delete(&binary);

    if (!module) {
        // throw WebAssembly.CompileError
        ErrorObject::throwBuiltinError(state, ErrorObject::WASMCompileError, ErrorObject::Messages::WASM_CompileError);
        return Value();
    }

    // Construct a WebAssembly module object
    return new WASMModuleObject(state, module);
}

static Value wasmInstantiateModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argc > 0);
    ASSERT(argv[0].isObject() && argv[0].asObject()->isWASMModuleObject());
    WASMModuleObject* moduleObj = argv[0].asPointerValue()->asWASMModuleObject();
    wasm_module_t* module = moduleObj->module();

    // TODO
    wasm_importtype_vec_t import_types;
    wasm_module_imports(module, &import_types);

    wasm_importtype_vec_delete(&import_types);

    return Value();
}
}

#endif // __EscargotWASMOperations__
#endif // ENABLE_WASM
