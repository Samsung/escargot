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

#if defined(ENABLE_WASM)

#include "Escargot.h"
#include "wasm.h"
#include "runtime/Context.h"
#include "runtime/Value.h"
#include "runtime/BigInt.h"
#include "wasm/WASMValueConverter.h"

namespace Escargot {

#define WASM_I32_VAL(result, i) \
    result.kind = WASM_I32;     \
    result.of.i32 = i;

#define WASM_I64_VAL(result, i) \
    result.kind = WASM_I64;     \
    result.of.i64 = i

#define WASM_F32_VAL(result, z) \
    result.kind = WASM_F32;     \
    result.of.f32 = z;

#define WASM_F64_VAL(result, z) \
    result.kind = WASM_F64;     \
    result.of.f64 = z

#define WASM_REF_VAL(result, r) \
    result.kind = WASM_ANYREF;  \
    result.of.ref = r

#define WASM_INIT_VAL(result)  \
    wasm_val_t result{};       \
    result.kind = WASM_ANYREF; \
    result.of.ref = NULL;

Value WASMValueConverter::wasmToJSValue(ExecutionState& state, const wasm_val_t& value)
{
    Value result;
    switch (value.kind) {
    case WASM_I32: {
        result = Value(value.of.i32);
        break;
    }
    case WASM_F32: {
        result = Value((double)value.of.f32);
        break;
    }
    case WASM_F64: {
        result = Value(value.of.f64);
        break;
    }
    case WASM_I64: {
        result = new BigInt(value.of.i64);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

wasm_val_t WASMValueConverter::wasmToWebAssemblyValue(ExecutionState& state, const Value& value, wasm_valkind_t type)
{
    WASM_INIT_VAL(result);
    switch (type) {
    case WASM_I32: {
        int32_t val = value.toInt32(state);
        WASM_I32_VAL(result, val);
        break;
    }
    case WASM_F32: {
        // FIXME Let f32 be ? ToNumber(v) rounded to the nearest representable value using IEEE 754-2019 round to nearest, ties to even mode.
        float32_t val = value.toNumber(state);
        WASM_F32_VAL(result, val);
        break;
    }
    case WASM_F64: {
        float64_t val = value.toNumber(state);
        WASM_F64_VAL(result, val);
        break;
    }
    case WASM_I64: {
        int64_t val = value.toBigInt(state)->toInt64();
        WASM_I64_VAL(result, val);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

wasm_val_t WASMValueConverter::wasmDefaultValue(wasm_valkind_t type)
{
    WASM_INIT_VAL(result);
    switch (type) {
    case WASM_I32: {
        WASM_I32_VAL(result, 0);
        break;
    }
    case WASM_I64: {
        WASM_I64_VAL(result, 0);
        break;
    }
    case WASM_F32: {
        WASM_F32_VAL(result, 0);
        break;
    }
    case WASM_F64: {
        WASM_F64_VAL(result, 0);
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
#endif // ENABLE_WASM
