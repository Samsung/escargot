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

#include "Escargot.h"
#include "BigIntObject.h"
#include "Context.h"
#include "heap/Heap.h"

namespace Escargot {

BigIntObject::BigIntObject(ExecutionState& state, BigInt* value)
    : BigIntObject(state, state.context()->globalObject()->bigIntPrototype(), value)
{
}

BigIntObject::BigIntObject(ExecutionState& state, Object* proto, BigInt* value)
    : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1)
    , m_primitiveValue(value)
{
}

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
void* BigIntObject::operator new(size_t size)
{
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::BigIntObject))) {
        GC_word bitmap[(sizeof(BigIntObject) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        fillCompressedGCDescriptor(bitmap);
        GC_set_bit(bitmap, offsetof(BigIntObject, m_primitiveValue) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::BigIntObject, size, bitmap, sizeof(BigIntObject) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::BigIntObject, size);
}
#endif

} // namespace Escargot
