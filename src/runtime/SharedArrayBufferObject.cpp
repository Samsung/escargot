/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#include "Escargot.h"
#include "SharedArrayBufferObject.h"
#include "VMInstance.h"
#include "Context.h"

namespace Escargot {

SharedArrayBufferObject::SharedArrayBufferObject(ExecutionState& state)
    : ArrayBufferObject(state)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->sharedArrayBufferPrototype());
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        SharedArrayBufferObject* self = (SharedArrayBufferObject*)obj;
        if (self->m_data) {
            self->m_context->vmInstance()->platform()->onSharedArrayBufferObjectDataBufferFree(self->m_context, self, self->m_data);
        }
    },
                                   nullptr, nullptr, nullptr);
}


void SharedArrayBufferObject::allocateBuffer(ExecutionState& state, size_t bytelength)
{
    const size_t ratio = std::max((size_t)GC_get_free_space_divisor() / 6, (size_t)1);
    if (maxArrayBufferSize <= bytelength) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), true, state.context()->staticStrings().ArrayBuffer.string(), "byteLength is highter then limit");
    }
    if (bytelength > (GC_get_heap_size() / ratio)) {
        size_t n = 0;
        size_t times = bytelength / (GC_get_heap_size() / ratio) / 3;
        do {
            GC_gcollect_and_unmap();
            n += 1;
        } while (n < times);
        GC_invoke_finalizers();
    }

    m_data = (uint8_t*)m_context->vmInstance()->platform()->onSharedArrayBufferObjectDataBufferMalloc(m_context, this, bytelength);
    m_bytelength = bytelength;
}
};
