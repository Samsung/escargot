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
#include <climits>
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
    // the object is being collected, so its napi_remove_wrap lookup entry
    // would otherwise dangle once this same address is reused later
    wrapData->env->napiEnv->takeWrapFinalizerData(reinterpret_cast<ObjectRef*>(self));
    // force any other still-weak napi_ref to this same object to already
    // read as cleared before this finalizer runs - see
    // NapiEnv::clearWeakRefTargets's own comment (NapiEnv.h) for why this
    // can't just be left to NapiWeakRefFinalizer/Escargot's own
    // registration-ordered GC finalizer list.
    wrapData->env->napiEnv->clearWeakRefTargets(self);
    // see NapiEnv::isInGCUnsafeFinalizer's own comment (NapiEnv.h): this
    // finalize_cb only ever received a node_api_basic_env (even though it's
    // stored here as a full napi_env for convenience), so it must not call
    // anything that needs to allocate/run JS - found via
    // test_finalizer/test_fatal_finalize.js's finalizerWithFailedJSCallback,
    // which deliberately casts basic_env back to napi_env and calls
    // napi_create_object to check exactly this is caught.
    wrapData->env->napiEnv->setInGCUnsafeFinalizer(true);
    wrapData->finalizeCb(wrapData->env, wrapData->nativeObject, wrapData->finalizeHint);
    wrapData->env->napiEnv->setInGCUnsafeFinalizer(false);

    if (wrapData->env->pendingException.hasValue()) {
        ValueRef* fatalErr = wrapData->env->pendingException.value();
        wrapData->env->pendingException = nullptr;
        napi_fatal_exception(wrapData->env, ToNapi(fatalErr));
    }

    delete wrapData;
}

// Forces every still-registered (i.e. neither napi_remove_wrap'd nor already
// GC-finalized) napi_wrap finalizer in `napiEnv` to run right now, regardless
// of whether its wrapped object is still reachable - the real Node-API
// contract for environment teardown: an object kept alive by, say, a
// module.exports property (so ordinary GC would never collect it) still gets
// its finalizer invoked once the owning env/process itself goes away. Called
// from NapiEnv::~NapiEnv() (NapiEnv.cpp). Only napi_wrap/napi_remove_wrap's
// own registry (NapiEnv::m_wrapFinalizerData) is covered - napi_add_finalizer
// (NapiExtras.cpp) has no equivalent forced-run path yet, since no target
// test.js (see test_general/testEnvCleanup.js) needs it.
void RunEnvCleanupWrapFinalizers(NapiEnv* napiEnv)
{
    // snapshot first: NapiWrapFinalizer (invoked below) removes itself from
    // napiEnv's registry as it runs, which would otherwise invalidate the
    // live map's iterators mid-walk.
    std::vector<std::pair<ObjectRef*, void*>> snapshot = napiEnv->snapshotWrapFinalizerData();
    for (auto& entry : snapshot) {
        ObjectRef* obj = entry.first;
        void* wrapDataRaw = entry.second;
        if (napiEnv->peekWrapFinalizerData(obj) != wrapDataRaw) {
            // already gone (napi_remove_wrap'd, replaced by a later
            // napi_wrap, or already actually GC-finalized) since the
            // snapshot was taken - possible if an earlier entry's own
            // finalize_cb reentrantly touched this object - skip it.
            continue;
        }
        WrapFinalizeData* wrapData = reinterpret_cast<WrapFinalizeData*>(wrapDataRaw);
        // unregister Boehm's own copy first, so a *later* real GC pass never
        // also invokes (and double-frees) this same wrapData once obj
        // actually becomes garbage.
        Memory::gcUnregisterFinalizer(obj, NapiWrapFinalizer, wrapData);
        NapiWrapFinalizer(obj, wrapDataRaw);
    }
}

