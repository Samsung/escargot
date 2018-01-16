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

#ifndef __EscargotWeakMapObject__
#define __EscargotWeakMapObject__

#include "runtime/Object.h"

namespace Escargot {

class WeakMapObject : public Object {
public:
    struct WeakMapObjectDataItem : public gc {
        Object* key;
        SmallValue data;

        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;
    };
    typedef TightVector<WeakMapObjectDataItem*, GCUtil::gc_malloc_ignore_off_page_allocator<WeakMapObjectDataItem*>> WeakMapObjectData;
    WeakMapObject(ExecutionState& state);

    virtual bool isWeakMapObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "WeakMap";
    }

    bool deleteOperation(ExecutionState& state, Object* key);
    Value get(ExecutionState& state, Object* key);
    bool has(ExecutionState& state, Object* key);
    void set(ExecutionState& state, Object* key, const Value& value);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    WeakMapObjectData m_storage;
};
}

#endif
