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

#ifndef __EscargotMapObject__
#define __EscargotMapObject__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class MapIteratorObject;

class MapObject : public DerivedObject {
    friend class MapIteratorObject;

public:
    typedef TightVector<std::pair<EncodedValue, EncodedValue>, GCUtil::gc_malloc_allocator<std::pair<EncodedValue, EncodedValue>>> MapObjectData;

    explicit MapObject(ExecutionState& state);
    explicit MapObject(ExecutionState& state, Object* proto);

    virtual bool isMapObject() const override
    {
        return true;
    }

    void clear(ExecutionState& state);
    bool deleteOperation(ExecutionState& state, const Value& key);
    Value get(ExecutionState& state, const Value& key);
    Value getOrInsert(ExecutionState& state, const Value& key, const Value& value);
    bool has(ExecutionState& state, const Value& key);
    void set(ExecutionState& state, const Value& key, const Value& value);
    size_t size(ExecutionState& state);

    IteratorObject* values(ExecutionState& state);
    IteratorObject* keys(ExecutionState& state);
    IteratorObject* entries(ExecutionState& state);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    const MapObjectData& storage()
    {
        return m_storage;
    }

private:
    MapObjectData m_storage;
};

class MapIteratorObject : public IteratorObject {
public:
    enum Type {
        TypeKey,
        TypeValue,
        TypeKeyValue
    };

    MapIteratorObject(ExecutionState& state, MapObject* map, Type type);

    virtual bool isMapIteratorObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    MapObject* m_map;
    size_t m_iteratorIndex;
    Type m_type;
};
} // namespace Escargot

#endif
