/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if ESCARGOT_ENABLE_TYPEDARRAY
#include "Escargot.h"
#include "ArrayBufferObject.h"
#include "Context.h"

namespace Escargot {

static void* callocWrapper(size_t siz)
{
    return calloc(1, siz);
}

ArrayBufferObjectBufferMallocFunction g_arrayBufferObjectBufferMallocFunction = callocWrapper;
bool g_arrayBufferObjectBufferMallocFunctionNeedsZeroFill = false;
ArrayBufferObjectBufferFreeFunction g_arrayBufferObjectBufferFreeFunction = free;


ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    , m_data(nullptr)
    , m_bytelength(0)
{
    setPrototype(state, state.context()->globalObject()->arrayBufferPrototype());
}

void ArrayBufferObject::allocateBuffer(size_t bytelength)
{
    ASSERT(isDetachedBuffer());

    m_data = (uint8_t*)g_arrayBufferObjectBufferMallocFunction(bytelength);
    if (g_arrayBufferObjectBufferMallocFunctionNeedsZeroFill) {
        memset(m_data, 0, bytelength);
    }
    m_bytelength = bytelength;
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        g_arrayBufferObjectBufferFreeFunction(self->m_data);
    },
                                   nullptr, nullptr, nullptr);
}

void ArrayBufferObject::attachBuffer(void* buffer, size_t bytelength)
{
    ASSERT(isDetachedBuffer());
    m_data = (uint8_t*)buffer;
    m_bytelength = bytelength;
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        g_arrayBufferObjectBufferFreeFunction(self->m_data);
    },
                                   nullptr, nullptr, nullptr);
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-clonearraybuffer
bool ArrayBufferObject::cloneBuffer(ArrayBufferObject* srcBuffer, size_t srcByteOffset)
{
    unsigned srcLength = srcBuffer->bytelength();
    ASSERT(srcByteOffset <= srcLength);
    unsigned cloneLength = srcLength - srcByteOffset;
    return cloneBuffer(srcBuffer, srcByteOffset, cloneLength);
}

bool ArrayBufferObject::cloneBuffer(ArrayBufferObject* srcBuffer, size_t srcByteOffset, size_t cloneLength)
{
    if (srcBuffer->isDetachedBuffer())
        return false;
    allocateBuffer(cloneLength);
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
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}

#endif
