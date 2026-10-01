/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTypedArrayInlines__
#define __EscargotTypedArrayInlines__

#include "util/Float16.h"
#include "runtime/TypedArrayObject.h"

namespace Escargot {

template <typename Adapter>
struct TypedArrayAdaptor {
    typedef typename Adapter::Type Type;
    static Type toNative(ExecutionState& state, const Value& val)
    {
        return Adapter::toNative(state, val);
    }

    ALWAYS_INLINE static Type toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return Adapter::toNativeFromInt32(state, value);
    }

    ALWAYS_INLINE static Type toNativeFromDouble(ExecutionState& state, double value)
    {
        return Adapter::toNativeFromDouble(state, value);
    }
};

template <typename TypeArg>
struct IntegralTypedArrayAdapter {
    typedef TypeArg Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        auto number = val.toNumber(state);
        if (std::isnan(number)) {
            return 0;
        }
        return static_cast<Type>(number);
    }

    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    ALWAYS_INLINE static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        // Speculative fast-path cast: value can be far outside int32_t's
        // range here (e.g. a large Atomics operand), which is UB for a
        // plain static_cast, but the mismatch check right below always
        // catches it and falls back to the well-defined slow path.
        int32_t result = Value::truncateDoubleToInt32Unchecked(value);
        if (static_cast<double>(result) != value)
            result = Value(Value::EncodeAsDouble, value).toInt32(state);
        return static_cast<TypeArg>(result);
    }
};

template <typename TypeArg>
struct FloatTypedArrayAdaptor {
    typedef TypeArg Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        return static_cast<Type>(val.toNumber(state));
    }

    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        return value;
    }
};

struct Float16TypedArrayAdaptor {
    typedef Float16 Type;

    static Float16 toNative(ExecutionState& state, const Value& val)
    {
        return Float16(val.toNumber(state));
    }

    static Float16 toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return Float16(static_cast<double>(value));
    }
    static Float16 toNativeFromDouble(ExecutionState& state, double value)
    {
        return Float16(value);
    }
};

template <typename TypeArg>
struct BigIntegralArrayAdaptor {
    typedef TypeArg Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        auto n = val.toBigInt(state);
        if (std::is_same<Type, uint64_t>::value) {
            return n->toUint64();
        } else {
            return n->toInt64();
        }
    }
};

