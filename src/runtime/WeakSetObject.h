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

#ifndef __EscargotWeakSetObject__
#define __EscargotWeakSetObject__

#include "runtime/Object.h"

namespace Escargot {

class WeakSetObject : public DerivedObject {
public:
    typedef Vector<Object*, GCUtil::gc_malloc_atomic_allocator<Object*>> WeakSetObjectData;

    explicit WeakSetObject(ExecutionState& state);
    explicit WeakSetObject(ExecutionState& state, Object* proto);

    virtual bool isWeakSetObject() const
    {
        return true;
    }

    void add(ExecutionState& state, Object* key);
    bool deleteOperation(ExecutionState& state, Object* key);
    bool has(ExecutionState& state, Object* key);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    static void finalizer(Object* self, void* data);

    WeakSetObjectData m_storage;
};
} // namespace Escargot
#endif
