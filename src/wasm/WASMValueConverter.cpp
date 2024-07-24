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
#include "wasm/WASMObject.h"
#include "wasm/ExportedFunctionObject.h"

namespace Escargot {

// redefine macros due to compile error in msvc
#undef WASM_I32_VAL
#undef WASM_I64_VAL
#undef WASM_F32_VAL
#undef WASM_F64_VAL
#undef WASM_REF_VAL
#undef WASM_FUNC_VAL
#undef WASM_INIT_VAL
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

#define WASM_FUNC_VAL(result, r) \
    result.kind = WASM_FUNCREF;  \
    result.of.ref = r

#define WASM_INIT_VAL(result)  \
    wasm_val_t result{};       \
    result.kind = WASM_ANYREF; \
    result.of.ref = nullptr;

wasm_val_t WASMValueConverter::defaultValue(ExecutionState& state, wasm_valkind_t type)
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
    case WASM_ANYREF: {
        // return ToWebAssemblyValue(undefined, valuetype).
        WASM_REF_VAL(result, toWebAssemblyValue(state, Value(), type).of.ref);
        break;
    }
    case WASM_FUNCREF: {
        WASM_FUNC_VAL(result, nullptr);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

Value WASMValueConverter::toJSValue(ExecutionState& state, const wasm_val_t& value)
{
    Value result;
    switch (value.kind) {
    case WASM_I32: {
        result = Value(value.of.i32);
        break;
    }
    case WASM_F32: {
        result = Value(Value::DoubleToIntConvertibleTestNeeds, (double)value.of.f32);
        break;
    }
    case WASM_F64: {
        result = Value(Value::DoubleToIntConvertibleTestNeeds, value.of.f64);
        break;
    }
    case WASM_I64: {
        result = new BigInt(value.of.i64);
        break;
    }
    case WASM_ANYREF: {
        if (!value.of.ref) {
            result = Value(Value::Null);
        } else {
            result = state.context()->wasmCache()->findValueByRef(value.of.ref);
        }
        break;
    }
    case WASM_FUNCREF: {
        if (!value.of.ref) {
            result = Value(Value::Null);
        } else {
            // FIXME set parameter index as 0, need to be fixed
            result = ExportedFunctionObject::createExportedFunction(state, wasm_ref_as_func(value.of.ref), 0);
        }
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    }

    return result;
}

wasm_valkind_t WASMValueConverter::toValueType(ExecutionState& state, const Value& valTypeValue)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!valTypeValue.isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    if (valTypeValue.asString()->equals(strings->i32.string())) {
        return WASM_I32;
    } else if (valTypeValue.asString()->equals(strings->i64.string())) {
        return WASM_I64;
    } else if (valTypeValue.asString()->equals(strings->f32.string())) {
        return WASM_F32;
    } else if (valTypeValue.asString()->equals(strings->f64.string())) {
        return WASM_F64;
    } else if (valTypeValue.asString()->equals(strings->v128.string())) {
        return WASM_V128;
    } else if (valTypeValue.asString()->equals(strings->anyfunc.string())) {
        return WASM_FUNCREF;
    } else if (valTypeValue.asString()->equals(strings->externref.string())) {
        return WASM_ANYREF;
    }

    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    return WASM_ANYREF;
}

wasm_val_t WASMValueConverter::toWebAssemblyValue(ExecutionState& state, const Value& value, wasm_valkind_t type)
{
    WASM_INIT_VAL(result);
    switch (type) {
    case WASM_I32: {
        int32_t val = value.toInt32(state);
        WASM_I32_VAL(result, val);
        break;
    }
    case WASM_F32: {
        double num = value.toNumber(state);
        float32_t val;
        if (std::isnan(num)) {
            val = std::numeric_limits<float>::quiet_NaN();
        } else {
            // FIXME Let f32 be ? ToNumber(v) rounded to the nearest representable value using IEEE 754-2019 round to nearest, ties to even mode.
            val = static_cast<float>(num);
        }
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
    case WASM_ANYREF: {
        if (value.isNull()) {
            // Return ref.null externref.
            WASM_REF_VAL(result, nullptr);
        } else {
            // Let map be the surrounding agent's associated extern value cache.
            wasm_ref_t* val = state.context()->wasmCache()->findRefByValue(value);
            // If a extern address externaddr exists such that map[externaddr] is the same as v,
            // Return ref.extern externaddr.
            if (!val) {
                // Set map[externaddr] to v.
                val = state.context()->wasmCache()->insertRefByValue(value);
            }
            // Return ref.extern externaddr.
            WASM_REF_VAL(result, val);
        }
        break;
    }
    case WASM_FUNCREF: {
        if (value.isNull()) {
            // Return ref.null funcref.
            WASM_FUNC_VAL(result, nullptr);
        } else if (value.isObject() && value.asObject()->isExportedFunctionObject()) {
            // Let funcaddr be the value of v’s [[FunctionAddress]] internal slot.
            // Return ref.func funcaddr.
            wasm_ref_t* funcaddr = wasm_func_as_ref(value.asObject()->asExportedFunctionObject()->function());
            WASM_FUNC_VAL(result, funcaddr);
        } else {
            // Throw a TypeError
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
        }
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
