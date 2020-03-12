/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "WeakSetObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakSetObject::WeakSetObject(ExecutionState& state)
    : Object(state)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->weakSetPrototype());
}

WeakSetObject::WeakSetObject(ExecutionState& state, Object* proto)
    : Object(state)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, proto);
}

void* WeakSetObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WeakSetObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakSetObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakSetObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakSetObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakSetObject, m_storage));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WeakSetObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool WeakSetObject::deleteOperation(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]) {
            Object* existingKey = m_storage[i]->key;
            if (existingKey == key) {
                m_storage[i] = nullptr;
                return true;
            }
        }
    }
    return false;
}

void WeakSetObject::add(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]) {
            Object* existingKey = m_storage[i]->key;
            if (existingKey == key) {
                return;
            }
        }
    }

    auto newData = new WeakSetObjectDataItem();
    newData->key = key;
    GC_GENERAL_REGISTER_DISAPPEARING_LINK((void**)&(newData->key), newData->key);
    m_storage.pushBack(newData);
}

bool WeakSetObject::has(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]) {
            Object* existingKey = m_storage[i]->key;
            if (existingKey == key) {
                return true;
            }
        }
    }
    return false;
}
}
