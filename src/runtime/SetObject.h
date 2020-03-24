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

#ifndef __EscargotSetObject__
#define __EscargotSetObject__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class SetIteratorObject;

class SetObject : public Object {
    friend class SetIteratorObject;

public:
    typedef TightVector<SmallValue, GCUtil::gc_malloc_allocator<SmallValue>> SetObjectData;

    explicit SetObject(ExecutionState& state);
    explicit SetObject(ExecutionState& state, Object* proto);

    virtual bool isSetObject() const override
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty(ExecutionState& state) override
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

private:
    SetObjectData m_storage;
};

class SetIteratorObject : public IteratorObject {
public:
    enum Type {
        TypeKey,
        TypeValue,
        TypeKeyValue
    };

    SetIteratorObject(ExecutionState& state, SetObject* set, Type type);
    SetIteratorObject(ExecutionState& state, Object* proto, SetObject* set, Type type);

    virtual bool isSetIteratorObject() const override
    {
        return true;
    }
    virtual const char* internalClassProperty(ExecutionState& state) override
    {
        return "Set Iterator";
    }
    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    SetObject* m_set;
    size_t m_iteratorIndex;
    Type m_type;
};
}

#endif
