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
