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
#include "runtime/ArrayBufferObject.h"
#include "runtime/BackingStore.h"
#include "wasm/WASMObject.h"
#include "wasm/WASMValueConverter.h"
#include "wasm/ExportedFunctionObject.h"

// represent ownership of each object
// object marked with 'own' should be deleted in the current context
#define own

namespace Escargot {

WASMHostFunctionEnvironment::WASMHostFunctionEnvironment(Context* r, Object* f, wasm_functype_t* ft)
    : realm(r)
    , func(f)
    , functype(ft)
{
    ASSERT(!!r && !!ft && !!f);
    ASSERT(f->isCallable());

    // FIXME current wasm_func_new_with_env does not support env-finalizer,
    // so WASMHostFunctionEnvironment needs to be deallocated manually.
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMHostFunctionEnvironment* self = (WASMHostFunctionEnvironment*)obj;
        wasm_functype_delete(self->functype);
    },
                                   nullptr, nullptr, nullptr);
}

void* WASMHostFunctionEnvironment::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMHostFunctionEnvironment)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMHostFunctionEnvironment, realm));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMHostFunctionEnvironment, func));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMHostFunctionEnvironment));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMModuleObject::WASMModuleObject(ExecutionState& state, Object* proto, wasm_module_t* module)
    : DerivedObject(state, proto)
    , m_module(module)
{
    ASSERT(!!m_module);

    addFinalizer([](PointerValue* obj, void* data) {
        WASMModuleObject* self = (WASMModuleObject*)obj;
        wasm_module_delete(self->module());
    },
                 nullptr);
}

void* WASMModuleObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMModuleObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMModuleObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMInstanceObject::WASMInstanceObject(ExecutionState& state, wasm_instance_t* instance, Object* exports)
    : WASMInstanceObject(state, state.context()->globalObject()->wasmInstancePrototype(), instance, exports)
{
}

WASMInstanceObject::WASMInstanceObject(ExecutionState& state, Object* proto, wasm_instance_t* instance, Object* exports)
    : DerivedObject(state, proto)
    , m_instance(instance)
    , m_exports(exports)
{
    ASSERT(!!m_instance && !!m_exports);

    addFinalizer([](PointerValue* obj, void* data) {
        WASMInstanceObject* self = (WASMInstanceObject*)obj;
        wasm_instance_delete(self->instance());
    },
                 nullptr);
}

void* WASMInstanceObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMInstanceObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMInstanceObject, m_exports));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMInstanceObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMMemoryObject::WASMMemoryObject(ExecutionState& state, wasm_memory_t* memory, ArrayBufferObject* buffer)
    : WASMMemoryObject(state, state.context()->globalObject()->wasmMemoryPrototype(), memory, buffer)
{
}

WASMMemoryObject::WASMMemoryObject(ExecutionState& state, Object* proto, wasm_memory_t* memory, ArrayBufferObject* buffer)
    : DerivedObject(state, proto)
    , m_memory(memory)
    , m_buffer(buffer)
{
    ASSERT(!!m_memory && !!m_buffer);

    addFinalizer([](PointerValue* obj, void* data) {
        WASMMemoryObject* self = (WASMMemoryObject*)obj;
        wasm_memory_delete(self->memory());
        self->buffer()->detachArrayBuffer();
    },
                 nullptr);
}

void* WASMMemoryObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMMemoryObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMMemoryObject, m_buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMMemoryObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMMemoryObject* WASMMemoryObject::createMemoryObject(ExecutionState& state, wasm_memory_t* memaddr)
{
    ASSERT(!!memaddr);

    wasm_ref_t* memref = wasm_memory_as_ref(memaddr);

    // Let map be the surrounding agent's associated Memory object cache.
    WASMMemoryObject* memory = state.context()->wasmCache()->findMemory(memref);
    if (memory) {
        return memory;
    }

    // Let memory be a new Memory.
    // Initialize memory from memory.
    ArrayBufferObject* buffer = new ArrayBufferObject(state);

    // Note) wasm_memory_data with zero size returns null pointer
    // predefined temporal address is allocated for this case
    void* dataBlock = wasm_memory_size(memaddr) == 0 ? WASMEmptyBlockAddress : wasm_memory_data(memaddr);

    // Init BackingStore with empty deleter
    BackingStore* backingStore = BackingStore::createNonSharedBackingStore(dataBlock, wasm_memory_data_size(memaddr),
                                                                           [](void* data, size_t length, void* deleterData) {}, nullptr);
    buffer->attachBuffer(backingStore);

    // Set memory.[[Memory]] to memory.
    // Set memory.[[BufferObject]] to buffer.
    memory = new WASMMemoryObject(state, memaddr, buffer);

    // Set map[memory] to memory.
    state.context()->wasmCache()->appendMemory(memref, memory);

    //  Return memory.
    return memory;
}

