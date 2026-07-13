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
#include <vector>

namespace Escargot {
namespace Napi {

static ValueRef* NapiCallbackTrampoline(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall)
{
    FunctionObjectRef* callee = state->resolveCallee().value();
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callee->extraData());
    napi_env env = callbackData->env;

    ExecutionStateRef* previousState = env->executionState;
    env->executionState = state;

    napi_callback_info__ cbinfo{ argc, argv, thisValue, callbackData->data, nullptr };
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

// `this` for a constructor call is always a fresh, engine-created object here
// (see FunctionTemplateRef::NativeFunctionPointer's contract), unlike
// NapiCallbackTrampoline/FunctionObjectRef::NativeFunctionPointer above, so
// napi_get_new_target has a real value to report and napi_wrap can attach
// native data to `this` the same way a user's constructor callback expects.
static ValueRef* NapiClassConstructorTrampoline(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget)
{
    FunctionObjectRef* callee = state->resolveCallee().value();
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callee->extraData());
    napi_env env = callbackData->env;

    ExecutionStateRef* previousState = env->executionState;
    env->executionState = state;

    napi_callback_info__ cbinfo{ argc, argv, thisValue, callbackData->data, newTarget.hasValue() ? OptionalRef<ValueRef>(newTarget.value()) : nullptr };
    napi_value result = callbackData->callback(env, reinterpret_cast<napi_callback_info>(&cbinfo));

    env->executionState = previousState;

    if (env->pendingException.hasValue()) {
        ValueRef* exceptionValue = env->pendingException.value();
        env->pendingException = nullptr;
        state->throwException(exceptionValue); // does not return
    }

