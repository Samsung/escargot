// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Raw C API version 1 bindings, corresponding to `src/api/EscargotC.h`.
//!
//! Handles are thread confined and require a live runtime. Values are borrowed
//! GC pointers; retain them before storing them in Rust allocations. Calls must
//! obey the C header's ownership and VM constraints. Callbacks must not unwind.
//!
//! By default the build script builds the repository's engine with CMake.
//! Set `ESCARGOT_LIB_DIR` to link an existing shared C API library instead.
//! Coordinate runtime initialization/finalization with any other native host.

#![allow(non_camel_case_types)]
use std::ffi::{c_char, c_int, c_void};

#[repr(C)]
pub struct escargot_vm__ {
    _private: [u8; 0],
}
pub type escargot_vm = *mut escargot_vm__;

#[repr(C)]
pub struct escargot_context__ {
    _private: [u8; 0],
}
pub type escargot_context = *mut escargot_context__;

#[repr(C)]
pub struct escargot_value__ {
    _private: [u8; 0],
}
pub type escargot_value = *mut escargot_value__;

#[repr(C)]
pub struct escargot_execution_state__ {
    _private: [u8; 0],
}
pub type escargot_execution_state = *mut escargot_execution_state__;

#[repr(C)]
pub struct escargot_result__ {
    _private: [u8; 0],
}
pub type escargot_result = *mut escargot_result__;

#[repr(C)]
pub struct escargot_reference__ {
    _private: [u8; 0],
}
pub type escargot_reference = *mut escargot_reference__;

#[repr(C)]
pub struct escargot_handle_scope__ {
    _private: [u8; 0],
}
pub type escargot_handle_scope = *mut escargot_handle_scope__;

#[repr(C)]
pub struct escargot_template__ {
    _private: [u8; 0],
}
pub type escargot_template = *mut escargot_template__;

#[repr(C)]
pub struct escargot_backing_store__ {
    _private: [u8; 0],
}
pub type escargot_backing_store = *mut escargot_backing_store__;

pub type escargot_status = i32;
pub type escargot_value_type = i32;
pub const ESCARGOT_C_API_VERSION: u32 = 1;
pub const ESCARGOT_OK: i32 = 0;
pub const ESCARGOT_PENDING_EXCEPTION: i32 = 1;
pub const ESCARGOT_INVALID_ARGUMENT: i32 = 2;
pub const ESCARGOT_INVALID_STATE: i32 = 3;
pub const ESCARGOT_TYPE_ERROR: i32 = 4;
pub const ESCARGOT_OUT_OF_MEMORY: i32 = 5;
pub const ESCARGOT_INTERNAL_ERROR: i32 = 6;
pub const ESCARGOT_BUFFER_TOO_SMALL: i32 = 7;
pub const ESCARGOT_SCOPE_MISMATCH: i32 = 8;
pub const ESCARGOT_DEPENDENCY_UNAVAILABLE: i32 = 9;
pub const ESCARGOT_UNDEFINED: i32 = 0;
pub const ESCARGOT_NULL: i32 = 1;
pub const ESCARGOT_BOOLEAN: i32 = 2;
pub const ESCARGOT_NUMBER: i32 = 3;
pub const ESCARGOT_STRING: i32 = 4;
pub const ESCARGOT_SYMBOL: i32 = 5;
pub const ESCARGOT_BIGINT: i32 = 6;
pub const ESCARGOT_OBJECT: i32 = 7;

pub type escargot_function_callback = Option<
    unsafe extern "C" fn(
        context: escargot_context,
        receiver: escargot_value,
        argc: usize,
        argv: *const escargot_value,
        new_target: escargot_value,
        data: *mut c_void,
        out_value: *mut escargot_value,
    ) -> escargot_status,
>;
pub type escargot_backing_store_deleter =
    Option<unsafe extern "C" fn(data: *mut c_void, length: usize, callback_data: *mut c_void)>;
pub type escargot_execute_callback = Option<
    unsafe extern "C" fn(state: escargot_execution_state, data: *mut c_void) -> escargot_value,
>;

