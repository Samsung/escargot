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
#include "wasm/WASMModuleObject.h"
#include "wasm.h"

namespace Escargot {

WASMModuleObject::WASMModuleObject(ExecutionState& state, wasm_module_t* module)
    : Object(state, state.context()->globalObject()->wasmModulePrototype())
    , m_module(module)
    , m_kind(ImportExportKind::Function)
{
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
}

#endif // ENABLE_WASM
