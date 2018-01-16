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
#include "WeakSetObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakSetObject::WeakSetObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->weakSetPrototype());
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
