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
#include "runtime/ThreadLocal.h"
#include "runtime/VMInstance.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/SharedArrayBufferObject.h"
#include "runtime/BackingStore.h"
#include "runtime/PromiseObject.h"
#include "runtime/Job.h"
#include "wasm/WASMObject.h"
#include "wasm/ExportedFunctionObject.h"
#include "wasm/WASMValueConverter.h"
#include "wasm/WASMOperations.h"

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

// WebAssembly
static Value builtinWASMValidate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value source = argv[0];
    if (!source.isPointerValue() || (!source.asPointerValue()->isTypedArrayObject() && !source.asPointerValue()->isArrayBufferObject())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().validate.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    ArrayBufferObject* srcBuffer = WASMOperations::copyStableBufferBytes(state, source).asPointerValue()->asArrayBufferObject();
    ASSERT(!srcBuffer->isDetachedBuffer());
    size_t byteLength = srcBuffer->byteLength();

    own wasm_byte_vec_t binary;
    wasm_byte_vec_new_uninitialized(&binary, byteLength);
    memcpy(binary.data, srcBuffer->data(), byteLength);

    bool result = wasm_module_validate(ThreadLocal::wasmStore(), &binary);
    wasm_byte_vec_delete(&binary);

    return Value(result);
}

static Value builtinWASMCompile(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let stableBytes be a copy of the bytes held by the buffer bytes.
    Value source = WASMOperations::copyStableBufferBytes(state, argv[0]);

    // Asynchronously compile a WebAssembly module from stableBytes and return the result.
    return WASMOperations::asyncCompileModule(state, source);
}

static Value builtinWASMInstantiate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value firstArg = argv[0];
    Value importObj = argc > 1 ? argv[1] : Value();

    if (firstArg.isObject() && (firstArg.asObject()->isArrayBufferObject() || firstArg.asObject()->isTypedArrayObject())) {
        // Let stableBytes be a copy of the bytes held by the buffer bytes.
        Value stableBytes = WASMOperations::copyStableBufferBytes(state, firstArg);

        // Asynchronously compile a WebAssembly module from stableBytes and let promiseOfModule be the result.
        PromiseObject* promiseOfModule = WASMOperations::asyncCompileModule(state, stableBytes)->asPromiseObject();

        // Instantiate promiseOfModule with imports importObject and return the result.
        return WASMOperations::instantiatePromiseOfModuleWithImportObject(state, promiseOfModule, importObj);
    }

    // Asynchronously instantiate the WebAssembly module moduleObject importing importObject, and return the result.
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());

    // Read the imports of module with imports importObject, and let imports be the result. If this operation throws an exception, catch it, reject promise with the exception, and return promise.
    own wasm_extern_vec_t imports;
    try {
        Value moduleValue = firstArg;
        if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::WASM_InstantiateModuleError);
        }

        // Let module be moduleObject.[[Module]].
        WASMModuleObject* moduleObj = moduleValue.asPointerValue()->asWASMModuleObject();
        wasm_module_t* module = moduleObj->module();

        // Read the imports of module with imports importObject, and let imports be the result.
        wasm_extern_vec_new_empty(&imports);
        WASMOperations::readImportsOfModule(state, module, importObj, &imports);
    } catch (const Value& v) {
        // If this operation throws an exception, catch it, reject promise with the exception.
        Value reason = v;
        Object::call(state, capability.m_rejectFunction, Value(), 1, &reason);
        return capability.m_promise;
    }

    // Note) pass imports data and its size into ExtendedNativeFunctionObjectImpl and delete imports vector inside ExtendedNativeFunctionObjectImpl call
    auto moduleInstantiator = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), WASMOperations::instantiateCoreModule, 1, NativeFunctionInfo::Strict));
    moduleInstantiator->setInternalSlot(0, Value(imports.size));
    moduleInstantiator->setInternalSlotAsPointer(1, imports.data);

    Job* job = new PromiseReactionJob(state.context(), PromiseReaction(moduleInstantiator, capability), firstArg);
    state.context()->vmInstance()->enqueueJob(job);

    return capability.m_promise;
}

// WebAssembly.Module
static Value builtinWASMModuleConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Module.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value source = WASMOperations::copyStableBufferBytes(state, argv[0]);
    Value arg[] = { source };
    return WASMOperations::compileModule(state, thisValue, 1, arg, newTarget);
}

