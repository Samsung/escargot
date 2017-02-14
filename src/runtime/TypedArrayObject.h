#ifndef __EscargotTypedArrayObject__
#define __EscargotTypedArrayObject__

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayBufferObject.h"

namespace Escargot {


class ArrayBufferView : public Object {
public:
    ArrayBufferView(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    {
        m_rawBuffer = nullptr;
        m_buffer = nullptr;
        m_arraylength = m_byteoffset = m_bytelength = 0;
    }

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
    TypedArrayObject(ExecutionState& state)
        : ArrayBufferView(state)
    {
        typedArrayObjectPrototypeFiller(state);
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty();

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

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        uint64_t index;
        if (LIKELY(P.isUIntType())) {
            index = P.uintValue();
        } else {
            index = P.string(state)->tryToUseAsIndex();
        }

        if (LIKELY(Value::InvalidIndexValue != index)) {
            if ((unsigned)index < arraylength()) {
                unsigned idxPosition = index * typedArrayElementSize;
                return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
            }
            return ObjectGetResult();
        }
        return ArrayBufferView::getOwnProperty(state, P);
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        uint64_t index;
        if (LIKELY(P.isUIntType())) {
            index = P.uintValue();
        } else {
            index = P.string(state)->tryToUseAsIndex();
        }

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

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        return ArrayBufferView::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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

    virtual bool isTypedArrayObject() const
    {
        return true;
    }

    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property)
    {
        uint64_t idx;
        if (LIKELY(property.isUInt32())) {
            idx = property.asUInt32();
        } else {
            idx = property.toString(state)->tryToUseAsIndex();
        }
        if (LIKELY(idx != Value::InvalidIndexValue)) {
            if (LIKELY((unsigned)idx < arraylength())) {
                unsigned idxPosition = idx * typedArrayElementSize;
                return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
            }
        }
        return get(state, ObjectPropertyName(state, property));
    }

    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
    {
        uint64_t index;
        if (LIKELY(property.isUInt32())) {
            index = property.asUInt32();
        } else {
            index = property.toString(state)->tryToUseAsIndex();
        }
        if (LIKELY(Value::InvalidIndexValue != index)) {
            if (LIKELY((unsigned)index < arraylength())) {
                unsigned idxPosition = index * typedArrayElementSize;
                setValueInBuffer<TypeAdaptor>(state, idxPosition, value);
                return true;
            }
        }
        return set(state, ObjectPropertyName(state, property), value, this);
    }

protected:
};

typedef TypedArrayObject<Int8Adaptor, 1> Int8ArrayObject;
typedef TypedArrayObject<Int16Adaptor, 2> Int16ArrayObject;
typedef TypedArrayObject<Int32Adaptor, 4> Int32ArrayObject;
typedef TypedArrayObject<Uint8Adaptor, 1> Uint8ArrayObject;
typedef TypedArrayObject<Uint16Adaptor, 2> Uint16ArrayObject;
typedef TypedArrayObject<Uint32Adaptor, 4> EUint32ArrayObject;
typedef TypedArrayObject<Uint8ClampedAdaptor, 1> Uint8ClampedArrayObject;
typedef TypedArrayObject<Float32Adaptor, 4> Float32ArrayObject;
typedef TypedArrayObject<Float64Adaptor, 8> Float64ArrayObject;
}

#endif

#endif
