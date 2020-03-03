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
#include "ArrayBufferObject.h"
#include "VMInstance.h"
#include "Context.h"

namespace Escargot {

ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    , m_context(state.context())
    , m_data(nullptr)
    , m_bytelength(0)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->arrayBufferPrototype());

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        if (self->m_data) {
            self->m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferFree(self->m_context, self, self->m_data);
        }
    },
                                   nullptr, nullptr, nullptr);
}

void ArrayBufferObject::allocateBuffer(ExecutionState& state, size_t bytelength)
{
    ASSERT(isDetachedBuffer());

    const size_t ratio = std::max((size_t)GC_get_free_space_divisor() / 6, (size_t)1);
    if (ArrayBufferObject::maxArrayBufferSize <= bytelength) {
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

    m_data = (uint8_t*)m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferMalloc(m_context, this, bytelength);
    m_bytelength = bytelength;
}

void ArrayBufferObject::attachBuffer(ExecutionState& state, void* buffer, size_t bytelength)
{
    ASSERT(isDetachedBuffer());
    m_data = (uint8_t*)buffer;
    m_bytelength = bytelength;
}

void ArrayBufferObject::detachArrayBuffer(ExecutionState& state)
{
    if (m_data) {
        m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferFree(m_context, this, m_data);
    }
    ASSERT(!this->isSharedArrayBufferObject());
    m_data = NULL;
    m_bytelength = 0;
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-clonearraybuffer
bool ArrayBufferObject::cloneBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset)
{
    unsigned srcLength = srcBuffer->byteLength();
    ASSERT(srcByteOffset <= srcLength);
    unsigned cloneLength = srcLength - srcByteOffset;
    return cloneBuffer(state, srcBuffer, srcByteOffset, cloneLength);
}

bool ArrayBufferObject::cloneBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset, size_t cloneLength)
{
    if (srcBuffer->isDetachedBuffer())
        return false;
    allocateBuffer(state, cloneLength);
    fillData(srcBuffer->data() + srcByteOffset, cloneLength);
    return true;
}

void* ArrayBufferObject::operator new(size_t size)
{
    return CustomAllocator<ArrayBufferObject>().allocate(1);
}
}
