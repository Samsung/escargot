/* Copyright (c) 2026-present Samsung Electronics Co., Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ESCARGOT_C_H
#define ESCARGOT_C_H

#include <stddef.h>
#include <stdint.h>

#ifndef ESCARGOT_C_EXPORT
#if defined(_WIN32)
#if defined(ENABLE_C_API)
#define ESCARGOT_C_EXPORT __declspec(dllexport)
#else
#define ESCARGOT_C_EXPORT
#endif
#else
#define ESCARGOT_C_EXPORT __attribute__((visibility("default")))
#endif
#endif
#if defined(_WIN32)
#define ESCARGOT_C_CALL __cdecl
#else
#define ESCARGOT_C_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define ESCARGOT_C_API_VERSION 1

typedef struct escargot_vm__* escargot_vm;
typedef struct escargot_context__* escargot_context;
typedef struct escargot_value__* escargot_value;
typedef struct escargot_execution_state__* escargot_execution_state;
typedef struct escargot_result__* escargot_result;
typedef struct escargot_reference__* escargot_reference;
typedef struct escargot_handle_scope__* escargot_handle_scope;

/* Fixed-width status/type codes; no compiler-dependent enum ABI. */
typedef int32_t escargot_status;
#define ESCARGOT_OK 0
#define ESCARGOT_PENDING_EXCEPTION 1
#define ESCARGOT_INVALID_ARGUMENT 2
#define ESCARGOT_INVALID_STATE 3
#define ESCARGOT_TYPE_ERROR 4
#define ESCARGOT_OUT_OF_MEMORY 5
#define ESCARGOT_INTERNAL_ERROR 6
#define ESCARGOT_BUFFER_TOO_SMALL 7
#define ESCARGOT_SCOPE_MISMATCH 8

typedef int32_t escargot_value_type;
#define ESCARGOT_UNDEFINED 0
#define ESCARGOT_NULL 1
#define ESCARGOT_BOOLEAN 2
#define ESCARGOT_NUMBER 3
#define ESCARGOT_STRING 4
#define ESCARGOT_SYMBOL 5
#define ESCARGOT_BIGINT 6
#define ESCARGOT_OBJECT 7

/* All handles belong to their creating engine thread. Use and release them on
 * that thread, before finalization. Values are borrowed GC pointers: retain a
 * reference or add them to a scope before storing them outside the native
 * stack. Result handles root their value, exception and stack until release.
 * Input handles must be live and values must belong to the executing VM.
 * A VM may contain multiple contexts; values may cross contexts in that VM.
 * Null is never a JS value: use escargot_value_null() for JavaScript null.
 * On failure, output handles are null and scalar outputs are zero unless
 * explicitly documented otherwise. No C++ exception crosses this API.
 */
