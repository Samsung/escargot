/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotStringObject__
#define __EscargotStringObject__

#include "runtime/Object.h"

namespace Escargot {

class StringObject : public Object {
public:
    StringObject(ExecutionState& state, String* value = String::emptyString);

    String* primitiveValue()
    {
        return m_primitiveValue;
    }

    virtual bool isStringObject() const
    {
        return true;
    }

    void setPrimitiveValue(ExecutionState& state, String* data)
    {
        m_primitiveValue = data;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual uint64_t length(ExecutionState& state)
    {
        return m_primitiveValue->length();
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "String";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;


protected:
    String* m_primitiveValue;
};
}

#endif
