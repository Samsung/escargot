/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "runtime/VMInstance.h"
#include "runtime/BackingStore.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

ArrayBufferObject* ArrayBufferObject::allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-allocatearraybuffer
    Object* proto = Object::getPrototypeFromConstructor(state, constructor, [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->arrayBufferPrototype();
    });

    if (byteLength >= ArrayBuffer::maxArrayBufferSize) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    ArrayBufferObject* obj = new ArrayBufferObject(state, proto);
    obj->allocateBuffer(state, byteLength);

    return obj;
}

ArrayBufferObject* ArrayBufferObject::cloneArrayBuffer(ExecutionState& state, ArrayBuffer* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-clonearraybuffer
    ASSERT(constructor->isConstructor());

    ArrayBufferObject* targetBuffer = ArrayBufferObject::allocateArrayBuffer(state, constructor, srcLength);
    srcBuffer->throwTypeErrorIfDetached(state);

    targetBuffer->fillData(srcBuffer->data() + srcByteOffset, srcLength);

    return targetBuffer;
}

ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : ArrayBufferObject(state, state.context()->globalObject()->arrayBufferPrototype())
{
}

ArrayBufferObject::ArrayBufferObject(ExecutionState& state, Object* proto)
    : ArrayBuffer(state, proto)
{
}

void ArrayBufferObject::allocateBuffer(ExecutionState& state, size_t byteLength)
{
    detachArrayBuffer();

    ASSERT(byteLength < ArrayBuffer::maxArrayBufferSize);

    const size_t ratio = std::max((size_t)GC_get_free_space_divisor() / 6, (size_t)1);
    if (byteLength > (GC_get_heap_size() / ratio)) {
        size_t n = 0;
        size_t times = byteLength / (GC_get_heap_size() / ratio) / 3;
        do {
            GC_gcollect_and_unmap();
            n += 1;
        } while (n < times);
        GC_invoke_finalizers();
    }

    m_backingStore = BackingStore::createDefaultNonSharedBackingStore(byteLength);
}

void ArrayBufferObject::attachBuffer(BackingStore* backingStore)
{
    // BackingStore should not be Shared Data Block
    ASSERT(!backingStore->isShared());
    detachArrayBuffer();

    m_backingStore = backingStore;
}

void ArrayBufferObject::detachArrayBuffer()
{
    m_backingStore.reset();
}

void* ArrayBufferObject::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferObject)] = { 0 };
        ArrayBuffer::fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<ArrayBufferObject>().allocate(1);
#endif
}
} // namespace Escargot
