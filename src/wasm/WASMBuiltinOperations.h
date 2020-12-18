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

#ifndef __EscargotWASMBuiltinOperations__
#define __EscargotWASMBuiltinOperations__

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
} // namespace Escargot
#endif // __EscargotWASMBuiltinOperations__
#endif // ENABLE_WASM
