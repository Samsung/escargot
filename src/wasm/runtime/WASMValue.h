/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_WASM_INTERPRETER)

#ifndef __EscargotWASMValue__
#define __EscargotWASMValue__

#include "runtime/Value.h"

namespace Escargot {

class WASMV128 {
public:
    uint8_t m_data[16];
};

class WASMValue {
public:
    // https://webassembly.github.io/spec/core/syntax/types.html
    enum Type : uint8_t {
        I32,
        I64,
        F32,
        F64,
        V128,
        FuncRef,
        ExternRef,
        Void,
    };

    WASMValue()
        : m_i32(0)
        , m_type(Void)
    {
    }

    explicit WASMValue(const int32_t& v)
        : m_i32(v)
        , m_type(I32)
    {
    }

    explicit WASMValue(const int64_t& v)
        : m_i64(v)
        , m_type(I64)
    {
    }

    explicit WASMValue(const float& v)
        : m_f32(v)
        , m_type(F32)
    {
    }

    explicit WASMValue(const double& v)
        : m_f64(v)
        , m_type(F64)
    {
    }

    WASMValue(Type type, const uint8_t* const memory)
        : m_type(type)
    {
        switch (m_type) {
        case I32:
            m_i32 = *reinterpret_cast<const int32_t* const>(memory);
            break;
        case F32:
            m_f32 = *reinterpret_cast<const float* const>(memory);
            break;
        case F64:
            m_f64 = *reinterpret_cast<const double* const>(memory);
            break;
        case I64:
            m_i64 = *reinterpret_cast<const int64_t* const>(memory);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    WASMValue(ExecutionState& state, Type type, const Value& v);

    Type type() const
    {
        return m_type;
    }

    int32_t i32() const
    {
        ASSERT(type() == I32);
        return m_i32;
    }

    int64_t i64() const
    {
        ASSERT(type() == I64);
        return m_i64;
    }

    float f32() const
    {
        ASSERT(type() == F32);
        return m_f32;
    }

    double f64() const
    {
        ASSERT(type() == F64);
        return m_f64;
    }

    Value toValue() const;
    void writeToStack(uint8_t* ptr)
    {
        switch (m_type) {
        case I32: {
            *reinterpret_cast<int32_t*>(ptr) = m_i32;
            break;
        }
        case F32: {
            *reinterpret_cast<float*>(ptr) = m_f32;
            break;
        }
        case F64: {
            *reinterpret_cast<double*>(ptr) = m_f64;
            break;
        }
        case I64: {
            *reinterpret_cast<int64_t*>(ptr) = m_i64;
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
        }
    }

private:
    union {
        int32_t m_i32;
        int64_t m_i64;
        float m_f32;
        double m_f64;
        WASMV128 m_v128;
        void* m_ref;
    };

    Type m_type;
};

template <typename T>
size_t stackAllocatedSize()
{
    if (sizeof(T) < sizeof(size_t) && sizeof(T) % sizeof(size_t)) {
        return sizeof(size_t);
    } else if (sizeof(T) > sizeof(size_t) && sizeof(T) % sizeof(size_t)) {
        return sizeof(size_t) * ((sizeof(T) / sizeof(size_t)) + 1);
    } else {
        return sizeof(T);
    }
}

inline size_t wasmValueSizeInStack(WASMValue::Type type)
{
    switch (type) {
    case WASMValue::I32:
    case WASMValue::F32:
        return stackAllocatedSize<int32_t>();
    case WASMValue::I64:
    case WASMValue::F64:
        return 8;
    case WASMValue::V128:
        return 16;
    default:
        return sizeof(size_t);
    }
}

} // namespace Escargot

#endif // __EscargotWASMValue__
#endif // ENABLE_WASM_INTERPRETER
