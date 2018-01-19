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

#ifndef __EscargotSetObject__
#define __EscargotSetObject__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class SetIteratorObject;

class SetObject : public Object {
    friend class SetIteratorObject;

public:
    typedef TightVector<SmallValue, GCUtil::gc_malloc_ignore_off_page_allocator<SmallValue>> SetObjectData;
    SetObject(ExecutionState& state);

    virtual bool isSetObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Set";
    }

    void add(ExecutionState& state, const Value& key);
    void clear(ExecutionState& state);
    bool deleteOperation(ExecutionState& state, const Value& key);
    bool has(ExecutionState& state, const Value& key);
    size_t size(ExecutionState& state);

    SetIteratorObject* entries(ExecutionState& state);
    SetIteratorObject* values(ExecutionState& state);
    SetIteratorObject* keys(ExecutionState& state);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    const SetObjectData& storage()
    {
        return m_storage;
    }

protected:
    SetObjectData m_storage;
};

class SetIteratorObject : public IteratorObject {
public:
    enum Type {
        TypeKey,
        TypeValue,
        TypeKeyValue
    };
    SetIteratorObject(ExecutionState& state, SetObject* map, Type type);

    virtual bool isSetIteratorObject() const
    {
        return true;
    }
    virtual const char* internalClassProperty() override
    {
        return "Set Iterator";
    }
    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    SetObject* m_set;
    size_t m_iteratorIndex;
    Type m_type;
};
}

#endif