// clears a weak napi_ref's target once it is collected, so
// napi_get_reference_value stops returning a dangling pointer. `ref` itself
// is a plain (non-GC) heap allocation; see napi_delete_reference's own
// comment for why the actual `delete ref` for a still-weakFinalizerRegistered
// ref is deferred to here (via ref->pendingDelete) instead of happening
// synchronously there.
static void NapiWeakRefFinalizer(void* self, void* data)
{
    napi_ref__* ref = reinterpret_cast<napi_ref__*>(data);
    if (ref->pendingDelete) {
        delete ref;
        return;
    }
    ref->value = nullptr;
}

// idempotent: safe to call whenever `ref` is (or becomes) weak, regardless of
// whether NapiWeakRefFinalizer is already registered for it
void RegisterWeakRefFinalizerIfNeeded(napi_env env, napi_ref__* ref)
{
    if (!ref->weakFinalizerRegistered && ref->value.hasValue() && ref->value.value()->isStoredInHeap()) {
        Memory::gcRegisterFinalizer(ref->value.value(), NapiWeakRefFinalizer, ref);
        ref->weakFinalizerRegistered = true;
        // see NapiEnv::trackWeakRefTarget's own comment (NapiEnv.h): tracked
        // independently of the GC finalizer list itself, so a
        // napi_wrap/napi_add_finalizer finalizer on the same object can
        // force this ref's value cleared up front, ahead of Escargot's own
        // (registration-ordered, not phase-ordered) finalizer invocation.
        env->napiEnv->trackWeakRefTarget(reinterpret_cast<void*>(ref->value.value()), ref);
    }
}

// idempotent counterpart of RegisterWeakRefFinalizerIfNeeded; call before
// `ref` stops being weak (napi_reference_ref) or is deleted (napi_delete_reference)
static void UnregisterWeakRefFinalizerIfNeeded(napi_env env, napi_ref__* ref)
{
    if (ref->weakFinalizerRegistered && ref->value.hasValue()) {
        env->napiEnv->untrackWeakRefTarget(reinterpret_cast<void*>(ref->value.value()), ref);
        Memory::gcUnregisterFinalizer(ref->value.value(), NapiWeakRefFinalizer, ref);
        ref->weakFinalizerRegistered = false;
    }
}

