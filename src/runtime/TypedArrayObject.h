/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTypedArrayObject__
#define __EscargotTypedArrayObject__

#include "runtime/ArrayBufferObject.h"
#include "util/Float16.h"

namespace Escargot {

#define FOR_EACH_TYPEDARRAY_TYPES(F)           \
    F(Int8, int8, 1, int8_t);                  \
    F(Int16, int16, 2, int16_t);               \
    F(Int32, int32, 4, int32_t);               \
    F(Uint8, uint8, 1, uint8_t);               \
    F(Uint8Clamped, uint8Clamped, 1, uint8_t); \
    F(Uint16, uint16, 2, uint16_t);            \
    F(Uint32, uint32, 4, uint32_t);            \
    F(Float16, float16, 2, Float16);           \
    F(Float32, float32, 4, double);            \
    F(Float64, float64, 8, double);            \
    F(BigInt64, bigInt64, 8, int64_t);         \
    F(BigUint64, bigUint64, 8, uint64_t);

class TypedArrayPrototypeObject : public DerivedObject {
public:
    TypedArrayPrototypeObject(ExecutionState& state)
        : DerivedObject(state)
    {
    }

    virtual bool isTypedArrayPrototypeObject() const override
    {
        return true;
    }
};

class TypedArrayObject : public ArrayBufferView {
public:
    virtual bool isTypedArrayObject() const override
    {
        return true;
    }

    virtual TypedArrayType typedArrayType()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual String* typedArrayName(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool hasOwnEnumeration() const override
    {
        return true;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) override;
    virtual void sort(ExecutionState& state, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp) override;
    virtual void toSorted(ExecutionState& state, Object* target, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp) override;

    static ArrayBuffer* validateTypedArray(ExecutionState& state, const Value& O, bool checkDetachedError = true);

protected:
    explicit TypedArrayObject(ExecutionState& state, Object* proto)
        : ArrayBufferView(state, proto)
    {
    }

    // https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementget
    inline ObjectGetResult integerIndexedElementGet(ExecutionState& state, double index);
    // https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementset
    inline bool integerIndexedElementSet(ExecutionState& state, double index, const Value& value);
};

#define DECLARE_TYPEDARRAY(TYPE, type, siz, nativeType)                                                                                            \
    class TYPE##ArrayObject : public TypedArrayObject {                                                                                            \
    public:                                                                                                                                        \
        explicit TYPE##ArrayObject(ExecutionState& state)                                                                                          \
            : TYPE##ArrayObject(state, state.context()->globalObject()->type##ArrayPrototype())                                                    \
        {                                                                                                                                          \
        }                                                                                                                                          \
        explicit TYPE##ArrayObject(ExecutionState& state, Object* proto)                                                                           \
            : TypedArrayObject(state, proto)                                                                                                       \
        {                                                                                                                                          \
        }                                                                                                                                          \
        static TypedArrayObject* allocateTypedArray(ExecutionState& state, Object* newTarget, size_t length = std::numeric_limits<size_t>::max()); \
        virtual TypedArrayType typedArrayType() override                                                                                           \
        {                                                                                                                                          \
            return TypedArrayType::TYPE;                                                                                                           \
        }                                                                                                                                          \
        virtual String* typedArrayName(ExecutionState& state) override                                                                             \
        {                                                                                                                                          \
            return state.context()->staticStrings().TYPE##Array.string();                                                                          \
        }                                                                                                                                          \
        virtual size_t elementSize() override                                                                                                      \
        {                                                                                                                                          \
            return siz;                                                                                                                            \
        }                                                                                                                                          \
        virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver) override;                  \
        virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver) override;         \
        virtual Value getIndexedPropertyValue(ExecutionState& state, const Value& property, const Value& receiver) override;                       \
                                                                                                                                                   \
    private:                                                                                                                                       \
        template <const bool isLittleEndian = true>                                                                                                \
        inline Value getDirectValueFromBuffer(ExecutionState& state, size_t byteindex);                                                            \
        template <const bool isLittleEndian = true>                                                                                                \
        inline void setDirectValueInBuffer(ExecutionState& state, size_t byteindex, const Value& val);                                             \
    };

FOR_EACH_TYPEDARRAY_TYPES(DECLARE_TYPEDARRAY)
#undef DECLARE_TYPEDARRAY
} // namespace Escargot

#endif
