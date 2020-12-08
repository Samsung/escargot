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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/Job.h"
#include "wasm/WASMObject.h"
#include "wasm.h"

namespace Escargot {

static Value copyStableBufferBytes(ExecutionState& state, Value source)
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

static Value compileWASMModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value instantiateWASMModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

// WebAssembly
static Value builtinWASMValidate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value source = argv[0];
    if (!source.isPointerValue() || (!source.asPointerValue()->isTypedArrayObject() && !source.asPointerValue()->isArrayBufferObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().validate.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    ArrayBufferObject* srcBuffer = copyStableBufferBytes(state, source).asPointerValue()->asArrayBufferObject();
    ASSERT(!srcBuffer->isDetachedBuffer());
    size_t byteLength = srcBuffer->byteLength();

    wasm_byte_vec_t binary;
    wasm_byte_vec_new_uninitialized(&binary, byteLength);
    memcpy(binary.data, srcBuffer->data(), byteLength);

    bool result = wasm_module_validate(state.context()->vmInstance()->wasmStore(), &binary);
    wasm_byte_vec_delete(&binary);

    return Value(result);
}

static Value builtinWASMCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value source = copyStableBufferBytes(state, argv[0]);

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    NativeFunctionObject* asyncCompiler = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), compileWASMModule, 1, NativeFunctionInfo::Strict));
    Job* job = new PromiseReactionJob(state.context(), PromiseReaction(asyncCompiler, capability), source);
    state.context()->vmInstance()->enqueuePromiseJob(capability.m_promise->asPromiseObject(), job);

    return capability.m_promise;
}

static Value builtinWASMInstantiate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value source = copyStableBufferBytes(state, argv[0]);

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    PromiseObject* promiseOfModule = capability.m_promise->asPromiseObject();

    NativeFunctionObject* asyncCompiler = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), compileWASMModule, 1, NativeFunctionInfo::Strict));
    Job* job = new PromiseReactionJob(state.context(), PromiseReaction(asyncCompiler, capability), source);
    state.context()->vmInstance()->enqueuePromiseJob(promiseOfModule, job);

    Object* importObj = argv[1].isObject() ? argv[1].asObject() : nullptr;
    auto moduleInstantiator = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), instantiateWASMModule, 1, NativeFunctionInfo::Strict));
    moduleInstantiator->setInternalSlot(0, importObj);

    return promiseOfModule->then(state, moduleInstantiator);
}

// WebAssembly.Module
static Value builtinWASMModuleConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Module.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value source = copyStableBufferBytes(state, argv[0]);
    Value arg[] = { source };
    return compileWASMModule(state, thisValue, 1, arg, newTarget);
}