static Value builtinWASMModuleCustomSections(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->customSections.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->customSections.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // TODO wasm-c-api do not support custom sections
    // Let module be moduleObject.[[Module]].
    // wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->exports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
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
        exports->setIndexedProperty(state, Value(i), Value(obj), exports);
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->imports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

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
        imports->setIndexedProperty(state, Value(i), Value(obj), imports);
    }

    wasm_importtype_vec_delete(&import_types);
    //Return imports.
    return imports;
}

// WebAssembly.Instance
static Value builtinWASMInstanceConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssembly.string(), false, state.context()->staticStrings().Instance.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    if (!argv[0].isObject() || !argv[0].asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotModule.string(), false, state.context()->staticStrings().constructor.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be module.[[Module]].
    wasm_module_t* module = argv[0].asObject()->asWASMModuleObject()->module();

    // Read the imports of module with imports importObject, and let imports be the result.
    Value importObject = argc > 1 ? argv[1] : Value();
    own wasm_extern_vec_t imports;
    wasm_extern_vec_new_empty(&imports);
    WASMOperations::readImportsOfModule(state, module, importObject, &imports);

    // Instantiate the core of a WebAssembly module module with imports, and let instance be the result.
    own wasm_trap_t* trap = nullptr;
    own wasm_instance_t* instance = wasm_instance_new(ThreadLocal::wasmStore(), module, &imports, &trap);
    wasm_extern_vec_delete(&imports);

    if (!instance) {
        own wasm_name_t message;
        wasm_trap_message(trap, &message);
        ESCARGOT_LOG_ERROR("[WASM Message] %s\n", message.data);
        wasm_name_delete(&message);
        wasm_trap_delete(trap);
        ErrorObject::throwBuiltinError(state, ErrorCode::WASMLinkError, ErrorObject::Messages::WASM_InstantiateModuleError);
    }

    // Initialize this from module and instance.
    // Create an exports object from module and instance and let exportsObject be the result.
    Object* exportsObject = WASMOperations::createExportsObject(state, module, instance);

    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%WebAssemblyInstancePrototype%").
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->wasmInstancePrototype();
    });

    // Let instanceObject be a new Instance.
    // Set instanceObject.[[Instance]] to instance.
    // Set instanceObject.[[Exports]] to exportsObject.
    WASMInstanceObject* instanceObject = new WASMInstanceObject(state, proto, instance, exportsObject);

    // Return instanceObject.
    return instanceObject;
}

static Value builtinWASMInstanceExportsGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMInstanceObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotInstance.string(), false, state.context()->staticStrings().exports.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    return thisValue.asObject()->asWASMInstanceObject()->exports();
}

// WebAssembly.Memory

static ArrayBuffer* createFixedLengthMemoryBuffer(ExecutionState& state, wasm_memory_t* memaddr, bool isShared)
{
    ASSERT(!!memaddr);

    // Note) wasm_memory_data with zero size returns null pointer
    // predefined temporal address is allocated for this case
    void* dataBlock = wasm_memory_size(memaddr) == 0 ? WASMEmptyBlockAddress : wasm_memory_data(memaddr);

    ArrayBuffer* buffer = nullptr;
    if (isShared) {
        ASSERT(wasm_memory_is_shared(memaddr));
        // Let block be a Shared Data Block which is identified with the underlying memory of memaddr.
        // Let buffer be a new SharedArrayBuffer with the internal slots [[ArrayBufferData]] and [[ArrayBufferByteLength]].
        buffer = SharedArrayBufferObject::allocateExternalSharedArrayBuffer(state, dataBlock, wasm_memory_data_size(memaddr));

        // Perform ! SetIntegrityLevel(buffer, "frozen").
        if (UNLIKELY(!Object::setIntegrityLevel(state, buffer, false))) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    } else {
        // Let block be a Data Block which is identified with the underlying memory of memaddr.
        // Let buffer be a new ArrayBuffer with the internal slots [[ArrayBufferData]], [[ArrayBufferByteLength]], and [[ArrayBufferDetachKey]].
        buffer = ArrayBufferObject::allocateExternalArrayBuffer(state, dataBlock, wasm_memory_data_size(memaddr));
    }

    // Return buffer.
    return buffer;
}

