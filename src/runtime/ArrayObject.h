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

#ifndef __EscargotArrayObject__
#define __EscargotArrayObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

#define ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE 65536 * 16
#define ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP 1024

extern size_t g_arrayObjectTag;

class ArrayIteratorObject;

class ArrayObject : public Object {
    friend class VMInstance;
    friend class Context;
    friend class Object;
    friend class ByteCodeInterpreter;
    friend Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayObject(void* ptr, GC_mark_custom_result* arr);

public:
    explicit ArrayObject(ExecutionState& state);
    ArrayObject(ExecutionState& state, double size); // http://www.ecma-international.org/ecma-262/7.0/index.html#sec-arraycreate
    ArrayObject(ExecutionState& state, const uint64_t& size);
    ArrayObject(ExecutionState& state, const Value* src, const uint64_t& size);

    static ArrayObject* createSpreadArray(ExecutionState& state);

    virtual bool isArrayObject() const override
    {
        return true;
    }

    virtual bool isArray(ExecutionState& state) const override
    {
        return true;
    }

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override;
    void defineOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
    {
        if (!ArrayObject::defineOwnProperty(state, P, desc)) {
            throwCannotDefineError(state, P.toObjectStructurePropertyName(state));
        }
    }
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override;
    virtual uint64_t length(ExecutionState& state) override
    {
        return getArrayLength(state);
    }
    virtual void sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp) override;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property) override;
    virtual ObjectHasPropertyResult hasIndexedProperty(ExecutionState& state, const Value& propertyName) override;
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value) override;
    virtual bool preventExtensions(ExecutionState&) override;

    // Use custom allocator for Array object (for Badtime)
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static void iterateArrays(ExecutionState& state, HeapObjectIteratorCallback callback);
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& newData);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Array";
    }

private:
    ALWAYS_INLINE bool isFastModeArray()
    {
        auto rd = rareData();
        if (LIKELY(rd == nullptr)) {
            return true;
        }
        return rd->m_isFastModeArrayObject;
    }

    bool isLengthPropertyWritable()
    {
        return rareData() ? rareData()->m_isArrayObjectLengthWritable : true;
    }

    void setFastModeArrayValueWithoutExpanding(ExecutionState& state, size_t idx, const Value& v)
    {
        ASSERT(isFastModeArray());
        ASSERT(idx < getArrayLength(state));
        m_fastModeData[idx] = v;
    }

    ALWAYS_INLINE uint32_t getArrayLength(ExecutionState&)
    {
        return m_arrayLength;
    }

    bool setArrayLength(ExecutionState& state, const Value& newLength);
    bool setArrayLength(ExecutionState& state, const uint32_t newLength, bool useFitStorage = false);
    void convertIntoNonFastMode(ExecutionState& state);

    ObjectGetResult getVirtualValue(ExecutionState& state, const ObjectPropertyName& P);

    uint32_t m_arrayLength;
    SmallValue* m_fastModeData;
};

class ArrayObjectPrototype : public ArrayObject {
public:
    ArrayObjectPrototype(ExecutionState& state)
        : ArrayObject(state)
    {
    }

    virtual bool isArrayPrototypeObject() const override
    {
        return true;
    }
};

class ArrayIteratorObject : public IteratorObject {
public:
    enum Type {
        TypeKey,
        TypeValue,
        TypeKeyValue
    };

    ArrayIteratorObject(ExecutionState& state, Object* array, Type type);

    virtual bool isArrayIteratorObject() const override
    {
        return true;
    }

    virtual const char* internalClassProperty() override
    {
        return "Array Iterator";
    }
    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    Object* m_array;
    size_t m_iteratorNextIndex;
    Type m_type;
};
}

#endif
