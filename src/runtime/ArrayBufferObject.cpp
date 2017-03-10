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

ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    , m_data(nullptr)
    , m_bytelength(0)
{
    setPrototype(state, state.context()->globalObject()->arrayBufferPrototype());
}

// static int callocTotal;

void ArrayBufferObject::allocateBuffer(size_t bytelength)
{
    ASSERT(isDetachedBuffer());

    m_data = (uint8_t*)calloc(1, bytelength);
    m_bytelength = bytelength;
    // callocTotal += bytelength;
    // ESCARGOT_LOG_INFO("callocTotal %lf\n", callocTotal / 1024.0 / 1024.0);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        free(self->m_data);
        // callocTotal -= self->m_bytelength;
        // ESCARGOT_LOG_INFO("callocTotal %lf\n", callocTotal / 1024.0 / 1024.0);
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
