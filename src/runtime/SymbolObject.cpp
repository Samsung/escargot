/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "SymbolObject.h"
#include "Context.h"

namespace Escargot {

SymbolObject::SymbolObject(ExecutionState& state, Symbol* value)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, true)
    , m_primitiveValue(value)
{
    m_structure = state.context()->defaultStructureForSymbolObject();
    setPrototype(state, state.context()->globalObject()->symbolPrototype());
}

void* SymbolObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SymbolObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SymbolObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SymbolObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SymbolObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SymbolObject, m_primitiveValue));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SymbolObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
