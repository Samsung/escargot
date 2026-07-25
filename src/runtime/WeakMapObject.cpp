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
#include "WeakMapObject.h"
#include "ArrayObject.h"
#include "Context.h"
#include "KeyedCollectionHashIndex.h"

namespace Escargot {

WeakMapObject::WeakMapObject(ExecutionState& state)
    : WeakMapObject(state, state.context()->globalObject()->weakMapPrototype())
{
}

WeakMapObject::WeakMapObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

void* WeakMapObject::WeakMapObjectDataItem::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(WeakMapObjectDataItem)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(WeakMapObjectDataItem, data));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(WeakMapObjectDataItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<WeakMapObjectDataItem>().allocate(1);
#endif
}

void* WeakMapObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(WeakMapObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(WeakMapObject, m_storage));
        GC_set_bit(desc, GC_WORD_OFFSET(WeakMapObject, m_hashIndex));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(WeakMapObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool WeakMapObject::deleteOperation(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    size_t i = findKeyIndex(key);
    if (i == SIZE_MAX) {
        return false;
    }
    GC_unregister_disappearing_link(reinterpret_cast<void**>(&m_storage[i]->key));
    GC_unregister_disappearing_link(reinterpret_cast<void**>(&m_storage[i]->data));
    if (m_hashIndex) {
        // keep storage indices stable for the hash index; the dead entry is
        // compacted away at the next rebuild
        m_storage[i]->key = nullptr;
        m_storage[i]->data = Value(Value::EmptyValue);
    } else {
        m_storage.erase(i);
    }
    return true;
}

Value WeakMapObject::get(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    size_t i = findKeyIndex(key);
    return i == SIZE_MAX ? Value() : Value(m_storage[i]->data);
}

size_t WeakMapObject::findKeyIndex(PointerValue* key)
{
    if (LIKELY(!m_hashIndex)) {
        if (UNLIKELY(m_storage.size() >= KeyedCollectionHashIndex::buildThreshold)) {
            buildOrRebuildHashIndex();
        } else {
            for (size_t i = 0; i < m_storage.size(); i++) {
                auto existingKey = m_storage[i]->key.unwrap();
                if (existingKey && existingKey == key) {
                    return i;
                }
            }
            return SIZE_MAX;
        }
    }
    size_t mask = m_hashIndex->capacity - 1;
    size_t i = keyedCollectionPointerHash(key) & mask;
    while (true) {
        uint32_t b = m_hashIndex->buckets[i];
        if (!b) {
            return SIZE_MAX;
        }
        if (m_storage[b - 1]->key.unwrap() == key) {
            return b - 1;
        }
        i = (i + 1) & mask;
    }
}

void WeakMapObject::buildOrRebuildHashIndex()
{
    // compact dead entries first (their disappearing links have already fired
    // or were unregistered on delete) so the storage stays bounded; WeakMap has
    // no iterators, so storage indices may change here
    size_t live = 0;
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]->key) {
            m_storage[live++] = m_storage[i];
        }
    }
    m_storage.resize(live);
    KeyedCollectionHashIndex* index = KeyedCollectionHashIndex::create(live);
    for (size_t i = 0; i < live; i++) {
        index->insert(keyedCollectionPointerHash(m_storage[i]->key.unwrap()), i);
    }
    m_hashIndex = index;
}

void WeakMapObject::addToHashIndex(size_t storageIndex)
{
    if (!m_hashIndex) {
        return;
    }
    if (UNLIKELY(m_hashIndex->needsRebuild())) {
        // the rebuilt index already covers storageIndex
        buildOrRebuildHashIndex();
        return;
    }
    m_hashIndex->insert(keyedCollectionPointerHash(m_storage[storageIndex]->key.unwrap()), storageIndex);
}

WeakMapObject::WeakMapObjectDataItem* WeakMapObject::insertNew(PointerValue* key, const Value& value)
{
    if (!m_hashIndex) {
        for (size_t i = 0; i < m_storage.size(); i++) {
            if (!m_storage[i]->key) {
                m_storage[i]->key = key;
                m_storage[i]->data = value;
                GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&m_storage[i]->key), key);
                GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&m_storage[i]->data), key);
                return m_storage[i];
            }
        }
    }
    auto newData = new WeakMapObjectDataItem();
    newData->key = key;
    newData->data = value;
    m_storage.pushBack(newData);
    addToHashIndex(m_storage.size() - 1);
    GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&newData->key), key);
    GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&newData->data), key);
    return newData;
}

Value WeakMapObject::getOrInsert(ExecutionState& state, PointerValue* key, const Value& value)
{
    ASSERT(key->isObject() || key->isSymbol());
    size_t i = findKeyIndex(key);
    if (i != SIZE_MAX) {
        return m_storage[i]->data;
    }
    return insertNew(key, value)->data;
}

Value WeakMapObject::getOrInsertComputed(ExecutionState& state, PointerValue* key, const Value& callback)
{
    ASSERT(key->isObject() || key->isSymbol());
    if (!callback.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    size_t i = findKeyIndex(key);
    if (i != SIZE_MAX) {
        return m_storage[i]->data;
    }

    Value argv[1] = { key };
    Value value = Object::call(state, callback, Value(), 1, argv);

    // the callback may have mutated the map
    i = findKeyIndex(key);
    if (i != SIZE_MAX) {
        m_storage[i]->data = value;
        return value;
    }

    insertNew(key, value);
    return value;
}

bool WeakMapObject::has(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    return findKeyIndex(key) != SIZE_MAX;
}


void WeakMapObject::set(ExecutionState& state, PointerValue* key, const Value& value)
{
    ASSERT(key->isObject() || key->isSymbol());
    size_t i = findKeyIndex(key);
    if (i != SIZE_MAX) {
        m_storage[i]->data = value;
        return;
    }
    insertNew(key, value);
}
} // namespace Escargot
