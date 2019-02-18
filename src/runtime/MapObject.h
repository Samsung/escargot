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

#ifndef __EscargotMapObject__
#define __EscargotMapObject__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class MapIteratorObject;

class MapObject : public Object {
    friend class MapIteratorObject;

public:
    typedef TightVector<std::pair<SmallValue, SmallValue>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<SmallValue, SmallValue>>> MapObjectData;
    explicit MapObject(ExecutionState& state);

    virtual bool isMapObject() const override
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Map";
    }

    void clear(ExecutionState& state);
    bool deleteOperation(ExecutionState& state, const Value& key);
    Value get(ExecutionState& state, const Value& key);
    bool has(ExecutionState& state, const Value& key);
    void set(ExecutionState& state, const Value& key, const Value& value);
    size_t size(ExecutionState& state);

    MapIteratorObject* values(ExecutionState& state);
    MapIteratorObject* keys(ExecutionState& state);
    MapIteratorObject* entries(ExecutionState& state);

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
    virtual const char* internalClassProperty() override
    {
        return "Map Iterator";
    }
    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    MapObject* m_map;
    size_t m_iteratorIndex;
    Type m_type;
};
}

#endif