ESCARGOT_C_EXPORT uint32_t ESCARGOT_C_CALL escargot_api_version(void);
/* initialize adopts an already initialized host without replacing its platform.
 * finalize only tears down a runtime initialized by this API and rejects live
 * C API resources. Custom C++ platforms can also host the C API. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_initialize(void);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_finalize(void);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_thread_initialize(void);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_thread_finalize(void);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_vm_create(const char* locale, const char* timezone, escargot_vm* out_vm);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_vm_destroy(escargot_vm vm);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_context_create(escargot_vm vm, escargot_context* out_context);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_context_destroy(escargot_context context);
ESCARGOT_C_EXPORT escargot_vm ESCARGOT_C_CALL escargot_context_get_vm(escargot_context context);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_context_get_global(escargot_context context);

/* Each execution returns an independent, owned result, including on a JS
 * exception. This preserves nested catches/rethrows without overwriting an
 * outer exception. out_result is null on an API failure. Source lengths are
 * bytes (UTF-8); embedded NUL is permitted. Jobs are pumped explicitly. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_context_eval(escargot_context context, const char* source, size_t source_length, const char* source_name, size_t name_length, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_vm_run_jobs(escargot_vm vm, escargot_result* out_result);
ESCARGOT_C_EXPORT int ESCARGOT_C_CALL escargot_vm_has_pending_job(escargot_vm vm);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_result_status(escargot_result result);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_result_value(escargot_result result);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_result_exception(escargot_result result);
ESCARGOT_C_EXPORT size_t ESCARGOT_C_CALL escargot_result_stack_size(escargot_result result);
/* Source/function strings are borrowed from result. Positions follow the
 * engine's Evaluator stack format. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_result_stack_frame(escargot_result result, size_t index, escargot_value* out_source_name, escargot_value* out_function_name, size_t* out_line, size_t* out_column);
ESCARGOT_C_EXPORT void ESCARGOT_C_CALL escargot_result_release(escargot_result result);

ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_reference_create(escargot_value value, escargot_reference* out_reference);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_reference_retain(escargot_reference reference);
ESCARGOT_C_EXPORT void ESCARGOT_C_CALL escargot_reference_release(escargot_reference reference);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_reference_get(escargot_reference reference);
/* A cleared weak reference cannot be strengthened. Reference retain/release
 * counts owners of the handle, independently of its strong/weak GC mode. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_reference_set_weak(escargot_reference reference, int weak);
ESCARGOT_C_EXPORT int ESCARGOT_C_CALL escargot_reference_is_weak(escargot_reference reference);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_handle_scope_open(escargot_handle_scope* out_scope);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_handle_scope_add(escargot_handle_scope scope, escargot_value value);
/* Move one value to the parent scope. Exactly one escape per scope. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_handle_scope_escape(escargot_handle_scope scope, escargot_value value);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_handle_scope_close(escargot_handle_scope scope);

ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_value_undefined(void);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_value_null(void);
ESCARGOT_C_EXPORT escargot_value ESCARGOT_C_CALL escargot_value_boolean(int value);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_number(double value, escargot_value* out_value);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_get_type(escargot_value value, escargot_value_type* out_type);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_get_number(escargot_value value, double* out_number);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_get_boolean(escargot_value value, int* out_boolean);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_string_create_utf8(const char* data, size_t length, escargot_value* out_value);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_string_create_utf16(const uint16_t* data, size_t length, escargot_value* out_value);
/* Copy APIs report required length excluding any terminator. They never add
 * a terminator or partially copy. NULL buffer + zero capacity queries size.
 * out_length is also populated on ESCARGOT_BUFFER_TOO_SMALL. */
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_string_copy_utf8(escargot_value value, char* buffer, size_t capacity, size_t* out_length);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_string_copy_utf16(escargot_value value, uint16_t* buffer, size_t capacity, size_t* out_length);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_to_string(escargot_context context, escargot_value value, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_value_to_number(escargot_context context, escargot_value value, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_create(escargot_context context, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_array_create(escargot_context context, uint32_t length, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_get_property(escargot_context context, escargot_value object, escargot_value key, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_set_property(escargot_context context, escargot_value object, escargot_value key, escargot_value value, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_has_property(escargot_context context, escargot_value object, escargot_value key, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_delete_property(escargot_context context, escargot_value object, escargot_value key, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_call(escargot_context context, escargot_value function, escargot_value receiver, size_t argc, const escargot_value* argv, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_construct(escargot_context context, escargot_value function, size_t argc, const escargot_value* argv, escargot_result* out_result);

/* Synchronous callbacks only. Return OK with a value (NULL means undefined),
 * or PENDING_EXCEPTION with the exception value in out_value. Never unwind
 * through this callback. The host keeps data alive while the function is
 * reachable. Callback values are borrowed for the duration of the call. */
typedef escargot_status(ESCARGOT_C_CALL* escargot_function_callback)(escargot_context context, escargot_value receiver, size_t argc, const escargot_value* argv, escargot_value new_target, void* data, escargot_value* out_value);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_create(escargot_context context, const char* name, size_t name_length, uint32_t argument_count, int constructible, escargot_function_callback callback, void* data, escargot_result* out_result);

/* Templates are owned handles. Do not modify them after instantiation. They
 * preserve per-context function identity for V8-style template adapters. */
typedef struct escargot_template__* escargot_template;
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_template_create(escargot_template* out_template);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_template_create(escargot_context context, const char* name, size_t name_length, uint32_t argument_count, int constructible, escargot_function_callback callback, void* data, escargot_template* out_template);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_template_set(escargot_template target, escargot_value key, escargot_value value, int writable, int enumerable, int configurable);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_template_set_template(escargot_template target, escargot_value key, escargot_template value, int writable, int enumerable, int configurable);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_template_instantiate(escargot_context context, escargot_template target, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_template_get_instance(escargot_template target, escargot_template* out_template);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_template_get_prototype(escargot_template target, escargot_template* out_template);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_function_template_inherit(escargot_template target, escargot_template parent);
ESCARGOT_C_EXPORT void ESCARGOT_C_CALL escargot_template_release(escargot_template target);

/* Backing-store ownership is independent of any individual ArrayBuffer. The
 * external deleter runs under the collector lock: native resource cleanup
 * only, no engine calls or GC allocations. data ownership transfers only on
 * successful creation. callback_data must outlive the backing store. */
typedef struct escargot_backing_store__* escargot_backing_store;
typedef void(ESCARGOT_C_CALL* escargot_backing_store_deleter)(void* data, size_t length, void* callback_data);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_backing_store_create(size_t byte_length, escargot_backing_store* out_store);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_backing_store_create_external(void* data, size_t byte_length, escargot_backing_store_deleter deleter, void* callback_data, escargot_backing_store* out_store);
ESCARGOT_C_EXPORT void* ESCARGOT_C_CALL escargot_backing_store_data(escargot_backing_store store);
ESCARGOT_C_EXPORT size_t ESCARGOT_C_CALL escargot_backing_store_size(escargot_backing_store store);
ESCARGOT_C_EXPORT void ESCARGOT_C_CALL escargot_backing_store_release(escargot_backing_store store);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_array_buffer_create(escargot_context context, escargot_backing_store store, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_array_buffer_get_backing_store(escargot_value buffer, escargot_backing_store* out_store);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_array_buffer_detach(escargot_value buffer);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_promise_create(escargot_context context, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_promise_resolve(escargot_context context, escargot_value promise, escargot_value value, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_promise_reject(escargot_context context, escargot_value promise, escargot_value reason, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_object_define_data_property(escargot_context context, escargot_value object, escargot_value key, escargot_value value, int writable, int enumerable, int configurable, escargot_result* out_result);

/* Execution hook used by native binding adapters. state is borrowed only
 * during the callback. Nested calls use the requested context and link to the
 * currently active state only within the same VM. */
typedef escargot_value(ESCARGOT_C_CALL* escargot_execute_callback)(escargot_execution_state state, void* data);
ESCARGOT_C_EXPORT escargot_status ESCARGOT_C_CALL escargot_context_execute(escargot_context context, escargot_execute_callback callback, void* data, escargot_result* out_result);
ESCARGOT_C_EXPORT escargot_context ESCARGOT_C_CALL escargot_execution_get_context(escargot_execution_state state);

#ifdef __cplusplus
}
#endif
#endif
