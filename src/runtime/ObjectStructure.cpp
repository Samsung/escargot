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
#include "Object.h"

namespace Escargot {

void* ObjectStructure::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructure)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructure, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructure, m_transitionTable));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructure));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureWithFastAccess::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        const size_t len = GC_BITMAP_SIZE(ObjectStructureWithFastAccess);
        GC_word obj_bitmap[len] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_transitionTable));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_propertyNameMap));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithFastAccess));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