static Value builtinWASMMemoryConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    bool isShared = false;
    Value desc = argv[0];

    // check and get 'initial' property from the first argument
    Value initValue = wasmGetValueFromObjectProperty(state, desc, strings->initial, strings->valueOf).second;
    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    uint32_t initial = initValue.asUInt32();

    // If descriptor["maximum"] is present, let maximum be descriptor["maximum"]; otherwise, let maximum be empty.
    // but we set "wasm_limits_max_default"(max value of uint32_t) as its default value
    ASSERT(desc.isObject());
    uint32_t maximum = wasm_limits_max_default;
    {
        auto maxResult = wasmGetValueFromObjectProperty(state, desc, strings->maximum, strings->valueOf);
        auto sharedResult = wasmGetValueFromObjectProperty(state, desc, strings->shared, AtomicString());
        isShared = sharedResult.first ? sharedResult.second.toBoolean() : false;

        if (maxResult.first) {
            Value maxValue = maxResult.second;
            if (!maxValue.isUInt32()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        } else if (isShared) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Let memtype be { min initial, max maximum }
    wasm_limits_t limits = { initial, maximum };
    own wasm_memorytype_t* memtype = wasm_memorytype_new(&limits);

    // Let (store, memaddr) be mem_alloc(store, memtype). If allocation fails, throw a RangeError exception.
    own wasm_memory_t* memaddr = nullptr;
    if (isShared) {
        memaddr = wasm_shared_memory_new(ThreadLocal::wasmStore(), memtype);
    } else {
        memaddr = wasm_memory_new(ThreadLocal::wasmStore(), memtype);
    }
    wasm_memorytype_delete(memtype);

    // Let map be the surrounding agent's associated Memory object cache.
    // Assert: map[memaddr] doesn’t exist.
    ASSERT(!state.context()->wasmCache()->findMemory(wasm_memory_as_ref(memaddr)));

    // Let buffer be the result of creating a fixed length memory buffer from memaddr.
    ArrayBuffer* buffer = createFixedLengthMemoryBuffer(state, memaddr, isShared);

    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%WebAssemblyMemoryPrototype%").
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->wasmMemoryPrototype();
    });

    // Set memory.[[Memory]] to memaddr.
    // Set memory.[[BufferObject]] to buffer.
    WASMMemoryObject* memoryObj = new WASMMemoryObject(state, proto, memaddr, buffer);

    // Set map[memaddr] to memory.
    state.context()->wasmCache()->appendMemory(wasm_memory_as_ref(memaddr), memoryObj);

    return memoryObj;
}

static Value builtinWASMMemoryGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMMemoryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_memory_pages_t delta = deltaValue.asUInt32();

    // Let memaddr be this.[[Memory]].
    wasm_memory_t* memaddr = thisValue.asObject()->asWASMMemoryObject()->memory();

    // Let ret be the mem_size(store, memaddr).
    // wasm_memory_pages_t maps to uint32_t
    wasm_memory_pages_t ret = wasm_memory_size(memaddr);

    // Let store be mem_grow(store, memaddr, delta).
    bool success = wasm_memory_grow(memaddr, delta);
    if (!success) {
        // If store is error, throw a RangeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssemblyDotMemory.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Refresh the memory buffer of memaddr.
    // Let map be the surrounding agent's associated Memory object cache.
    // Assert: map[memaddr] exists.
    ASSERT(state.context()->wasmCache()->findMemory(wasm_memory_as_ref(memaddr)));

    // Let memory be map[memaddr].
    WASMMemoryObject* memoryObj = state.context()->wasmCache()->findMemory(wasm_memory_as_ref(memaddr));
    // Let buffer be memory.[[BufferObject]].
    ArrayBuffer* buffer = memoryObj->buffer();

    // If IsSharedArrayBuffer(buffer) is false,
    // Perform ! DetachArrayBuffer(buffer, "WebAssembly.Memory").
    bool isShared = buffer->isSharedArrayBufferObject();
    if (!isShared) {
        buffer->asArrayBufferObject()->detachArrayBuffer();
    }

    // Let newBuffer be the result of creating a fixed length memory buffer from memaddr.
    ArrayBuffer* newBuffer = createFixedLengthMemoryBuffer(state, memaddr, isShared);
    // Set memory.[[BufferObject]] to newBuffer.
    memoryObj->setBuffer(newBuffer);

    // Return ret.
    return Value(ret);
}

static Value builtinWASMMemoryBufferGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMMemoryObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotMemory.string(), false, state.context()->staticStrings().buffer.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    WASMMemoryObject* memoryObj = thisValue.asObject()->asWASMMemoryObject();
    wasm_memory_t* memaddr = memoryObj->memory();

    if (memoryObj->buffer()->isSharedArrayBufferObject()) {
        // Let map be the surrounding agent's associated Memory object cache.
        // Assert: map[memaddr] exists.
        ASSERT(state.context()->wasmCache()->findMemory(wasm_memory_as_ref(memaddr)));

        // Let newMemory be map[memaddr].
        WASMMemoryObject* newMemory = state.context()->wasmCache()->findMemory(wasm_memory_as_ref(memaddr));
        // Let newBufferObject be newMemory.[[BufferObject]].
        ArrayBuffer* newBufferObject = newMemory->buffer();

        // Set this.[[BufferObject]] to newBufferObject.
        memoryObj->setBuffer(newBufferObject);

        // Return newBufferObject.
        ASSERT(wasm_memory_data_size(memoryObj->memory()) == memoryObj->buffer()->byteLength());
        return newBufferObject;
    }

    // Return this.[[BufferObject]].
    ASSERT(wasm_memory_data_size(memoryObj->memory()) == memoryObj->buffer()->byteLength());
    return memoryObj->buffer();
}

// WebAssembly.Table
static Value builtinWASMTableConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    // if NewTarget is undefined, throw a TypeError
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];

    // Let elementType be ToValueType(descriptor["element"]).
    wasm_valkind_t elementType = WASMValueConverter::toValueType(state, wasmGetValueFromObjectProperty(state, desc, strings->element, strings->toString).second);
    // If elementType is not a reftype,
    if (UNLIKELY(elementType != WASM_FUNCREF && elementType != WASM_ANYREF)) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // check and get 'initial' property from the first argument
    ASSERT(desc.isObject());
    Value initValue = wasmGetValueFromObjectProperty(state, desc, strings->initial, strings->valueOf).second;
    if (UNLIKELY(!initValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
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
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Memory.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
            }
            maximum = maxValue.asUInt32();
        }
    }

    if (maximum < initial) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssembly.string(), false, strings->Table.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // If value is missing,
    // Let ref be DefaultValue(elementType).
    ASSERT(elementType == WASM_ANYREF || elementType == WASM_FUNCREF);
    wasm_ref_t* ref = nullptr;
    wasm_val_t value;
    if (argc > 1) {
        // Let ref be ? ToWebAssemblyValue(value, elementType).
        value = WASMValueConverter::toWebAssemblyValue(state, argv[1], elementType);
    } else {
        // If value is missing,
        // Let ref be DefaultValue(elementType).
        value = WASMValueConverter::defaultValue(state, elementType);
    }
    ASSERT(value.kind == WASM_ANYREF || value.kind == WASM_FUNCREF);
    ref = value.of.ref;

    // Let type be the table type {min n, max maximum} anyfunc.
    wasm_limits_t limits = { initial, maximum };
    // wasm_valtype_t is deleted inside wasm_tabletype_t constructor
    own wasm_tabletype_t* tabletype = wasm_tabletype_new(wasm_valtype_new(elementType), &limits);

    // Let (store, tableaddr) be table_alloc(store, type, ref).
    own wasm_table_t* tableaddr = wasm_table_new(ThreadLocal::wasmStore(), tabletype, ref);
    wasm_tabletype_delete(tabletype);

    wasm_ref_t* tableref = wasm_table_as_ref(tableaddr);

    // Let map be the surrounding agent's associated Table object cache.
    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%WebAssemblyTablePrototype%").
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->wasmTablePrototype();
    });

    // Set table.[[Table]] to tableaddr.
    WASMTableObject* tableObj = new WASMTableObject(state, proto, tableaddr);

    // Set map[tableaddr] to table.
    state.context()->wasmCache()->appendTable(tableref, tableObj);

    return tableObj;
}