// WebAssembly.Memory
static Value builtinWASMMemoryConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Memory.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    // check and get 'initial' property from the first argument
    Value initValue;
    Value desc = argv[0];
    if (LIKELY(desc.isObject())) {
        Object* descObj = desc.asObject();
        auto initDesc = descObj->get(state, state.context()->staticStrings().initial);
        if (initDesc.hasValue()) {
            initValue = initDesc.value(state, descObj);

            // handle valueOf property
            if (initValue.isObject()) {
                initDesc = initValue.asObject()->get(state, state.context()->staticStrings().valueOf);
                if (initDesc.hasValue()) {
                    Value valueOf = initDesc.value(state, initValue);
                    if (valueOf.isCallable()) {
                        initValue = Object::call(state, valueOf, initValue, 0, nullptr);
                    }
                }
            }
        }
    }

    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    uint32_t initial = initValue.asUInt32();

    // If descriptor["maximum"] is present, let maximum be descriptor["maximum"]; otherwise, let maximum be empty.
    // but we set "wasm_limits_max_default"(max value of uint32_t) as its default value
    uint32_t maximum = wasm_limits_max_default;
    {
        auto maxDesc = desc.asObject()->get(state, state.context()->staticStrings().maximum);
        if (maxDesc.hasValue()) {
            Value maxValue = maxDesc.value(state, desc);

            // handle valueOf property
            if (maxValue.isObject()) {
                maxDesc = maxValue.asObject()->get(state, state.context()->staticStrings().valueOf);
                if (maxDesc.hasValue()) {
                    Value valueOf = maxDesc.value(state, maxValue);
                    if (valueOf.isCallable()) {
                        maxValue = Object::call(state, valueOf, maxValue, 0, nullptr);
                    }
                }
            }

            if (!maxValue.isUInt32()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Memory.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Let memtype be { min initial, max maximum }
    wasm_limits_t limits = { initial, maximum };
    wasm_memorytype_t* memtype = wasm_memorytype_new(&limits);

    // Let (store, memaddr) be mem_alloc(store, memtype). If allocation fails, throw a RangeError exception.
    wasm_memory_t* memaddr = wasm_memory_new(state.context()->vmInstance()->wasmStore(), memtype);
    wasm_memorytype_delete(memtype);

    // Let map be the surrounding agent's associated Memory object cache.
    // Assert: map[memaddr] doesn’t exist.
    WASMMemoryMap& map = state.context()->wasmCache()->memoryMap;
    ASSERT(map.find(memaddr) == map.end());

    // Create a memory buffer from memaddr
    ArrayBufferObject* buffer = new ArrayBufferObject(state, ArrayBufferObject::FromExternalMemory);
    // FIXME wasm_memory_data with zero size returns null pointer
    // temporal address is allocated by calloc for this case
    void* dataBlock = initial == 0 ? calloc(0, 0) : wasm_memory_data(memaddr);
    buffer->attachBuffer(state, dataBlock, wasm_memory_data_size(memaddr));

    // Set memory.[[Memory]] to memaddr.
    // Set memory.[[BufferObject]] to buffer.
    WASMMemoryObject* memoryObj = new WASMMemoryObject(state, memaddr, buffer);

    // Set map[memaddr] to memory.
    map.insert(std::make_pair(memaddr, memoryObj));

    return memoryObj;
}

static Value builtinWASMMemoryGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMMemoryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = argv[0];
    // handle valueOf property
    if (UNLIKELY(deltaValue.isObject())) {
        auto deltaDesc = deltaValue.asObject()->get(state, state.context()->staticStrings().valueOf);
        if (deltaDesc.hasValue()) {
            Value valueOf = deltaDesc.value(state, deltaValue);
            if (valueOf.isCallable()) {
                deltaValue = Object::call(state, valueOf, deltaValue, 0, nullptr);
            }
        }
    }
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_memory_pages_t delta = deltaValue.asUInt32();

    // Let memaddr be this.[[Memory]].
    WASMMemoryObject* memoryObj = thisValue.asObject()->asWASMMemoryObject();
    wasm_memory_t* memaddr = memoryObj->memory();

    // Let ret be the mem_size(store, memaddr).
    // wasm_memory_pages_t maps to uint32_t
    wasm_memory_pages_t ret = wasm_memory_size(memaddr);

    // Let store be mem_grow(store, memaddr, delta).
    bool success = wasm_memory_grow(memaddr, delta);
    if (!success) {
        // If store is error, throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Let map be the surrounding agent's associated Memory object cache.
    // Assert: map[memaddr] exists.
    WASMMemoryMap& map = state.context()->wasmCache()->memoryMap;
    ASSERT(map.find(memaddr) != map.end());

    // Perform ! DetachArrayBuffer(memory.[[BufferObject]], "WebAssembly.Memory").
    memoryObj->buffer()->detachArrayBufferWithoutFree();

    // Let buffer be a the result of creating a memory buffer from memaddr.
    ArrayBufferObject* buffer = new ArrayBufferObject(state, ArrayBufferObject::FromExternalMemory);
    // FIXME wasm_memory_data with zero size returns null pointer
    // temporal address is allocated by calloc for this case
    size_t dataSize = wasm_memory_data_size(memaddr);
    void* dataBlock = dataSize == 0 ? calloc(0, 0) : wasm_memory_data(memaddr);
    buffer->attachBuffer(state, dataBlock, dataSize);

    // Set memory.[[BufferObject]] to buffer.
    memoryObj->setBuffer(buffer);

    // Return ret.
    return Value(ret);
}

static Value builtinWASMMemoryBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMMemoryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().buffer.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    return thisValue.asObject()->asWASMMemoryObject()->buffer();
}

// WebAssembly.Table
static Value builtinWASMTableConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Table.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];
    // check 'element' property from the first argument
    {
        Value elemValue;
        if (LIKELY(desc.isObject())) {
            Object* descObj = desc.asObject();
            auto elemDesc = descObj->get(state, state.context()->staticStrings().element);
            if (elemDesc.hasValue()) {
                elemValue = elemDesc.value(state, descObj);

                // handle toString property
                if (UNLIKELY(elemValue.isObject())) {
                    elemDesc = elemValue.asObject()->get(state, state.context()->staticStrings().toString);
                    if (elemDesc.hasValue()) {
                        Value toString = elemDesc.value(state, elemValue);
                        if (toString.isCallable()) {
                            elemValue = Object::call(state, toString, elemValue, 0, nullptr);
                        }
                    }
                }
            }
        }
        // element property should be 'anyfunc'
        if (UNLIKELY(!elemValue.isString() || !elemValue.asString()->equals(state.context()->staticStrings().anyfunc.string()))) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    }

    // check and get 'initial' property from the first argument
    ASSERT(desc.isObject());
    Value initValue;
    {
        auto initDesc = desc.asObject()->get(state, state.context()->staticStrings().initial);
        if (initDesc.hasValue()) {
            initValue = initDesc.value(state, desc);

            // handle valueOf property
            if (UNLIKELY(initValue.isObject())) {
                initDesc = initValue.asObject()->get(state, state.context()->staticStrings().valueOf);
                if (initDesc.hasValue()) {
                    Value valueOf = initDesc.value(state, initValue);
                    if (valueOf.isCallable()) {
                        initValue = Object::call(state, valueOf, initValue, 0, nullptr);
                    }
                }
            }
        }
    }
    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    uint32_t initial = initValue.asUInt32();

    // If descriptor["maximum"] is present, let maximum be descriptor["maximum"]; otherwise, let maximum be empty.
    // but we set "wasm_limits_max_default"(max value of uint32_t) as its default value
    uint32_t maximum = wasm_limits_max_default;
    {
        auto maxDesc = desc.asObject()->get(state, state.context()->staticStrings().maximum);
        if (maxDesc.hasValue()) {
            Value maxValue = maxDesc.value(state, desc);

            // handle valueOf property
            if (UNLIKELY(maxValue.isObject())) {
                maxDesc = maxValue.asObject()->get(state, state.context()->staticStrings().valueOf);
                if (maxDesc.hasValue()) {
                    Value valueOf = maxDesc.value(state, maxValue);
                    if (valueOf.isCallable()) {
                        maxValue = Object::call(state, valueOf, maxValue, 0, nullptr);
                    }
                }
            }

            if (!maxValue.isUInt32()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Table.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Let type be the table type {min n, max maximum} anyfunc.
    wasm_limits_t limits = { initial, maximum };
    // wasm_valtype_t is deleted inside wasm_tabletype_t constructor
    wasm_tabletype_t* tabletype = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), &limits);

    // Let (store, tableaddr) be table_alloc(store, type).
    wasm_table_t* tableaddr = wasm_table_new(state.context()->vmInstance()->wasmStore(), tabletype, nullptr);
    wasm_tabletype_delete(tabletype);

    // Let map be the surrounding agent's associated Table object cache.
    // Assert: map[tableaddr] doesn’t exist.
    WASMTableMap& map = state.context()->wasmCache()->tableMap;
    ASSERT(map.find(tableaddr) == map.end());

    // FIXME Let values be a list whose length is table_size(store, tableaddr) where each element is null.
    ValueVector* values = new ValueVector();
    values->resize(wasm_table_size(tableaddr), Value(Value::Null));

    // Set table.[[Table]] to tableaddr.
    // Set table.[[Values]] to values.
    WASMTableObject* tableObj = new WASMTableObject(state, tableaddr, values);

    // Set map[tableaddr] to table.
    map.insert(std::make_pair(tableaddr, tableObj));

    return tableObj;
}

