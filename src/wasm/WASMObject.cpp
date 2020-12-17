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
#include "wasm/WASMObject.h"
#include "wasm/WASMValueConverter.h"

namespace Escargot {

WASMHostFunctionEnvironment::WASMHostFunctionEnvironment(Object* f, wasm_functype_t* ft)
    : func(f)
    , functype(ft)
{
    ASSERT(!!ft && !!f);
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
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMHostFunctionEnvironment)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMHostFunctionEnvironment, func));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMHostFunctionEnvironment));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMModuleObject::WASMModuleObject(ExecutionState& state, wasm_module_t* module)
    : Object(state, state.context()->globalObject()->wasmModulePrototype())
    , m_module(module)
{
    ASSERT(!!m_module);

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMModuleObject* self = (WASMModuleObject*)obj;
        wasm_module_delete(self->module());
    },
                                   nullptr, nullptr, nullptr);
}

void* WASMModuleObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMModuleObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMModuleObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMMemoryObject::WASMMemoryObject(ExecutionState& state, wasm_memory_t* memory, ArrayBufferObject* buffer)
    : Object(state, state.context()->globalObject()->wasmMemoryPrototype())
    , m_memory(memory)
    , m_buffer(buffer)
{
    ASSERT(!!m_memory && !!m_buffer);

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMMemoryObject* self = (WASMMemoryObject*)obj;
        wasm_memory_delete(self->memory());
        self->buffer()->detachArrayBufferWithoutFree();
    },
                                   nullptr, nullptr, nullptr);
}

void* WASMMemoryObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMMemoryObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMMemoryObject, m_buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMMemoryObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
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
    : Object(state, state.context()->globalObject()->wasmTablePrototype())
    , m_table(table)
{
    ASSERT(!!m_table);

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMTableObject* self = (WASMTableObject*)obj;
        wasm_table_delete(self->table());
    },
                                   nullptr, nullptr, nullptr);
}

void* WASMTableObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMTableObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMTableObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

WASMGlobalObject::WASMGlobalObject(ExecutionState& state, wasm_global_t* global)
    : Object(state, state.context()->globalObject()->wasmGlobalPrototype())
    , m_global(global)
{
    ASSERT(!!m_global);

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMGlobalObject* self = (WASMGlobalObject*)obj;
        wasm_global_delete(self->global());
    },
                                   nullptr, nullptr, nullptr);
}

void* WASMGlobalObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WASMGlobalObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMGlobalObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Value WASMGlobalObject::getGlobalValue(ExecutionState& state) const
{
    // Let globaladdr be global.[[Global]].
    wasm_global_t* globaladdr = global();

    // Let value be global_read(store, globaladdr).
    wasm_val_t value;
    wasm_global_get(globaladdr, &value);

    // Return ToJSValue(value).
    return WASMValueConverter::wasmToJSValue(state, value);
}
}

#endif // ENABLE_WASM
