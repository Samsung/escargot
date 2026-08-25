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

// ArrayBuffer/TypedArray/DataView/External/type-tag/Buffer slice of
// js_native_api.h + node_api.h, following the same patterns as
// NapiFunctions.cpp (see especially napi_wrap/setExtraData/finalizer
// registration there).

#include "NapiTypes.h"

#include <cstring>
#include <vector>
#include <utility>

namespace Escargot {
namespace Napi {

// shared by napi_create_external_arraybuffer and napi_create_external_buffer:
// bridges Escargot's BackingStoreRefDeleterCallback (void*, size_t, void*)
// back to the N-API node_api_basic_finalize (env, data, hint) contract,
// carrying just enough state to make that call once the backing store itself
// is torn down.
struct ExternalBackingStoreFinalizeData {
    napi_env env;
    node_api_basic_finalize finalizeCb;
    void* nativeData;
    void* finalizeHint;
};

static void FinalizeExternalBackingStoreForEnvTeardown(void* data)
{
    ExternalBackingStoreFinalizeData* finalizeData = reinterpret_cast<ExternalBackingStoreFinalizeData*>(data);
    if (finalizeData->finalizeCb != nullptr) {
        finalizeData->finalizeCb(finalizeData->env, finalizeData->nativeData, finalizeData->finalizeHint);
        finalizeData->finalizeCb = nullptr;
    }
    finalizeData->env = nullptr;
}

static void NapiExternalBackingStoreDeleter(void* data, size_t length, void* deleterData)
{
    ExternalBackingStoreFinalizeData* finalizeData = reinterpret_cast<ExternalBackingStoreFinalizeData*>(deleterData);
    if (finalizeData->env != nullptr) {
        finalizeData->env->napiEnv->takeNativeFinalizerData(finalizeData);
    }
    FinalizeExternalBackingStoreForEnvTeardown(finalizeData);
    delete finalizeData;
}

// backs napi_create_external: an ObjectRef with the user's pointer stashed in
// the same extraData() slot napi_wrap uses (see NapiFunctions.cpp), plus a GC
// finalizer that invokes the user's finalize_cb once the object is collected
// - same registration pattern as NapiWrapFinalizer/WrapFinalizeData there.
struct ExternalObjectFinalizeData {
    napi_env env;
    node_api_basic_finalize finalizeCb;
    void* nativeData;
    void* finalizeHint;
};

static void NapiExternalObjectFinalizer(void* self, void* data)
{
    ExternalObjectFinalizeData* finalizeData = reinterpret_cast<ExternalObjectFinalizeData*>(data);
    napi_env env = finalizeData->env;
    env->napiEnv->takeEnvFinalizerData(data);
    finalizeData->finalizeCb(env, finalizeData->nativeData, finalizeData->finalizeHint);

    if (env->pendingException.hasValue()) {
        ValueRef* fatalErr = env->pendingException.value();
        env->pendingException = nullptr;
        napi_fatal_exception(env, ToNapi(fatalErr));
    }

    delete finalizeData;
}

// napi_type_tag_object/napi_check_object_type_tag side storage. Native STL
// storage cannot safely retain a bare GC pointer, so every entry owns a weak
// persistent holder. The entry itself is the GC finalizer identity; by the
// time the callback runs its disappearing link may already be null.
struct NapiTypeTagData {
    PersistentRefHolder<ObjectRef> target;
    uint64_t lower;
    uint64_t upper;
};

static std::vector<NapiTypeTagData*> g_typeTags;

static void NapiTypeTagFinalizer(void* self, void* data)
{
    NapiTypeTagData* tagData = reinterpret_cast<NapiTypeTagData*>(data);
    for (auto iter = g_typeTags.begin(); iter != g_typeTags.end(); ++iter) {
        if (*iter == tagData) {
            g_typeTags.erase(iter);
            break;
        }
    }
    delete tagData;
}
static size_t NapiTypedArrayElementSize(napi_typedarray_type type)
{
    switch (type) {
    case napi_int8_array:
    case napi_uint8_array:
    case napi_uint8_clamped_array:
        return 1;
    case napi_int16_array:
    case napi_uint16_array:
    case napi_float16_array:
        return 2;
    case napi_int32_array:
    case napi_uint32_array:
    case napi_float32_array:
        return 4;
    case napi_float64_array:
    case napi_bigint64_array:
    case napi_biguint64_array:
    default:
        return 8;
    }
}

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = FromNapi(value)->isArrayBuffer();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_arraybuffer(napi_env env, size_t byte_length, void** data, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, size_t byteLength, void** data) -> ValueRef* {
            ArrayBufferObjectRef* buffer = ArrayBufferObjectRef::create(state);
            buffer->allocateBuffer(state, byteLength);
            if (data != nullptr) {
                *data = buffer->rawBuffer();
            }
            return buffer;
        },
        byte_length, data);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_external_arraybuffer(napi_env env, void* external_data, size_t byte_length, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_value* result)
{
    ExternalBackingStoreFinalizeData* finalizeData = new ExternalBackingStoreFinalizeData();
    finalizeData->env = env;
    finalizeData->finalizeCb = finalize_cb;
    finalizeData->nativeData = external_data;
    finalizeData->finalizeHint = finalize_hint;
    env->napiEnv->registerNativeFinalizer(FinalizeExternalBackingStoreForEnvTeardown, finalizeData);

    BackingStoreRef* backingStore = BackingStoreRef::createNonSharedBackingStore(external_data, byte_length, NapiExternalBackingStoreDeleter, finalizeData);

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, BackingStoreRef* backingStore) -> ValueRef* {
            ArrayBufferObjectRef* buffer = ArrayBufferObjectRef::create(state);
            buffer->attachBuffer(backingStore);
            return buffer;
        },
        backingStore);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_arraybuffer_info(napi_env env, napi_value arraybuffer, void** data, size_t* byte_length)
{
    ValueRef* v = FromNapi(arraybuffer);
    if (!v->isArrayBuffer()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferRef* buf = v->asArrayBuffer();

    if (data != nullptr) {
        *data = buf->rawBuffer();
    }
    if (byte_length != nullptr) {
        *byte_length = buf->byteLength();
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_detach_arraybuffer(napi_env env, napi_value arraybuffer)
{
    ValueRef* v = FromNapi(arraybuffer);
    if (!v->isArrayBufferObject()) {
        return SetLastError(env, napi_invalid_arg);
    }
    v->asArrayBufferObject()->detachArrayBuffer();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_detached_arraybuffer(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ValueRef* v = FromNapi(value);
    if (!v->isArrayBufferObject()) {
        *result = false;
        return napi_ok;
    }

    ArrayBufferObjectRef* buf = v->asArrayBufferObject();
    // A real detach (napi_detach_arraybuffer/ArrayBuffer.prototype.transfer)
    // sets isDetachedBuffer() - but an ArrayBuffer whose backing store was
    // never attached to real memory in the first place (e.g.
    // napi_create_external_arraybuffer(env, NULL, 0, ...), test_typedarray/
    // test.js's NullArrayBuffer()) reports isDetachedBuffer() == false (it
    // was never attached-then-detached; it just has a null data pointer) yet
    // Node-API's own napi_is_detached_arraybuffer treats a null backing
    // store's data pointer as detached too - so check rawBuffer() directly as
    // well, not just the explicit-detach flag.
    *result = buf->isDetachedBuffer() || buf->rawBuffer() == nullptr;
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = FromNapi(value)->isTypedArrayObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_typedarray(napi_env env, napi_typedarray_type type, size_t length, napi_value arraybuffer, size_t byte_offset, napi_value* result)
{
    ValueRef* bufValue = FromNapi(arraybuffer);
    if (!bufValue->isArrayBuffer()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferRef* buf = bufValue->asArrayBuffer();

    size_t elementSize = NapiTypedArrayElementSize(type);
    size_t byteLength = length * elementSize;

    // Per the typed array spec (and this API's own documented contract):
    //  - a view may not extend past the end of its backing buffer, and
    //  - byte_offset must be a multiple of the view's element size.
    // Neither was previously checked - view->setBuffer() below happily wires
    // up an out-of-bounds or misaligned view - so callers silently got a
    // view that reads/writes past the buffer's storage (or at the wrong
    // offset) instead of the RangeError real Node-API raises (found via
    // test_typedarray/test.js's `CreateTypedArray(template, buffer, 0, 136)`
    // on a 128-byte buffer, and `CreateTypedArray(template, buffer,
    // currentType.BYTES_PER_ELEMENT + 1, 1)`, both expected to throw
    // RangeError).
    if (byte_offset + byteLength > buf->byteLength()) {
        napi_throw_range_error(env, nullptr, "byte_offset + length must be smaller than the size in bytes of the array passed in");
        return SetLastError(env, napi_pending_exception);
    }
    if (elementSize > 1 && (byte_offset % elementSize) != 0) {
        napi_throw_range_error(env, nullptr, "start offset of typed array must be a multiple of the byte length of the element type");
        return SetLastError(env, napi_pending_exception);
    }

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, napi_typedarray_type type, ArrayBufferRef* buffer, size_t byteOffset, size_t byteLength, size_t length) -> ValueRef* {
            ArrayBufferViewRef* view;
            switch (type) {
            case napi_int8_array:
                view = Int8ArrayObjectRef::create(state);
                break;
            case napi_uint8_array:
                view = Uint8ArrayObjectRef::create(state);
                break;
            case napi_uint8_clamped_array:
                view = Uint8ClampedArrayObjectRef::create(state);
                break;
            case napi_int16_array:
                view = Int16ArrayObjectRef::create(state);
                break;
            case napi_uint16_array:
                view = Uint16ArrayObjectRef::create(state);
                break;
            case napi_int32_array:
                view = Int32ArrayObjectRef::create(state);
                break;
            case napi_uint32_array:
                view = Uint32ArrayObjectRef::create(state);
                break;
            case napi_float32_array:
                view = Float32ArrayObjectRef::create(state);
                break;
            case napi_float64_array:
                view = Float64ArrayObjectRef::create(state);
                break;
            case napi_bigint64_array:
                view = BigInt64ArrayObjectRef::create(state);
                break;
            case napi_biguint64_array:
                view = BigUint64ArrayObjectRef::create(state);
                break;
            case napi_float16_array:
                view = Float16ArrayObjectRef::create(state);
                break;
            default:
                return nullptr;
            }

            view->setBuffer(buffer, byteOffset, byteLength, length);
            return view;
        },
        type, buf, byte_offset, byteLength, length);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    if (evalResult.result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_typedarray_info(napi_env env, napi_value typedarray, napi_typedarray_type* type, size_t* length, void** data, napi_value* arraybuffer, size_t* byte_offset)
{
    ValueRef* v = FromNapi(typedarray);
    if (!v->isTypedArrayObject()) {
        return SetLastError(env, napi_invalid_arg);
    }

    napi_typedarray_type resolvedType;
    if (v->isInt8ArrayObject()) {
        resolvedType = napi_int8_array;
    } else if (v->isUint8ArrayObject()) {
        resolvedType = napi_uint8_array;
    } else if (v->isUint8ClampedArrayObject()) {
        resolvedType = napi_uint8_clamped_array;
    } else if (v->isInt16ArrayObject()) {
        resolvedType = napi_int16_array;
    } else if (v->isUint16ArrayObject()) {
        resolvedType = napi_uint16_array;
    } else if (v->isInt32ArrayObject()) {
        resolvedType = napi_int32_array;
    } else if (v->isUint32ArrayObject()) {
        resolvedType = napi_uint32_array;
    } else if (v->isFloat32ArrayObject()) {
        resolvedType = napi_float32_array;
    } else if (v->isFloat64ArrayObject()) {
        resolvedType = napi_float64_array;
    } else if (v->isBigInt64ArrayObject()) {
        resolvedType = napi_bigint64_array;
    } else if (v->isBigUint64ArrayObject()) {
        resolvedType = napi_biguint64_array;
    } else if (v->isFloat16ArrayObject()) {
        resolvedType = napi_float16_array;
    } else {
        return SetLastError(env, napi_invalid_arg);
    }

    ArrayBufferViewRef* view = v->asArrayBufferView();

    if (type != nullptr) {
        *type = resolvedType;
    }
    if (length != nullptr) {
        *length = view->arrayLength();
    }
    if (data != nullptr) {
        *data = view->rawBuffer(); // already offset-adjusted (points at the view's own first element)
    }
    if (arraybuffer != nullptr) {
        *arraybuffer = ToNapi(view->buffer());
    }
    if (byte_offset != nullptr) {
        *byte_offset = view->byteOffset();
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_dataview(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = FromNapi(value)->isDataViewObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_dataview(napi_env env, size_t length, napi_value arraybuffer, size_t byte_offset, napi_value* result)
{
    ValueRef* bufValue = FromNapi(arraybuffer);
    if (!bufValue->isArrayBuffer()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferRef* buf = bufValue->asArrayBuffer();

    // Node-API contract: byte_offset + length must fit within the buffer,
    // else throw a RangeError and report napi_pending_exception (matches
    // v8impl::napi_create_dataview). Guard against size_t overflow too.
    if (byte_offset + length < byte_offset || byte_offset + length > buf->byteLength()) {
        napi_throw_range_error(env, "ERR_NAPI_INVALID_DATAVIEW_ARGS",
                               "byte_offset + byte_length should be less than or "
                               "equal to the size in bytes of the array passed in");
        return SetLastError(env, napi_pending_exception);
    }

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, ArrayBufferRef* buffer, size_t byteOffset, size_t byteLength) -> ValueRef* {
            DataViewObjectRef* view = DataViewObjectRef::create(state);
            view->setBuffer(buffer, byteOffset, byteLength);
            return view;
        },
        buf, byte_offset, length);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_dataview_info(napi_env env, napi_value dataview, size_t* bytelength, void** data, napi_value* arraybuffer, size_t* byte_offset)
{
    ValueRef* v = FromNapi(dataview);
    if (!v->isDataViewObject()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferViewRef* view = v->asArrayBufferView();

    if (bytelength != nullptr) {
        *bytelength = view->byteLength();
    }
    if (data != nullptr) {
        *data = view->rawBuffer();
    }
    if (arraybuffer != nullptr) {
        *arraybuffer = ToNapi(view->buffer());
    }
    if (byte_offset != nullptr) {
        *byte_offset = view->byteOffset();
    }
    return napi_ok;
}

// Escargot has no first-class "external" value kind, unlike V8's
// napi_external. This is approximated as a plain ObjectRef carrying the raw
// pointer in its extraData() slot (the same slot napi_wrap uses on other
// objects) plus a GC finalizer that runs the user's finalize_cb - see
// napi_wrap/WrapFinalizeData in NapiFunctions.cpp for the identical pattern.
// CAVEAT: napi_typeof on the resulting napi_value reports napi_object, not
// napi_external, since NapiFunctions.cpp's napi_typeof has no way to
// distinguish this from any other plain object (there is no dedicated
// external/opaque ValueRef kind in EscargotPublic.h to check against).
ESCARGOT_NAPI_EXPORT napi_status napi_create_external(napi_env env, void* data, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_value* result)
{
    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, napi_env env, void* data, node_api_basic_finalize finalizeCb, void* finalizeHint) -> ValueRef* {
            ObjectRef* object = ObjectRef::create(state);
            object->setExtraData(data);
            if (finalizeCb != nullptr) {
                ExternalObjectFinalizeData* finalizeData = new ExternalObjectFinalizeData{ env, finalizeCb, data, finalizeHint };
                env->napiEnv->registerEnvFinalizer(object, NapiExternalObjectFinalizer, finalizeData);
            }
            return object;
        },
        env, data, finalize_cb, finalize_hint);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_value_external(napi_env env, napi_value value, void** result)
{
    ValueRef* v = FromNapi(value);
    if (!v->isObject()) {
        return SetLastError(env, napi_invalid_arg);
    }
    *result = v->asObject()->extraData();
    return napi_ok;
}

// no engine-level tracking of external memory pressure to hook into here;
// per the task's guidance this simply echoes the requested delta back as the
// "new" adjusted value instead of accumulating any real running total.
ESCARGOT_NAPI_EXPORT napi_status napi_adjust_external_memory(node_api_basic_env env, int64_t change_in_bytes, int64_t* adjusted_value)
{
    if (adjusted_value != nullptr) {
        *adjusted_value = change_in_bytes;
    }
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_type_tag_object(napi_env env, napi_value value, const napi_type_tag* type_tag)
{
    ValueRef* v = FromNapi(value);
    if (!v->isObject()) {
        return SetLastError(env, napi_object_expected);
    }
    ObjectRef* obj = v->asObject();

    // Matches Node's contract: an object may only be tagged once.
    for (NapiTypeTagData* entry : g_typeTags) {
        if (entry->target.get() == obj) {
            return SetLastError(env, napi_invalid_arg);
        }
    }

    NapiTypeTagData* entry = new NapiTypeTagData();
    entry->target.reset(obj);
    entry->target.setWeak();
    entry->lower = type_tag->lower;
    entry->upper = type_tag->upper;
    g_typeTags.push_back(entry);
    Memory::gcRegisterFinalizer(obj, NapiTypeTagFinalizer, entry);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_check_object_type_tag(napi_env env, napi_value value, const napi_type_tag* type_tag, bool* result)
{
    ValueRef* v = FromNapi(value);
    if (!v->isObject()) {
        return SetLastError(env, napi_object_expected);
    }
    ObjectRef* obj = v->asObject();

    *result = false;
    for (NapiTypeTagData* entry : g_typeTags) {
        if (entry->target.get() == obj) {
            *result = entry->lower == type_tag->lower && entry->upper == type_tag->upper;
            break;
        }
    }
    return napi_ok;
}

// Node's Buffer is a Uint8Array subclass; Escargot has no dedicated Buffer
// object kind, so a plain Uint8ArrayObjectRef over the requested backing
// store stands in for it here at the engine level (napi_is_buffer therefore
// also returns true for any plain Uint8Array created via
// napi_create_typedarray, not just ones created through the functions
// below - an approximation of Node's real, distinct Buffer type).
ESCARGOT_NAPI_EXPORT napi_status napi_create_buffer(napi_env env, size_t length, void** data, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, size_t length, void** data) -> ValueRef* {
            ArrayBufferObjectRef* buffer = ArrayBufferObjectRef::create(state);
            buffer->allocateBuffer(state, length);
            Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
            view->setBuffer(buffer, 0, length, length);
            if (data != nullptr) {
                *data = view->rawBuffer();
            }
            return view;
        },
        length, data);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_external_buffer(napi_env env, size_t length, void* data, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_value* result)
{
    ExternalBackingStoreFinalizeData* finalizeData = new ExternalBackingStoreFinalizeData();
    finalizeData->env = env;
    finalizeData->finalizeCb = finalize_cb;
    finalizeData->nativeData = data;
    finalizeData->finalizeHint = finalize_hint;
    env->napiEnv->registerNativeFinalizer(FinalizeExternalBackingStoreForEnvTeardown, finalizeData);

    BackingStoreRef* backingStore = BackingStoreRef::createNonSharedBackingStore(data, length, NapiExternalBackingStoreDeleter, finalizeData);

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, BackingStoreRef* backingStore, size_t length) -> ValueRef* {
            ArrayBufferObjectRef* buffer = ArrayBufferObjectRef::create(state);
            buffer->attachBuffer(backingStore);
            Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
            view->setBuffer(buffer, 0, length, length);
            return view;
        },
        backingStore, length);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_buffer_copy(napi_env env, size_t length, const void* data, void** result_data, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    Evaluator::EvaluatorResult evalResult = Evaluator::execute(
        env->context(), [](ExecutionStateRef* state, size_t length, const void* data, void** resultData) -> ValueRef* {
            ArrayBufferObjectRef* buffer = ArrayBufferObjectRef::create(state);
            buffer->allocateBuffer(state, length);
            if (length > 0 && data != nullptr) {
                memcpy(buffer->rawBuffer(), data, length);
            }
            Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
            view->setBuffer(buffer, 0, length, length);
            if (resultData != nullptr) {
                *resultData = view->rawBuffer();
            }
            return view;
        },
        length, data, result_data);
    napi_status status = SetPendingExceptionFromEvaluatorResult(env, evalResult);
    if (status != napi_ok) {
        return status;
    }
    *result = ToNapi(evalResult.result);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_buffer(napi_env env, napi_value value, bool* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    *result = FromNapi(value)->isUint8ArrayObject();
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_get_buffer_info(napi_env env, napi_value value, void** data, size_t* length)
{
    ValueRef* v = FromNapi(value);
    if (!v->isUint8ArrayObject()) {
        return SetLastError(env, napi_invalid_arg);
    }
    ArrayBufferViewRef* view = v->asArrayBufferView();

    if (data != nullptr) {
        *data = view->rawBuffer();
    }
    if (length != nullptr) {
        *length = view->arrayLength();
    }
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
