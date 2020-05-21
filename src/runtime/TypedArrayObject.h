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

namespace Escargot {

#define FOR_EACH_TYPEDARRAY(F)        \
    F(Int8, int8, 1);                 \
    F(Int16, int16, 2);               \
    F(Int32, int32, 4);               \
    F(Uint8, uint8, 1);               \
    F(Uint8Clamped, uint8Clamped, 1); \
    F(Uint16, uint16, 2);             \
    F(Uint32, uint32, 4);             \
    F(Float32, float32, 4);           \
    F(Float64, float64, 8);

class ArrayBufferView : public Object {
public:
    explicit ArrayBufferView(ExecutionState& state, Object* proto)
        : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , m_buffer(nullptr)
        , m_byteLength(0)
        , m_byteOffset(0)
        , m_arrayLength(0)
    {
    }

    virtual TypedArrayType typedArrayType()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual String* typedArrayName(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual size_t elementSize()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE ArrayBufferObject* buffer() { return m_buffer; }
    ALWAYS_INLINE size_t byteLength() { return m_byteLength; }
    ALWAYS_INLINE size_t byteOffset() { return m_byteOffset; }
    ALWAYS_INLINE size_t arrayLength() { return m_arrayLength; }
    ALWAYS_INLINE uint8_t* rawBuffer()
    {
        return m_buffer ? (uint8_t*)(m_buffer->data() + m_byteOffset) : nullptr;
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, size_t byteOffset, size_t byteLength, size_t arrayLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
        m_arrayLength = arrayLength;
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, size_t byteOffset, size_t byteLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
    }

    virtual bool isArrayBufferView() const override
    {
        return true;
    }

    void* operator new(size_t size)
    {
        static bool typeInited = false;
        static GC_descr descr;
        if (!typeInited) {
            GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferView)] = { 0 };
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_structure));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_prototype));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_values));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_buffer));
            descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferView));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

private:
    ArrayBufferObject* m_buffer;
    size_t m_byteLength;
    size_t m_byteOffset;
    size_t m_arrayLength;
};

template <typename T>
class TypedArrayObject : public ArrayBufferView {
public:
    virtual bool isTypedArrayObject() const override
    {
        return true;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;
    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override;
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property) override;
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value) override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) override;
    virtual void sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp) override;

protected:
    explicit TypedArrayObject(ExecutionState& state, Object* proto)
        : ArrayBufferView(state, proto)
    {
    }

    inline Value getDirectValueFromBuffer(ExecutionState& state, size_t byteindex, bool isLittleEndian = 1);
    inline void setDirectValueInBuffer(ExecutionState& state, size_t byteindex, double val, bool isLittleEndian = 1);

    // https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementget
    ObjectGetResult integerIndexedElementGet(ExecutionState& state, double index);
    // https://www.ecma-international.org/ecma-262/10.0/#sec-integerindexedelementset
    bool integerIndexedElementSet(ExecutionState& state, double index, const Value& value);
};

#define DECLARE_TYPEDARRAY(TYPE, type, siz)                                                                                                         \
    typedef TypedArrayObject<TYPE##Adaptor> TYPE##ArrayObjectWrapper;                                                                               \
    class TYPE##ArrayObject : public TYPE##ArrayObjectWrapper {                                                                                     \
    public:                                                                                                                                         \
        explicit TYPE##ArrayObject(ExecutionState& state)                                                                                           \
            : TYPE##ArrayObject(state, state.context()->globalObject()->type##ArrayPrototype())                                                     \
        {                                                                                                                                           \
        }                                                                                                                                           \
        explicit TYPE##ArrayObject(ExecutionState& state, Object* proto)                                                                            \
            : TypedArrayObject(state, proto)                                                                                                        \
        {                                                                                                                                           \
        }                                                                                                                                           \
        virtual TypedArrayType typedArrayType() override                                                                                            \
        {                                                                                                                                           \
            return TypedArrayType::TYPE;                                                                                                            \
        }                                                                                                                                           \
        virtual String* typedArrayName(ExecutionState& state) override                                                                              \
        {                                                                                                                                           \
            return state.context()->staticStrings().TYPE##Array.string();                                                                           \
        }                                                                                                                                           \
        virtual size_t elementSize() override                                                                                                       \
        {                                                                                                                                           \
            return siz;                                                                                                                             \
        }                                                                                                                                           \
        static TYPE##ArrayObject* allocateTypedArray(ExecutionState& state, Object* newTarget, size_t length = std::numeric_limits<size_t>::max())  \
        {                                                                                                                                           \
            ASSERT(!!newTarget);                                                                                                                    \
            Object* proto = Object::getPrototypeFromConstructor(state, newTarget, [](ExecutionState& state, Context* constructorRealm) -> Object* { \
                return constructorRealm->globalObject()->type##ArrayPrototype();                                                                    \
            });                                                                                                                                     \
            TYPE##ArrayObject* obj = new TYPE##ArrayObject(state, proto);                                                                           \
            if (length == std::numeric_limits<size_t>::max()) {                                                                                     \
                obj->setBuffer(nullptr, 0, 0, 0);                                                                                                   \
            } else {                                                                                                                                \
                auto buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), length * siz);          \
                obj->setBuffer(buffer, 0, length* siz, length);                                                                                     \
            }                                                                                                                                       \
            return obj;                                                                                                                             \
        }                                                                                                                                           \
    };

FOR_EACH_TYPEDARRAY(DECLARE_TYPEDARRAY)
#undef DECLARE_TYPEDARRAY

class TypedArrayPrototypeObject : public Object {
public:
    TypedArrayPrototypeObject(ExecutionState& state)
        : Object(state)
    {
    }

    virtual bool isTypedArrayPrototypeObject() const
    {
        return true;
    }
};
}

#endif
