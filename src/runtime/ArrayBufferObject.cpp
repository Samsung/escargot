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
    Object::setPrototype(state, state.context()->globalObject()->arrayBufferPrototype());
}

void ArrayBufferObject::allocateBuffer(ExecutionState& state, size_t bytelength)
{
    ASSERT(isDetachedBuffer());

    m_data = (uint8_t*)m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferMalloc(m_context, this, bytelength);
    m_bytelength = bytelength;
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        self->m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferFree(self->m_context, self, self->m_data);
    },
                                   nullptr, nullptr, nullptr);
}

void ArrayBufferObject::attachBuffer(ExecutionState& state, void* buffer, size_t bytelength)
{
    ASSERT(isDetachedBuffer());
    m_data = (uint8_t*)buffer;
    m_bytelength = bytelength;
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        self->m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferFree(self->m_context, self, self->m_data);
    },
                                   nullptr, nullptr, nullptr);
}

void ArrayBufferObject::detachArrayBuffer(ExecutionState& state)
{
    m_context->vmInstance()->platform()->onArrayBufferObjectDataBufferFree(m_context, this, m_data);
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
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_context));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
