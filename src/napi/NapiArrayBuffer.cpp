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
#include <unordered_map>
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
    void* finalizeHint;
};

static void NapiExternalBackingStoreDeleter(void* data, size_t length, void* deleterData)
{
    ExternalBackingStoreFinalizeData* finalizeData = reinterpret_cast<ExternalBackingStoreFinalizeData*>(deleterData);
    if (finalizeData->finalizeCb != nullptr) {
        finalizeData->finalizeCb(finalizeData->env, data, finalizeData->finalizeHint);
    }
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
    finalizeData->finalizeCb(env, finalizeData->nativeData, finalizeData->finalizeHint);

    if (env->pendingException.hasValue()) {
        ValueRef* fatalErr = env->pendingException.value();
        env->pendingException = nullptr;
        napi_fatal_exception(env, ToNapi(fatalErr));
    }

    delete finalizeData;
}

// napi_type_tag_object/napi_check_object_type_tag: Escargot has no spare
// per-object slot left for this (extraData() is already napi_wrap's/
// napi_create_external's), so tags are kept in this file-local side table
// instead, keyed by the object's raw pointer. A GC finalizer erases the
// entry once the object is collected (registered in napi_type_tag_object
// below), so a later, unrelated object allocated at the same address can't
// spuriously read as tagged. This is still not as robust as a real engine
// slot would be: if the finalizer somehow doesn't run before the address is
// reused (it always should for Boehm-GC'd objects here, but this is a
// PoC-level assumption, not a guarantee enforced by the type system) a
// stale/incorrect tag could be observed. Fine for this PoC's scope.
static std::unordered_map<ObjectRef*, std::pair<uint64_t, uint64_t>> g_typeTags;

static void NapiTypeTagFinalizer(void* self, void* data)
{
    g_typeTags.erase(reinterpret_cast<ObjectRef*>(self));
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

    ExecutionStateRef* state = env->executionState;

    ArrayBufferObjectRef* buf = ArrayBufferObjectRef::create(state);
    buf->allocateBuffer(state, byte_length);

    if (data != nullptr) {
        *data = buf->rawBuffer();
    }
    *result = ToNapi(buf);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_external_arraybuffer(napi_env env, void* external_data, size_t byte_length, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;

    ExternalBackingStoreFinalizeData* finalizeData = new ExternalBackingStoreFinalizeData();
    finalizeData->env = env;
    finalizeData->finalizeCb = finalize_cb;
    finalizeData->finalizeHint = finalize_hint;

    BackingStoreRef* backingStore = BackingStoreRef::createNonSharedBackingStore(external_data, byte_length, NapiExternalBackingStoreDeleter, finalizeData);

    ArrayBufferObjectRef* buf = ArrayBufferObjectRef::create(state);
    buf->attachBuffer(backingStore);

    *result = ToNapi(buf);
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
    ExecutionStateRef* state = env->executionState;

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
        env->pendingException = ErrorObjectRef::create(state, ErrorObjectRef::RangeError, StringRef::createFromASCII("byte_offset + length must be smaller than the size in bytes of the array passed in"));
        return SetLastError(env, napi_pending_exception);
    }
    if (elementSize > 1 && (byte_offset % elementSize) != 0) {
        env->pendingException = ErrorObjectRef::create(state, ErrorObjectRef::RangeError, StringRef::createFromASCII("start offset of typed array must be a multiple of the byte length of the element type"));
        return SetLastError(env, napi_pending_exception);
    }

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
        return SetLastError(env, napi_invalid_arg);
    }

    view->setBuffer(buf, byte_offset, byteLength, length);
    *result = ToNapi(view);
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
    ExecutionStateRef* state = env->executionState;

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

    DataViewObjectRef* view = DataViewObjectRef::create(state);
    view->setBuffer(buf, byte_offset, length);

    *result = ToNapi(view);
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
    ExecutionStateRef* state = env->executionState;

    ObjectRef* obj = ObjectRef::create(state);
    obj->setExtraData(data);

    if (finalize_cb != nullptr) {
        ExternalObjectFinalizeData* finalizeData = new ExternalObjectFinalizeData();
        finalizeData->env = env;
        finalizeData->finalizeCb = finalize_cb;
        finalizeData->nativeData = data;
        finalizeData->finalizeHint = finalize_hint;
        Memory::gcRegisterFinalizer(obj, NapiExternalObjectFinalizer, finalizeData);
    }

    *result = ToNapi(obj);
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

    // matches Node's own contract: an object may only be tagged once
    if (g_typeTags.find(obj) != g_typeTags.end()) {
        return SetLastError(env, napi_invalid_arg);
    }

    g_typeTags[obj] = std::make_pair(type_tag->lower, type_tag->upper);
    Memory::gcRegisterFinalizer(obj, NapiTypeTagFinalizer, nullptr);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_check_object_type_tag(napi_env env, napi_value value, const napi_type_tag* type_tag, bool* result)
{
    ValueRef* v = FromNapi(value);
    if (!v->isObject()) {
        return SetLastError(env, napi_object_expected);
    }
    ObjectRef* obj = v->asObject();

    auto iter = g_typeTags.find(obj);
    if (iter == g_typeTags.end()) {
        *result = false;
    } else {
        *result = (iter->second.first == type_tag->lower && iter->second.second == type_tag->upper);
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

    ExecutionStateRef* state = env->executionState;

    ArrayBufferObjectRef* buf = ArrayBufferObjectRef::create(state);
    buf->allocateBuffer(state, length);

    Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
    view->setBuffer(buf, 0, length, length);

    if (data != nullptr) {
        *data = view->rawBuffer();
    }
    *result = ToNapi(view);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_external_buffer(napi_env env, size_t length, void* data, node_api_basic_finalize finalize_cb, void* finalize_hint, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;

    ExternalBackingStoreFinalizeData* finalizeData = new ExternalBackingStoreFinalizeData();
    finalizeData->env = env;
    finalizeData->finalizeCb = finalize_cb;
    finalizeData->finalizeHint = finalize_hint;

    BackingStoreRef* backingStore = BackingStoreRef::createNonSharedBackingStore(data, length, NapiExternalBackingStoreDeleter, finalizeData);

    ArrayBufferObjectRef* buf = ArrayBufferObjectRef::create(state);
    buf->attachBuffer(backingStore);

    Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
    view->setBuffer(buf, 0, length, length);

    *result = ToNapi(view);
    return napi_ok;
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_buffer_copy(napi_env env, size_t length, const void* data, void** result_data, napi_value* result)
{
    if (result == nullptr) {
        return SetLastError(env, napi_invalid_arg);
    }

    ExecutionStateRef* state = env->executionState;

    ArrayBufferObjectRef* buf = ArrayBufferObjectRef::create(state);
    buf->allocateBuffer(state, length);
    if (length > 0 && data != nullptr) {
        memcpy(buf->rawBuffer(), data, length);
    }

    Uint8ArrayObjectRef* view = Uint8ArrayObjectRef::create(state);
    view->setBuffer(buf, 0, length, length);

    if (result_data != nullptr) {
        *result_data = view->rawBuffer();
    }
    *result = ToNapi(view);
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