// Node's own js_native_api_v8.cc error_messages[], indexed by napi_status
// (js_native_api_types.h's napi_status enum order) - see SetLastError
// (NapiTypes.h)/env->lastErrorCode (NapiEnv.h). A null entry (napi_ok) means
// "no message" per napi_get_last_error_info's own contract.
static const char* const kNapiErrorMessages[] = {
    nullptr, // napi_ok
    "Invalid argument", // napi_invalid_arg
    "An object was expected", // napi_object_expected
    "A string was expected", // napi_string_expected
    "A string or symbol was expected", // napi_name_expected
    "A function was expected", // napi_function_expected
    "A number was expected", // napi_number_expected
    "A boolean was expected", // napi_boolean_expected
    "An array was expected", // napi_array_expected
    "Unknown failure", // napi_generic_failure
    "An exception is pending", // napi_pending_exception
    "The async work item was cancelled", // napi_cancelled
    "napi_escape_handle already called on scope", // napi_escape_called_twice
    "Invalid handle scope usage", // napi_handle_scope_mismatch
    "Invalid callback scope usage", // napi_callback_scope_mismatch
    "Thread-safe function queue is full", // napi_queue_full
    "Thread-safe function handle is closing", // napi_closing
    "A bigint was expected", // napi_bigint_expected
    "A date was expected", // napi_date_expected
    "An arraybuffer was expected", // napi_arraybuffer_expected
    "A detachable arraybuffer was expected", // napi_detachable_arraybuffer_expected
    "Main thread would deadlock", // napi_would_deadlock
    "External buffers are not allowed", // napi_no_external_buffers_allowed
    "Cannot run JS trigger by StopIfNecessary", // napi_cannot_run_js
};

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_get_last_error_info(node_api_basic_env env, const napi_extended_error_info** result)
{
    if (env == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    size_t index = static_cast<size_t>(env->lastErrorCode);
    size_t tableSize = sizeof(kNapiErrorMessages) / sizeof(kNapiErrorMessages[0]);
    env->lastErrorInfo.error_message = (index < tableSize) ? kNapiErrorMessages[index] : "Unknown error code";
    env->lastErrorInfo.engine_reserved = nullptr;
    env->lastErrorInfo.engine_error_code = 0;
    env->lastErrorInfo.error_code = env->lastErrorCode;
    *result = &env->lastErrorInfo;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_undefined(napi_env env, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::createUndefined());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_double(napi_env env, double value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_global(napi_env env, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(env->context()->globalObject());
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_object(napi_env env, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    // See NapiEnv::isInGCUnsafeFinalizer's own comment (NapiEnv.h): a
    // synchronous GC-triggered finalizer (napi_wrap/napi_add_finalizer/
    // napi_create_external's finalize_cb) is only ever handed a
    // node_api_basic_env - allocating a new JS object from inside one anyway
    // (by casting it back to a real napi_env, as
    // test_finalizer/test_fatal_finalize.js's finalizerWithFailedJSCallback
    // deliberately does) is exactly the "calling a function that may affect
    // GC state" contract violation real Node-API fatally aborts on.
    if (env != nullptr && env->napiEnv->isInGCUnsafeFinalizer()) {
        napi_fatal_error(nullptr, 0, "Finalizer is calling a function that may affect GC state.", NAPI_AUTO_LENGTH);
    }

    *result = ToNapi(ObjectRef::create(env->executionState));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_string_utf8(napi_env env, const char* str, size_t length, napi_value* result)
{
    if (str == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    // matches real Node-API's own guard (js_native_api_v8.cc): an explicit
    // (not NAPI_AUTO_LENGTH) length beyond INT_MAX is rejected outright,
    // rather than actually reading that many bytes from `str` - found via
    // test_string/test.js's TestLargeUtf8, which deliberately passes
    // INT_MAX+1 alongside a 1-byte (empty) `str` specifically to check this
    // is rejected instead of reading far out of bounds (this previously
    // SIGBUS'd instead).
    if (length != NAPI_AUTO_LENGTH && length > static_cast<size_t>(INT_MAX)) {
        return SetLastError(env, napi_invalid_arg);
    }

    size_t byteLength = (length == NAPI_AUTO_LENGTH) ? strlen(str) : length;
    // Transparent compressible-string routing: a large string handed in
    // through a perfectly standard napi_create_string_utf8 call is, above
    // kCompressibleStringThreshold bytes, allocated as one of Escargot's
    // compressible strings instead of a plain one - functionally identical
    // (it decompresses on any access) but eligible to be compressed back down
    // at idle (VMInstanceRef::enterIdleMode) or during GC
    // (CompressCompressibleStringsWhileGC), which is how an unmodified N-API
    // addon that simply holds onto lots of document-sized text ends up using
    // substantially less resident memory on Escargot with no source changes
    // of its own. Small strings (below the threshold - every existing test
    // string) keep using the plain path unchanged.
    if (byteLength >= kCompressibleStringThreshold && StringRef::isCompressibleStringEnabled() && env != nullptr && env->napiEnv->vmInstance() != nullptr) {
        *result = ToNapi(StringRef::createFromUTF8ToCompressibleString(env->napiEnv->vmInstance(), str, byteLength));
    } else {
        *result = ToNapi(StringRef::createFromUTF8(str, byteLength));
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_set_named_property(napi_env env, napi_value object, const char* utf8name, napi_value value)
{
    ObjectRef* obj = FromNapi(object)->asObject();
    StringRef* propertyName = StringRef::createFromUTF8(utf8name, strlen(utf8name));
    ValueRef* propertyValue = FromNapi(value);
    ExecutionStateRef* state = env->executionState;

    // ObjectRef::set can invoke a user setter (or a Proxy `set` trap), either
    // of which may throw a raw C++ exception - this was previously left
    // unsandboxed here (unlike napi_set_property's identical-in-spirit
    // Evaluator::execute wrapping, NapiObject.cpp), so a throwing setter's
    // exception would escape all the way past this function's own napi_status
    // return contract, past the calling native addon function entirely, and
    // surface as a raw uncaught top-level exception instead of
    // napi_pending_exception (found via test_object/test_exceptions.js, whose
    // Proxy's `set` trap always throws).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ObjectRef* obj, StringRef* name, ValueRef* value) -> ValueRef* {
            obj->set(state, name, value);
            return ValueRef::createUndefined();
        },
        obj, propertyName, propertyValue);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return napi_pending_exception;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_call_function(napi_env env, napi_value recv, napi_value func, size_t argc, const napi_value* argv, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    ValueRef* fn = FromNapi(func);
    ValueRef* thisArg = FromNapi(recv);

    std::vector<ValueRef*> args(argc);
    for (size_t i = 0; i < argc; i++) {
        args[i] = FromNapi(argv[i]);
    }

    // ValueRef::call() throws a raw C++ exception (Escargot::Value, by
    // value) on an uncaught JS exception, with nothing in between catching
    // it - it must not be allowed to cross this function's own stack frame,
    // or N-API's "exceptions never cross an API call boundary" contract
    // breaks. Evaluator::execute's ExecutionStateRef* overload runs the call
    // in a nested SandBox that catches it for us.
    Evaluator::EvaluatorResult callResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* fn, ValueRef* thisArg, size_t argc, ValueRef** argv) -> ValueRef* {
            return fn->call(state, thisArg, argc, argv);
        },
        fn, thisArg, argc, args.data());

    if (!callResult.isSuccessful()) {
        env->pendingException = callResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    if (result != nullptr) {
        *result = ToNapi(callResult.result);
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
    // env/value/result checked (and *before* the isNumber() type check) -
    // see NapiValue.cpp's napi_get_value_bool for the identical reasoning
    // and originating test (test_conversions/test_null.c's
    // GEN_NULL_CHECK_BINDING(..., napi_get_value_double)).
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isNumber()) {
        return SetLastError(env, napi_number_expected);
    }
    *result = v->asNumber();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_uint32(napi_env env, napi_value value, uint32_t* result)
{
    // see napi_get_value_double's identical reasoning (above).
    if (env == nullptr || value == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isNumber()) {
        return SetLastError(env, napi_number_expected);
    }
    *result = v->toUint32(env->executionState);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_function(napi_env env, const char* utf8name, size_t length, napi_callback cb, void* data, napi_value* result)
{
    // env/cb/result are all mandatory (utf8name is not - an anonymous
    // function is valid Node-API); env==nullptr in particular must be
    // checked *before* anything below dereferences it. This was previously
    // entirely unchecked, unconditionally crashing (SIGSEGV on
    // `env->context()`) instead of returning napi_invalid_arg - found via
    // test_function/test.js's TestCreateFunctionParameters, which - like
    // test_number/test_null.js's similar pattern - deliberately calls this
    // with each of env/cb/result null in turn to verify graceful rejection.
    if (env == nullptr) {
        return napi_invalid_arg;
    }
    if (cb == nullptr || result == nullptr) {
        // env is non-null here, so (unlike the env==nullptr case above)
        // napi_get_last_error_info can and should report a real message for
        // this - see SetLastError (NapiTypes.h)/kNapiErrorMessages above -
        // and this addon's test (like several others) checks that message,
        // not just the returned napi_status.
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;
    ContextRef* context = env->context();

    size_t nameLen = (utf8name == nullptr) ? 0 : ((length == NAPI_AUTO_LENGTH) ? strlen(utf8name) : length);
    AtomicStringRef* name = AtomicStringRef::create(context, utf8name ? utf8name : "", nameLen);

    // Route through FunctionTemplateRef + NapiClassConstructorTrampoline -
    // exactly what napi_define_class (below) already does - instead of the
    // plain FunctionObjectRef::create(..., NapiCallbackTrampoline) this used
    // previously. Per the Node-API contract, a function created by
    // napi_create_function may be invoked with `new` (and, transitively,
    // used as an ES6 `class ... extends` target) exactly like one created by
    // napi_define_class does; it's only napi_define_class's *properties*
    // (static vs instance, via napi_static) that differ, not constructibility
    // of the plain function case. The previous NapiCallbackTrampoline path
    // hardcoded isConstructor=false *and* always reported new.target as NULL
    // (napi_callback_info__'s newTarget field was a hardcoded `nullptr`),
    // silently breaking `new`/`extends`/napi_get_new_target on any function
    // returned from napi_create_function. Found via test_new_target/test.js:
    // `class Class extends binding.BaseClass` (BaseClass created via
    // napi_create_function, per Node.js's own test) threw "Class extends
    // value is not object nor null", and even after allowing constructibility
    // alone, the addon's own new.target-non-null assertion (invoked through
    // `super()`) would still have failed.
    FunctionTemplateRef* tpl = FunctionTemplateRef::create(name, 0, true, true, NapiClassConstructorTrampoline);
    // addToContextCache=false: napi_create_function is called dynamically and
    // unboundedly, so the throwaway template must not be pinned in the context
    // cache (which would leak it and keep the function permanently rooted).
    ObjectRef* fnObj = tpl->instantiate(context, false);
    FunctionObjectRef* fn = fnObj->asFunctionObject();

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
    ExecutionStateRef* state = env->executionState;

    // ApplyPropertyDescriptor's own defineDataProperty/defineAccessorProperty
    // calls can invoke a Proxy's defineProperty trap and throw - previously
    // unsandboxed here, unlike every other property mutator in this file/
    // NapiObject.cpp (found via test_object/test_exceptions.js, whose Proxy's
    // `defineProperty` trap always throws). Note that the lambda below still
    // calls ApplyPropertyDescriptor(env, ...) - which internally keeps
    // reading env->executionState, not this lambda's own `state` parameter -
    // rather than threading `state` through; that's fine (not stale/wrong):
    // Evaluator::execute's exception-catching sandbox is a plain C++
    // try/catch around this whole closure invocation, so it catches a throw
    // regardless of which ExecutionStateRef the throwing call happened to
    // use, and env->executionState itself is left untouched (so, unlike a
    // temporary env->executionState swap would be, never at risk of being
    // left dangling on the object should an exception actually unwind through here).
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, napi_env env, ObjectRef* obj, size_t property_count, const napi_property_descriptor* properties) -> ValueRef* {
            for (size_t i = 0; i < property_count; i++) {
                ApplyPropertyDescriptor(env, obj, properties[i]);
            }
            return ValueRef::createUndefined();
        },
        env, obj, property_count, properties);

    if (!evalResult.isSuccessful()) {
        env->pendingException = evalResult.error.value();
        return napi_pending_exception;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_define_class(napi_env env, const char* utf8name, size_t length, napi_callback constructor, void* data, size_t property_count, const napi_property_descriptor* properties, napi_value* result)
{
    // env==nullptr must be checked *before* anything below dereferences it -
    // same pattern/rationale as napi_create_function's own env==nullptr
    // check above. Found via test_constructor/test.js's TestDefineClass,
    // which - like test_number/test_null.js's similar pattern - deliberately
    // calls this with each of env/utf8name/constructor/properties/result null
    // in turn (data==nullptr alone is valid - it's an optional user pointer)
    // to verify graceful rejection instead of a SIGSEGV.
    if (env == nullptr) {
        return napi_invalid_arg;
    }
    // matches real Node-API's NAPI_PREAMBLE(env), which clears the env's
    // last-error status before doing any work, so a subsequent successful
    // call reports napi_ok (not whatever an earlier failed sibling call left
    // behind) - found via TestDefineClass's cbDataIsNull case, which expects
    // exactly that after a preceding cbIsNull failure.
    SetLastError(env, napi_ok);
    if (utf8name == nullptr || constructor == nullptr || result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    if (property_count > 0 && properties == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;
    ContextRef* context = env->context();

    size_t nameLen = (utf8name == nullptr) ? 0 : ((length == NAPI_AUTO_LENGTH) ? strlen(utf8name) : length);
    AtomicStringRef* name = AtomicStringRef::create(context, utf8name ? utf8name : "", nameLen);

    FunctionTemplateRef* tpl = FunctionTemplateRef::create(name, 0, true, true, NapiClassConstructorTrampoline);
    // addToContextCache=false: same rationale as napi_create_function - each
    // napi_define_class builds a fresh throwaway template, so caching it would
    // leak the class constructor for the context's lifetime.
    ObjectRef* consObj = tpl->instantiate(context, false);
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
        env->napiEnv->setWrapFinalizerData(obj, wrapData);
    }

    if (result != nullptr) {
        napi_ref__* ref = new napi_ref__();
        ref->value = obj;
        ref->refcount = 0;
        RegisterWeakRefFinalizerIfNeeded(env, ref);
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

    // suppress the napi_wrap finalizer: a real Node-API remove_wrap must not
    // invoke it, neither now nor when obj is eventually collected
    void* wrapDataRaw = env->napiEnv->takeWrapFinalizerData(obj);
    if (wrapDataRaw != nullptr) {
        WrapFinalizeData* wrapData = reinterpret_cast<WrapFinalizeData*>(wrapDataRaw);
        Memory::gcUnregisterFinalizer(obj, NapiWrapFinalizer, wrapData);
        delete wrapData;
    }
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
    // weak (refcount == 0) refs are not rooted by the map above, so track
    // collection of the target directly; RegisterWeakRefFinalizerIfNeeded
    // skips non-heap values (undefined/number/boolean/...) on its own, since
    // those can never be collected
    if (initial_refcount == 0) {
        RegisterWeakRefFinalizerIfNeeded(env, ref);
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

    if (ref->weakFinalizerRegistered) {
        // Do NOT free `ref` here (and don't bother with
        // UnregisterWeakRefFinalizerIfNeeded's Memory::gcUnregisterFinalizer
        // call either, for the same reason): NapiWeakRefFinalizer may
        // already be irrevocably armed for the target's *current* GC pass.
        // Boehm's per-object finalizer list, once a collection decides to
        // finalize an object, is a fixed snapshot taken up front -
        // unregistering from *within* another finalizer callback for that
        // same object (as this reentrant call does when
        // napi_delete_reference is invoked, directly or indirectly, from
        // inside a napi_wrap/napi_add_finalizer finalizer sharing the same
        // target - test_reference/test.js's validateDeleteBeforeFinalize) is
        // a silent no-op against Boehm, not a real removal, with no way to
        // detect that from here. Freeing `ref` immediately would then leave
        // that still-armed callback writing into freed memory once it runs
        // later in the very same sweep - a real, previously-crashing (heap
        // corruption: "malloc(): unaligned tcache chunk detected")
        // use-after-free. Instead, mark it and let NapiWeakRefFinalizer
        // itself perform the actual `delete ref` whenever it does end up
        // running (immediately afterward, in the reentrant case above; at
        // the target's own eventual collection, in the ordinary case) - see
        // its updated definition. This does mean a `ref` deleted this way
        // isn't reclaimed until its target is next collected (which may
        // never happen before process exit, if the target itself outlives
        // this NapiEnv) - an acceptable, bounded leak for this PoC.
        if (ref->value.hasValue()) {
            env->napiEnv->untrackWeakRefTarget(reinterpret_cast<void*>(ref->value.value()), ref);
        }
        ref->pendingDelete = true;
        return napi_ok;
    }

    delete ref;
    return napi_ok;
}

// napi_reference_ref/napi_reference_unref can move `ref` between weak
// (refcount == 0) and strong (refcount > 0) any number of times over its
// life. Each call adds/removes exactly one PersistentValueRefMap rooting
// unit, same as napi_create_reference's initial add() loop - so ref->refcount
// stays equal to how many add() calls this ref has contributed for its
// value, which is what napi_delete_reference's own remove() loop assumes.
// Only the 0<->non-zero crossing also flips the weak-staleness finalizer.
ESCARGOT_NAPI_EXPORT napi_status napi_reference_ref(napi_env env, napi_ref ref, uint32_t* result)
{
    if (ref->refcount == 0 && !ref->value.hasValue()) {
        // weak ref whose target is already gone - nothing to strengthen
        return SetLastError(env, napi_generic_failure);
    }

    bool wasWeak = (ref->refcount == 0);
    ref->refcount++;
    env->napiEnv->persistentValueRefMap()->add(ref->value.value());
    if (wasWeak) {
        UnregisterWeakRefFinalizerIfNeeded(env, ref);
    }
    if (result != nullptr) {
        *result = ref->refcount;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_reference_unref(napi_env env, napi_ref ref, uint32_t* result)
{
    if (ref->refcount == 0) {
        return SetLastError(env, napi_generic_failure);
    }

    ref->refcount--;
    if (ref->value.hasValue()) {
        env->napiEnv->persistentValueRefMap()->remove(ref->value.value());
    }
    if (ref->refcount == 0) {
        RegisterWeakRefFinalizerIfNeeded(env, ref);
    }
    if (result != nullptr) {
        *result = ref->refcount;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_reference_value(napi_env env, napi_ref ref, napi_value* result)
{
    // a weak (refcount == 0) ref's value is cleared by NapiWeakRefFinalizer
    // once its target is collected, so this no longer dangles
    *result = ref->value.hasValue() ? ToNapi(ref->value.value()) : nullptr;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_open_handle_scope(napi_env env, napi_handle_scope* result)
{
    napi_handle_scope__* scope = new napi_handle_scope__();
    scope->parent = env->topHandleScope;
    env->topHandleScope = scope;
    *result = scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_close_handle_scope(napi_env env, napi_handle_scope scope)
{
    if (scope == nullptr || scope != env->topHandleScope) {
        return SetLastError(env, napi_handle_scope_mismatch);
    }
    env->topHandleScope = scope->parent;
    delete scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_open_escapable_handle_scope(napi_env env, napi_escapable_handle_scope* result)
{
    napi_escapable_handle_scope__* scope = new napi_escapable_handle_scope__();
    scope->parent = env->topHandleScope;
    env->topHandleScope = scope;
    *result = scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_close_escapable_handle_scope(napi_env env, napi_escapable_handle_scope scope)
{
    if (scope == nullptr || scope != env->topHandleScope) {
        return SetLastError(env, napi_handle_scope_mismatch);
    }
    env->topHandleScope = scope->parent;
    delete scope;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_escape_handle(napi_env env, napi_escapable_handle_scope scope, napi_value escapee, napi_value* result)
{
    if (scope->escapeCalled) {
        return SetLastError(env, napi_escape_called_twice);
    }
    scope->escapeCalled = true;
    // napi_value is already the GC pointer itself (see ToNapi/FromNapi in
    // NapiTypes.h), so there is nothing to actually copy into an outer
    // scope's buffer - `escapee` is already reachable however the caller is
    // holding it.
    *result = escapee;
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

    // ValueRef::construct(), like ValueRef::call() (see napi_call_function
    // above), throws a raw C++ exception (Escargot::Value, by value) on an
    // uncaught JS exception (e.g. the constructor itself throwing) - this
    // was previously left unsandboxed here, so a throwing constructor's
    // exception would escape all the way past this function's own napi_status
    // return contract instead of becoming napi_pending_exception, breaking
    // any addon that (correctly, per the Node-API contract) checks the
    // returned status instead of expecting a raw throw across the API
    // boundary (found via test_exception/test.js's constructReturnException/
    // constructAllowException, which do exactly that).
    Evaluator::EvaluatorResult constructResult = Evaluator::execute(
        state, [](ExecutionStateRef* state, ValueRef* ctor, size_t argc, ValueRef** argv) -> ValueRef* {
            return ctor->construct(state, argc, argv);
        },
        FromNapi(constructor), argc, args.data());

    if (!constructResult.isSuccessful()) {
        env->pendingException = constructResult.error.value();
        return SetLastError(env, napi_pending_exception);
    }

    *result = ToNapi(constructResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = ToNapi(ValueRef::create(value));
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_boolean(napi_env env, bool value, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

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
