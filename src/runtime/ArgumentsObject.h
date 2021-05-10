/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotArgumentsObject__
#define __EscargotArgumentsObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class FunctionEnvironmentRecord;
class InterpretedCodeBlock;
class ScriptFunctionObject;

class ArgumentsObject : public Object {
public:
    ArgumentsObject(ExecutionState& state, Object* proto, ScriptFunctionObject* sourceFunctionObject, size_t argc, Value* argv, FunctionEnvironmentRecord* environmentRecordWillArgumentsObjectBeLocatedIn, bool isMapped);
    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver) override;
    virtual ObjectHasPropertyResult hasIndexedProperty(ExecutionState& state, const Value& propertyName) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver) override;
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver) override;

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual bool isArgumentsObject() const override
    {
        return true;
    }

    ScriptFunctionObject* sourceFunctionObject() const
    {
        return m_sourceFunctionObject;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    FunctionEnvironmentRecord* m_targetRecord;
    ScriptFunctionObject* m_sourceFunctionObject;
    TightVectorWithNoSize<std::pair<EncodedValue, AtomicString>, GCUtil::gc_malloc_allocator<std::pair<EncodedValue, AtomicString>>> m_parameterMap;

    struct ModifiedArguments : public gc {
        StorePositiveNumberAsOddNumber m_argc;
        bool* m_modified;

        ModifiedArguments(size_t s)
            : m_argc(s)
            , m_modified(nullptr)
        {
            m_modified = new (PointerFreeGC) bool[s];
            memset(m_modified, 0, s * sizeof(bool));
        }
    };
    union {
        size_t m_argc;
        ModifiedArguments* m_modifiedArguments;
    };

    size_t argc()
    {
        if ((m_argc & 1) == 0) {
            return m_modifiedArguments->m_argc;
        }
        return m_argc >> 1;
    }

    ModifiedArguments* modifiedArguments()
    {
        if ((m_argc & 1) == 0) {
            return m_modifiedArguments;
        }
        return nullptr;
    }

    Value getIndexedPropertyValueQuickly(ExecutionState& state, uint64_t index);
    void setIndexedPropertyValueQuickly(ExecutionState& state, uint64_t index, const Value& value);

    bool isModifiedArgument(uint64_t index);
    void setModifiedArgument(uint64_t index);
    bool isMatchedArgument(uint64_t index);
};
} // namespace Escargot

#endif