ArrayBufferObject* WASMMemoryObject::buffer() const
{
    ASSERT(!!m_buffer && !m_buffer->isDetachedBuffer());
    return m_buffer;
}

void WASMMemoryObject::setBuffer(ArrayBufferObject* buffer)
{
    ASSERT(!!buffer && !buffer->isDetachedBuffer());
    m_buffer = buffer;
}

WASMTableObject::WASMTableObject(ExecutionState& state, wasm_table_t* table)
    : WASMTableObject(state, state.context()->globalObject()->wasmTablePrototype(), table)
{
}

WASMTableObject::WASMTableObject(ExecutionState& state, Object* proto, wasm_table_t* table)
    : DerivedObject(state, proto)
    , m_table(table)
{
    ASSERT(!!m_table);

    addFinalizer([](PointerValue* obj, void* data) {
        WASMTableObject* self = (WASMTableObject*)obj;
        wasm_table_delete(self->table());
    },
                 nullptr);
}

void* WASMTableObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMTableObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMTableObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMTableObject* WASMTableObject::createTableObject(ExecutionState& state, wasm_table_t* tableaddr)
{
    ASSERT(!!tableaddr);

    wasm_ref_t* tableref = wasm_table_as_ref(tableaddr);

    // Let map be the surrounding agent's associated Table object cache.
    // If map[tableaddr] exists,
    WASMTableObject* table = state.context()->wasmCache()->findTable(tableref);
    if (table) {
        return table;
    }

    // Let table be a new Table.
    table = new WASMTableObject(state, tableaddr);

    // Initialize table from tableaddr.
    // Set map[tableaddr] to table.
    state.context()->wasmCache()->appendTable(tableref, table);

    // Return table.
    return table;
}

WASMGlobalObject::WASMGlobalObject(ExecutionState& state, wasm_global_t* global)
    : WASMGlobalObject(state, state.context()->globalObject()->wasmGlobalPrototype(), global)
{
}

WASMGlobalObject::WASMGlobalObject(ExecutionState& state, Object* proto, wasm_global_t* global)
    : DerivedObject(state, proto)
    , m_global(global)
{
    ASSERT(!!m_global);

    addFinalizer([](PointerValue* obj, void* data) {
        WASMGlobalObject* self = (WASMGlobalObject*)obj;
        wasm_global_delete(self->global());
    },
                 nullptr);
}

void* WASMGlobalObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMGlobalObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMGlobalObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMGlobalObject* WASMGlobalObject::createGlobalObject(ExecutionState& state, wasm_global_t* globaladdr)
{
    ASSERT(!!globaladdr);

    wasm_ref_t* globalref = wasm_global_as_ref(globaladdr);

    // Let map be the current agent's associated Global object cache.
    // If map[globaladdr] exists,
    WASMGlobalObject* global = state.context()->wasmCache()->findGlobal(globalref);
    if (global) {
        return global;
    }

    // Let global be a new Global.
    global = new WASMGlobalObject(state, globaladdr);

    // Initialize global from globaladdr.
    // Set map[globaladdr] to global.
    state.context()->wasmCache()->appendGlobal(globalref, global);

    // Return global.
    return global;
}

Value WASMGlobalObject::getGlobalValue(ExecutionState& state) const
{
    // Let globaladdr be global.[[Global]].
    wasm_global_t* globaladdr = global();

    // Let value be global_read(store, globaladdr).
    // Note) value should not have any reference in itself, so we don't have to call `wasm_val_delete`
    own wasm_val_t value;
    wasm_global_get(globaladdr, &value);

    // Return ToJSValue(value).
    return WASMValueConverter::wasmToJSValue(state, value);
}

void WASMCacheMap::appendMemory(wasm_ref_t* ref, WASMMemoryObject* memoryObj)
{
    ASSERT(!!ref);
    ASSERT(!findMemory(ref));
    m_memoryMap.pushBack(std::make_pair(ref, memoryObj));
}

void WASMCacheMap::appendTable(wasm_ref_t* ref, WASMTableObject* tableObj)
{
    ASSERT(!!ref);
    ASSERT(!findTable(ref));
    m_tableMap.pushBack(std::make_pair(ref, tableObj));
}

void WASMCacheMap::appendGlobal(wasm_ref_t* ref, WASMGlobalObject* globalObj)
{
    ASSERT(!!ref);
    ASSERT(!findGlobal(ref));
    m_globalMap.pushBack(std::make_pair(ref, globalObj));
}

