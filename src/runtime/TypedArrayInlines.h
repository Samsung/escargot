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
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
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

    static Type toNativeFromDouble(ExecutionState& state, double value)
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
        return static_cast<Type>(lrint(value));
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

    static Value rawBytesToNumber(ExecutionState& state, TypedArrayType type, uint8_t* rawBytes)
    {
        switch (type) {
        case TypedArrayType::Int8:
            return Value(*reinterpret_cast<Int8Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint8:
            return Value(*reinterpret_cast<Uint8Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint8Clamped:
            return Value(*reinterpret_cast<Uint8ClampedAdaptor::Type*>(rawBytes));
        case TypedArrayType::Int16:
            return Value(readRawBytesAs<Int16Adaptor::Type>(rawBytes));
        case TypedArrayType::Uint16:
            return Value(readRawBytesAs<Uint16Adaptor::Type>(rawBytes));
        case TypedArrayType::Int32:
            return Value(readRawBytesAs<Int32Adaptor::Type>(rawBytes));
        case TypedArrayType::Uint32:
            return Value(readRawBytesAs<Uint32Adaptor::Type>(rawBytes));
        case TypedArrayType::Float16:
            return Value(Value::DoubleToIntConvertibleTestNeeds, convertFloat16ToFloat64(readRawBytesAs<uint16_t>(rawBytes)));
        case TypedArrayType::Float32:
            return Value(Value::DoubleToIntConvertibleTestNeeds, readFloat32(rawBytes));
        case TypedArrayType::Float64:
            return Value(Value::DoubleToIntConvertibleTestNeeds, readFloat64(rawBytes));
        case TypedArrayType::BigInt64:
            return Value(new BigInt(readInt64(rawBytes)));
        case TypedArrayType::BigUint64:
            return Value(new BigInt(readUint64(rawBytes)));
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return Value();
        }
    }

    static void numberToRawBytes(ExecutionState& state, TypedArrayType type, const Value& val, uint8_t* rawBytes)
    {
        switch (type) {
        case TypedArrayType::Int8:
            *reinterpret_cast<Int8Adaptor::Type*>(rawBytes) = Int8Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint8:
            *reinterpret_cast<Uint8Adaptor::Type*>(rawBytes) = Uint8Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint8Clamped:
            *reinterpret_cast<Uint8ClampedAdaptor::Type*>(rawBytes) = Uint8ClampedAdaptor::toNative(state, val);
            break;
        case TypedArrayType::Int16:
            writeRawBytesAs(rawBytes, Int16Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Uint16:
            writeRawBytesAs(rawBytes, Uint16Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Int32:
            writeRawBytesAs(rawBytes, Int32Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Uint32:
            writeRawBytesAs(rawBytes, Uint32Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Float16:
            writeRawBytesAs(rawBytes, Float16Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Float32:
            writeRawBytesAs(rawBytes, Float32Adaptor::toNative(state, val));
            break;
        case TypedArrayType::Float64:
            writeRawBytesAs(rawBytes, Float64Adaptor::toNative(state, val));
            break;
        case TypedArrayType::BigInt64:
            writeRawBytesAs(rawBytes, BigInt64Adaptor::toNative(state, val));
            break;
        case TypedArrayType::BigUint64:
            writeRawBytesAs(rawBytes, BigUint64Adaptor::toNative(state, val));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
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
} // namespace Escargot
#endif
