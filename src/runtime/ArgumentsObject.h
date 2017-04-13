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

#ifndef __EscargotArgumentsObject__
#define __EscargotArgumentsObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ExecutionContext;
class FunctionEnvironmentRecord;
class InterpretedCodeBlock;

extern size_t g_argumentsObjectTag;

class ArgumentsObject : public Object {
public:
    ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec);
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data);
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value);
    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Arguments";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    FunctionEnvironmentRecord* m_targetRecord;
    InterpretedCodeBlock* m_codeBlock;
    TightVector<std::pair<SmallValue, AtomicString>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<SmallValue, AtomicString>>> m_argumentPropertyInfo;
};
}

#endif
