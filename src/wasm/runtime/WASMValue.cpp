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

#include "Escargot.h"
#include "wasm/runtime/WASMModule.h"

namespace Escargot {

WASMValue::WASMValue(ExecutionState& state, WASMValue::Type type, const Value& value)
    : m_type(type)
{
    switch (m_type) {
    case I32:
        m_i32 = value.toInt32(state);
        break;
    case F32:
        // FIXME Let f32 be ? ToNumber(v) rounded to the nearest representable value using IEEE 754-2019 round to nearest, ties to even mode.
        m_f32 = value.toNumber(state);
        break;
    case F64:
        m_f64 = value.toNumber(state);
        break;
    case I64:
        m_i64 = value.toBigInt(state)->toInt64();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

Value WASMValue::toValue() const
{
    Value result;
    switch (m_type) {
    case I32: {
        result = Value(m_i32);
        break;
    }
    case F32: {
        result = Value((double)m_f32);
        break;
    }
    case F64: {
        result = Value(m_f64);
        break;
    }
    case I64: {
        result = new BigInt(m_i64);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
