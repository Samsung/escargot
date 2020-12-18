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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/Job.h"
#include "wasm/WASMObject.h"
#include "wasm/WASMValueConverter.h"
#include "wasm/WASMBuiltinOperations.h"
#include "wasm/ExportedFunctionObject.h"

// represent ownership of each object
// object marked with 'own' should be deleted in the current context
#define own

namespace Escargot {

static own wasm_trap_t* callbackHostFunction(void* env, const wasm_val_t args[], wasm_val_t results[])
{
    WASMHostFunctionEnvironment* funcEnv = (WASMHostFunctionEnvironment*)env;

    Object* func = funcEnv->func;
    ASSERT(func->isCallable());

    // Let realm be func's associated Realm.
    // Let relevant settings be realm?s settings object.
    ExecutionState temp(nullptr);
    ExecutionState state(func->getFunctionRealm(temp));

    // Let [parameters] ? [results] be functype.
    wasm_functype_t* functype = funcEnv->functype;
    const wasm_valtype_vec_t* params = wasm_functype_params(functype);
    const wasm_valtype_vec_t* res = wasm_functype_results(functype);
    size_t argSize = params->size;

    // Let jsArguments be << >>
    Value* jsArguments = ALLOCA(sizeof(Value) * argSize, Value, state);

    // For each arg of arguments,
    for (size_t i = 0; i < argSize; i++) {
        // Append ! ToJSValue(arg) to jsArguments.
        jsArguments[i] = WASMValueConverter::wasmToJSValue(state, args[i]);
    }

    // Let ret be ? Call(func, undefined, jsArguments).
    Value ret = Object::call(state, func, Value(), argSize, jsArguments);

    // Let resultsSize be results?s size.
    size_t resultsSize = res->size;
    if (resultsSize == 0) {
        // If resultsSize is 0, return << >>
        return nullptr;
    }

    if (resultsSize == 1) {
        // Otherwise, if resultsSize is 1, return << ToWebAssemblyValue(ret, results[0]) >>
        results[0] = WASMValueConverter::wasmToWebAssemblyValue(state, ret, wasm_valtype_kind(res->data[0]));
    } else {
        // Otherwise,
        // Let method be ? GetMethod(ret, @@iterator).
        Value method = Object::getMethod(state, ret, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
        // If method is undefined, throw a TypeError.
        if (method.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_FuncCallError);
        }

        // Let values be ? IterableToList(ret, method).
        ValueVectorWithInlineStorage values = IteratorObject::iterableToList(state, ret, method);
        // If values's size is not resultsSize, throw a TypeError exception.
        if (values.size() != resultsSize) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_FuncCallError);
        }

        // For each value and resultType in values and results, paired linearly,
        // Append ToWebAssemblyValue(value, resultType) to wasmValues.
        for (size_t i = 0; i < resultsSize; i++) {
            results[i] = WASMValueConverter::wasmToWebAssemblyValue(state, values[i], wasm_valtype_kind(res->data[i]));
        }
    }

    // TODO
    // Assert: result.[[Type]] is throw or normal.
    // If result.[[Type]] is throw, then trigger a WebAssembly trap, and propagate result.[[Value]] to the enclosing JavaScript.
    // Otherwise, return result.[[Value]].

    return nullptr;
}

// TODO fix to static function
wasm_func_t* createHostFunction(ExecutionState& state, Object* func, wasm_functype_t* functype)
{
    // Assert: IsCallable(func).
    ASSERT(func->isCallable());

    // Let store be the surrounding agent's associated store.
    // Let (store, funcaddr) be func_alloc(store, functype, hostfunc).
    WASMHostFunctionEnvironment* env = new WASMHostFunctionEnvironment(func, functype);
    wasm_func_t* funcaddr = wasm_func_new_with_env(state.context()->vmInstance()->wasmStore(), functype, callbackHostFunction, env, nullptr);

    state.context()->wasmEnvCache()->push_back(env);

    // Return funcaddr.
    return funcaddr;
}

