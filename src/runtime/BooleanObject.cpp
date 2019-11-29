/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "BooleanObject.h"
#include "Context.h"

namespace Escargot {

BooleanObject::BooleanObject(ExecutionState& state, bool value)
    : Object(state)
    , m_primitiveValue(value)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->booleanPrototype());
}

void* BooleanObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(BooleanObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_values));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(BooleanObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