static Value builtinWASMTableGrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value deltaValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!deltaValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_table_size_t delta = deltaValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    // Let initialSize be table_size(store, tableaddr).
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();
    wasm_table_size_t initialSize = wasm_table_size(tableaddr);

    // Let (limits, elementType) be table_type(tableaddr).
    own wasm_tabletype_t* tabletype = wasm_table_type(tableaddr);
    wasm_valkind_t elementType = wasm_valtype_kind(wasm_tabletype_element(tabletype));
    wasm_tabletype_delete(tabletype);
    ASSERT(elementType == WASM_ANYREF || elementType == WASM_FUNCREF);

    wasm_ref_t* ref = nullptr;
    wasm_val_t value;
    if (argc > 1) {
        // Let ref be ? ToWebAssemblyValue(value, elementType).
        value = WASMValueConverter::toWebAssemblyValue(state, argv[1], elementType);
    } else {
        // If value is missing,
        // Let ref be DefaultValue(elementType).
        value = WASMValueConverter::defaultValue(state, elementType);
    }
    ASSERT(value.kind == WASM_ANYREF || value.kind == WASM_FUNCREF);
    ref = value.of.ref;


    // Let result be table_grow(store, tableaddr, delta, ref).
    bool result = wasm_table_grow(tableaddr, delta, ref);
    // If result is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssemblyDotTable.string(), false, strings->grow.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Return initialSize,
    return Value(initialSize);
}

static Value builtinWASMTableGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value indexValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }
    wasm_table_size_t index = indexValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();

    // If result is error, throw a RangeError exception.
    if (UNLIKELY(index >= wasm_table_size(tableaddr))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssemblyDotTable.string(), false, strings->get.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }
    // Let result be table_read(store, tableaddr, index).
    own wasm_ref_t* result = wasm_table_get(tableaddr, index);

    own wasm_tabletype_t* tabletype = wasm_table_type(tableaddr);
    wasm_valkind_t elementType = wasm_valtype_kind(wasm_tabletype_element(tabletype));
    wasm_tabletype_delete(tabletype);

    // Return ToJSValue(result).
    wasm_val_t resultVal;
    if (elementType == WASM_ANYREF) {
        resultVal.kind = WASM_ANYREF;
        resultVal.of.ref = result;
    } else {
        ASSERT(elementType == WASM_FUNCREF);
        resultVal.kind = WASM_FUNCREF;
        resultVal.of.ref = result;
    }
    Value resultValue = WASMValueConverter::toJSValue(state, resultVal);
    if (result && !WASMCacheMap::isOtherExternAddr(result)) {
        // result could be nullptr (ref.null)
        wasm_ref_delete(result);
    }

    return resultValue;
}

static Value builtinWASMTableSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value indexValue = wasmGetValueFromMaybeObject(state, argv[0], strings->valueOf);
    if (UNLIKELY(!indexValue.isUInt32())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_table_size_t index = indexValue.asUInt32();

    // Let tableaddr be this.[[Table]].
    wasm_table_t* tableaddr = thisValue.asObject()->asWASMTableObject()->table();

    // Let (limits, elementType) be table_type(tableaddr).
    own wasm_tabletype_t* tabletype = wasm_table_type(tableaddr);
    wasm_valkind_t elementType = wasm_valtype_kind(wasm_tabletype_element(tabletype));
    wasm_tabletype_delete(tabletype);

    ASSERT(elementType == WASM_ANYREF || elementType == WASM_FUNCREF);

    wasm_ref_t* ref = nullptr;
    wasm_val_t value;
    if (argc > 1) {
        // Let ref be ? ToWebAssemblyValue(value, elementType).
        value = WASMValueConverter::toWebAssemblyValue(state, argv[1], elementType);
    } else {
        // If value is missing,
        // Let ref be DefaultValue(elementType).
        value = WASMValueConverter::defaultValue(state, elementType);
    }
    ASSERT(value.kind == WASM_ANYREF || value.kind == WASM_FUNCREF);
    ref = value.of.ref;

    // Let store be table_write(store, tableaddr, index, ref).
    bool result = wasm_table_set(tableaddr, index, ref);
    // If store is error, throw a RangeError exception.
    if (!result) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->WebAssemblyDotTable.string(), false, strings->set.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    // Return undefined.
    return Value();
}

