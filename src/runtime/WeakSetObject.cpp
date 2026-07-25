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

#include "runtime/KeyedCollectionHashIndex.h"

namespace Escargot {

WeakSetObject::WeakSetObject(ExecutionState& state)
    : WeakSetObject(state, state.context()->globalObject()->weakSetPrototype())
{
}

WeakSetObject::WeakSetObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

void* WeakSetObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(WeakSetObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(WeakSetObject, m_storage));
        GC_set_bit(desc, GC_WORD_OFFSET(WeakSetObject, m_hashIndex));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(WeakSetObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

size_t WeakSetObject::findKeyIndex(PointerValue* key)
{
    if (LIKELY(!m_hashIndex)) {
        if (UNLIKELY(m_storage.size() >= KeyedCollectionHashIndex::buildThreshold)) {
            buildOrRebuildHashIndex();
        } else {
            for (size_t i = 0; i < m_storage.size(); i++) {
                if (m_storage[i]->key.unwrap() == key) {
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

void WeakSetObject::buildOrRebuildHashIndex()
{
    // compact dead entries first (their disappearing links have already fired
    // or were unregistered on delete) so the storage stays bounded; WeakSet has
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

void WeakSetObject::addToHashIndex(size_t storageIndex)
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

bool WeakSetObject::deleteOperation(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    size_t i = findKeyIndex(key);
    if (i == SIZE_MAX) {
        return false;
    }
    GC_unregister_disappearing_link(reinterpret_cast<void**>(&m_storage[i]->key));
    if (m_hashIndex) {
        // keep storage indices stable for the hash index; the dead entry is
        // compacted away at the next rebuild
        m_storage[i]->key = nullptr;
    } else {
        m_storage.erase(i);
    }
    return true;
}

void WeakSetObject::add(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    if (findKeyIndex(key) != SIZE_MAX) {
        return;
    }

    if (!m_hashIndex) {
        for (size_t i = 0; i < m_storage.size(); i++) {
            if (!m_storage[i]->key) {
                m_storage[i]->key = key;
                GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&m_storage[i]->key), key);
                return;
            }
        }
    }

    auto newData = new WeakSetObjectDataItem(key);
    m_storage.pushBack(newData);
    addToHashIndex(m_storage.size() - 1);

    GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&newData->key), key);
}

bool WeakSetObject::has(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    return findKeyIndex(key) != SIZE_MAX;
}

} // namespace Escargot
