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
 *  Lesser General Public License for more detaials.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "AsyncFromSyncIteratorObject.h"

#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "heap/Heap.h"

namespace Escargot {

void* AsyncFromSyncIteratorObject::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::AsyncFromSyncIteratorObject))) {
        GC_word bitmap[(sizeof(AsyncFromSyncIteratorObject) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        Object::fillCompressedGCDescriptor(bitmap);
        GC_set_bit(bitmap, offsetof(AsyncFromSyncIteratorObject, m_syncIteratorRecord) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::AsyncFromSyncIteratorObject, size, bitmap, sizeof(AsyncFromSyncIteratorObject) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::AsyncFromSyncIteratorObject, size);
#else
    return GC_MALLOC(size);
#endif
}

AsyncFromSyncIteratorObject::AsyncFromSyncIteratorObject(ExecutionState& state, Object* proto, IteratorRecord* syncIteratorRecord)
    : DerivedObject(state, proto)
    , m_syncIteratorRecord(syncIteratorRecord)
{
}
} // namespace Escargot