static Value builtinWASMTableLengthGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMTableObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotTable.string(), false, state.context()->staticStrings().length.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
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
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Global.string(), ErrorObject::Messages::Not_Invoked_With_New);
    }

    Value desc = argv[0];

    // check and get 'mutable' property from the first argument
    // set default value as const
    wasm_mutability_t mut = WASM_CONST;
    {
        auto mutValue = wasmGetValueFromObjectProperty(state, desc, strings->stringMutable, AtomicString()).second;
        mut = mutValue.toBoolean() ? WASM_VAR : WASM_CONST;
    }

    // Let valuetype be ToValueType(descriptor["value"]).
    wasm_valkind_t valuetype = WASMValueConverter::toValueType(state, wasmGetValueFromObjectProperty(state, desc, strings->value, strings->toString).second);
    // If valuetype is v128,
    if (valuetype == WASM_V128) {
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssembly.string(), false, strings->Global.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    wasm_val_t value;
    if (argc > 1 && !argv[1].isUndefined()) {
        // Let value be ToWebAssemblyValue(v, valuetype).
        value = WASMValueConverter::toWebAssemblyValue(state, argv[1], valuetype);
    } else {
        // If v is missing,
        // Let value be DefaultValue(valuetype).
        value = WASMValueConverter::defaultValue(state, valuetype);
    }

    // If mutable is true, let globaltype be var valuetype; otherwise, let globaltype be const valuetype.
    own wasm_globaltype_t* globaltype = wasm_globaltype_new(wasm_valtype_new(valuetype), mut);

    // Let (store, globaladdr) be global_alloc(store, globaltype, value).
    own wasm_global_t* globaladdr = wasm_global_new(ThreadLocal::wasmStore(), globaltype, &value);
    wasm_globaltype_delete(globaltype);

    wasm_ref_t* globalref = wasm_global_as_ref(globaladdr);

    // Let map be the surrounding agent's associated Global object cache.

    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%WebAssemblyGlobalPrototype%").
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->wasmGlobalPrototype();
    });

    // Set global.[[Global]] to globaladdr.
    WASMGlobalObject* globalObj = new WASMGlobalObject(state, proto, globaladdr);

    // Set map[globaladdr] to global.
    state.context()->wasmCache()->appendGlobal(globalref, globalObj);

    return globalObj;
}

static Value builtinWASMGlobalValueGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMGlobalObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    Value value = thisValue.asObject()->asWASMGlobalObject()->getGlobalValue(state);
    return value;
}

static Value builtinWASMGlobalValueSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isWASMGlobalObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    if (argc == 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let globaladdr be this.[[Global]].
    wasm_global_t* globaladdr = thisValue.asObject()->asWASMGlobalObject()->global();

    // Let mut valuetype be global_type(store, globaladdr).
    own wasm_globaltype_t* globaltype = wasm_global_type(globaladdr);
    wasm_mutability_t mut = wasm_globaltype_mutability(globaltype);
    wasm_valkind_t valuetype = wasm_valtype_kind(wasm_globaltype_content(globaltype));
    wasm_globaltype_delete(globaltype);

    // If mut is const, throw a TypeError.
    if (mut == WASM_CONST) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().WebAssemblyDotGlobal.string(), false, state.context()->staticStrings().value.string(), ErrorObject::Messages::WASM_SetToGlobalConstValue);
    }

    // Let value be ToWebAssemblyValue(the given value, valuetype).
    // Note) value should not have any reference in itself, so we don't have to call `wasm_val_delete`
    own wasm_val_t value = WASMValueConverter::toWebAssemblyValue(state, argv[0], valuetype);

    // Let store be global_write(store, globaladdr, value).
    // TODO If store is error, throw a RangeError exception.
    wasm_global_set(globaladdr, &value);

    return Value();
}

// WebAssemblly.Error
#define DEFINE_ERROR_CTOR(errorName, lowerCaseErrorName)                                                                                                                                                                                                \
    static Value builtin##errorName##ErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)                                                                                                    \
    {                                                                                                                                                                                                                                                   \
        if (!newTarget.hasValue()) {                                                                                                                                                                                                                    \
            newTarget = state.resolveCallee();                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                               \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {                                                                                                 \
            return constructorRealm->globalObject()->lowerCaseErrorName##ErrorPrototype();                                                                                                                                                              \
        });                                                                                                                                                                                                                                             \
        ErrorObject* obj = new errorName##ErrorObject(state, proto, String::emptyString);                                                                                                                                                               \
        Value message = argv[0];                                                                                                                                                                                                                        \
        if (!message.isUndefined()) {                                                                                                                                                                                                                   \
            obj->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,                                                                                                                                                      \
                                                  ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
        }                                                                                                                                                                                                                                               \
        return obj;                                                                                                                                                                                                                                     \
    }

