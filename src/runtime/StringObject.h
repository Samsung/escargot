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

#ifndef __EscargotStringObject__
#define __EscargotStringObject__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class StringObject : public DerivedObject {
public:
    StringObject(ExecutionState& state, String* value = String::emptyString());
    StringObject(ExecutionState& state, Object* proto, String* value = String::emptyString());

    String* primitiveValue()
    {
        return m_primitiveValue;
    }

    virtual bool isStringObject() const override
    {
        return true;
    }

    virtual bool hasOwnEnumeration() const override
    {
        return true;
    }

    void setPrimitiveValue(ExecutionState& state, String* data)
    {
        m_primitiveValue = data;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver) override;
    virtual Value getIndexedPropertyValue(ExecutionState& state, const Value& property, const Value& receiver) override;
    virtual ObjectHasPropertyResult hasIndexedProperty(ExecutionState& state, const Value& propertyName) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;


private:
    String* m_primitiveValue;
};

class StringIteratorObject : public IteratorObject {
public:
    StringIteratorObject(ExecutionState& state, String* string);

    virtual bool isStringIteratorObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    String* m_string;
    size_t m_iteratorNextIndex;
};
} // namespace Escargot

#endif
