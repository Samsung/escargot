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

#define WASM_I32_VAL(i)                     \
    {                                       \
        .kind = WASM_I32, .of = {.i32 = i } \
    }
#define WASM_I64_VAL(i)                     \
    {                                       \
        .kind = WASM_I64, .of = {.i64 = i } \
    }
#define WASM_F32_VAL(z)                     \
    {                                       \
        .kind = WASM_F32, .of = {.f32 = z } \
    }
#define WASM_F64_VAL(z)                     \
    {                                       \
        .kind = WASM_F64, .of = {.f64 = z } \
    }
#define WASM_REF_VAL(r)                        \
    {                                          \
        .kind = WASM_ANYREF, .of = {.ref = r } \
    }
#define WASM_INIT_VAL                             \
    {                                             \
        .kind = WASM_ANYREF, .of = {.ref = NULL } \
    }

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
        result = new BigInt(state.context()->vmInstance(), value.of.i64);
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
    wasm_val_t result;
    switch (type) {
    case WASM_I32: {
        int32_t val = value.toInt32(state);
        result = WASM_I32_VAL(val);
        break;
    }
    case WASM_F32: {
        // FIXME Let f32 be ? ToNumber(v) rounded to the nearest representable value using IEEE 754-2019 round to nearest, ties to even mode.
        float32_t val = value.toNumber(state);
        result = WASM_F32_VAL(val);
        break;
    }
    case WASM_F64: {
        float64_t val = value.toNumber(state);
        result = WASM_F64_VAL(val);
        break;
    }
    case WASM_I64: {
        int64_t val = value.toBigInt(state)->toInt64();
        result = WASM_I64_VAL(val);
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
    wasm_val_t result;
    switch (type) {
    case WASM_I32: {
        result = WASM_I32_VAL(0);
        break;
    }
    case WASM_I64: {
        result = WASM_I64_VAL(0);
        break;
    }
    case WASM_F32: {
        result = WASM_F32_VAL(0);
        break;
    }
    case WASM_F64: {
        result = WASM_F64_VAL(0);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}
}
#endif // ENABLE_WASM