// WebAssembly
static Value builtinWASMValidate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value source = argv[0];
    if (!source.isPointerValue() || (!source.asPointerValue()->isTypedArrayObject() && !source.asPointerValue()->isArrayBufferObject())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().validate.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    ArrayBufferObject* srcBuffer = wasmCopyStableBufferBytes(state, source).asPointerValue()->asArrayBufferObject();
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
    Value source = wasmCopyStableBufferBytes(state, argv[0]);

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    NativeFunctionObject* asyncCompiler = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), wasmCompileModule, 1, NativeFunctionInfo::Strict));
    Job* job = new PromiseReactionJob(state.context(), PromiseReaction(asyncCompiler, capability), source);
    state.context()->vmInstance()->enqueuePromiseJob(capability.m_promise->asPromiseObject(), job);

    return capability.m_promise;
}

static Value builtinWASMInstantiate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().instantiate.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    Object* firstArg = argv[0].asObject();

    if (firstArg->isArrayBufferObject() || firstArg->isTypedArrayObject()) {
        // Let stableBytes be a copy of the bytes held by the buffer bytes.
        Value stableBytes = wasmCopyStableBufferBytes(state, firstArg);

        // Asynchronously compile a WebAssembly module from stableBytes and let promiseOfModule be the result.
        PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
        PromiseObject* promiseOfModule = capability.m_promise->asPromiseObject();

        NativeFunctionObject* asyncCompiler = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), wasmCompileModule, 1, NativeFunctionInfo::Strict));
        Job* job = new PromiseReactionJob(state.context(), PromiseReaction(asyncCompiler, capability), stableBytes);
        state.context()->vmInstance()->enqueuePromiseJob(promiseOfModule, job);

        Value importObj = argc > 1 ? argv[1] : Value();
        auto moduleInstantiator = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), wasmInstantiateModule, 1, NativeFunctionInfo::Strict));
        moduleInstantiator->setInternalSlot(0, importObj);

        return promiseOfModule->then(state, moduleInstantiator);
    }

    return Value();
}

// WebAssembly.Module
static Value builtinWASMModuleConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Module.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value source = wasmCopyStableBufferBytes(state, argv[0]);
    Value arg[] = { source };
    return wasmCompileModule(state, thisValue, 1, arg, newTarget);
}

static Value builtinWASMModuleCustomSections(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotModule.string(), false, strings->customSections.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotModule.string(), false, strings->customSections.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

    // TODO wasm-c-api do not support custom sections
    // Let bytes be moduleObject.[[Bytes]].
    // Let customSections be << >>
    // For each custom section customSection of bytes, interpreted according to the module grammar,
    // Let name be the name of customSection, decoded as UTF-8.
    // Assert: name is not failure (moduleObject.[[Module]] is valid).
    // If name equals sectionName as string values,
    // Append a new ArrayBuffer containing a copy of the bytes in bytes for the range matched by this customsec production to customSections.
    // Return customSections.

    return Value();
}

