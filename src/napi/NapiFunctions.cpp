#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

// Implements just enough of js_native_api.h to run
// test/napi-tc/test/js-native-api/2_function_arguments. This is an early
// slice of a larger PoC function list, not the full surface.

#include "NapiTypes.h"

#include <algorithm>
#include <cstring>

namespace Escargot {
namespace Napi {

static ValueRef* NapiCallbackTrampoline(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    FunctionObjectRef* callee = state->resolveCallee().value();
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callee->extraData());
    napi_env env = callbackData->env;

    ExecutionStateRef* previousState = env->executionState;
    env->executionState = state;

    napi_callback_info__ cbinfo{ argc, argv, thisValue, callbackData->data };
    napi_value result = callbackData->callback(env, reinterpret_cast<napi_callback_info>(&cbinfo));

    env->executionState = previousState;

    if (env->pendingException.hasValue()) {
        ValueRef* exceptionValue = env->pendingException.value();
        env->pendingException = nullptr;
        state->throwException(exceptionValue); // does not return
    }

    return result ? FromNapi(result) : ValueRef::createUndefined();
}

static void NapiFunctionCallbackDataFinalizer(void* self, void* data)
{
    delete reinterpret_cast<CallbackData*>(data);
}

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_get_last_error_info(node_api_basic_env env, const napi_extended_error_info** result)
{
    env->lastErrorInfo.error_message = env->lastErrorMessage.empty() ? nullptr : env->lastErrorMessage.c_str();
    env->lastErrorInfo.engine_reserved = nullptr;
    env->lastErrorInfo.engine_error_code = 0;
    env->lastErrorInfo.error_code = napi_ok;
    *result = &env->lastErrorInfo;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_undefined(napi_env env, napi_value* result)
{
    *result = ToNapi(ValueRef::createUndefined());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_double(napi_env env, double value, napi_value* result)
{
    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
{
    ValueRef* v = FromNapi(value);
    if (v->isUndefined()) {
        *result = napi_undefined;
    } else if (v->isNull()) {
        *result = napi_null;
    } else if (v->isBoolean()) {
        *result = napi_boolean;
    } else if (v->isNumber()) {
        *result = napi_number;
    } else if (v->isString()) {
        *result = napi_string;
    } else if (v->isSymbol()) {
        *result = napi_symbol;
    } else if (v->isCallable()) {
        *result = napi_function;
    } else if (v->isBigInt()) {
        *result = napi_bigint;
    } else {
        *result = napi_object;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_double(napi_env env, napi_value value, double* result)
{
    ValueRef* v = FromNapi(value);
    if (!v->isNumber()) {
        return napi_number_expected;
    }
    *result = v->asNumber();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_function(napi_env env, const char* utf8name, size_t length, napi_callback cb, void* data, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    ContextRef* context = env->context();

    size_t nameLen = (utf8name == nullptr) ? 0 : ((length == NAPI_AUTO_LENGTH) ? strlen(utf8name) : length);
    AtomicStringRef* name = AtomicStringRef::create(context, utf8name ? utf8name : "", nameLen);

    FunctionObjectRef* fn = FunctionObjectRef::create(state, FunctionObjectRef::NativeFunctionInfo(name, NapiCallbackTrampoline, 0, true, false));

    CallbackData* callbackData = new CallbackData();
    callbackData->env = env;
    callbackData->callback = cb;
    callbackData->data = data;
    fn->setExtraData(callbackData);
    // free callbackData once the FunctionObjectRef itself is collected, instead of leaking it
    Memory::gcRegisterFinalizer(fn, NapiFunctionCallbackDataFinalizer, callbackData);

    *result = ToNapi(fn);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_define_properties(napi_env env, napi_value object, size_t property_count, const napi_property_descriptor* properties)
{
    ExecutionStateRef* state = env->executionState;
    ObjectRef* obj = FromNapi(object)->asObject();

    for (size_t i = 0; i < property_count; i++) {
        const napi_property_descriptor& p = properties[i];

        ValueRef* propertyName = p.utf8name ? static_cast<ValueRef*>(StringRef::createFromUTF8(p.utf8name, strlen(p.utf8name))) : FromNapi(p.name);

        bool isWritable = (p.attributes & napi_writable) != 0;
        bool isEnumerable = (p.attributes & napi_enumerable) != 0;
        bool isConfigurable = (p.attributes & napi_configurable) != 0;

        ValueRef* value;
        if (p.method) {
            napi_value fn;
            napi_create_function(env, p.utf8name, p.utf8name ? strlen(p.utf8name) : NAPI_AUTO_LENGTH, p.method, p.data, &fn);
            value = FromNapi(fn);
        } else if (p.value) {
            value = FromNapi(p.value);
        } else {
            // getter/setter-backed properties are not implemented yet (not needed by the 2_function_arguments TC)
            continue;
        }

        obj->defineDataProperty(state, propertyName, value, isWritable, isEnumerable, isConfigurable);
    }

    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_cb_info(napi_env env, napi_callback_info cbinfo, size_t* argc, napi_value* argv, napi_value* this_arg, void** data)
{
    napi_callback_info__* info = reinterpret_cast<napi_callback_info__*>(cbinfo);

    if (argv != nullptr && argc != nullptr) {
        size_t capacity = *argc;
        size_t count = std::min(capacity, info->argc);
        for (size_t i = 0; i < count; i++) {
            argv[i] = ToNapi(info->argv[i]);
        }
        for (size_t i = count; i < capacity; i++) {
            argv[i] = ToNapi(ValueRef::createUndefined());
        }
    }
    if (argc != nullptr) {
        *argc = info->argc;
    }
    if (this_arg != nullptr) {
        *this_arg = ToNapi(info->thisValue);
    }
    if (data != nullptr) {
        *data = info->data;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_throw(napi_env env, napi_value error)
{
    env->pendingException = FromNapi(error);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_throw_error(napi_env env, const char* code, const char* msg)
{
    ExecutionStateRef* state = env->executionState;
    StringRef* message = StringRef::createFromUTF8(msg, strlen(msg));
    ErrorObjectRef* error = ErrorObjectRef::create(state, ErrorObjectRef::Code::None, message);
    if (code) {
        error->set(state, StringRef::createFromASCII("code"), StringRef::createFromUTF8(code, strlen(code)));
    }
    env->pendingException = error;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_exception_pending(napi_env env, bool* result)
{
    *result = env->pendingException.hasValue();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_and_clear_last_exception(napi_env env, napi_value* result)
{
    if (env->pendingException.hasValue()) {
        *result = ToNapi(env->pendingException.value());
        env->pendingException = nullptr;
    } else {
        *result = ToNapi(ValueRef::createUndefined());
    }
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
