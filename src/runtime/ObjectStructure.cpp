/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