void WASMCacheMap::appendFunction(wasm_ref_t* ref, ExportedFunctionObject* funcObj)
{
    ASSERT(!!ref);
    ASSERT(!findFunction(ref));
    m_functionMap.pushBack(std::make_pair(ref, funcObj));
}

wasm_ref_t* WASMCacheMap::insertRefByValue(const Value& value)
{
    ASSERT(value.isObject());
    Object* val = value.asObject();
    wasm_ref_t* result = nullptr;

    if (val->isWASMMemoryObject()) {
        result = wasm_memory_as_ref(val->asWASMMemoryObject()->memory());
        appendMemory(result, val->asWASMMemoryObject());
    } else if (val->isWASMTableObject()) {
        result = wasm_table_as_ref(val->asWASMTableObject()->table());
        appendTable(result, val->asWASMTableObject());
    } else if (val->isWASMGlobalObject()) {
        result = wasm_global_as_ref(val->asWASMGlobalObject()->global());
        appendGlobal(result, val->asWASMGlobalObject());
    } else if (val->isExportedFunctionObject()) {
        result = wasm_func_as_ref(val->asExportedFunctionObject()->function());
        appendFunction(result, val->asExportedFunctionObject());
    } else {
        ASSERT_NOT_REACHED();
    }

    return result;
}

WASMMemoryObject* WASMCacheMap::findMemory(wasm_ref_t* ref)
{
    for (auto iter = m_memoryMap.begin(); iter != m_memoryMap.end(); iter++) {
        if (wasm_ref_same(iter->first, ref)) {
            return iter->second;
        }
    }
    return nullptr;
}

WASMTableObject* WASMCacheMap::findTable(wasm_ref_t* ref)
{
    for (auto iter = m_tableMap.begin(); iter != m_tableMap.end(); iter++) {
        if (wasm_ref_same(iter->first, ref)) {
            return iter->second;
        }
    }
    return nullptr;
}

WASMGlobalObject* WASMCacheMap::findGlobal(wasm_ref_t* ref)
{
    for (auto iter = m_globalMap.begin(); iter != m_globalMap.end(); iter++) {
        if (wasm_ref_same(iter->first, ref)) {
            return iter->second;
        }
    }
    return nullptr;
}

ExportedFunctionObject* WASMCacheMap::findFunction(wasm_ref_t* ref)
{
    for (auto iter = m_functionMap.begin(); iter != m_functionMap.end(); iter++) {
        if (wasm_ref_same(iter->first, ref)) {
            return iter->second;
        }
    }
    return nullptr;
}

wasm_ref_t* WASMCacheMap::findRefByValue(const Value& value)
{
    ASSERT(value.isObject());
    Object* val = value.asObject();
    wasm_ref_t* ref = nullptr;

    if (val->isWASMMemoryObject()) {
        ref = wasm_memory_as_ref(val->asWASMMemoryObject()->memory());
        for (auto iter = m_memoryMap.begin(); iter != m_memoryMap.end(); iter++) {
            if (wasm_ref_same(iter->first, ref)) {
                return ref;
            }
        }
        return nullptr;
    } else if (val->isWASMTableObject()) {
        ref = wasm_table_as_ref(val->asWASMTableObject()->table());
        for (auto iter = m_tableMap.begin(); iter != m_tableMap.end(); iter++) {
            if (wasm_ref_same(iter->first, ref)) {
                return ref;
            }
        }
        return nullptr;
    } else if (val->isWASMGlobalObject()) {
        ref = wasm_global_as_ref(val->asWASMGlobalObject()->global());
        for (auto iter = m_globalMap.begin(); iter != m_globalMap.end(); iter++) {
            if (wasm_ref_same(iter->first, ref)) {
                return ref;
            }
        }
        return nullptr;
    } else if (val->isExportedFunctionObject()) {
        ref = wasm_func_as_ref(val->asExportedFunctionObject()->function());
        for (auto iter = m_functionMap.begin(); iter != m_functionMap.end(); iter++) {
            if (wasm_ref_same(iter->first, ref)) {
                return ref;
            }
        }
        return nullptr;
    } else {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Value WASMCacheMap::findValueByRef(wasm_ref_t* ref)
{
    ASSERT(!!ref);

    Object* result = nullptr;

    if ((result = findFunction(ref))) {
        return result;
    }

    if ((result = findGlobal(ref))) {
        return result;
    }

    if ((result = findMemory(ref))) {
        return result;
    }

    if ((result = findTable(ref))) {
        return result;
    }

    // Assert: map[externaddr] exists.
    ASSERT_NOT_REACHED();
    return result;
}
} // namespace Escargot

#endif // ENABLE_WASM
