/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotEnumerateObject__
#define __EscargotEnumerateObject__

#include "runtime/Object.h"

namespace Escargot {

class EnumerateObject : public PointerValue {
public:
    virtual bool isEnumerateObject() const
    {
        return true;
    }

    bool checkLastEnumerateKey(ExecutionState& state);

    virtual void fillRestElement(ExecutionState& state, Object* result)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    size_t m_index;
    SmallValueVector m_keys;

protected:
    EnumerateObject(Object* obj)
        : m_index(0)
        , m_object(obj)
        , m_arrayLength(0)
    {
        ASSERT(!!m_object);
    }

    void update(ExecutionState& state);

    virtual void executeEnumeration(ExecutionState& state, SmallValueVector& keys) = 0;
    virtual bool checkIfModified(ExecutionState& state) = 0;

    Object* m_object;
    uint64_t m_arrayLength;
};

// enumerate object for destruction operation e.g. var obj = { a, ...b };
// include symbol, exclude prototype chain and check modification during enumetation
class EnumerateObjectWithDestruction : public EnumerateObject {
public:
    EnumerateObjectWithDestruction(ExecutionState& state, Object* obj)
        : EnumerateObject(obj)
        , m_hiddenClass(nullptr)
    {
        executeEnumeration(state, m_keys);
    }

    virtual void fillRestElement(ExecutionState& state, Object* result) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    virtual void executeEnumeration(ExecutionState& state, SmallValueVector& keys) override;
    virtual bool checkIfModified(ExecutionState& state) override;

    ObjectStructure* m_hiddenClass;
};

// enumerate object for iteration operation (for-in)
// exclude symbol, include prototype chain and check modification during enumetation
class EnumerateObjectWithIteration : public EnumerateObject {
public:
    EnumerateObjectWithIteration(ExecutionState& state, Object* obj)
        : EnumerateObject(obj)
    {
        executeEnumeration(state, m_keys);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    virtual void executeEnumeration(ExecutionState& state, SmallValueVector& keys) override;
    virtual bool checkIfModified(ExecutionState& state) override;

    Vector<ObjectStructure*, GCUtil::gc_malloc_allocator<ObjectStructure*>> m_hiddenClassChain;
};
}

#endif