    if (newTarget.hasValue()) {
        // ES construct semantics: an explicit object return overrides the
        // pre-created `this` (rare in practice; N-API constructors usually
        // just `return _this`)
        OptionalRef<ValueRef> returnValue = FromNapi(result); // FromNapi(nullptr) is an empty OptionalRef
        return (returnValue.hasValue() && returnValue->isObject()) ? returnValue.value() : thisValue;
    }
    return result ? FromNapi(result) : ValueRef::createUndefined();
}

struct WrapFinalizeData {
    napi_env env;
    node_api_basic_finalize finalizeCb;
    void* nativeObject;
    void* finalizeHint;
};

static void NapiWrapFinalizer(void* self, void* data)
{
    WrapFinalizeData* wrapData = reinterpret_cast<WrapFinalizeData*>(data);
    wrapData->finalizeCb(wrapData->env, wrapData->nativeObject, wrapData->finalizeHint);
    delete wrapData;
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

ESCARGOT_NAPI_EXPORT napi_status napi_get_global(napi_env env, napi_value* result)
{
    *result = ToNapi(env->context()->globalObject());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_object(napi_env env, napi_value* result)
{
    *result = ToNapi(ObjectRef::create(env->executionState));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_string_utf8(napi_env env, const char* str, size_t length, napi_value* result)
{
    size_t byteLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    *result = ToNapi(StringRef::createFromUTF8(str, byteLength));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_set_named_property(napi_env env, napi_value object, const char* utf8name, napi_value value)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    obj->set(env->executionState, StringRef::createFromUTF8(utf8name, strlen(utf8name)), FromNapi(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_call_function(napi_env env, napi_value recv, napi_value func, size_t argc, const napi_value* argv, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    ValueRef* fn = FromNapi(func);

    std::vector<ValueRef*> args(argc);
    for (size_t i = 0; i < argc; i++) {
        args[i] = FromNapi(argv[i]);
    }

    ValueRef* callResult = fn->call(state, FromNapi(recv), argc, args.data());
    if (result != nullptr) {
        *result = ToNapi(callResult);
    }
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

// shared by napi_define_properties (applies to the target object itself) and
// napi_define_class (applies to the constructor's .prototype, or to the
// constructor itself for napi_static members)
static void ApplyPropertyDescriptor(napi_env env, ObjectRef* target, const napi_property_descriptor& p)
{
    ExecutionStateRef* state = env->executionState;

    ValueRef* propertyName = p.utf8name ? static_cast<ValueRef*>(StringRef::createFromUTF8(p.utf8name, strlen(p.utf8name))) : FromNapi(p.name);
    size_t nameLength = p.utf8name ? strlen(p.utf8name) : NAPI_AUTO_LENGTH;

    bool isWritable = (p.attributes & napi_writable) != 0;
    bool isEnumerable = (p.attributes & napi_enumerable) != 0;
    bool isConfigurable = (p.attributes & napi_configurable) != 0;

    if (p.getter != nullptr || p.setter != nullptr) {
        ValueRef* getter = ValueRef::createUndefined();
        if (p.getter != nullptr) {
            napi_value fn;
            napi_create_function(env, p.utf8name, nameLength, p.getter, p.data, &fn);
            getter = FromNapi(fn);
        }
        OptionalRef<ValueRef> setter;
        if (p.setter != nullptr) {
            napi_value fn;
            napi_create_function(env, p.utf8name, nameLength, p.setter, p.data, &fn);
            setter = FromNapi(fn);
        }
        ObjectRef::PresentAttribute attr = static_cast<ObjectRef::PresentAttribute>(
            (isEnumerable ? ObjectRef::EnumerablePresent : ObjectRef::NonEnumerablePresent) | (isConfigurable ? ObjectRef::ConfigurablePresent : ObjectRef::NonConfigurablePresent));
        target->defineAccessorProperty(state, propertyName, ObjectRef::AccessorPropertyDescriptor(getter, setter, attr));
        return;
    }

    ValueRef* value;
    if (p.method) {
        napi_value fn;
        napi_create_function(env, p.utf8name, nameLength, p.method, p.data, &fn);
        value = FromNapi(fn);
    } else if (p.value) {
        value = FromNapi(p.value);
    } else {
        return;
    }

    target->defineDataProperty(state, propertyName, value, isWritable, isEnumerable, isConfigurable);
}

ESCARGOT_NAPI_EXPORT napi_status napi_define_properties(napi_env env, napi_value object, size_t property_count, const napi_property_descriptor* properties)
{
    ObjectRef* obj = FromNapi(object)->asObject();

    for (size_t i = 0; i < property_count; i++) {
        ApplyPropertyDescriptor(env, obj, properties[i]);
    }

    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_define_class(napi_env env, const char* utf8name, size_t length, napi_callback constructor, void* data, size_t property_count, const napi_property_descriptor* properties, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    ContextRef* context = env->context();

    size_t nameLen = (utf8name == nullptr) ? 0 : ((length == NAPI_AUTO_LENGTH) ? strlen(utf8name) : length);
    AtomicStringRef* name = AtomicStringRef::create(context, utf8name ? utf8name : "", nameLen);

    FunctionTemplateRef* tpl = FunctionTemplateRef::create(name, 0, true, true, NapiClassConstructorTrampoline);
    ObjectRef* consObj = tpl->instantiate(context);
    FunctionObjectRef* cons = consObj->asFunctionObject();

    CallbackData* callbackData = new CallbackData();
    callbackData->env = env;
    callbackData->callback = constructor;
    callbackData->data = data;
    cons->setExtraData(callbackData);
    Memory::gcRegisterFinalizer(cons, NapiFunctionCallbackDataFinalizer, callbackData);

    ObjectRef* proto = cons->getFunctionPrototype(state)->asObject();

    for (size_t i = 0; i < property_count; i++) {
        const napi_property_descriptor& p = properties[i];
        ObjectRef* target = (p.attributes & napi_static) ? static_cast<ObjectRef*>(cons) : proto;
        ApplyPropertyDescriptor(env, target, p);
    }

    *result = ToNapi(cons);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_wrap(napi_env env, napi_value js_object, void* native_object, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_ref* result)
{
    ObjectRef* obj = FromNapi(js_object)->asObject();
    obj->setExtraData(native_object);

    if (finalize_cb != nullptr) {
        WrapFinalizeData* wrapData = new WrapFinalizeData();
        wrapData->env = env;
        wrapData->finalizeCb = finalize_cb;
        wrapData->nativeObject = native_object;
        wrapData->finalizeHint = finalize_hint;
        Memory::gcRegisterFinalizer(obj, NapiWrapFinalizer, wrapData);
    }

    if (result != nullptr) {
        napi_ref__* ref = new napi_ref__();
        ref->value = obj;
        ref->refcount = 0;
        *result = ref;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_unwrap(napi_env env, napi_value js_object, void** result)
{
    *result = FromNapi(js_object)->asObject()->extraData();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_remove_wrap(napi_env env, napi_value js_object, void** result)
{
    ObjectRef* obj = FromNapi(js_object)->asObject();
    if (result != nullptr) {
        *result = obj->extraData();
    }
    obj->setExtraData(nullptr);
    // does not unregister the napi_wrap finalizer yet (would need the
    // WrapFinalizeData pointer stashed somewhere retrievable); harmless for
    // every TC that currently exercises this PoC, since none of them expect
    // the finalizer to be suppressed after remove_wrap
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_reference(napi_env env, napi_value value, uint32_t initial_refcount, napi_ref* result)
{
    napi_ref__* ref = new napi_ref__();
    ref->value = FromNapi(value);
    ref->refcount = initial_refcount;
    for (uint32_t i = 0; i < initial_refcount; i++) {
        env->napiEnv->persistentValueRefMap()->add(ref->value.value());
    }
    *result = ref;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_delete_reference(node_api_basic_env env, napi_ref ref)
{
    if (ref->value.hasValue()) {
        for (uint32_t i = 0; i < ref->refcount; i++) {
            env->napiEnv->persistentValueRefMap()->remove(ref->value.value());
        }
    }
    delete ref;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_reference_value(napi_env env, napi_ref ref, napi_value* result)
{
    // a weak (refcount == 0) ref whose target has already been collected
    // would dangle here; not tracked yet (see napi_ref__'s doc comment)
    *result = ref->value.hasValue() ? ToNapi(ref->value.value()) : nullptr;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_new_target(napi_env env, napi_callback_info cbinfo, napi_value* result)
{
    napi_callback_info__* info = reinterpret_cast<napi_callback_info__*>(cbinfo);
    *result = info->newTarget.hasValue() ? ToNapi(info->newTarget.value()) : nullptr;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_new_instance(napi_env env, napi_value constructor, size_t argc, const napi_value* argv, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;

    std::vector<ValueRef*> args(argc);
    for (size_t i = 0; i < argc; i++) {
        args[i] = FromNapi(argv[i]);
    }

    *result = ToNapi(FromNapi(constructor)->construct(state, argc, args.data()));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result)
{
    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_boolean(napi_env env, bool value, napi_value* result)
{
    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_set_instance_data(node_api_basic_env env, void* data, napi_finalize finalize_cb, void* finalize_hint)
{
    env->instanceData = data;
    env->instanceDataFinalizer = finalize_cb;
    env->instanceDataFinalizeHint = finalize_hint;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_instance_data(node_api_basic_env env, void** data)
{
    *data = env->instanceData;
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