struct Uint8ClampedAdaptor {
    typedef uint8_t Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        return toNativeFromDouble(state, val.toNumber(state));
    }

    static Type toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        if (value < 0) {
            return 0;
        }
        if (value > 255) {
            return 255;
        }
        return static_cast<Type>(value);
    }

    ALWAYS_INLINE static Type toNativeFromDouble(ExecutionState& state, double value)
    {
        if (std::isnan(value)) {
            return 0;
        }
        if (value < 0) {
            return 0;
        }
        if (value > 255) {
            return 255;
        }
        int32_t integer = static_cast<int32_t>(value);
        double fraction = value - integer;
        if (fraction > 0.5 || (fraction == 0.5 && (integer & 1))) {
            integer++;
        }
        return static_cast<Type>(integer);
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
struct Float16Adaptor : TypedArrayAdaptor<Float16TypedArrayAdaptor> {
};
struct Float32Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<float>> {
};
struct Float64Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<double>> {
};
struct BigInt64Adaptor : TypedArrayAdaptor<BigIntegralArrayAdaptor<int64_t>> {
};
struct BigUint64Adaptor : TypedArrayAdaptor<BigIntegralArrayAdaptor<uint64_t>> {
};

struct TypedArrayHelper {
    static unsigned elementSizeTable[12];

    inline static size_t elementSize(TypedArrayType type)
    {
        return elementSizeTable[(size_t)type];
    }

#if defined(CPU_ARM32)
    // reading unaligned address raises SIGBUS error in armeabi-v7a
#define ATTRIBUTE_NO_OPTIMIZE_IF_ARM32 ATTRIBUTE_NO_OPTIMIZE
#else
#define ATTRIBUTE_NO_OPTIMIZE_IF_ARM32
#endif

    // DataView allows byte-granular (not element-size-aligned) access by
    // spec, so rawBytes here is not guaranteed to satisfy T's alignment.
    // Dereferencing it through a T* is UB in the C++ memory model even
    // though x86 supports unaligned loads/stores in hardware (UBSan's
    // misaligned-load/store check flags it). memcpy is the portable,
    // well-defined equivalent and compiles down to the same single
    // (possibly unaligned) load/store instruction wherever the target
    // supports it, so this is zero-cost on x64/ARM64.
    template <typename T>
    ALWAYS_INLINE static T readRawBytesAs(uint8_t* rawBytes)
    {
        T result;
        memcpy(&result, rawBytes, sizeof(T));
        return result;
    }

    template <typename T>
    ALWAYS_INLINE static void writeRawBytesAs(uint8_t* rawBytes, const T& value)
    {
        memcpy(rawBytes, &value, sizeof(T));
    }

    ATTRIBUTE_NO_OPTIMIZE_IF_ARM32 static Float32Adaptor::Type readFloat32(uint8_t* rawBytes)
    {
        return readRawBytesAs<Float32Adaptor::Type>(rawBytes);
    }
    ATTRIBUTE_NO_OPTIMIZE_IF_ARM32 static Float64Adaptor::Type readFloat64(uint8_t* rawBytes)
    {
#if defined(CPU_ARM32)
        uint64_t result;
        uint32_t* bufferAs32 = reinterpret_cast<uint32_t*>(rawBytes);
        uint32_t* resultAs32 = reinterpret_cast<uint32_t*>(&result);
        resultAs32[0] = bufferAs32[0];
        resultAs32[1] = bufferAs32[1];
        return bitwise_cast<Float64Adaptor::Type>(result);
#else
        return readRawBytesAs<Float64Adaptor::Type>(rawBytes);
#endif
    }

    // Read through 32-bit integer memcpy so ARM32 never emits an aligned-only
    // float load (vldr), using hardware unaligned integer load (ldr) instead.
    // This can be inlined into TypedArray indexed access; DataView keeps its
    // existing byte-granular read path.
    ALWAYS_INLINE static uint32_t readFloat32BitsInline(const uint8_t* rawBytes)
    {
        return readRawBytesAs<uint32_t>(const_cast<uint8_t*>(rawBytes));
    }

    ALWAYS_INLINE static uint64_t readFloat64BitsInline(const uint8_t* rawBytes)
    {
#if defined(ESCARGOT_LITTLE_ENDIAN)
        return static_cast<uint64_t>(readFloat32BitsInline(rawBytes))
            | (static_cast<uint64_t>(readFloat32BitsInline(rawBytes + 4)) << 32);
#else
        return (static_cast<uint64_t>(readFloat32BitsInline(rawBytes)) << 32)
            | static_cast<uint64_t>(readFloat32BitsInline(rawBytes + 4));
#endif
    }
    ATTRIBUTE_NO_OPTIMIZE_IF_ARM32 static BigInt64Adaptor::Type readInt64(uint8_t* rawBytes)
    {
#if defined(CPU_ARM32)
        BigInt64Adaptor::Type result;
        uint32_t* bufferAs32 = reinterpret_cast<uint32_t*>(rawBytes);
        uint32_t* resultAs32 = reinterpret_cast<uint32_t*>(&result);
        resultAs32[0] = bufferAs32[0];
        resultAs32[1] = bufferAs32[1];
        return result;
#else
        return readRawBytesAs<BigInt64Adaptor::Type>(rawBytes);
#endif
    }
    ATTRIBUTE_NO_OPTIMIZE_IF_ARM32 static BigUint64Adaptor::Type readUint64(uint8_t* rawBytes)
    {
#if defined(CPU_ARM32)
        BigUint64Adaptor::Type result;
        uint32_t* bufferAs32 = reinterpret_cast<uint32_t*>(rawBytes);
        uint32_t* resultAs32 = reinterpret_cast<uint32_t*>(&result);
        resultAs32[0] = bufferAs32[0];
        resultAs32[1] = bufferAs32[1];
        return result;
#else
        return readRawBytesAs<BigUint64Adaptor::Type>(rawBytes);
#endif
    }

    template <bool indexed, unsigned elementShift>
    ALWAYS_INLINE static uint8_t* elementAddress(uint8_t* rawBytes, uint32_t index)
    {
        return rawBytes + (indexed ? (static_cast<size_t>(index) << elementShift) : 0);
    }

    NEVER_INLINE static Value readFloat16Value(uint8_t* rawBytes)
    {
        return Value(Value::DoubleToIntConvertibleTestNeeds, convertFloat16ToFloat64(readRawBytesAs<uint16_t>(rawBytes)));
    }

    template <bool inlineFloatRead = false, bool indexed = false>
    ALWAYS_INLINE static Value rawBytesToNumber(ExecutionState& state, TypedArrayType type, uint8_t* rawBytes, uint32_t index = 0)
    {
        switch (type) {
        case TypedArrayType::Int8:
            return Value(*reinterpret_cast<Int8Adaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)));
        case TypedArrayType::Uint8:
            return Value(*reinterpret_cast<Uint8Adaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)));
        case TypedArrayType::Uint8Clamped:
            return Value(*reinterpret_cast<Uint8ClampedAdaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)));
        case TypedArrayType::Int16:
            return Value(readRawBytesAs<Int16Adaptor::Type>(elementAddress<indexed, 1>(rawBytes, index)));
        case TypedArrayType::Uint16:
            return Value(readRawBytesAs<Uint16Adaptor::Type>(elementAddress<indexed, 1>(rawBytes, index)));
        case TypedArrayType::Int32:
            return Value(readRawBytesAs<Int32Adaptor::Type>(elementAddress<indexed, 2>(rawBytes, index)));
        case TypedArrayType::Uint32:
            return Value(readRawBytesAs<Uint32Adaptor::Type>(elementAddress<indexed, 2>(rawBytes, index)));
        case TypedArrayType::Float16:
            if (inlineFloatRead) {
                return readFloat16Value(elementAddress<indexed, 1>(rawBytes, index));
            }
            return Value(Value::DoubleToIntConvertibleTestNeeds, convertFloat16ToFloat64(readRawBytesAs<uint16_t>(elementAddress<indexed, 1>(rawBytes, index))));
        case TypedArrayType::Float32:
            if (inlineFloatRead) {
                return Value(Value::DoubleToIntConvertibleTestNeeds, bitwise_cast<float>(readFloat32BitsInline(elementAddress<indexed, 2>(rawBytes, index))));
            }
            return Value(Value::DoubleToIntConvertibleTestNeeds, readFloat32(elementAddress<indexed, 2>(rawBytes, index)));
        case TypedArrayType::Float64:
            if (inlineFloatRead) {
                return Value(Value::DoubleToIntConvertibleTestNeeds, bitwise_cast<double>(readFloat64BitsInline(elementAddress<indexed, 3>(rawBytes, index))));
            }
            return Value(Value::DoubleToIntConvertibleTestNeeds, readFloat64(elementAddress<indexed, 3>(rawBytes, index)));
        case TypedArrayType::BigInt64:
            return Value(new BigInt(readInt64(elementAddress<indexed, 3>(rawBytes, index))));
        case TypedArrayType::BigUint64:
            return Value(new BigInt(readUint64(elementAddress<indexed, 3>(rawBytes, index))));
        default:
            ASSERT_UNREACHABLE();
            return Value();
        }
    }

    /* Converts `val` to the array's native element type.
       - inlineNumericConversion: handle Int32/Double inline before falling back to the generic
         Adaptor::toNative(), which can run user code (valueOf) and throw.
       - numericOnly: the caller guarantees val.isNumber(), so the generic fallback is dropped
         altogether. That leaves the conversion leaf and non-throwing, which is what lets the
         switch inlined into Interpreter::interpret() stay small. */
    template <typename Adaptor, bool inlineNumericConversion, bool numericOnly = false>
    ALWAYS_INLINE static typename Adaptor::Type toNativeElement(ExecutionState& state, const Value& val)
    {
        ASSERT(!numericOnly || val.isNumber());
        if (numericOnly) {
            return val.isInt32() ? Adaptor::toNativeFromInt32(state, val.asInt32())
                                 : Adaptor::toNativeFromDouble(state, val.asDouble());
        }
        if (inlineNumericConversion) {
            if (val.isInt32()) {
                return Adaptor::toNativeFromInt32(state, val.asInt32());
            }
            if (val.isDouble()) {
                return Adaptor::toNativeFromDouble(state, val.asDouble());
            }
        }
        return Adaptor::toNative(state, val);
    }

    /* With numericOnly the BigInt64/BigUint64 cases are unreachable: storing a Number into a
       BigInt array is a TypeError, so the caller must filter those types out beforehand. */
    template <bool inlineNumericConversion = false, bool indexed = false, bool numericOnly = false>
    ALWAYS_INLINE static void numberToRawBytes(ExecutionState& state, TypedArrayType type, const Value& val, uint8_t* rawBytes, uint32_t index = 0)
    {
        ASSERT(!numericOnly || (val.isNumber() && type < TypedArrayType::BigInt64));
        switch (type) {
        case TypedArrayType::Int8:
            *reinterpret_cast<Int8Adaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)) = toNativeElement<Int8Adaptor, inlineNumericConversion, numericOnly>(state, val);
            break;
        case TypedArrayType::Uint8:
            *reinterpret_cast<Uint8Adaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)) = toNativeElement<Uint8Adaptor, inlineNumericConversion, numericOnly>(state, val);
            break;
        case TypedArrayType::Uint8Clamped:
            *reinterpret_cast<Uint8ClampedAdaptor::Type*>(elementAddress<indexed, 0>(rawBytes, index)) = toNativeElement<Uint8ClampedAdaptor, inlineNumericConversion, numericOnly>(state, val);
            break;
        case TypedArrayType::Int16:
            writeRawBytesAs(elementAddress<indexed, 1>(rawBytes, index), toNativeElement<Int16Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Uint16:
            writeRawBytesAs(elementAddress<indexed, 1>(rawBytes, index), toNativeElement<Uint16Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Int32:
            writeRawBytesAs(elementAddress<indexed, 2>(rawBytes, index), toNativeElement<Int32Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Uint32:
            writeRawBytesAs(elementAddress<indexed, 2>(rawBytes, index), toNativeElement<Uint32Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Float16:
            writeRawBytesAs(elementAddress<indexed, 1>(rawBytes, index), toNativeElement<Float16Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Float32:
            writeRawBytesAs(elementAddress<indexed, 2>(rawBytes, index), toNativeElement<Float32Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::Float64:
            writeRawBytesAs(elementAddress<indexed, 3>(rawBytes, index), toNativeElement<Float64Adaptor, inlineNumericConversion, numericOnly>(state, val));
            break;
        case TypedArrayType::BigInt64:
            if (numericOnly) {
                ASSERT_NOT_REACHED();
                break;
            }
            writeRawBytesAs(elementAddress<indexed, 3>(rawBytes, index), BigInt64Adaptor::toNative(state, val));
            break;
        case TypedArrayType::BigUint64:
            if (numericOnly) {
                ASSERT_NOT_REACHED();
                break;
            }
            writeRawBytesAs(elementAddress<indexed, 3>(rawBytes, index), BigUint64Adaptor::toNative(state, val));
            break;
        default:
            ASSERT_UNREACHABLE();
            break;
        }
    }

    static FunctionObject* typeToConstructor(ExecutionState& state, TypedArrayType type)
    {
        auto globalObject = state.context()->globalObject();
#define DEFINE_TYPE(TYPE, type, siz, nativeType) \
    case TypedArrayType::TYPE:                   \
        return globalObject->type##Array();

        switch (type) {
            FOR_EACH_TYPEDARRAY_TYPES(DEFINE_TYPE)
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }
#undef DEFINE_TYPE
    }
};

ALWAYS_INLINE void TypedArrayObject::setDirectTypedArrayElement(ExecutionState& state, uint32_t index, const Value& value)
{
    ASSERT(static_cast<size_t>(index) < arrayLength() && !buffer()->isDetachedBuffer() && value.isPrimitive());
    TypedArrayHelper::numberToRawBytes<true, true>(state, m_type, value, rawBuffer(), index);
}

template <TypedArrayType type>
ALWAYS_INLINE Value TypedArrayObject::getDirectTypedArrayElementOfType(ExecutionState& state, uint32_t index)
{
    ASSERT(m_type == type && static_cast<size_t>(index) < arrayLength() && !buffer()->isDetachedBuffer());
    return TypedArrayHelper::rawBytesToNumber<true, true>(state, type, rawBuffer(), index);
}

template <TypedArrayType type>
ALWAYS_INLINE void TypedArrayObject::setDirectTypedArrayElementNumericOfType(ExecutionState& state, uint32_t index, const Value& value)
{
    ASSERT(m_type == type && static_cast<size_t>(index) < arrayLength() && !buffer()->isDetachedBuffer());
    ASSERT(value.isNumber() && type < TypedArrayType::BigInt64);
    TypedArrayHelper::numberToRawBytes<true, true, true>(state, type, value, rawBuffer(), index);
}
} // namespace Escargot
#endif