static Value builtinWASMModuleExports(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotModule.string(), false, strings->exports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

    own wasm_exporttype_vec_t export_types;
    wasm_module_exports(module, &export_types);

    // Let exports be << >>.
    // set default array length as export_types.size
    ArrayObject* exports = new ArrayObject(state, (uint64_t)export_types.size);

    for (size_t i = 0; i < export_types.size; i++) {
        // For each (name, type) of module_exports(module),
        const wasm_name_t* name = wasm_exporttype_name(export_types.data[i]);
        wasm_externkind_t type = wasm_externtype_kind(wasm_exporttype_type(export_types.data[i]));

        String* nameValue = String::fromASCII(name->data, name->size);
        // Let kind be the string value of the extern type type.
        String* kindValue = wasmStringValueOfExternType(state, type);

        // Let obj be ?[ "name" ? name, "kind" ? kind ]?.
        Object* obj = new Object(state);
        obj->defineOwnProperty(state, ObjectPropertyName(strings->name), ObjectPropertyDescriptor(nameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->kind), ObjectPropertyDescriptor(kindValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

        // Append obj to exports.
        exports->setIndexedProperty(state, Value(i), Value(obj));
    }

    wasm_exporttype_vec_delete(&export_types);
    // Return exports.
    return exports;
}

static Value builtinWASMModuleImports(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotModule.string(), false, strings->imports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

    /* TODO wabt should be updated
    own wasm_importtype_vec_t import_types;
    wasm_module_imports(module, &import_types);

    // Let imports be << >>
    // set default array length as import_types.size
    ArrayObject* imports = new ArrayObject(state, (uint64_t)import_types.size);

    for (size_t i = 0; i < import_types.size; i++) {
        // For each (moduleName, name, type) of module_imports(module),
        const wasm_name_t* moduleName = wasm_importtype_module(import_types.data[i]);
        const wasm_name_t* name = wasm_importtype_name(import_types.data[i]);
        wasm_externkind_t type = wasm_externtype_kind(wasm_importtype_type(import_types.data[i]));

        String* moduleNameValue = String::fromASCII(moduleName->data, moduleName->size);
        String* nameValue = String::fromASCII(name->data, name->size);
        // Let kind be the string value of the extern type type.
        String* kindValue = wasmStringValueOfExternType(state, type);

        // Let obj be ?[ "module" ? moduleName, "name" ? name, "kind" ? kind ]?.
        Object* obj = new Object(state);
        obj->defineOwnProperty(state, ObjectPropertyName(strings->module), ObjectPropertyDescriptor(moduleNameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->name), ObjectPropertyDescriptor(nameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->kind), ObjectPropertyDescriptor(kindValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

        // Append obj to imports.
        imports->setIndexedProperty(state, Value(i), Value(obj));
    }

    wasm_importtype_vec_delete(&import_types);
    //Return imports.
    return imports;
    */
    return Value();
}

// WebAssembly.Memory
static Value builtinWASMMemoryConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];

    // check and get 'initial' property from the first argument
    Value initValue = wasmGetValueFromObjectProperty(state, desc, strings->initial, strings->valueOf).second;
    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    uint32_t initial = initValue.asUInt32();

    // If descriptor["maximum"] is present, let maximum be descriptor["maximum"]; otherwise, let maximum be empty.
    // but we set "wasm_limits_max_default"(max value of uint32_t) as its default value
    ASSERT(desc.isObject());
    uint32_t maximum = wasm_limits_max_default;
    {
        auto maxResult = wasmGetValueFromObjectProperty(state, desc, strings->maximum, strings->valueOf);
        if (maxResult.first) {
            Value maxValue = maxResult.second;
            if (!maxValue.isUInt32()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_RangeError);
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
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMMemoryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
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
    const StaticStrings* strings = &state.context()->staticStrings();

    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];

    // check 'element' property from the first argument
    {
        Value elemValue = wasmGetValueFromObjectProperty(state, desc, strings->element, strings->toString).second;
        // element property should be 'anyfunc'
        if (UNLIKELY(!elemValue.isString() || !elemValue.asString()->equals(strings->anyfunc.string()))) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    }

    // check and get 'initial' property from the first argument
    ASSERT(desc.isObject());
    Value initValue = wasmGetValueFromObjectProperty(state, desc, strings->initial, strings->valueOf).second;
    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    uint32_t initial = initValue.asUInt32();

    // If descriptor["maximum"] is present, let maximum be descriptor["maximum"]; otherwise, let maximum be empty.
    // but we set "wasm_limits_max_default"(max value of uint32_t) as its default value
    ASSERT(desc.isObject());
    uint32_t maximum = wasm_limits_max_default;
    {
        auto maxResult = wasmGetValueFromObjectProperty(state, desc, strings->maximum, strings->valueOf);
        if (maxResult.first) {
            Value maxValue = maxResult.second;
            if (!maxValue.isUInt32()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Let type be the table type {min n, max maximum} anyfunc.
    wasm_limits_t limits = { initial, maximum };
    // wasm_valtype_t is deleted inside wasm_tabletype_t constructor
    own wasm_tabletype_t* tabletype = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), &limits);

    // Let (store, tableaddr) be table_alloc(store, type).
    wasm_table_t* tableaddr = wasm_table_new(state.context()->vmInstance()->wasmStore(), tabletype, nullptr);
    wasm_tabletype_delete(tabletype);

    // Let map be the surrounding agent's associated Table object cache.
    // Assert: map[tableaddr] doesn’t exist.
    WASMTableMap& map = state.context()->wasmCache()->tableMap;
    ASSERT(map.find(tableaddr) == map.end());

    // Set table.[[Table]] to tableaddr.
    WASMTableObject* tableObj = new WASMTableObject(state, tableaddr);

    // Set map[tableaddr] to table.
    map.insert(std::make_pair(tableaddr, tableObj));

    return tableObj;
}

static Value builtinWASMTableGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_table_size_t delta = deltaValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    // Let initialSize be table_size(store, tableaddr).
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();
    wasm_table_size_t initialSize = wasm_table_size(tableaddr);

    // Let result be table_grow(store, tableaddr, delta).
    bool result = wasm_table_grow(tableaddr, delta, nullptr);
    // If result is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Return initialSize,
    return Value(initialSize);
}

static Value builtinWASMTableGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value indexValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    wasm_table_size_t index = indexValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();

    // Let result be table_read(store, tableaddr, index).
    wasm_ref_t* result = wasm_table_get(tableaddr, index);

    // If result is error, throw a RangeError exception.
    // FIXME check the error by comparing the size
    if (index >= wasm_table_size(tableaddr)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // FIXME for the case of empty function (nullptr)
    if (!result) {
        return Value(Value::Null);
    }

    // Let function be the result of creating a new Exported Function from result.
    ExportedFunctionObject* function = ExportedFunctionObject::createExportedFunction(state, wasm_ref_as_func(result));

    // Return function.
    return function;
}

static Value builtinWASMTableSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value indexValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_table_size_t index = indexValue.asUInt32();
    Value value = argv[1];

    // Let tableaddr be this.[[Table]].
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();

    // FIXME If value is null, let funcaddr be an empty function element.
    wasm_ref_t* funcaddr = nullptr;
    if (!value.isNull()) {
        // If value does not have a [[FunctionAddress]] internal slot, throw a TypeError exception.
        if (!value.isObject() || !value.asObject()->isExportedFunctionObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
        // Let funcaddr be value.[[FunctionAddress]].
        funcaddr = wasm_func_as_ref(value.asObject()->asExportedFunctionObject()->function());
    }

    // Let store be table_write(store, tableaddr, index, funcaddr).
    bool result = wasm_table_set(tableaddr, index, funcaddr);
    // If store is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Return undefined.
    return Value();
}

static Value builtinWASMTableLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().length.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    // Let tableaddr be this.[[Table]].
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();

    // Return table_size(store, tableaddr).
    return Value(wasm_table_size(tableaddr));
}

