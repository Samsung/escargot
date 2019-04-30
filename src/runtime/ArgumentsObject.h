/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true);
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value);
    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Arguments";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    FunctionEnvironmentRecord* m_targetRecord;
    InterpretedCodeBlock* m_codeBlock;
    TightVector<std::pair<SmallValue, AtomicString>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<SmallValue, AtomicString>>> m_argumentPropertyInfo;
};
}

#endif