DEFINE_ERROR_CTOR(WASMCompile, wasmCompile)
DEFINE_ERROR_CTOR(WASMLink, wasmLink)
DEFINE_ERROR_CTOR(WASMRuntime, wasmRuntime)

void GlobalObject::initializeWebAssembly(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->wasm();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().WebAssembly), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installWebAssembly(ExecutionState& state)
{
    // builtin Error should be installed ahead
    if (!m_error) {
        error();
    }
    ASSERT(!!m_error);

    Object* wasm = new Object(state);
    wasm->setGlobalIntrinsicObject(state);
    m_wasm = wasm;

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
                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->instantiate, builtinWASMInstantiate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));


    // WebAssembly.Module
    FunctionObject* wasmModule = new NativeFunctionObject(state, NativeFunctionInfo(strings->Module, builtinWASMModuleConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmModule->setGlobalIntrinsicObject(state);

    m_wasmModulePrototype = new Object(state);
    m_wasmModulePrototype->setGlobalIntrinsicObject(state, true);

    m_wasmModulePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(wasmModule, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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


    // WebAssembly.Instance
    FunctionObject* wasmInstance = new NativeFunctionObject(state, NativeFunctionInfo(strings->Instance, builtinWASMInstanceConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmInstance->setGlobalIntrinsicObject(state);

    m_wasmInstancePrototype = new Object(state);
    m_wasmInstancePrototype->setGlobalIntrinsicObject(state, true);

    m_wasmInstancePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(wasmInstance, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_wasmInstancePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                               ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotInstance.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getExports, builtinWASMInstanceExportsGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor exportsDesc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
        m_wasmInstancePrototype->defineOwnProperty(state, ObjectPropertyName(strings->exports), exportsDesc);
    }

    wasmInstance->setFunctionPrototype(state, m_wasmInstancePrototype);
    wasm->defineOwnProperty(state, ObjectPropertyName(strings->Instance),
                            ObjectPropertyDescriptor(wasmInstance, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // WebAssembly.Memory
    FunctionObject* wasmMemory = new NativeFunctionObject(state, NativeFunctionInfo(strings->Memory, builtinWASMMemoryConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    wasmMemory->setGlobalIntrinsicObject(state);

    m_wasmMemoryPrototype = new Object(state);
    m_wasmMemoryPrototype->setGlobalIntrinsicObject(state, true);

    m_wasmMemoryPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(wasmMemory, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(wasmTable, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                            ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotTable.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->grow),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->grow, builtinWASMTableGrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->get),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->get, builtinWASMTableGet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    m_wasmTablePrototype->defineOwnProperty(state, ObjectPropertyName(strings->set),
                                            ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->set, builtinWASMTableSet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getLength, builtinWASMTableLengthGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
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

    m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(wasmGlobal, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                             ObjectPropertyDescriptor(state.context()->staticStrings().WebAssemblyDotGlobal.string(), ObjectPropertyDescriptor::ConfigurablePresent));

    m_wasmGlobalPrototype->defineOwnProperty(state, ObjectPropertyName(strings->valueOf),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinWASMGlobalValueGetter, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings->getValue, builtinWASMGlobalValueGetter, 0, NativeFunctionInfo::Strict)),
            new NativeFunctionObject(state, NativeFunctionInfo(strings->setValue, builtinWASMGlobalValueSetter, 1, NativeFunctionInfo::Strict)));
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


    redefineOwnProperty(state, ObjectPropertyName(strings->WebAssembly),
                        ObjectPropertyDescriptor(wasm, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot

#else

#include "Escargot.h"
#include "runtime/GlobalObject.h"

namespace Escargot {
class ExecutionState;

void GlobalObject::initializeWebAssembly(ExecutionState& state)
{
    // dummy initialize function
}
} // namespace Escargot

#endif