static Value builtinWASMTableGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = argv[0];
    // handle valueOf property
    if (UNLIKELY(deltaValue.isObject())) {
        auto deltaDesc = deltaValue.asObject()->get(state, state.context()->staticStrings().valueOf);
        if (deltaDesc.hasValue()) {
            Value valueOf = deltaDesc.value(state, deltaValue);
            if (valueOf.isCallable()) {
                deltaValue = Object::call(state, valueOf, deltaValue, 0, nullptr);
            }
        }
    }
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_table_size_t delta = deltaValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    // Let initialSize be the length of this.[[Values]].
    WASMTableObject* tableObj = thisValue.asObject()->asWASMTableObject();
    wasm_table_t* tableaddr = tableObj->table();
    ValueVector* values = tableObj->values();
    size_t initialSize = tableObj->length();

    // Let result be table_grow(store, tableaddr, delta).
    bool result = wasm_table_grow(tableaddr, delta, nullptr);
    // If result is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Append null to this.[[Values]] delta times.
    for (uint32_t i = 0; i < delta; i++) {
        values->push_back(Value(Value::Null));
    }
    ASSERT((size_t)wasm_table_size(tableaddr) == values->size());

    // Return initialSize,
    return Value(initialSize);
}

static Value builtinWASMTableGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().get.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    WASMTableObject* tableObj = thisValue.asObject()->asWASMTableObject();

    Value indexValue = argv[0];
    // handle valueOf property
    if (UNLIKELY(indexValue.isObject())) {
        auto deltaDesc = indexValue.asObject()->get(state, state.context()->staticStrings().valueOf);
        if (deltaDesc.hasValue()) {
            Value valueOf = deltaDesc.value(state, indexValue);
            if (valueOf.isCallable()) {
                indexValue = Object::call(state, valueOf, indexValue, 0, nullptr);
            }
        }
    }
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // If index ≥ size, throw a RangeError exception.
    uint32_t index = indexValue.asUInt32();
    if ((size_t)index >= tableObj->length()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().length.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Return values[index].
    return tableObj->getElement(index);
}

