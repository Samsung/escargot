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

#include "Escargot.h"
#include "wasm.h"
#include "runtime/VMInstance.h"
#include "runtime/Job.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "wasm/WASMObject.h"
#include "wasm/ExportedFunctionObject.h"
#include "wasm/WASMValueConverter.h"
#include "wasm/WASMOperations.h"

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

static own wasm_func_t* wasmCreateHostFunction(ExecutionState& state, Object* func, wasm_functype_t* functype)
{
    // Assert: IsCallable(func).
    ASSERT(func->isCallable());

    // Let store be the surrounding agent's associated store.
    // Let (store, funcaddr) be func_alloc(store, functype, hostfunc).
    // NOTE) we should clone functype here because functype needs to be maintained for host function call later.
    own wasm_functype_t* functypeCopy = wasm_functype_copy(functype);

    WASMHostFunctionEnvironment* env = new WASMHostFunctionEnvironment(func, functypeCopy);
    own wasm_func_t* funcaddr = wasm_func_new_with_env(state.context()->vmInstance()->wasmStore(), functypeCopy, callbackHostFunction, env, nullptr);

    state.context()->wasmEnvCache()->push_back(env);

    // Return funcaddr.
    return funcaddr;
}

static Value wasmInstantiateModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argc > 0);
    ASSERT(argv[0].isObject() && argv[0].asObject()->isWASMModuleObject());

    // Let module be moduleObject.[[Module]].
    WASMModuleObject* moduleObject = argv[0].asPointerValue()->asWASMModuleObject();
    wasm_module_t* module = moduleObject->module();

    // Read the imports of module with imports importObject, and let imports be the result.
    Value importObject = state.resolveCallee()->asExtendedNativeFunctionObject()->internalSlot(0);
    own wasm_extern_vec_t imports;
    wasm_extern_vec_new_empty(&imports);
    WASMOperations::readImportsOfModule(state, module, importObject, &imports);

    // Instantiate the core of a WebAssembly module module with imports, and let instance be the result.
    own wasm_trap_t* trap = nullptr;
    own wasm_instance_t* instance = wasm_instance_new(state.context()->vmInstance()->wasmStore(), module, imports.data, &trap);
    wasm_extern_vec_delete(&imports);

    if (!instance) {
        own wasm_name_t message;
        wasm_trap_message(trap, &message);
        ESCARGOT_LOG_ERROR("[WASM Message] %s\n", message.data);
        wasm_name_delete(&message);
        wasm_trap_delete(trap);
        ErrorObject::throwBuiltinError(state, ErrorObject::WASMLinkError, ErrorObject::Messages::WASM_InstantiateModuleError);
    }

    // Initialize instanceObject from module and instance.
    // Create an exports object from module and instance and let exportsObject be the result.
    Object* exportsObject = WASMOperations::createExportsObject(state, module, instance);

    // FIXME consider getting proto by GetPrototypeFromConstructor
    // Let instanceObject be a new Instance.
    // Set instanceObject.[[Instance]] to instance.
    // Set instanceObject.[[Exports]] to exportsObject.
    WASMInstanceObject* instanceObject = new WASMInstanceObject(state, instance, exportsObject);

    // Let result be the WebAssemblyInstantiatedSource value <<[ "module" -> module, "instance" -> instance ]>>.
    Object* result = new Object(state);
    result->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().module), ObjectPropertyDescriptor(moduleObject, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    result->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().instance), ObjectPropertyDescriptor(instanceObject, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

    return result;
}

Value WASMOperations::copyStableBufferBytes(ExecutionState& state, Value source)
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

Value WASMOperations::compileModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

    own wasm_byte_vec_t binary;
    wasm_byte_vec_new_uninitialized(&binary, byteLength);
    memcpy(binary.data, srcBuffer->data(), byteLength);

    own wasm_module_t* module = wasm_module_new(state.context()->vmInstance()->wasmStore(), &binary);
    wasm_byte_vec_delete(&binary);

    if (!module) {
        // throw WebAssembly.CompileError
        ErrorObject::throwBuiltinError(state, ErrorObject::WASMCompileError, ErrorObject::Messages::WASM_CompileError);
        return Value();
    }

    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%WebAssemblyModulePrototype%").
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->wasmModulePrototype();
    });

    // Construct a WebAssembly module object
    return new WASMModuleObject(state, proto, module);
}

