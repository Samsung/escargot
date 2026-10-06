/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "ScriptClassMethodFunctionObject.h"
#include "heap/Heap.h"

#include "FunctionObjectInlines.h"

namespace Escargot {
void* ScriptClassMethodFunctionObject::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ScriptClassMethodFunctionObject))) {
        GC_word bitmap[(sizeof(ScriptClassMethodFunctionObject) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        ScriptFunctionObject::fillCompressedGCDescriptor(bitmap);
        GC_set_bit(bitmap, offsetof(ScriptClassMethodFunctionObject, m_homeObject) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ScriptClassMethodFunctionObject, size, bitmap, sizeof(ScriptClassMethodFunctionObject) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ScriptClassMethodFunctionObject, size);
#else
    return ScriptFunctionObject::operator new(size);
#endif
}
} // namespace Escargot