static Value builtinWASMTableSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().get.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    WASMTableObject* tableObj = thisValue.asObject()->asWASMTableObject();

    Value indexValue = argv[0];
    // handle valueOf property
    if (UNLIKELY(indexValue.isObject())) {
        auto deltaDesc = indexValue.asObject()->get(state, state.context()->staticStrings().valueOf);
        if (deltaDesc.hasValue()) {
            Value valueOf = deltaDesc.value(state, indexValue);
            if (valueOf.isCallable()) {
                indexValue = Object::call(state, valueOf, indexValue, 0, nullptr);
            }
        }
    }
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    uint32_t index = indexValue.asUInt32();
    Value value = argv[1];

    // Let tableaddr be this.[[Table]].
    // Let values be this.[[Values]].
    wasm_table_t* tableaddr = tableObj->table();
    ValueVector* values = tableObj->values();

    // If value is null, let funcaddr be an empty function element.
    wasm_ref_t* funcaddr = nullptr;
    if (!value.isNull()) {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
        // If value does not have a [[FunctionAddress]] internal slot, throw a TypeError exception.
        // Let funcaddr be value.[[FunctionAddress]].
    }

    // Let store be table_write(store, tableaddr, index, funcaddr).
    bool result = wasm_table_set(tableaddr, index, funcaddr);
    // If store is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().set.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Set values[index] to value.
    tableObj->setElement(index, value);

    // Return undefined.
    return Value();
}

static Value builtinWASMTableLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().length.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    return Value(thisValue.asObject()->asWASMTableObject()->length());
}

// WebAssemblly.Error
#define DEFINE_ERROR_CTOR(errorName, lowerCaseErrorName)                                                                                                                                                                                                              \
    static Value builtin##errorName##ErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)                                                                                                                  \
    {                                                                                                                                                                                                                                                                 \
        if (!newTarget.hasValue()) {                                                                                                                                                                                                                                  \
            newTarget = state.resolveCallee();                                                                                                                                                                                                                        \
        }                                                                                                                                                                                                                                                             \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {                                                                                                               \
            return constructorRealm->globalObject()->lowerCaseErrorName##ErrorPrototype();                                                                                                                                                                            \
        });                                                                                                                                                                                                                                                           \
        ErrorObject* obj = new errorName##ErrorObject(state, proto, String::emptyString);                                                                                                                                                                             \
        Value message = argv[0];                                                                                                                                                                                                                                      \
        if (!message.isUndefined()) {                                                                                                                                                                                                                                 \
            obj->defineOwnPropertyThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message,                                                                                                                                                      \
                                                                ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
        }                                                                                                                                                                                                                                                             \
        return obj;                                                                                                                                                                                                                                                   \
    }

DEFINE_ERROR_CTOR(WASMCompile, wasmCompile)
DEFINE_ERROR_CTOR(WASMLink, wasmLink)
DEFINE_ERROR_CTOR(WASMRuntime, wasmRuntime)