Object* WASMOperations::asyncCompileModule(ExecutionState& state, Value source)
{
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    NativeFunctionObject* asyncCompiler = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), WASMOperations::compileModule, 1, NativeFunctionInfo::Strict));
    Job* job = new PromiseReactionJob(state.context(), PromiseReaction(asyncCompiler, capability), source);
    state.context()->vmInstance()->enqueueJob(job);

    return capability.m_promise;
}

Object* WASMOperations::createExportsObject(ExecutionState& state, wasm_module_t* module, wasm_instance_t* instance)
{
    // Let exportsObject be ! ObjectCreate(null).
    Object* exportsObject = new Object(state, Object::PrototypeIsNull);

    own wasm_exporttype_vec_t export_types;
    wasm_module_exports(module, &export_types);

    own wasm_extern_vec_t exports;
    wasm_instance_exports(instance, &exports);
    ASSERT(export_types.size == exports.size);

    for (size_t i = 0; i < export_types.size; i++) {
        // For each (name, externtype) of module_exports(module),
        const wasm_name_t* name = wasm_exporttype_name(export_types.data[i]);
        String* nameValue = String::fromASCII(name->data, name->size);
        wasm_externkind_t externtype = wasm_externtype_kind(wasm_exporttype_type(export_types.data[i]));

        // FIXME Let externval be instance_export(instance, name).
        wasm_extern_t* externval = exports.data[i];
        ASSERT(externtype == wasm_extern_kind(externval));

        Value value;
        switch (externtype) {
        case WASM_EXTERN_FUNC: {
            // If externtype is of the form func functype,
            // Let func funcaddr be externval.
            wasm_func_t* funcaddr = wasm_extern_as_func(externval);

            // FIXME getting a function index from instance which is not a standard (we should obtain the index from module)
            uint32_t funcIndex = wasm_instance_func_index(instance, funcaddr);
            ASSERT(funcIndex != wasm_limits_max_default);

            // Let func be the result of creating a new Exported Function from funcaddr.
            // Let value be func.
            ExportedFunctionObject* func = ExportedFunctionObject::createExportedFunction(state, funcaddr, funcIndex);
            value = Value(func);
            break;
        }
        case WASM_EXTERN_GLOBAL: {
            // If externtype is of the form global mut globaltype,
            // Let global globaladdr be externval.
            wasm_global_t* globaladdr = wasm_extern_as_global(externval);

            // Let global be a new Global object created from globaladdr.
            // Let value be global.
            WASMGlobalObject* global = WASMGlobalObject::createGlobalObject(state, globaladdr);
            value = Value(global);
            break;
        }
        case WASM_EXTERN_MEMORY: {
            // If externtype is of the form mem memtype,
            // Let mem memaddr be externval.
            wasm_memory_t* memaddr = wasm_extern_as_memory(externval);

            // Let memory be a new Memory object created from memaddr.
            // Let value be memory.
            WASMMemoryObject* memory = WASMMemoryObject::createMemoryObject(state, memaddr);
            value = Value(memory);
            break;
        }
        case WASM_EXTERN_TABLE: {
            // If externtype is of the form table tabletype,
            // Let table tableaddr be externval.
            wasm_table_t* tableaddr = wasm_extern_as_table(externval);

            // Let table be a new Table object created from tableaddr.
            // Let value be table.
            WASMTableObject* table = WASMTableObject::createTableObject(state, tableaddr);
            value = Value(table);
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
        }

        // Let status be ! CreateDataProperty(exportsObject, name, value).
        exportsObject->defineOwnProperty(state, ObjectPropertyName(state, nameValue), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    }
    // Perform ! SetIntegrityLevel(exportsObject, "frozen").
    Object::setIntegrityLevel(state, exportsObject, false);

    wasm_exporttype_vec_delete(&export_types);

    // Return exportsObject.
    return exportsObject;
}

void WASMOperations::readImportsOfModule(ExecutionState& state, wasm_module_t* module, const Value& importObj, own wasm_extern_vec_t* imports)
{
    // If importObject is not undefined and not Object, throw a TypeError exception.
    if (!importObj.isUndefined() && !importObj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
    }

    own wasm_importtype_vec_t import_types;
    wasm_module_imports(module, &import_types);

    if (!importObj.isObject()) {
        ASSERT(importObj.isUndefined());
        // If module.imports is not empty, and importObject is not Object, throw a TypeError exception.
        bool throwError = import_types.size > 0 ? true : false;
        wasm_importtype_vec_delete(&import_types);
        if (throwError) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
        }
        return;
    }

    Object* importObject = importObj.asObject();

    // Let imports be << >>
    wasm_extern_vec_new_uninitialized(imports, import_types.size);
    size_t importsSize = 0;

    // For each (moduleName, componentName, externtype) of module_imports(module),
    for (size_t i = 0; i < import_types.size; i++) {
        const wasm_name_t* moduleName = wasm_importtype_module(import_types.data[i]);
        const wasm_name_t* compName = wasm_importtype_name(import_types.data[i]);
        wasm_externtype_t* externtype = const_cast<wasm_externtype_t*>(wasm_importtype_type(import_types.data[i]));

        String* moduleNameValue = String::fromASCII(moduleName->data, moduleName->size);
        String* compNameValue = String::fromASCII(compName->data, compName->size);

        // Let o be ? Get(importObject, moduleName).
        Value o = importObject->get(state, ObjectPropertyName(state, moduleNameValue)).value(state, importObject);
        // If Type(o) is not Object, throw a TypeError exception.
        if (!o.isObject()) {
            wasm_importtype_vec_delete(&import_types);
            wasm_extern_vec_delete_with_size(imports, importsSize);

            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
        }

        // Let v be ? Get(o, componentName).
        Value v = o.asObject()->get(state, ObjectPropertyName(state, compNameValue)).value(state, o);

        bool throwLinkError = false;
        size_t externFuncCount = 0;
        switch (wasm_externtype_kind(externtype)) {
        case WASM_EXTERN_FUNC: {
            // If externtype is of the form func functype,
            if (!v.isCallable()) {
                // If IsCallable(v) is false, throw a LinkError exception.
                throwLinkError = true;
                break;
            }

            wasm_func_t* funcaddr = nullptr;
            if (v.asObject()->isExportedFunctionObject()) {
                // If v has a [[FunctionAddress]] internal slot, and therefore is an Exported Function,
                // Let funcaddr be the value of v?s [[FunctionAddress]] internal slot.
                // Note) copy func reference because ExportedFunctionObject has the origin wasm_func_t reference
                funcaddr = wasm_func_copy(v.asObject()->asExportedFunctionObject()->function());
            } else {
                // Create a host function from v and functype, and let funcaddr be the result.
                funcaddr = wasmCreateHostFunction(state, v.asObject(), wasm_externtype_as_functype(externtype));

                // TODO Let index be the number of external functions in imports. This value index is known as the index of the host function funcaddr.
                externFuncCount++;
            }

            // Let externfunc be the external value func funcaddr.
            // Append externfunc to imports.
            imports->data[i] = wasm_func_as_extern(funcaddr);
            break;
        }
        case WASM_EXTERN_GLOBAL: {
            // If externtype is of the form global mut valtype,
            const wasm_valtype_t* valtype = wasm_globaltype_content(wasm_externtype_as_globaltype(externtype));

            wasm_global_t* globaladdr = nullptr;
            // If Type(v) is Number or BigInt,
            if (v.isNumber() || v.isBigInt()) {
                // If valtype is i64 and Type(v) is Number,
                if ((wasm_valtype_kind(valtype) == WASM_I64) && v.isNumber()) {
                    // Throw a LinkError exception.
                    throwLinkError = true;
                    break;
                }

                // If valtype is not i64 and Type(v) is BigInt,
                if ((wasm_valtype_kind(valtype) != WASM_I64) && v.isBigInt()) {
                    // Throw a LinkError exception.
                    throwLinkError = true;
                    break;
                }

                // Let value be ToWebAssemblyValue(v, valtype).
                // Note) value should not have any reference in itself, so we don't have to call `wasm_val_delete`
                own wasm_val_t value = WASMValueConverter::wasmToWebAssemblyValue(state, v, wasm_valtype_kind(valtype));

                // Let (store, globaladdr) be global_alloc(store, const valtype, value).
                // FIXME globaltype
                own wasm_globaltype_t* globaltype = wasm_globaltype_new(wasm_valtype_new(wasm_valtype_kind(valtype)), WASM_CONST);
                globaladdr = wasm_global_new(state.context()->vmInstance()->wasmStore(), globaltype, &value);
                wasm_globaltype_delete(globaltype);

            } else if (v.isObject() && v.asObject()->isWASMGlobalObject()) {
                // Otherwise, if v implements Global,
                // Let globaladdr be v.[[Global]].
                // Note) copy global reference because WASMGlobalObject has the origin wasm_global_t reference
                globaladdr = wasm_global_copy(v.asObject()->asWASMGlobalObject()->global());
            } else {
                // Throw a LinkError exception.
                throwLinkError = true;
                break;
            }

            // Let externglobal be global globaladdr.
            // Append externglobal to imports.
            imports->data[i] = wasm_global_as_extern(globaladdr);
            break;
        }
        case WASM_EXTERN_MEMORY: {
            // If externtype is of the form mem memtype,
            wasm_memorytype_t* memtype = wasm_externtype_as_memorytype(externtype);

            // If v does not implement Memory, throw a LinkError exception.
            if (!v.isObject() || !v.asObject()->isWASMMemoryObject()) {
                throwLinkError = true;
                break;
            }

            // Let externmem be the external value mem v.[[Memory]].
            // Append externmem to imports.
            // Note) copy memory reference because WASMMemoryObject has the origin wasm_memory_t reference
            wasm_memory_t* externmem = wasm_memory_copy(v.asObject()->asWASMMemoryObject()->memory());
            imports->data[i] = wasm_memory_as_extern(externmem);
            break;
        }
        case WASM_EXTERN_TABLE: {
            // If externtype is of the form table tabletype,
            wasm_tabletype_t* tabletype = wasm_externtype_as_tabletype(externtype);

            // If v does not implement Table, throw a LinkError exception.
            if (!v.isObject() || !v.asObject()->isWASMTableObject()) {
                throwLinkError = true;
                break;
            }

            // Let tableaddr be v.[[Table]].
            // Let externtable be the external value table tableaddr.
            // Append externtable to imports.
            // Note) copy table reference because WASMTableObject has the origin wasm_table_t reference
            wasm_table_t* tableaddr = wasm_table_copy(v.asObject()->asWASMTableObject()->table());
            imports->data[i] = wasm_table_as_extern(tableaddr);
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
        }

        if (UNLIKELY(throwLinkError)) {
            wasm_importtype_vec_delete(&import_types);
            wasm_extern_vec_delete_with_size(imports, importsSize);

            ErrorObject::throwBuiltinError(state, ErrorObject::WASMLinkError, ErrorObject::Messages::WASM_ReadImportsError);
        }

        importsSize++;
    }

    wasm_importtype_vec_delete(&import_types);
}

Value WASMOperations::instantiateCoreModule(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // argv[0] should be module object
    ASSERT(argc > 0);
    ASSERT(argv[0].isObject() && argv[0].asObject()->isWASMModuleObject());

    WASMModuleObject* moduleObj = argv[0].asPointerValue()->asWASMModuleObject();
    wasm_module_t* module = moduleObj->module();

    ExtendedNativeFunctionObject* callee = state.resolveCallee()->asExtendedNativeFunctionObject();
    size_t importsSize = callee->internalSlot(0).asUInt32();
    wasm_extern_t** importsData = callee->internalSlotAsPointer<wasm_extern_t*>(1);
    own wasm_extern_vec_t imports = { importsSize, importsData };
    callee->setInternalSlotAsPointer(1, nullptr);

    // Instantiate the core of a WebAssembly module module with imports, and let instance be the result.
    own wasm_trap_t* trap = nullptr;
    own wasm_instance_t* instance = wasm_instance_new(state.context()->vmInstance()->wasmStore(), module, imports.data, &trap);
    wasm_extern_vec_delete(&imports);

    if (!instance) {
        own wasm_name_t message;
        wasm_trap_message(trap, &message);
        ESCARGOT_LOG_ERROR("[WASM Message] %s\n", message.data);
        wasm_name_delete(&message);
        wasm_trap_delete(trap);
        ErrorObject::throwBuiltinError(state, ErrorObject::WASMLinkError, ErrorObject::Messages::WASM_InstantiateModuleError);
    }

    // Create an exports object from module and instance and let exportsObject be the result.
    Object* exportsObject = WASMOperations::createExportsObject(state, module, instance);

    // FIXME consider getting proto by GetPrototypeFromConstructor
    // Let instanceObject be a new Instance.
    // Initialize instanceObject from module and instance. If this throws an exception, catch it, reject promise with the exception, and terminate these substeps.
    WASMInstanceObject* instanceObject = new WASMInstanceObject(state, instance, exportsObject);

    // Resolve promise with instanceObject.
    return instanceObject;
}

Object* WASMOperations::instantiatePromiseOfModuleWithImportObject(ExecutionState& state, PromiseObject* promiseOfModule, Value importObj)
{
    auto moduleInstantiator = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), wasmInstantiateModule, 1, NativeFunctionInfo::Strict));
    moduleInstantiator->setInternalSlot(0, importObj);

    return promiseOfModule->then(state, moduleInstantiator);
}

} // namespace Escargot

#endif // ENABLE_WASM
