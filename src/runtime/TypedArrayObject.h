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
        m_buffer = nullptr;
        m_byteoffset = m_bytelength = 0;
    }

    ALWAYS_INLINE ArrayBufferObject* buffer() { return m_buffer; }
    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo) { m_buffer = bo; }
    ALWAYS_INLINE unsigned bytelength() { return m_bytelength; }
    ALWAYS_INLINE void setBytelength(unsigned l) { m_bytelength = l; }
    ALWAYS_INLINE unsigned byteoffset() { return m_byteoffset; }
    ALWAYS_INLINE void setByteoffset(unsigned o) { m_byteoffset = o; }
protected:
    ArrayBufferObject* m_buffer;
    unsigned m_bytelength;
    unsigned m_byteoffset;
};

template <typename Adapter>
struct TypedArrayAdaptor {
    typedef typename Adapter::Type Type;
    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return Adapter::toNativeFromInt32(val.asInt32());
        } else if (val.isDouble()) {
            return Adapter::toNativeFromDouble(val.asDouble());
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

template <typename TypeAdaptor, int elementSize>
class TypedArrayObject : public ArrayBufferView {
public:
    TypedArrayObject(ExecutionState& state)
        : ArrayBufferView(state)
    {
        m_arraylength = 0;
    }

    Value get(const uint64_t& key)
    {
        if ((unsigned)key < arraylength()) {
            unsigned idxPosition = key * elementSize + byteoffset();
            ArrayBufferObject* b = buffer();
            return b->getValueFromBuffer<typename TypeAdaptor::Type>(idxPosition);
        }
        return Value();
    }
    bool set(const uint64_t& key, const Value& val)
    {
        if ((unsigned)key >= arraylength())
            return false;
        unsigned idxPosition = key * elementSize + byteoffset();
        ArrayBufferObject* b = buffer();
        return b->setValueInBuffer<TypeAdaptor>(idxPosition, val);
    }

    ALWAYS_INLINE unsigned arraylength() { return m_arraylength; }
protected:
    unsigned m_arraylength;
};

typedef TypedArrayObject<Int8Adaptor, 1> Int8Array;
typedef TypedArrayObject<Int16Adaptor, 2> Int16Array;
typedef TypedArrayObject<Int32Adaptor, 4> Int32Array;
typedef TypedArrayObject<Uint8Adaptor, 1> Uint8Array;
typedef TypedArrayObject<Uint16Adaptor, 2> Uint16Array;
typedef TypedArrayObject<Uint32Adaptor, 4> EUint32Array;
typedef TypedArrayObject<Uint8ClampedAdaptor, 1> Uint8ClampedArray;
typedef TypedArrayObject<Float32Adaptor, 4> Float32Array;
typedef TypedArrayObject<Float64Adaptor, 8> Float64Array;
}

#endif

#endif