void GlobalObject::installWASM(ExecutionState& state)
{
    // builtin Error should be installed ahead
    ASSERT(!!this->error());

    Object* wasm = new Object(state);
    wasm->setGlobalIntrinsicObject(state);

    const StaticStrings* strings = &state.context()->staticStrings();

    wasm->defineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                            ObjectPropertyDescriptor(Value(strings->WebAssembly.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    // WebAssembly.validate(bufferSource)
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->validate),
                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->validate, builtinWASMValidate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    // WebAssembly.compile(bufferSource)
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->compile),
                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->compile, builtinWASMCompile, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    // WebAssembly.instantiate()
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->instantiate),
                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->instantiate, builtinWASMInstantiate, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));


    // WebAssembly.Module
    FunctionObject* wasmModule = new NativeFunctionObject(state, NativeFunctionInfo(strings->Module, builtinWASMModuleConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmModule->setGlobalIntrinsicObject(state);

    m_wasmModulePrototype = new Object(state);
    m_wasmModulePrototype->setGlobalIntrinsicObject(state, true);

    m_wasmModulePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                             ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotModule.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    wasmModule->setFunctionPrototype(state, m_wasmModulePrototype);
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->Module),
                            ObjectPropertyDescriptor(wasmModule, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // WebAssembly.Memory
    FunctionObject* wasmMemory = new NativeFunctionObject(state, NativeFunctionInfo(strings->Memory, builtinWASMMemoryConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmMemory->setGlobalIntrinsicObject(state);

    m_wasmMemoryPrototype = new Object(state);
    m_wasmMemoryPrototype->setGlobalIntrinsicObject(state, true);

    m_wasmMemoryPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                             ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotMemory.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    m_wasmMemoryPrototype->defineOwnProperty(state, ObjectPropertyName(strings->grow),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->grow, builtinWASMMemoryGrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getBuffer, builtinWASMMemoryBufferGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor bufferDesc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
        m_wasmMemoryPrototype->defineOwnProperty(state, ObjectPropertyName(strings->buffer), bufferDesc);
    }

    wasmMemory->setFunctionPrototype(state, m_wasmMemoryPrototype);
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->Memory),
                            ObjectPropertyDescriptor(wasmMemory, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // WebAssembly.Table
    FunctionObject* wasmTable = new NativeFunctionObject(state, NativeFunctionInfo(strings->Table, builtinWASMTableConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmTable->setGlobalIntrinsicObject(state);

    m_wasmTablePrototype = new Object(state);
    m_wasmTablePrototype->setGlobalIntrinsicObject(state, true);

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                            ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotTable.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->grow),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->grow, builtinWASMTableGrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->get),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinWASMTableGet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->set),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinWASMTableSet, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->length, builtinWASMTableLengthGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor lengthDesc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
        m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->length), lengthDesc);
    }

    wasmTable->setFunctionPrototype(state, m_wasmTablePrototype);
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->Table),
                            ObjectPropertyDescriptor(wasmTable, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


// WebAssembly.Error
#define DEFINE_ERROR(errorname, errorName, bname)                                                                                                                                                                                                                                                                                       \
    FunctionObject* errorname##Error = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().bname##Error, builtin##errorName##ErrorConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);                                                                                                    \
    errorname##Error->setPrototype(state, m_error);                                                                                                                                                                                                                                                                                     \
    m_##errorname##ErrorPrototype = new Object(state, m_errorPrototype);                                                                                                                                                                                                                                                                \
    m_##errorname##ErrorPrototype->setGlobalIntrinsicObject(state, true);                                                                                                                                                                                                                                                               \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().constructor, ObjectPropertyDescriptor(errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                                \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().message, ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                                 \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(state.context()->staticStrings().bname##Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
    errorname##Error->setFunctionPrototype(state, m_##errorname##ErrorPrototype);                                                                                                                                                                                                                                                       \
    wasm->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().bname##Error),                                                                                                                                                                                                                                   \
                            ObjectPropertyDescriptor(errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    DEFINE_ERROR(wasmCompile, WASMCompile, Compile);
    DEFINE_ERROR(wasmLink, WASMLink, Link);
    DEFINE_ERROR(wasmRuntime, WASMRuntime, Runtime)


    defineOwnProperty(state, ObjectPropertyName(strings->WebAssembly),
                      ObjectPropertyDescriptor(wasm, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}

#endif
