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
#include "runtime/Context.h"
#include "runtime/Object.h"
#include "runtime/BigInt.h"
#include "runtime/ArrayBufferObject.h"
#include "wasm/WASMObject.h"
#include "wasm.h"

namespace Escargot {

WASMModuleObject::WASMModuleObject(ExecutionState& state, wasm_module_t* module)
    : Object(state, state.context()->globalObject()->wasmModulePrototype())
    , m_module(module)
    , m_kind(ImportExportKind::Function)
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

WASMTableObject::WASMTableObject(ExecutionState& state, wasm_table_t* table, ValueVector* values)
    : Object(state, state.context()->globalObject()->wasmTablePrototype())
    , m_table(table)
    , m_values(values)
{
    ASSERT(!!m_table && !!m_values);
    ASSERT((size_t)wasm_table_size(table) == values->size());

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        WASMTableObject* self = (WASMTableObject*)obj;
        wasm_table_delete(self->table());
        self->values()->clear();
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
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WASMTableObject, m_values));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WASMTableObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Value WASMTableObject::getElement(size_t index) const
{
    ASSERT(!!m_table && !!m_values);
    ASSERT(index < length());

    return m_values->at(index);
}

void WASMTableObject::setElement(size_t index, const Value& value)
{
    ASSERT(!!m_table && !!m_values);
    ASSERT(index < length());
    m_values->at(index) = value;
}

size_t WASMTableObject::length() const
{
    ASSERT(!!m_table && !!m_values);
    return m_values->size();
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
    Value result;
    switch (value.kind) {
    case WASM_I32: {
        result = Value(value.of.i32);
        break;
    }
    case WASM_F32: {
        result = Value((double)value.of.f32);
        break;
    }
    case WASM_F64: {
        result = Value(value.of.f64);
        break;
    }
    case WASM_I64: {
        result = new BigInt(state.context()->vmInstance(), value.of.i64);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}
}

#endif // ENABLE_WASM
