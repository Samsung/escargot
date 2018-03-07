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

#ifndef __EscargotWeakSetObject__
#define __EscargotWeakSetObject__

#include "runtime/Object.h"

namespace Escargot {

class WeakSetObject : public Object {
public:
    struct WeakSetObjectDataItem : public gc {
        Object* key;
        void* operator new(size_t size)
        {
            return GC_MALLOC_ATOMIC(size);
        }
        void* operator new[](size_t size) = delete;
    };

    typedef TightVector<WeakSetObjectDataItem*, GCUtil::gc_malloc_ignore_off_page_allocator<WeakSetObjectDataItem*>> WeakSetObjectData;
    WeakSetObject(ExecutionState& state);

    virtual bool isWeakSetObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "WeakSet";
    }

    void add(ExecutionState& state, Object* key);
    bool deleteOperation(ExecutionState& state, Object* key);
    bool has(ExecutionState& state, Object* key);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    WeakSetObjectData m_storage;
};
}

#endif