unsafe extern "C" {
    pub fn escargot_api_version() -> u32;
    pub fn escargot_initialize() -> escargot_status;
    pub fn escargot_finalize() -> escargot_status;
    pub fn escargot_thread_initialize() -> escargot_status;
    pub fn escargot_thread_finalize() -> escargot_status;
    pub fn escargot_vm_create(
        locale: *const c_char,
        timezone: *const c_char,
        out_vm: *mut escargot_vm,
    ) -> escargot_status;
    pub fn escargot_vm_destroy(vm: escargot_vm) -> escargot_status;
    pub fn escargot_context_create(
        vm: escargot_vm,
        out_context: *mut escargot_context,
    ) -> escargot_status;
    pub fn escargot_context_destroy(context: escargot_context) -> escargot_status;
    pub fn escargot_context_get_vm(context: escargot_context) -> escargot_vm;
    pub fn escargot_context_get_global(context: escargot_context) -> escargot_value;
    pub fn escargot_context_eval(
        context: escargot_context,
        source: *const c_char,
        source_length: usize,
        source_name: *const c_char,
        name_length: usize,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_vm_run_jobs(
        vm: escargot_vm,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_vm_has_pending_job(vm: escargot_vm) -> c_int;
    pub fn escargot_result_status(result: escargot_result) -> escargot_status;
    pub fn escargot_result_value(result: escargot_result) -> escargot_value;
    pub fn escargot_result_exception(result: escargot_result) -> escargot_value;
    pub fn escargot_result_stack_size(result: escargot_result) -> usize;
    pub fn escargot_result_stack_frame(
        result: escargot_result,
        index: usize,
        out_source_name: *mut escargot_value,
        out_function_name: *mut escargot_value,
        out_line: *mut usize,
        out_column: *mut usize,
    ) -> escargot_status;
    pub fn escargot_result_release(result: escargot_result);
    pub fn escargot_reference_create(
        value: escargot_value,
        out_reference: *mut escargot_reference,
    ) -> escargot_status;
    pub fn escargot_reference_retain(reference: escargot_reference) -> escargot_status;
    pub fn escargot_reference_release(reference: escargot_reference);
    pub fn escargot_reference_get(reference: escargot_reference) -> escargot_value;
    pub fn escargot_reference_set_weak(
        reference: escargot_reference,
        weak: c_int,
    ) -> escargot_status;
    pub fn escargot_reference_is_weak(reference: escargot_reference) -> c_int;
    pub fn escargot_handle_scope_open(out_scope: *mut escargot_handle_scope) -> escargot_status;
    pub fn escargot_handle_scope_add(
        scope: escargot_handle_scope,
        value: escargot_value,
    ) -> escargot_status;
    pub fn escargot_handle_scope_escape(
        scope: escargot_handle_scope,
        value: escargot_value,
    ) -> escargot_status;
    pub fn escargot_handle_scope_close(scope: escargot_handle_scope) -> escargot_status;
    pub fn escargot_value_undefined() -> escargot_value;
    pub fn escargot_value_null() -> escargot_value;
    pub fn escargot_value_boolean(value: c_int) -> escargot_value;
    pub fn escargot_value_number(value: f64, out_value: *mut escargot_value) -> escargot_status;
    pub fn escargot_value_get_type(
        value: escargot_value,
        out_type: *mut escargot_value_type,
    ) -> escargot_status;
    pub fn escargot_value_get_number(
        value: escargot_value,
        out_number: *mut f64,
    ) -> escargot_status;
    pub fn escargot_value_get_boolean(
        value: escargot_value,
        out_boolean: *mut c_int,
    ) -> escargot_status;
    pub fn escargot_string_create_utf8(
        data: *const c_char,
        length: usize,
        out_value: *mut escargot_value,
    ) -> escargot_status;
    pub fn escargot_string_create_utf16(
        data: *const u16,
        length: usize,
        out_value: *mut escargot_value,
    ) -> escargot_status;
    pub fn escargot_string_copy_utf8(
        value: escargot_value,
        buffer: *mut c_char,
        capacity: usize,
        out_length: *mut usize,
    ) -> escargot_status;
    pub fn escargot_string_copy_utf16(
        value: escargot_value,
        buffer: *mut u16,
        capacity: usize,
        out_length: *mut usize,
    ) -> escargot_status;
    pub fn escargot_value_to_string(
        context: escargot_context,
        value: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_value_to_number(
        context: escargot_context,
        value: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_create(
        context: escargot_context,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_array_create(
        context: escargot_context,
        length: u32,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_get_property(
        context: escargot_context,
        object: escargot_value,
        key: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_set_property(
        context: escargot_context,
        object: escargot_value,
        key: escargot_value,
        value: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_has_property(
        context: escargot_context,
        object: escargot_value,
        key: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_delete_property(
        context: escargot_context,
        object: escargot_value,
        key: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_function_call(
        context: escargot_context,
        function: escargot_value,
        receiver: escargot_value,
        argc: usize,
        argv: *const escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_function_construct(
        context: escargot_context,
        function: escargot_value,
        argc: usize,
        argv: *const escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_function_create(
        context: escargot_context,
        name: *const c_char,
        name_length: usize,
        argument_count: u32,
        constructible: c_int,
        callback: escargot_function_callback,
        data: *mut c_void,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_template_create(out_template: *mut escargot_template)
        -> escargot_status;
    pub fn escargot_function_template_create(
        context: escargot_context,
        name: *const c_char,
        name_length: usize,
        argument_count: u32,
        constructible: c_int,
        callback: escargot_function_callback,
        data: *mut c_void,
        out_template: *mut escargot_template,
    ) -> escargot_status;
    pub fn escargot_template_set(
        target: escargot_template,
        key: escargot_value,
        value: escargot_value,
        writable: c_int,
        enumerable: c_int,
        configurable: c_int,
    ) -> escargot_status;
    pub fn escargot_template_set_template(
        target: escargot_template,
        key: escargot_value,
        value: escargot_template,
        writable: c_int,
        enumerable: c_int,
        configurable: c_int,
    ) -> escargot_status;
    pub fn escargot_template_instantiate(
        context: escargot_context,
        target: escargot_template,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_function_template_get_instance(
        target: escargot_template,
        out_template: *mut escargot_template,
    ) -> escargot_status;
    pub fn escargot_function_template_get_prototype(
        target: escargot_template,
        out_template: *mut escargot_template,
    ) -> escargot_status;
    pub fn escargot_function_template_inherit(
        target: escargot_template,
        parent: escargot_template,
    ) -> escargot_status;
    pub fn escargot_template_release(target: escargot_template);
    pub fn escargot_backing_store_create(
        byte_length: usize,
        out_store: *mut escargot_backing_store,
    ) -> escargot_status;
    pub fn escargot_backing_store_create_external(
        data: *mut c_void,
        byte_length: usize,
        deleter: escargot_backing_store_deleter,
        callback_data: *mut c_void,
        out_store: *mut escargot_backing_store,
    ) -> escargot_status;
    pub fn escargot_backing_store_data(store: escargot_backing_store) -> *mut c_void;
    pub fn escargot_backing_store_size(store: escargot_backing_store) -> usize;
    pub fn escargot_backing_store_release(store: escargot_backing_store);
    pub fn escargot_array_buffer_create(
        context: escargot_context,
        store: escargot_backing_store,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_array_buffer_get_backing_store(
        buffer: escargot_value,
        out_store: *mut escargot_backing_store,
    ) -> escargot_status;
    pub fn escargot_array_buffer_detach(buffer: escargot_value) -> escargot_status;
    pub fn escargot_promise_create(
        context: escargot_context,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_promise_resolve(
        context: escargot_context,
        promise: escargot_value,
        value: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_promise_reject(
        context: escargot_context,
        promise: escargot_value,
        reason: escargot_value,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_object_define_data_property(
        context: escargot_context,
        object: escargot_value,
        key: escargot_value,
        value: escargot_value,
        writable: c_int,
        enumerable: c_int,
        configurable: c_int,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_context_execute(
        context: escargot_context,
        callback: escargot_execute_callback,
        data: *mut c_void,
        out_result: *mut escargot_result,
    ) -> escargot_status;
    pub fn escargot_execution_get_context(state: escargot_execution_state) -> escargot_context;
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ptr;

    unsafe extern "C" fn add_offset(
        _context: escargot_context,
        _receiver: escargot_value,
        argc: usize,
        argv: *const escargot_value,
        _new_target: escargot_value,
        data: *mut c_void,
        out: *mut escargot_value,
    ) -> escargot_status {
        // Rust panics must be handled before returning through the C ABI.
        std::panic::catch_unwind(|| {
            if argc != 1 || data.is_null() || out.is_null() {
                return ESCARGOT_INVALID_ARGUMENT;
            }
            let mut number = 0.0;
            let status = unsafe { escargot_value_get_number(*argv, &mut number) };
            if status != ESCARGOT_OK {
                return status;
            }
            unsafe { escargot_value_number(number + *data.cast::<f64>(), out) }
        })
        .unwrap_or(ESCARGOT_INTERNAL_ERROR)
    }

    #[test]
    fn raw_callback_round_trip_uses_the_c_abi() {
        unsafe {
            assert_eq!(escargot_api_version(), ESCARGOT_C_API_VERSION);
            assert_eq!(escargot_initialize(), ESCARGOT_OK);
            let (mut vm, mut context) = (ptr::null_mut(), ptr::null_mut());
            assert_eq!(
                escargot_vm_create(ptr::null(), ptr::null(), &mut vm),
                ESCARGOT_OK
            );
            assert_eq!(escargot_context_create(vm, &mut context), ESCARGOT_OK);
            let mut function = ptr::null_mut();
            let offset = 40.0f64;
            assert_eq!(
                escargot_function_create(
                    context,
                    c"add".as_ptr(),
                    3,
                    1,
                    0,
                    Some(add_offset),
                    (&offset as *const f64).cast_mut().cast(),
                    &mut function
                ),
                ESCARGOT_OK
            );
            let mut argument = ptr::null_mut();
            assert_eq!(escargot_value_number(2.0, &mut argument), ESCARGOT_OK);
            let mut result = ptr::null_mut();
            assert_eq!(
                escargot_function_call(
                    context,
                    escargot_result_value(function),
                    escargot_value_undefined(),
                    1,
                    &argument,
                    &mut result
                ),
                ESCARGOT_OK
            );
            let mut answer = 0.0;
            assert_eq!(
                escargot_value_get_number(escargot_result_value(result), &mut answer),
                ESCARGOT_OK
            );
            assert_eq!(answer, 42.0);
            escargot_result_release(result);
            escargot_result_release(function);
            assert_eq!(escargot_context_destroy(context), ESCARGOT_OK);
            assert_eq!(escargot_vm_destroy(vm), ESCARGOT_OK);
            assert_eq!(escargot_finalize(), ESCARGOT_OK);
        }
    }
}
