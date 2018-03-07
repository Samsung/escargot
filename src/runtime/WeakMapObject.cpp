/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "WeakMapObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakMapObject::WeakMapObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->weakMapPrototype());
}

void* WeakMapObject::WeakMapObjectDataItem::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WeakMapObject::WeakMapObjectDataItem)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakMapObject::WeakMapObjectDataItem, data));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WeakMapObject::WeakMapObjectDataItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* WeakMapObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(WeakMapObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakMapObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakMapObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakMapObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(WeakMapObject, m_storage));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(WeakMapObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool WeakMapObject::deleteOperation(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            m_storage[i]->key = nullptr;
            m_storage[i]->data = SmallValue(nullptr);
            return true;
        }
    }
    return false;
}

Value WeakMapObject::get(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            return m_storage[i]->data;
        }
    }
    return Value();
}

bool WeakMapObject::has(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            return true;
        }
    }
    return false;
}


void WeakMapObject::set(ExecutionState& state, Object* key, const Value& value)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            m_storage[i]->data = value;
            return;
        }
    }

    auto newData = new WeakMapObjectDataItem();
    newData->key = key;
    newData->data = value;
    GC_GENERAL_REGISTER_DISAPPEARING_LINK((void**)&(newData->key), newData->key);
    m_storage.pushBack(newData);
}
}
