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
    MapObject(ExecutionState& state);

    virtual bool isMapObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
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
    virtual IteratorObject* iterator(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    const MapObjectData& storage()
    {
        return m_storage;
    }

protected:
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

    virtual bool isMapIteratorObject() const
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

protected:
    MapObject* m_map;
    size_t m_iteratorIndex;
    Type m_type;
};
}

#endif
