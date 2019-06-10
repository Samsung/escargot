/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTypedArrayObject__
#define __EscargotTypedArrayObject__

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/ArrayObject.h"
#include "util/Util.h"

namespace Escargot {

enum TypedArrayType : unsigned {
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Uint8Clamped,
    Float32,
    Float64
};

class ArrayBufferView : public Object {
public:
    explicit ArrayBufferView(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    {
        m_rawBuffer = nullptr;
        m_buffer = nullptr;
        m_arraylength = m_byteoffset = m_bytelength = 0;
    }

    virtual TypedArrayType typedArrayType() = 0;
    ALWAYS_INLINE ArrayBufferObject* buffer() { return m_buffer; }
    ALWAYS_INLINE uint8_t* rawBuffer() { return m_rawBuffer; }
    ALWAYS_INLINE unsigned bytelength() { return m_bytelength; }
    ALWAYS_INLINE unsigned byteoffset() { return m_byteoffset; }
    ALWAYS_INLINE unsigned arraylength() { return m_arraylength; }
    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, unsigned byteOffset, unsigned byteLength, unsigned arrayLength)
    {
        m_buffer = bo;
        m_byteoffset = byteOffset;
        m_bytelength = byteLength;
        m_arraylength = arrayLength;
        m_rawBuffer = (uint8_t*)(bo->data() + m_byteoffset);
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, unsigned byteOffset, unsigned byteLength)
    {
        m_buffer = bo;
        m_byteoffset = byteOffset;
        m_bytelength = byteLength;
        m_rawBuffer = (uint8_t*)(bo->data() + m_byteoffset);
    }

    virtual bool isArrayBufferView() const
    {
        return true;
    }

    static int getElementSize(TypedArrayType type)
    {
        switch (type) {
        case TypedArrayType::Int8:
        case TypedArrayType::Uint8:
        case TypedArrayType::Uint8Clamped:
            return 1;
        case TypedArrayType::Int16:
        case TypedArrayType::Uint16:
            return 2;
        case TypedArrayType::Int32:
        case TypedArrayType::Uint32:
        case TypedArrayType::Float32:
            return 4;
        case TypedArrayType::Float64:
            return 8;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template <typename Type>
    Value getValueFromBuffer(ExecutionState& state, unsigned byteindex, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(bytelength());
        size_t elementSize = sizeof(Type);
        ASSERT(byteindex + elementSize <= bytelength());
        uint8_t* rawStart = rawBuffer() + byteindex;
        Type res;
        if (isLittleEndian != 1) { // bigEndian
            for (size_t i = 0; i < elementSize; i++) {
                ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];
            }
        } else { // littleEndian
            res = *((Type*)rawStart);
        }
        return Value(res);
    }

    template <typename Type>
    void setValueInBuffer(ExecutionState& state, unsigned byteindex, const Value& val, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(bytelength());
        size_t elementSize = sizeof(typename Type::Type);
        ASSERT(byteindex + elementSize <= bytelength());
        uint8_t* rawStart = rawBuffer() + byteindex;
        typename Type::Type littleEndianVal = Type::toNative(state, val);
        if (isLittleEndian != 1) {
            for (size_t i = 0; i < elementSize; i++) {
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
            }
        } else {
            *((typename Type::Type*)rawStart) = littleEndianVal;
        }
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
    uint8_t* m_rawBuffer;
    unsigned m_bytelength;
    unsigned m_byteoffset;
    unsigned m_arraylength;
};

template <typename Adapter>
struct TypedArrayAdaptor {
    typedef typename Adapter::Type Type;
    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return Adapter::toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return Adapter::toNativeFromDouble(state, val.asDouble());
        }
        return static_cast<Type>(val.toNumber(state));
    }
};

template <typename TypeArg>
struct IntegralTypedArrayAdapter {
    typedef TypeArg Type;
    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        int32_t result = static_cast<int32_t>(value);
        if (static_cast<double>(result) != value)
            result = Value(value).toInt32(state);
        return static_cast<TypeArg>(result);
    }
};

template <typename TypeArg>
struct FloatTypedArrayAdaptor {
    typedef TypeArg Type;
    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        return value;
    }
};

struct Int8Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int8_t>> {
};
struct Int16Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int16_t>> {
};
struct Int32Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int32_t>> {
};
struct Uint8Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint8_t>> {
};
struct Uint16Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint16_t>> {
};
struct Uint32Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint32_t>> {
};
struct Uint8ClampedAdaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint8_t>> {
};
struct Float32Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<float>> {
};
struct Float64Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<double>> {
};

