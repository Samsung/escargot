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

#ifndef __EscargotWeakMapObject__
#define __EscargotWeakMapObject__

#include "runtime/Object.h"

struct GC_ms_entry;
namespace Escargot {

struct KeyedCollectionHashIndex;

class WeakMapObject : public DerivedObject {
public:
    struct WeakMapObjectDataItem : public gc {
        Optional<PointerValue*> key; // should be Object or Symbol
        EncodedValue data;

        void* operator new(size_t size);
    };

    typedef Vector<WeakMapObjectDataItem*, GCUtil::gc_malloc_allocator<WeakMapObjectDataItem*>> WeakMapObjectData;

    explicit WeakMapObject(ExecutionState& state);
    explicit WeakMapObject(ExecutionState& state, Object* proto);

    virtual bool isWeakMapObject() const override
    {
        return true;
    }

    bool deleteOperation(ExecutionState& state, PointerValue* key);
    Value get(ExecutionState& state, PointerValue* key);
    Value getOrInsert(ExecutionState& state, PointerValue* key, const Value& value);
    Value getOrInsertComputed(ExecutionState& state, PointerValue* key, const Value& callback);
    bool has(ExecutionState& state, PointerValue* key);
    void set(ExecutionState& state, PointerValue* key, const Value& value);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    // returns index into m_storage or SIZE_MAX; builds the hash index once the
    // storage outgrows KeyedCollectionHashIndex::buildThreshold
    size_t findKeyIndex(PointerValue* key);
    WeakMapObjectDataItem* insertNew(PointerValue* key, const Value& value);
    void addToHashIndex(size_t storageIndex);
    void buildOrRebuildHashIndex();
    static GC_ms_entry* markEphemerons(GC_ms_entry* markStackTop, GC_ms_entry* markStackLimit, void* data);
    static void removeEphemeronListener(PointerValue* self, void* data);

    WeakMapObjectData m_storage;
    Optional<KeyedCollectionHashIndex*> m_hashIndex;
};
} // namespace Escargot

#endif
