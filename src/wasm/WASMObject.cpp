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

    addFinalizer([](Object* obj, void* data) {
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

    addFinalizer([](Object* obj, void* data) {
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

    addFinalizer([](Object* obj, void* data) {
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

WASMMemoryObject* WASMMemoryObject::createMemoryObject(ExecutionState& state, wasm_memory_t* memory)
{
    ASSERT(!!memory);

    wasm_ref_t* memref = wasm_memory_as_ref(memory);

    // Let map be the surrounding agent's associated Memory object cache.
    WASMMemoryMap& map = state.context()->wasmCache()->memoryMap;
    for (auto iter = map.begin(); iter != map.end(); iter++) {
        wasm_ref_t* ref = iter->first;
        if (wasm_ref_same(memref, ref)) {
            return iter->second;
        }
    }

    // Let memory be a new Memory.
    // Initialize memory from memory.
    ArrayBufferObject* buffer = new ArrayBufferObject(state);

    // Note) wasm_memory_data with zero size returns null pointer
    // predefined temporal address is allocated for this case
    void* dataBlock = wasm_memory_size(memory) == 0 ? WASMEmptyBlockAddress : wasm_memory_data(memory);

    // Init BackingStore with empty deleter
    BackingStore* backingStore = BackingStore::createNonSharedBackingStore(dataBlock, wasm_memory_data_size(memory),
                                                                           [](void* data, size_t length, void* deleterData) {}, nullptr);
    buffer->attachBuffer(backingStore);

    // Set memory.[[Memory]] to memory.
    // Set memory.[[BufferObject]] to buffer.
    WASMMemoryObject* memoryObj = new WASMMemoryObject(state, memory, buffer);

    // Set map[memory] to memory.
    map.pushBack(std::make_pair(memref, memoryObj));

    //  Return memory.
    return memoryObj;
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

    addFinalizer([](Object* obj, void* data) {
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
    WASMTableMap& map = state.context()->wasmCache()->tableMap;
    for (auto iter = map.begin(); iter != map.end(); iter++) {
        wasm_ref_t* ref = iter->first;
        if (wasm_ref_same(tableref, ref)) {
            return iter->second;
        }
    }

    // Let table be a new Table.
    WASMTableObject* table = new WASMTableObject(state, tableaddr);

    // Initialize table from tableaddr.
    // Set map[tableaddr] to table.
    map.pushBack(std::make_pair(tableref, table));

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

    addFinalizer([](Object* obj, void* data) {
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
    WASMGlobalMap& map = state.context()->wasmCache()->globalMap;
    for (auto iter = map.begin(); iter != map.end(); iter++) {
        wasm_ref_t* ref = iter->first;
        if (wasm_ref_same(globalref, ref)) {
            // Return map[globaladdr].
            return iter->second;
        }
    }

    // Let global be a new Global.
    WASMGlobalObject* global = new WASMGlobalObject(state, globaladdr);

    // Initialize global from globaladdr.
    // Set map[globaladdr] to global.
    map.pushBack(std::make_pair(globalref, global));

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
} // namespace Escargot

#endif // ENABLE_WASM
