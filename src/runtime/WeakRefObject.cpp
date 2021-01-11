/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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
#include "WeakRefObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakRefObject::WeakRefObject(ExecutionState& state, Object* target)
    : WeakRefObject(state, state.context()->globalObject()->weakRefPrototype(), target)
{
}

WeakRefObject::WeakRefObject(ExecutionState& state, Object* proto, Object* target)
    : Object(state, proto)
    , m_target(target)
{
}

void* WeakRefObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WeakRefObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakRefObject, m_target));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WeakRefObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool WeakRefObject::deleteOperation(ExecutionState& state)
{
    m_target = nullptr;
    return true;
}

} // namespace Escargot
