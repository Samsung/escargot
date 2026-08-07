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

// Functions missing from the original PoC slice, following the same
// conventions NapiFunctions.cpp establishes (napi_value <-> ValueRef*
// punning via ToNapi/FromNapi, Evaluator::execute-wrapped exception
// boundaries, SetLastError-routed error returns). Two kinds of gaps live
// here:
//  - functions declared in the vendored js_native_api.h/node_api.h but never
//    implemented (node_api_symbol_for, node_api_create_property_key_*,
//    napi_get_all_property_names, napi_add_finalizer);
//  - functions the vendored headers didn't even declare, patched in
//    alongside their implementation here because a newer vendored test
//    addon calls them (node_api_create_object_with_properties,
//    node_api_is_sharedarraybuffer, node_api_set_prototype,
//    node_api_post_finalizer - see the header comments at each declaration).

#include "NapiTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace Escargot {
namespace Napi {

namespace {

// napi_add_finalizer: a GC finalizer that just invokes the user's
// finalize_cb, deliberately NOT touching extraData()/the wrap-finalizer-data
// side table the way napi_wrap's WrapFinalizeData/NapiWrapFinalizer
// (NapiFunctions.cpp) do - napi_add_finalizer must be attachable to an
// object independently of (and without disturbing) any existing napi_wrap on
// the same object, so it gets its own, simpler finalizer plumbing instead of
// reusing napi_wrap's.
struct AddFinalizerData {
    node_api_basic_env env;
    node_api_basic_finalize finalizeCb;
    void* nativeData;
    void* finalizeHint;
};

static void NapiAddFinalizerFinalizer(void* self, void* data)
{
    AddFinalizerData* finalizeData = reinterpret_cast<AddFinalizerData*>(data);
    // force any other still-weak napi_ref to this same object to already
    // read as cleared before this finalizer runs - see
    // NapiEnv::clearWeakRefTargets's own comment (NapiEnv.h) and
    // NapiWrapFinalizer's identical call (NapiFunctions.cpp).
    finalizeData->env->napiEnv->clearWeakRefTargets(self);
    // see NapiEnv::isInGCUnsafeFinalizer's own comment (NapiEnv.h) and
    // NapiWrapFinalizer's identical bracketing (NapiFunctions.cpp):
    // finalizeCb only ever received a node_api_basic_env, so calling
    // anything that needs to allocate/run JS from it is a contract violation
    // this project surfaces as the same fatal error real Node-API would -
    // found via test_finalizer/test_fatal_finalize.js's own
    // addFinalizerFailOnJS, which uses napi_add_finalizer (not napi_wrap) for
    // exactly this.
    finalizeData->env->napiEnv->setInGCUnsafeFinalizer(true);
    finalizeData->finalizeCb(finalizeData->env, finalizeData->nativeData, finalizeData->finalizeHint);
    finalizeData->env->napiEnv->setInGCUnsafeFinalizer(false);
    delete finalizeData;
}

// napi_get_all_property_names' napi_key_keep_numbers conversion: a property
// key that is a canonical array-index string (ECMA-262 6.1.7's "array
// index": no leading zero other than "0" itself, value in
// [0, 2^32-2]) is reported back as an actual Number instead of the String
// every own property key otherwise naturally is - matching V8's own
// behavior for this option (napi_key_numbers_to_strings, the other option,
// requires no special handling: it just leaves every key as the String it
// already is).
bool TryParseArrayIndexString(StringRef* str, double* outValue)
{
    size_t len = str->length();
    if (len == 0 || len > 10) { // 2^32-1 is 10 digits; anything longer can't fit
        return false;
    }

    std::string digits;
    digits.reserve(len);
    for (size_t i = 0; i < len; i++) {
        char16_t c = str->charAt(i);
        if (c < u'0' || c > u'9') {
            return false;
        }
        digits.push_back(static_cast<char>(c));
    }
    if (digits.size() > 1 && digits[0] == '0') { // "01" etc is not a canonical index
        return false;
    }

    unsigned long long parsed = strtoull(digits.c_str(), nullptr, 10);
    if (parsed > 4294967294ULL) { // max valid array index is 2^32-2
        return false;
    }
    *outValue = static_cast<double>(parsed);
    return true;
}

// reads `name` off a property descriptor object (the {value, writable,
// enumerable, configurable}/{get, set, enumerable, configurable} shape
// ObjectRef::getOwnPropertyDescriptor returns) as a plain bool, defaulting
// to `defaultValue` if the descriptor doesn't have that key at all - this
// happens legitimately for "writable" on an accessor-property descriptor
// (which has no such concept), which napi_get_all_property_names then
// treats as "not writable" for its napi_key_writable filter, same as a
// getter-only property would report via any other API.
bool ReadDescriptorBool(ExecutionStateRef* state, ObjectRef* descriptor, const char* name, bool defaultValue)
{
    StringRef* key = StringRef::createFromASCII(name, strlen(name));
    if (!descriptor->hasOwnProperty(state, key)) {
        return defaultValue;
    }
    return descriptor->get(state, key)->toBoolean(state);
}

} // namespace

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status node_api_symbol_for(napi_env env, const char* utf8description, size_t length, napi_value* result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    size_t descLen = (length == NAPI_AUTO_LENGTH) ? (utf8description ? strlen(utf8description) : 0) : length;
    StringRef* desc = StringRef::createFromUTF8(utf8description ? utf8description : "", descLen);

    // SymbolRef::fromGlobalSymbolRegistry (EscargotPublic.h) already *is*
    // Symbol.for's global-registry lookup/insert, keyed process-wide off the
    // owning VMInstanceRef (matching the spec: the registry is a per-realm-
    // group, not per-Context, table) - no separate NapiEnv-level map needed.
    *result = ToNapi(SymbolRef::fromGlobalSymbolRegistry(env->napiEnv->vmInstance(), desc));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_property_key_latin1(napi_env env, const char* str, size_t length, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // a Latin1 string is already a valid property key (ToPropertyKey on any
    // String is the identity), so this is exactly napi_create_string_latin1
    // (NapiValue.cpp) under a name that documents the intended use.
    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    *result = ToNapi(StringRef::createFromLatin1(reinterpret_cast<const unsigned char*>(str), stringLength));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_property_key_utf8(napi_env env, const char* str, size_t length, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    size_t byteLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    *result = ToNapi(StringRef::createFromUTF8(str, byteLength));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_property_key_utf16(napi_env env, const char16_t* str, size_t length, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // same reasoning as node_api_create_property_key_latin1 above, mirroring
    // napi_create_string_utf16 (NapiValue.cpp).
    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? std::char_traits<char16_t>::length(str) : length;
    *result = ToNapi(StringRef::createFromUTF16(str, stringLength));
    return napi_ok;
}

// Also declared (js_native_api.h) but unimplemented - dlopen'd by
// test_string's addon right alongside node_api_create_property_key_utf16
// above. StringRef::createExternalFrom* (EscargotPublic.h) has no deleter
// callback parameter of its own, unlike e.g. BackingStoreRef's - but since
// the returned StringRef is itself an ordinary GC-heap pointer, a finalizer
// can still be attached to *it* directly via Memory::gcRegisterFinalizer,
// exactly as if it were an object (see napi_wrap/napi_create_external's
// identical pattern - NapiFunctions.cpp/NapiArrayBuffer.cpp), invoking
// finalize_callback for the caller's original buffer once the wrapping
// string is collected. This actually avoids copying `str` (`*copied =
// false`), unlike this milestone's earlier always-copy shortcut: some
// addons - e.g. test_string/test_string.c's create_external_latin1/
// create_external_utf16 helpers - specifically assert `copied` comes back
// false and treat a copy as a test failure.
struct ExternalStringFinalizeData {
    napi_env env;
    node_api_basic_finalize finalizeCb;
    void* nativeData;
    void* finalizeHint;
};

static void NapiExternalStringFinalizer(void* self, void* data)
{
    ExternalStringFinalizeData* finalizeData = reinterpret_cast<ExternalStringFinalizeData*>(data);
    finalizeData->finalizeCb(finalizeData->env, finalizeData->nativeData, finalizeData->finalizeHint);
    delete finalizeData;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_external_string_latin1(napi_env env, char* str, size_t length, node_api_basic_finalize finalize_callback, void* finalize_hint, napi_value* result, bool* copied)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    StringRef* strRef = StringRef::createExternalFromLatin1(reinterpret_cast<const unsigned char*>(str), stringLength);
    *result = ToNapi(strRef);
    if (copied != nullptr) {
        *copied = false;
    }
    if (finalize_callback != nullptr) {
        ExternalStringFinalizeData* finalizeData = new ExternalStringFinalizeData();
        finalizeData->env = env;
        finalizeData->finalizeCb = finalize_callback;
        finalizeData->nativeData = str;
        finalizeData->finalizeHint = finalize_hint;
        Memory::gcRegisterFinalizer(strRef, NapiExternalStringFinalizer, finalizeData);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_external_string_utf16(napi_env env, char16_t* str, size_t length, node_api_basic_finalize finalize_callback, void* finalize_hint, napi_value* result, bool* copied)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    size_t stringLength = (length == NAPI_AUTO_LENGTH) ? std::char_traits<char16_t>::length(str) : length;
    StringRef* strRef = StringRef::createExternalFromUTF16(str, stringLength);
    *result = ToNapi(strRef);
    if (copied != nullptr) {
        *copied = false;
    }
    if (finalize_callback != nullptr) {
        ExternalStringFinalizeData* finalizeData = new ExternalStringFinalizeData();
        finalizeData->env = env;
        finalizeData->finalizeCb = finalize_callback;
        finalizeData->nativeData = str;
        finalizeData->finalizeHint = finalize_hint;
        Memory::gcRegisterFinalizer(strRef, NapiExternalStringFinalizer, finalizeData);
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_all_property_names(napi_env env, napi_value object, napi_key_collection_mode key_mode, napi_key_filter key_filter, napi_key_conversion key_conversion, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* val_object = FromNapi(object);
    if (!val_object->isObject()) {
        return SetLastError(env, napi_object_expected);
    }
    ObjectRef* obj = val_object->asObject();
    ExecutionStateRef* state = env->executionState;

    // Walking own properties and following the prototype chain can each
    // invoke a Proxy trap (ownKeys/getOwnPropertyDescriptor/getPrototypeOf),
    // any of which may throw - same rationale as napi_get_property_names'
    // identical wrapping (NapiObject.cpp). Deliberately built on
    // ownPropertyKeys()/getOwnPropertyDescriptor() rather than
    // enumerateObjectOwnProperties(): ProxyObject::enumeration()
    // (ProxyObject.cpp) is explicitly documented there as *not* invoking the
    // Proxy's own ownKeys trap at all (it just walks the underlying target
    // directly, unlike ownPropertyKeys()/getOwnProperty(), which are
    // properly overridden to invoke the real traps) - found via
    // test_object/test_exceptions.js, whose every trap deliberately throws;
    // enumerateObjectOwnProperties on that Proxy silently produced zero
    // properties (reading straight through to the underlying, empty `{}`
    // target) instead of propagating the ownKeys trap's exception.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, napi_key_collection_mode key_mode, napi_key_filter key_filter, napi_key_conversion key_conversion) -> ValueRef* {
            bool skipStrings = (key_filter & napi_key_skip_strings) != 0;
            bool skipSymbols = (key_filter & napi_key_skip_symbols) != 0;
            unsigned attributeFilter = key_filter & (napi_key_writable | napi_key_enumerable | napi_key_configurable);

            ValueVectorRef* names = ValueVectorRef::create();

            OptionalRef<ObjectRef> current = obj;
            while (current.hasValue()) {
                ValueVectorRef* ownKeys = current.value()->ownPropertyKeys(state);
                for (size_t i = 0; i < ownKeys->size(); i++) {
                    ValueRef* propertyName = ownKeys->at(i);
                    if (propertyName->isString() && skipStrings) {
                        continue;
                    }
                    if (propertyName->isSymbol() && skipSymbols) {
                        continue;
                    }

                    if (attributeFilter != 0) {
                        ValueRef* descriptor = current.value()->getOwnPropertyDescriptor(state, propertyName);
                        if (descriptor->isUndefined()) {
                            // vanished (e.g. a trap reporting an
                            // inconsistent ownKeys/getOwnPropertyDescriptor
                            // pair) - nothing to filter on, so skip it.
                            continue;
                        }
                        ObjectRef* descriptorObj = descriptor->asObject();
                        if ((attributeFilter & napi_key_writable) && !ReadDescriptorBool(state, descriptorObj, "writable", false)) {
                            continue;
                        }
                        if ((attributeFilter & napi_key_enumerable) && !ReadDescriptorBool(state, descriptorObj, "enumerable", false)) {
                            continue;
                        }
                        if ((attributeFilter & napi_key_configurable) && !ReadDescriptorBool(state, descriptorObj, "configurable", false)) {
                            continue;
                        }
                    }

                    // a property already collected from a more-derived
                    // object in the chain shadows this (ancestor) one -
                    // matches for-in/Reflect.ownKeys-walk semantics.
                    bool alreadyCollected = false;
                    for (size_t j = 0; j < names->size(); j++) {
                        if (names->at(j)->equalsTo(state, propertyName)) {
                            alreadyCollected = true;
                            break;
                        }
                    }
                    if (alreadyCollected) {
                        continue;
                    }

                    ValueRef* keyToPush = propertyName;
                    if (key_conversion == napi_key_keep_numbers && propertyName->isString()) {
                        double numericValue;
                        if (TryParseArrayIndexString(propertyName->asString(), &numericValue)) {
                            keyToPush = ValueRef::create(numericValue);
                        }
                    }
                    names->pushBack(keyToPush);
                }

                if (key_mode == napi_key_own_only) {
                    break;
                }
                current = current.value()->getPrototypeObject(state);
            }

            return ArrayObjectRef::create(state, names);
        },
        obj, key_mode, key_filter, key_conversion);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return napi_pending_exception;
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_add_finalizer(napi_env env, napi_value js_object, void* finalize_data, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_ref* result)
{
    if (env == nullptr || js_object == nullptr || finalize_cb == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* obj = FromNapi(js_object);
    if (!obj->isObject() && !obj->isSymbol()) {
        return SetLastError(env, napi_object_expected);
    }

    AddFinalizerData* finalizeData = new AddFinalizerData();
    finalizeData->env = env;
    finalizeData->finalizeCb = finalize_cb;
    finalizeData->nativeData = finalize_data;
    finalizeData->finalizeHint = finalize_hint;
    // deliberately NOT obj->setExtraData(...) - see this file's
    // AddFinalizerData comment: napi_add_finalizer must not conflict with a
    // napi_wrap already (or later) placed on the same object.
    Memory::gcRegisterFinalizer(obj, NapiAddFinalizerFinalizer, finalizeData);

    if (result != nullptr) {
        // a weak napi_ref to the object, same as napi_wrap's `result` -
        // reuses NapiFunctions.cpp's own RegisterWeakRefFinalizerIfNeeded
        // (rather than duplicating it with a second, distinct finalizer
        // callback) precisely so this ref behaves identically to any other
        // napi_ref under napi_reference_ref/napi_delete_reference, both of
        // which look specifically for NapiWeakRefFinalizer via
        // Memory::gcUnregisterFinalizer to manage it.
        napi_ref__* ref = new napi_ref__();
        ref->value = obj;
        ref->refcount = 0;
        RegisterWeakRefFinalizerIfNeeded(env, ref);
        *result = ref;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_object_with_properties(napi_env env, napi_value prototype, const napi_value* names, const napi_value* values, size_t property_count, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;

    // setPrototype (only reached when `prototype` is non-NULL - a genuine
    // napi_value, itself possibly the JS `null` value, not merely "argument
    // omitted") can invoke a Proxy's setPrototypeOf trap and throw, so the
    // whole thing is sandboxed the same way napi_call_function is
    // (NapiFunctions.cpp).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, napi_value prototypeNapi, const napi_value* names, const napi_value* values, size_t property_count) -> ValueRef* {
            ObjectRef* obj = ObjectRef::create(state);
            if (prototypeNapi != nullptr) {
                obj->setPrototype(state, FromNapi(prototypeNapi));
            }
            for (size_t i = 0; i < property_count; i++) {
                obj->defineDataProperty(state, FromNapi(names[i]), FromNapi(values[i]), true, true, true);
            }
            return obj;
        },
        prototype, names, values, property_count);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return napi_pending_exception;
    }

    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_is_sharedarraybuffer(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    *result = FromNapi(value)->isSharedArrayBufferObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_sharedarraybuffer(napi_env env, size_t byte_length, void** data, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;

    // SharedArrayBufferObjectRef::create allocates the SharedDataBlock backing
    // store itself (unlike ArrayBufferObjectRef::create + allocateBuffer).
    SharedArrayBufferObjectRef* buf = SharedArrayBufferObjectRef::create(state, byte_length);

    if (data != nullptr) {
        *data = buf->rawBuffer();
    }
    *result = ToNapi(buf);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_set_prototype(napi_env env, napi_value object, napi_value prototype)
{
    ValueRef* val_object = FromNapi(object);
    if (!val_object->isObject()) {
        return SetLastError(env, napi_object_expected);
    }
    ObjectRef* obj = val_object->asObject();
    ValueRef* proto = FromNapi(prototype);
    ExecutionStateRef* state = env->executionState;

    // ObjectRef::setPrototype can invoke a Proxy's setPrototypeOf trap and
    // throw - same rationale as node_api_create_object_with_properties above.
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, ValueRef* proto) -> ValueRef* {
            obj->setPrototype(state, proto);
            return ValueRef::createUndefined();
        },
        obj, proto);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return napi_pending_exception;
    }
    return napi_ok;
}

// NAPI_NO_RETURN void, not napi_status - node_api.h's own declaration (there
// is no napi_env to report a status through in the first place: this is
// meant to be callable even where the engine/env state itself may already be
// broken).
ESCARGOT_NAPI_EXPORT NAPI_NO_RETURN void napi_fatal_error(const char* location, size_t location_len, const char* message, size_t message_len)
{
    std::string locationStr = (location == nullptr) ? std::string() : ((location_len == NAPI_AUTO_LENGTH) ? std::string(location) : std::string(location, location_len));
    std::string messageStr = (message == nullptr) ? std::string() : ((message_len == NAPI_AUTO_LENGTH) ? std::string(message) : std::string(message, message_len));

    if (!locationStr.empty()) {
        fprintf(stderr, "FATAL ERROR: %s %s\n", locationStr.c_str(), messageStr.c_str());
    } else {
        fprintf(stderr, "FATAL ERROR: %s\n", messageStr.c_str());
    }
    fflush(stderr);
    abort();
}

// Minimal viable behavior per this milestone's scope: report `err` to
// stderr the way an actual uncaught exception would print, then return -
// there is no process-wide 'uncaughtException' hook wired into this PoC yet
// (a separate wave handles uncaughtException support generally) for this to
// forward into instead.
ESCARGOT_NAPI_EXPORT napi_status napi_fatal_exception(napi_env env, napi_value err)
{
    if (env == nullptr || err == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // Node-API: napi_fatal_exception triggers an 'uncaughtException'. Model
    // that by making `err` the env's pending exception; the napi callback
    // trampoline (NapiCallbackTrampoline, NapiFunctions.cpp) rethrows it into
    // JS the instant the current native callback returns, so it propagates as
    // an ordinary uncaught exception - routed to any process.on(
    // 'uncaughtException') handler by the embedder/harness, or reported
    // fatally if none. Supersedes the previous stderr-only stub, which never
    // reached a handler (so test_fatal_exception's mustCall never fired).
    env->pendingException = FromNapi(err);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status node_api_post_finalizer(node_api_basic_env basic_env, napi_finalize finalize_cb, void* finalize_data, void* finalize_hint)
{
    if (basic_env == nullptr || finalize_cb == nullptr) {
        return SetLastError(basic_env, napi_invalid_arg);
    }
    // queued, not invoked here - see NapiEnv::drainPostFinalizers (NapiEnv.h/
    // NapiEnv.cpp) for why (a post-finalizer's whole point, unlike a plain
    // napi_wrap/napi_add_finalizer finalize_cb, is that it is safe to call
    // back into JS - which requires running it later, at a safe point, not
    // synchronously from here or from a GC sweep).
    basic_env->napiEnv->enqueuePostFinalizer(finalize_cb, finalize_data, finalize_hint);
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