template <typename TypeAdaptor, int typedArrayElementSize>
class TypedArrayObject : public ArrayBufferView {
    void typedArrayObjectPrototypeFiller(ExecutionState& state);

public:
    explicit TypedArrayObject(ExecutionState& state)
        : ArrayBufferView(state)
    {
        typedArrayObjectPrototypeFiller(state);
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override;

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        uint64_t index = P.tryToUseAsIndex();
        if (LIKELY(Value::InvalidIndexValue != index)) {
            if ((unsigned)index < arraylength()) {
                unsigned idxPosition = index * typedArrayElementSize;
                return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
            }
            return ObjectGetResult();
        }
        return ArrayBufferView::getOwnProperty(state, P);
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        uint64_t index = P.tryToUseAsIndex();
        if (LIKELY(Value::InvalidIndexValue != index)) {
            if ((unsigned)index >= arraylength())
                return false;
            unsigned idxPosition = index * typedArrayElementSize;

            if (LIKELY(desc.isValuePresentAlone() || desc.isDataWritableEnumerableConfigurable() || (desc.isWritable() && desc.isEnumerable()))) {
                setValueInBuffer<TypeAdaptor>(state, idxPosition, desc.value());
                return true;
            }
            return false;
        }
        return ArrayBufferView::defineOwnProperty(state, P, desc);
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        return ArrayBufferView::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        size_t len = arraylength();
        for (size_t i = 0; i < len; i++) {
            unsigned idxPosition = i * typedArrayElementSize + byteoffset();
            ArrayBufferObject* b = buffer();
            if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)), data)) {
                return;
            }
        }
        Object::enumeration(state, callback, data);
    }

    void allocateTypedArray(ExecutionState& state, unsigned length)
    {
        auto obj = new ArrayBufferObject(state);
        obj->allocateBuffer(length * typedArrayElementSize);
        setBuffer(obj, 0, length * typedArrayElementSize, length);
    }

    size_t elementSize()
    {
        return typedArrayElementSize;
    }

    virtual bool isTypedArrayObject() const override
    {
        return true;
    }

    virtual void sort(ExecutionState& state, const std::function<bool(const Value& a, const Value& b)>& comp) override
    {
        size_t arrayLen = arraylength();
        if (arrayLen) {
            Value* tempBuffer = (Value*)GC_MALLOC_IGNORE_OFF_PAGE(sizeof(Value) * arrayLen);

            for (size_t i = 0; i < arrayLen; i++) {
                unsigned idxPosition = i * typedArrayElementSize;
                tempBuffer[i] = getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition);
            }

            TightVector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> tempSpace;
            tempSpace.resizeWithUninitializedValues(arrayLen);
            mergeSort(tempBuffer, arrayLen, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
                *lessOrEqualp = comp(a, b);
                return true;
            });

            for (size_t i = 0; i < arrayLen; i++) {
                unsigned idxPosition = i * typedArrayElementSize;
                setValueInBuffer<TypeAdaptor>(state, idxPosition, tempBuffer[i]);
            }
            GC_FREE(tempBuffer);

            return;
        }
    }

    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property) override
    {
        Value::ValueIndex idx = property.tryToUseAsIndex(state);
        if (LIKELY(idx != Value::InvalidIndexValue) && LIKELY((unsigned)idx < arraylength())) {
            unsigned idxPosition = idx * typedArrayElementSize;
            return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
        }
        return get(state, ObjectPropertyName(state, property));
    }

    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value) override
    {
        Value::ValueIndex index = property.tryToUseAsIndex(state);
        if (LIKELY(Value::InvalidIndexValue != index) && LIKELY((unsigned)index < arraylength())) {
            unsigned idxPosition = index * typedArrayElementSize;
            setValueInBuffer<TypeAdaptor>(state, idxPosition, value);
            return true;
        }
        return set(state, ObjectPropertyName(state, property), value, this);
    }

protected:
};

typedef TypedArrayObject<Int8Adaptor, 1> Int8ArrayObjectWrapper;
class Int8ArrayObject : public Int8ArrayObjectWrapper {
public:
    explicit Int8ArrayObject(ExecutionState& state)
        : Int8ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Int8;
    }
};
typedef TypedArrayObject<Int16Adaptor, 2> Int16ArrayObjectWrapper;
class Int16ArrayObject : public Int16ArrayObjectWrapper {
public:
    explicit Int16ArrayObject(ExecutionState& state)
        : Int16ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Int16;
    }
};
typedef TypedArrayObject<Int32Adaptor, 4> Int32ArrayObjectWrapper;
class Int32ArrayObject : public Int32ArrayObjectWrapper {
public:
    explicit Int32ArrayObject(ExecutionState& state)
        : Int32ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Int32;
    }
};
typedef TypedArrayObject<Uint8Adaptor, 1> Uint8ArrayObjectWrapper;
class Uint8ArrayObject : public Uint8ArrayObjectWrapper {
public:
    explicit Uint8ArrayObject(ExecutionState& state)
        : Uint8ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Uint8;
    }
};
typedef TypedArrayObject<Uint16Adaptor, 2> Uint16ArrayObjectWrapper;
class Uint16ArrayObject : public Uint16ArrayObjectWrapper {
public:
    explicit Uint16ArrayObject(ExecutionState& state)
        : Uint16ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Uint16;
    }
};
typedef TypedArrayObject<Uint32Adaptor, 4> Uint32ArrayObjectWrapper;
class Uint32ArrayObject : public Uint32ArrayObjectWrapper {
public:
    explicit Uint32ArrayObject(ExecutionState& state)
        : Uint32ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Uint32;
    }
};
typedef TypedArrayObject<Uint8ClampedAdaptor, 1> Uint8ClampedArrayObjectWrapper;
class Uint8ClampedArrayObject : public Uint8ClampedArrayObjectWrapper {
public:
    explicit Uint8ClampedArrayObject(ExecutionState& state)
        : Uint8ClampedArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Uint8Clamped;
    }
};
typedef TypedArrayObject<Float32Adaptor, 4> Float32ArrayObjectWrapper;
class Float32ArrayObject : public Float32ArrayObjectWrapper {
public:
    explicit Float32ArrayObject(ExecutionState& state)
        : Float32ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Float32;
    }
};
typedef TypedArrayObject<Float64Adaptor, 8> Float64ArrayObjectWrapper;
class Float64ArrayObject : public Float64ArrayObjectWrapper {
public:
    explicit Float64ArrayObject(ExecutionState& state)
        : Float64ArrayObjectWrapper(state)
    {
    }
    virtual TypedArrayType typedArrayType()
    {
        return TypedArrayType::Float64;
    }
};

class TypedArrayObjectPrototype : public Object {
public:
    TypedArrayObjectPrototype(ExecutionState& state)
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

#endif
