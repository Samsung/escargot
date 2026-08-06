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

// Object/property/array-related js_native_api.h surface: property
// get/set/has/delete (both keyed and indexed variants), own-property-name
// enumeration, prototype access, freeze/seal, array creation/inspection, and
// instanceof.

#include "NapiTypes.h"

#include <cstring>

namespace Escargot {
namespace Napi {

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_get_property(napi_env env, napi_value object, napi_value key, napi_value* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyKey = FromNapi(key);
    ExecutionStateRef* state = env->executionState;

    // ObjectRef::get can invoke a user getter (or a Proxy `get` trap), either
    // of which may throw a raw C++ exception that must not cross this
    // function's own stack frame - see napi_call_function's comment on the
    // same Evaluator::execute pattern.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* key) -> ValueRef* {
            return obj->get(state, key);
        },
        obj, propertyKey);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_set_property(napi_env env, napi_value object, napi_value key, napi_value value)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyKey = FromNapi(key);
    ValueRef* propertyValue = FromNapi(value);
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* key, ValueRef* value) -> ValueRef* {
            obj->set(state, key, value);
            return ValueRef::createUndefined();
        },
        obj, propertyKey, propertyValue);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_has_property(napi_env env, napi_value object, napi_value key, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyKey = FromNapi(key);
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* key) -> ValueRef* {
            return ValueRef::create(obj->hasProperty(state, key));
        },
        obj, propertyKey);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = evalResult.result->asBoolean();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_delete_property(napi_env env, napi_value object, napi_value key, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyKey = FromNapi(key);
    ExecutionStateRef* state = env->executionState;

    // deleteOwnProperty (ECMA-262's [[Delete]]), not deleteProperty: the
    // latter (ObjectRef::deleteProperty, see its own deletePropertyOperation
    // helper in EscargotPublic.cpp) additionally pre-checks
    // hasOwnProperty(), then falls back to walking the prototype chain if
    // the key isn't found - neither of which the JS `delete` operator /
    // real Node-API's napi_delete_property ever do (delete is always an
    // own-property-only [[Delete]] call, no prototype walk, and no separate
    // existence pre-check trap on an exotic object). That pre-check trap
    // firing (and, in this test, throwing) before the *actual* delete ever
    // happens meant a Proxy's own `deleteProperty` trap - which
    // ProxyObject::deleteOwnProperty correctly invokes - was never reached
    // at all (found via test_object/test_exceptions.js, whose Proxy's
    // `deleteProperty` trap always throws but was never observed to be
    // called).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* key) -> ValueRef* {
            return ValueRef::create(obj->deleteOwnProperty(state, key));
        },
        obj, propertyKey);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (result != nullptr) {
        *result = evalResult.result->asBoolean();
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_has_own_property(napi_env env, napi_value object, napi_value key, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyKey = FromNapi(key);
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* key) -> ValueRef* {
            return ValueRef::create(obj->hasOwnProperty(state, key));
        },
        obj, propertyKey);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = evalResult.result->asBoolean();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_named_property(napi_env env, napi_value object, const char* utf8name, napi_value* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    StringRef* propertyName = StringRef::createFromUTF8(utf8name, strlen(utf8name));
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, StringRef* name) -> ValueRef* {
            return obj->get(state, name);
        },
        obj, propertyName);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_has_named_property(napi_env env, napi_value object, const char* utf8name, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    StringRef* propertyName = StringRef::createFromUTF8(utf8name, strlen(utf8name));
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, StringRef* name) -> ValueRef* {
            return ValueRef::create(obj->hasProperty(state, name));
        },
        obj, propertyName);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = evalResult.result->asBoolean();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_element(napi_env env, napi_value object, uint32_t index, napi_value* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, uint32_t index) -> ValueRef* {
            return obj->getIndexedProperty(state, ValueRef::create(index));
        },
        obj, index);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_set_element(napi_env env, napi_value object, uint32_t index, napi_value value)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ValueRef* propertyValue = FromNapi(value);
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, uint32_t index, ValueRef* value) -> ValueRef* {
            obj->setIndexedProperty(state, ValueRef::create(index), value);
            return ValueRef::createUndefined();
        },
        obj, index, propertyValue);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_has_element(napi_env env, napi_value object, uint32_t index, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, uint32_t index) -> ValueRef* {
            return ValueRef::create(obj->hasProperty(state, ValueRef::create(index)));
        },
        obj, index);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = evalResult.result->asBoolean();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_delete_element(napi_env env, napi_value object, uint32_t index, bool* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    // see napi_delete_property's identical deleteOwnProperty-not-
    // deleteProperty reasoning just above.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, uint32_t index) -> ValueRef* {
            return ValueRef::create(obj->deleteOwnProperty(state, ValueRef::create(index)));
        },
        obj, index);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (result != nullptr) {
        *result = evalResult.result->asBoolean();
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_property_names(napi_env env, napi_value object, napi_value* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    // Object.keys()-like: own, enumerable, string-keyed property names only.
    // Deliberately built on ownPropertyKeys()/getOwnPropertyDescriptor()
    // rather than enumerateObjectOwnProperties(): ProxyObject::enumeration()
    // (ProxyObject.cpp) is explicitly documented there as *not* invoking the
    // Proxy's own ownKeys trap at all (it just walks the underlying target
    // directly), unlike ownPropertyKeys()/getOwnProperty(), which are
    // properly overridden to invoke the real traps - see
    // napi_get_all_property_names's identical rationale (NapiExtras.cpp),
    // found via the same test (test_object/test_exceptions.js).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
            ValueVectorRef* names = ValueVectorRef::create();
            ValueVectorRef* ownKeys = obj->ownPropertyKeys(state);
            for (size_t i = 0; i < ownKeys->size(); i++) {
                ValueRef* propertyName = ownKeys->at(i);
                if (!propertyName->isString()) {
                    continue;
                }
                ValueRef* descriptor = obj->getOwnPropertyDescriptor(state, propertyName);
                if (descriptor->isUndefined()) {
                    continue;
                }
                StringRef* enumerableKey = StringRef::createFromASCII("enumerable");
                if (descriptor->asObject()->get(state, enumerableKey)->toBoolean(state)) {
                    names->pushBack(propertyName);
                }
            }
            return ArrayObjectRef::create(state, names);
        },
        obj);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_prototype(napi_env env, napi_value object, napi_value* result)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    // getPrototype can invoke a Proxy's getPrototypeOf trap.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
            return obj->getPrototype(state);
        },
        obj);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_object_freeze(napi_env env, napi_value object)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    // ObjectRef::setIntegrityLevel(state, false) matches
    // Object::setIntegrityLevel(state, O, false), which is exactly what
    // Object.freeze's builtin uses (BuiltinObject.cpp's builtinObjectFreeze) -
    // isSealed=false there means "frozen", not "not sealed".
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
            return ValueRef::create(obj->setIntegrityLevel(state, false));
        },
        obj);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (!evalResult.result->asBoolean()) {
        return SetLastError(env, napi_generic_failure);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_object_seal(napi_env env, napi_value object)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    ExecutionStateRef* state = env->executionState;

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
            return ValueRef::create(obj->setIntegrityLevel(state, true));
        },
        obj);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (!evalResult.result->asBoolean()) {
        return SetLastError(env, napi_generic_failure);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_array(napi_env env, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ArrayObjectRef::create(env->executionState));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_array_with_length(napi_env env, size_t length, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // A JS array's length is always a uint32 (ECMA-262), so truncate to
    // that range first instead of widening `length` (size_t) directly to
    // uint64_t. Without this, a caller that derives `length` from
    // napi_get_value_int32() and passes the maximum valid array length
    // (2^32-1) - which truncates to int32_t -1, then widens back to a huge
    // size_t/uint64_t when passed here - would spuriously hit
    // ArrayObjectRef::create's `size > 2^32-1` check and throw a RangeError,
    // instead of producing the (perfectly valid) 2^32-1-length array the
    // caller asked for (see test_array/test.js's `NewWithLength(4294967295)`).
    *result = ToNapi(ArrayObjectRef::create(env->executionState, static_cast<uint64_t>(static_cast<uint32_t>(length))));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_array_length(napi_env env, napi_value value, uint32_t* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ObjectRef* obj = FromNapi(value)->asObject();
    ExecutionStateRef* state = env->executionState;

    // ObjectRef::length() reads the "length" property (ToLength(Get(obj,
    // "length"))), which - like any other property get - could run through a
    // user-defined accessor.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
            return ValueRef::create(static_cast<double>(obj->length(state)));
        },
        obj);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = static_cast<uint32_t>(evalResult.result->asNumber());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_array(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // Escargot's isArrayObject() is a native type check on the value itself
    // (never runs user code), unlike the full ECMA-262 IsArray abstract
    // operation which additionally recurses through a Proxy's target chain -
    // there is no public API exposing that recursive form, so a
    // Proxy-wrapped array is not detected here. Every other value in this
    // file's API can throw crossing the C ABI; this one structurally cannot,
    // so it does not need the Evaluator::execute wrapper.
    *result = FromNapi(value)->isArrayObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_instanceof(napi_env env, napi_value object, napi_value constructor, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* obj = FromNapi(object);
    ValueRef* ctor = FromNapi(constructor);
    ExecutionStateRef* state = env->executionState;

    // instanceOf can throw (e.g. `constructor` is not callable, or a custom
    // Symbol.hasInstance implementation throws).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* obj, ValueRef* ctor) -> ValueRef* {
            return ValueRef::create(obj->instanceOf(state, ctor));
        },
        obj, ctor);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = evalResult.result->asBoolean();
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
