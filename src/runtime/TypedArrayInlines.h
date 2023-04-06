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
        int32_t result = static_cast<int32_t>(value);
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
struct Float32Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<float>> {
};
struct Float64Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<double>> {
};
struct BigInt64Adaptor : TypedArrayAdaptor<BigIntegralArrayAdaptor<int64_t>> {
};
struct BigUint64Adaptor : TypedArrayAdaptor<BigIntegralArrayAdaptor<uint64_t>> {
};

struct TypedArrayHelper {
    static unsigned elementSizeTable[11];

    inline static size_t elementSize(TypedArrayType type)
    {
        return elementSizeTable[(size_t)type];
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
            return Value(*reinterpret_cast<Int16Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint16:
            return Value(*reinterpret_cast<Uint16Adaptor::Type*>(rawBytes));
        case TypedArrayType::Int32:
            return Value(*reinterpret_cast<Int32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint32:
            return Value(*reinterpret_cast<Uint32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Float32:
            return Value(Value::DoubleToIntConvertibleTestNeeds, *reinterpret_cast<Float32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Float64:
            return Value(Value::DoubleToIntConvertibleTestNeeds, *reinterpret_cast<Float64Adaptor::Type*>(rawBytes));
        case TypedArrayType::BigInt64:
            return Value(new BigInt(*reinterpret_cast<BigInt64Adaptor::Type*>(rawBytes)));
        case TypedArrayType::BigUint64:
            return Value(new BigInt(*reinterpret_cast<BigUint64Adaptor::Type*>(rawBytes)));
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
            *reinterpret_cast<Int16Adaptor::Type*>(rawBytes) = Int16Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint16:
            *reinterpret_cast<Uint16Adaptor::Type*>(rawBytes) = Uint16Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Int32:
            *reinterpret_cast<Int32Adaptor::Type*>(rawBytes) = Int32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint32:
            *reinterpret_cast<Uint32Adaptor::Type*>(rawBytes) = Uint32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Float32:
            *reinterpret_cast<Float32Adaptor::Type*>(rawBytes) = Float32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Float64:
            *reinterpret_cast<Float64Adaptor::Type*>(rawBytes) = Float64Adaptor::toNative(state, val);
            break;
        case TypedArrayType::BigInt64:
            *reinterpret_cast<BigInt64Adaptor::Type*>(rawBytes) = BigInt64Adaptor::toNative(state, val);
            break;
        case TypedArrayType::BigUint64:
            *reinterpret_cast<BigUint64Adaptor::Type*>(rawBytes) = BigUint64Adaptor::toNative(state, val);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
};
} // namespace Escargot
#endif