// WebAssembly.Global
static Value builtinWASMGlobalConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Global.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];

    // check and get 'mutable' property from the first argument
    // set default value as const
    wasm_mutability_t mut = WASM_CONST;
    {
        auto mutValue = wasmGetValueFromObjectProperty(state, desc, strings->stringMutable, AtomicString()).second;
        mut = mutValue.toBoolean(state) ? WASM_VAR : WASM_CONST;
    }

    // check and get 'value' property from the first argument
    wasm_valkind_t valuetype;
    {
        // Let valuetype be ToValueType(descriptor["value"]).
        Value valTypeValue = wasmGetValueFromObjectProperty(state, desc, strings->value, strings->toString).second;
        if (!valTypeValue.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Global.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
        if (valTypeValue.asString()->equals(strings->i32.string())) {
            valuetype = WASM_I32;
        } else if (valTypeValue.asString()->equals(strings->i64.string())) {
            valuetype = WASM_I64;
        } else if (valTypeValue.asString()->equals(strings->f32.string())) {
            valuetype = WASM_F32;
        } else if (valTypeValue.asString()->equals(strings->f64.string())) {
            valuetype = WASM_F64;
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->WebAssembly.string(), false, strings->Global.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    }

    Value valueArg = argc > 1 ? argv[1] : Value();
    wasm_val_t value;
    if (valueArg.isUndefined()) {
        // let value be DefaultValue(valuetype).
        value = WASMValueConverter::wasmDefaultValue(valuetype);
    } else {
        // Let value be ToWebAssemblyValue(v, valuetype).
        value = WASMValueConverter::wasmToWebAssemblyValue(state, valueArg, valuetype);
    }

    // If mutable is true, let globaltype be var valuetype; otherwise, let globaltype be const valuetype.
    wasm_globaltype_t* globaltype = wasm_globaltype_new(wasm_valtype_new(valuetype), mut);

    // Let (store, globaladdr) be global_alloc(store, globaltype, value).
    wasm_global_t* globaladdr = wasm_global_new(state.context()->vmInstance()->wasmStore(), globaltype, &value);
    wasm_globaltype_delete(globaltype);

    // Let map be the surrounding agent's associated Global object cache.
    // Assert: map[globaladdr] doesn't exist.
    WASMGlobalMap& map = state.context()->wasmCache()->globalMap;
    ASSERT(map.find(globaladdr) == map.end());

    // Set global.[[Global]] to globaladdr.
    WASMGlobalObject* globalObj = new WASMGlobalObject(state, globaladdr);

    // Set map[globaladdr] to global.
    map.insert(std::make_pair(globaladdr, globalObj));

    return globalObj;
}

static Value builtinWASMGlobalValueGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMGlobalObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value value = thisValue.asObject()->asWASMGlobalObject()->getGlobalValue(state);
    return value;
}

static Value builtinWASMGlobalValueSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMGlobalObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    if (argc == 0) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let globaladdr be this.[[Global]].
    wasm_global_t* globaladdr = thisValue.asObject()->asWASMGlobalObject()->global();

    // Let mut valuetype be global_type(store, globaladdr).
    wasm_globaltype_t* globaltype = wasm_global_type(globaladdr);
    wasm_mutability_t mut = wasm_globaltype_mutability(globaltype);
    wasm_valkind_t valuetype = wasm_valtype_kind(wasm_globaltype_content(globaltype));

    // If mut is const, throw a TypeError.
    if (mut == WASM_CONST) {
        wasm_globaltype_delete(globaltype);
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::WASM_SetToGlobalConstValue);
    }
    wasm_globaltype_delete(globaltype);

    // Let value be ToWebAssemblyValue(the given value, valuetype).
    wasm_val_t value = WASMValueConverter::wasmToWebAssemblyValue(state, argv[0], valuetype);

    // Let store be global_write(store, globaladdr, value).
    // TODO If store is error, throw a RangeError exception.
    wasm_global_set(globaladdr, &value);

    return Value();
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

    wasmModule->defineOwnProperty(state, ObjectPropertyName(strings->customSections),
                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->customSections, builtinWASMModuleCustomSections, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    wasmModule->defineOwnProperty(state, ObjectPropertyName(strings->exports),
                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->exports, builtinWASMModuleExports, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    wasmModule->defineOwnProperty(state, ObjectPropertyName(strings->imports),
                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->imports, builtinWASMModuleImports, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

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


    // WebAssembly.Global
    FunctionObject* wasmGlobal = new NativeFunctionObject(state, NativeFunctionInfo(strings->Global, builtinWASMGlobalConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmGlobal->setGlobalIntrinsicObject(state);

    m_wasmGlobalPrototype = new Object(state);
    m_wasmGlobalPrototype->setGlobalIntrinsicObject(state, true);

    m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                             ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotGlobal.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(strings->valueOf),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinWASMGlobalValueGetter, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->value, builtinWASMGlobalValueGetter, 0, NativeFunctionInfo::Strict)),
            new NativeFunctionObject(state, NativeFunctionInfo(strings->value, builtinWASMGlobalValueSetter, 1, NativeFunctionInfo::Strict)));
        ObjectPropertyDescriptor valueDesc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
        m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(strings->value), valueDesc);
    }

    wasmGlobal->setFunctionPrototype(state, m_wasmGlobalPrototype);
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->Global),
                            ObjectPropertyDescriptor(wasmGlobal, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


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
} // namespace Escargot

#endif
