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
#include "runtime/SetObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/Context.h"
#include "runtime/KeyedCollectionHashIndex.h"

namespace Escargot {

SetObject::SetObject(ExecutionState& state)
    : SetObject(state, state.context()->globalObject()->setPrototypeObject())
{
}

SetObject::SetObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

SetObject::SetObject(ExecutionState& state, Object* proto, SetObjectData&& data)
    : SetObject(state, proto)
{
    m_storage = data;
}

void* SetObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObject, m_storage));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObject, m_hashIndex));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void SetObject::clear(ExecutionState& state)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        m_storage[i] = Value(Value::EmptyValue);
    }
    m_hashIndex = nullptr;
}

size_t SetObject::findKeyIndex(ExecutionState& state, const Value& key)
{
    if (LIKELY(!m_hashIndex)) {
        if (UNLIKELY(m_storage.size() >= KeyedCollectionHashIndex::buildThreshold)) {
            buildOrRebuildHashIndex();
        } else {
            for (size_t i = 0; i < m_storage.size(); i++) {
                Value existingKey = m_storage[i];
                if (existingKey.isEmpty()) {
                    continue;
                }
                if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
                    return i;
                }
            }
            return SIZE_MAX;
        }
    }
    size_t mask = m_hashIndex->capacity - 1;
    size_t i = keyedCollectionHash(key) & mask;
    while (true) {
        uint32_t b = m_hashIndex->buckets[i];
        if (!b) {
            return SIZE_MAX;
        }
        Value existingKey = m_storage[b - 1];
        if (!existingKey.isEmpty() && existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            return b - 1;
        }
        i = (i + 1) & mask;
    }
}

void SetObject::buildOrRebuildHashIndex()
{
    size_t live = 0;
    for (size_t i = 0; i < m_storage.size(); i++) {
        live += !Value(m_storage[i]).isEmpty();
    }
    KeyedCollectionHashIndex* index = KeyedCollectionHashIndex::create(live);
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value key = m_storage[i];
        if (!key.isEmpty()) {
            index->insert(keyedCollectionHash(key), i);
        }
    }
    m_hashIndex = index;
}

void SetObject::addToHashIndex(size_t storageIndex)
{
    if (!m_hashIndex) {
        return;
    }
    if (UNLIKELY(m_hashIndex->needsRebuild())) {
        // the rebuilt index already covers storageIndex
        buildOrRebuildHashIndex();
        return;
    }
    m_hashIndex->insert(keyedCollectionHash(m_storage[storageIndex]), storageIndex);
}

bool SetObject::deleteOperation(ExecutionState& state, const Value& key)
{
    size_t i = findKeyIndex(state, key);
    if (i == SIZE_MAX) {
        return false;
    }
    // the hash index keeps a stale bucket for i; it is skipped by comparison
    // and swept at the next rebuild
    m_storage[i] = Value(Value::EmptyValue);
    return true;
}

void SetObject::add(ExecutionState& state, const Value& key)
{
    if (findKeyIndex(state, key) != SIZE_MAX) {
        return;
    }

    // If key is -0, let key be +0.
    if (key.isNumber() && key.asNumber() == 0 && std::signbit(key.asNumber())) {
        m_storage.pushBack(Value(0));
    } else {
        m_storage.pushBack(key);
    }
    addToHashIndex(m_storage.size() - 1);
}

bool SetObject::has(ExecutionState& state, const Value& key)
{
    return findKeyIndex(state, key) != SIZE_MAX;
}

size_t SetObject::size(ExecutionState& state)
{
    size_t siz = 0;
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i];
        if (existingKey.isEmpty()) {
            continue;
        }
        siz++;
    }
    return siz;
}

IteratorObject* SetObject::values(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeValue);
}

IteratorObject* SetObject::keys(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeValue);
}

IteratorObject* SetObject::entries(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeKeyValue);
}

SetIteratorObject::SetIteratorObject(ExecutionState& state, SetObject* set, Type type)
    : IteratorObject(state, state.context()->globalObject()->setIteratorPrototype())
    , m_set(set)
    , m_iteratorIndex(0)
    , m_type(type)
{
}

void* SetIteratorObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetIteratorObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetIteratorObject, m_set));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> SetIteratorObject::advance(ExecutionState& state)
{
    // Let s be the value of the [[IteratedSet]] internal slot of O.
    // Let index be the value of the [[SetNextIndex]] internal slot of O.
    // Let itemKind be the value of the [[SetIterationKind]] internal slot of O.
    SetObject* s = m_set;
    size_t index = m_iteratorIndex;
    Type itemKind = m_type;

    // If s is undefined, return CreateIterResultObject(undefined, true).
    if (s == nullptr) {
        return std::make_pair(Value(), true);
    }

    // Let entries be the List that is the value of the [[SetData]] internal slot of s.
    // Repeat while index is less than the total number of elements of entries. The number of elements must be redetermined each time this method is evaluated.
    while (index < s->m_storage.size()) {
        // Let e be entries[index].
        Value e = s->m_storage[index];
        // Set index to index+1.
        index++;
        // Set the [[SetNextIndex]] internal slot of O to index.
        m_iteratorIndex = index;

        if (e.isEmpty()) {
            continue;
        }

        Value result;
        if (itemKind == Type::TypeKeyValue) {
            ArrayObject* arr = new ArrayObject(state, 2, false);
            arr->defineOwnIndexedPropertyWithoutExpanding(state, 0, e);
            arr->defineOwnIndexedPropertyWithoutExpanding(state, 1, e);
            result = arr;
        } else {
            result = e;
        }

        return std::make_pair(result, false);
    }

    // Set the [[IteratedSet]] internal slot of O to undefined.
    m_set = nullptr;
    // Return CreateIterResultObject(undefined, true).
    return std::make_pair(Value(), true);
}
} // namespace Escargot
