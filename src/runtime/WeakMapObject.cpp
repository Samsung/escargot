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
#include "WeakMapObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakMapObject::WeakMapObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->weakMapPrototype());
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
        auto existingKey = m_storage[i].first;
        if (existingKey == key) {
            m_storage[i] = std::make_pair(nullptr, Value(Value::EmptyValue));
            adjustGCThings();
            return true;
        }
    }
    return false;
}

Value WeakMapObject::get(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i].first;
        if (existingKey == key) {
            return m_storage[i].second;
        }
    }
    return Value();
}

bool WeakMapObject::has(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i].first;
        if (existingKey == key) {
            return true;
        }
    }
    return false;
}

void WeakMapObject::adjustGCThings()
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        GC_GENERAL_REGISTER_DISAPPEARING_LINK((void**)&(m_storage[i].first), m_storage[i].first);
        if (m_storage[i].second.isStoredInHeap()) {
            GC_GENERAL_REGISTER_DISAPPEARING_LINK((void**)&(m_storage[i].second), Value(m_storage[i].second).asPointerValue());
        }
    }
}

void WeakMapObject::set(ExecutionState& state, Object* key, const Value& value)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i].first;
        if (existingKey == key) {
            m_storage[i].second = value;
            return;
        }
    }

    m_storage.pushBack(std::make_pair(key, value));

    adjustGCThings();
}
}
