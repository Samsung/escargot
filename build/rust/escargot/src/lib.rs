// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

//! Safe, thread-confined access to Escargot through its C API.
//!
//! `Runtime`, `Vm`, `Context`, and `Value` share ownership. Values hold GC roots
//! and keep their context, VM, and runtime alive, including thrown values.
//! Context operations reject values from another VM before calling the engine.
//! Multiple runtimes on one thread share a session; creating one on another
//! thread while that session is alive returns `Error::RuntimeBusy`.
//!
//! ```
//! let runtime = escargot::Runtime::new()?;
//! let context = runtime.vm()?.context()?;
//! assert_eq!(context.eval("21 * 2", "example.js")?.as_number()?, 42.0);
//! # Ok::<(), escargot::Error>(())
//! ```
//!
//! Handles cannot be sent to another thread:
//! ```compile_fail
//! let runtime = escargot::Runtime::new().unwrap();
//! std::thread::spawn(move || drop(runtime));
//! ```
//!
//! The default source build requires CMake and a C++ compiler, and disables
//! ICU, Temporal, and WebAssembly. Set `ESCARGOT_LIB_DIR` to use a shared C API
//! build with your chosen engine features; its directory must also be on the
//! platform's runtime library search path. `escargot-sys` exposes the complete
//! unsafe ABI, including callbacks, templates, scopes, and backing stores.

pub use escargot_sys as sys;
use std::cell::RefCell;
use std::ffi::CString;
use std::fmt;
use std::ptr::{self, NonNull};
use std::rc::{Rc, Weak};
use std::sync::{Mutex, MutexGuard, TryLockError};
use sys as ffi;

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Debug)]
pub enum Error {
    Api(ffi::escargot_status),
    JavaScript(Box<Exception>),
    WrongVm,
    RuntimeBusy,
    IncompatibleApiVersion(u32),
    InvalidUtf8,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Api(status) => write!(f, "Escargot C API status {status}"),
            Self::JavaScript(_) => f.write_str("JavaScript exception"),
            Self::WrongVm => f.write_str("value belongs to a different VM"),
            Self::RuntimeBusy => f.write_str("another thread owns the Escargot runtime"),
            Self::IncompatibleApiVersion(version) => {
                write!(f, "unsupported Escargot C API version {version}")
            }
            Self::InvalidUtf8 => f.write_str("string is not valid UTF-8; use to_utf16"),
        }
    }
}

impl std::error::Error for Error {}

#[derive(Debug)]
pub struct Exception {
    pub value: Value,
    pub stack: Vec<StackFrame>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StackFrame {
    pub source_name: Option<String>,
    pub function_name: Option<String>,
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Symbol,
    BigInt,
    Object,
}

#[derive(Debug, Clone, Copy)]
pub struct PropertyAttributes {
    pub writable: bool,
    pub enumerable: bool,
    pub configurable: bool,
}

impl Default for PropertyAttributes {
    fn default() -> Self {
        Self {
            writable: true,
            enumerable: true,
            configurable: true,
        }
    }
}

fn check(status: ffi::escargot_status) -> Result<()> {
    if status == ffi::ESCARGOT_OK {
        Ok(())
    } else {
        Err(Error::Api(status))
    }
}

fn nonnull<T>(value: *mut T) -> Result<NonNull<T>> {
    NonNull::new(value).ok_or(Error::Api(ffi::ESCARGOT_INTERNAL_ERROR))
}

// Global::platform is process-wide. Keep its exclusive owner alive until the
// last Rust resource is released, even if the public Runtime was dropped first.
static RUNTIME_LOCK: Mutex<()> = Mutex::new(());
thread_local! {
    static CURRENT_RUNTIME: RefCell<Weak<RuntimeInner>> = const { RefCell::new(Weak::new()) };
}

struct RuntimeInner {
    guard: Option<MutexGuard<'static, ()>>,
}

impl Drop for RuntimeInner {
    fn drop(&mut self) {
        // Safe handles have all gone away. If an unsafe client retained C
        // resources, keep the lock instead of allowing another owner to race.
        if unsafe { ffi::escargot_finalize() } != ffi::ESCARGOT_OK {
            std::mem::forget(self.guard.take());
        }
    }
}

#[derive(Clone)]
pub struct Runtime(Rc<RuntimeInner>);

impl Runtime {
    pub fn new() -> Result<Self> {
        CURRENT_RUNTIME.with(|current| {
            if let Some(runtime) = current.borrow().upgrade() {
                return Ok(Self(runtime));
            }
            let guard = match RUNTIME_LOCK.try_lock() {
                Ok(guard) => guard,
                Err(TryLockError::Poisoned(error)) => error.into_inner(),
                Err(TryLockError::WouldBlock) => return Err(Error::RuntimeBusy),
            };
            let version = unsafe { ffi::escargot_api_version() };
            if version != ffi::ESCARGOT_C_API_VERSION {
                return Err(Error::IncompatibleApiVersion(version));
            }
            check(unsafe { ffi::escargot_initialize() })?;
            let runtime = Rc::new(RuntimeInner { guard: Some(guard) });
            *current.borrow_mut() = Rc::downgrade(&runtime);
            Ok(Self(runtime))
        })
    }

    pub fn vm(&self) -> Result<Vm> {
        self.vm_with_options(None, None)
    }

    pub fn vm_with_options(&self, locale: Option<&str>, timezone: Option<&str>) -> Result<Vm> {
        let string = |value: Option<&str>| {
            value
                .map(CString::new)
                .transpose()
                .map_err(|_| Error::Api(ffi::ESCARGOT_INVALID_ARGUMENT))
        };
        let locale = string(locale)?;
        let timezone = string(timezone)?;
        let mut raw = ptr::null_mut();
        check(unsafe {
            ffi::escargot_vm_create(
                locale.as_ref().map_or(ptr::null(), |s| s.as_ptr()),
                timezone.as_ref().map_or(ptr::null(), |s| s.as_ptr()),
                &mut raw,
            )
        })?;
        Ok(Vm(Rc::new(VmInner {
            raw: nonnull(raw)?,
            _runtime: self.0.clone(),
        })))
    }
}

struct VmInner {
    raw: NonNull<ffi::escargot_vm__>,
    _runtime: Rc<RuntimeInner>,
}

impl Drop for VmInner {
    fn drop(&mut self) {
        unsafe {
            ffi::escargot_vm_destroy(self.raw.as_ptr());
        }
    }
}

#[derive(Clone)]
pub struct Vm(Rc<VmInner>);

impl Vm {
    /// Borrow the native handle. Unsafe clients must not destroy it or finalize
    /// the runtime, and must keep this owner alive for the duration of its use.
    pub fn as_raw(&self) -> ffi::escargot_vm {
        self.0.raw.as_ptr()
    }

    pub fn context(&self) -> Result<Context> {
        let mut raw = ptr::null_mut();
        check(unsafe { ffi::escargot_context_create(self.0.raw.as_ptr(), &mut raw) })?;
        Ok(Context(Rc::new(ContextInner {
            raw: nonnull(raw)?,
            vm: self.0.clone(),
        })))
    }

    pub fn has_pending_job(&self) -> bool {
        unsafe { ffi::escargot_vm_has_pending_job(self.0.raw.as_ptr()) != 0 }
    }
}

struct ContextInner {
    raw: NonNull<ffi::escargot_context__>,
    vm: Rc<VmInner>,
}

impl Drop for ContextInner {
    fn drop(&mut self) {
        unsafe {
            ffi::escargot_context_destroy(self.raw.as_ptr());
        }
    }
}

#[derive(Clone)]
pub struct Context(Rc<ContextInner>);

struct OwnedResult(NonNull<ffi::escargot_result__>);
impl Drop for OwnedResult {
    fn drop(&mut self) {
        unsafe {
            ffi::escargot_result_release(self.0.as_ptr());
        }
    }
}

impl Context {
    fn raw(&self) -> ffi::escargot_context {
        self.0.raw.as_ptr()
    }

    /// Borrow the native handle for advanced `sys` operations. Keep this owner
    /// alive and do not destroy the handle or finalize the runtime through C.
    pub fn as_raw(&self) -> ffi::escargot_context {
        self.raw()
    }

    /// Retain a value returned by a raw C API operation.
    ///
    /// # Safety
    /// `value` must be a live, non-null JS value on this thread, belonging to
    /// this context's VM. Its native owner must keep it rooted until this call
    /// completes. It must not be a context, VM, or another non-value handle.
    pub unsafe fn root_value(&self, value: ffi::escargot_value) -> Result<Value> {
        self.root(value)
    }

    fn require_vm(&self, values: &[&Value]) -> Result<()> {
        if values
            .iter()
            .all(|value| Rc::ptr_eq(&self.0.vm, &value.0.context.vm))
        {
            Ok(())
        } else {
            Err(Error::WrongVm)
        }
    }

    // All callers pass live values borrowed from an owned result, a context,
    // or an immediately preceding allocation. Root before the next engine call.
    fn root(&self, raw: ffi::escargot_value) -> Result<Value> {
        let mut reference = ptr::null_mut();
        check(unsafe { ffi::escargot_reference_create(raw, &mut reference) })?;
        Ok(Value(Rc::new(ValueInner {
            reference: nonnull(reference)?,
            context: self.0.clone(),
        })))
    }

    fn execute(&self, call: impl FnOnce(*mut ffi::escargot_result) -> i32) -> Result<Value> {
        let mut raw = ptr::null_mut();
        let status = call(&mut raw);
        // Even unexpected error statuses must release a returned result.
        let result = NonNull::new(raw).map(OwnedResult);
        if status != ffi::ESCARGOT_OK && status != ffi::ESCARGOT_PENDING_EXCEPTION {
            return Err(Error::Api(status));
        }
        let result = result.ok_or(Error::Api(ffi::ESCARGOT_INTERNAL_ERROR))?;
        let raw = result.0.as_ptr();
        if status == ffi::ESCARGOT_OK {
            return self.root(unsafe { ffi::escargot_result_value(raw) });
        }
        let value = self.root(unsafe { ffi::escargot_result_exception(raw) })?;
        let mut stack = Vec::new();
        for index in 0..unsafe { ffi::escargot_result_stack_size(raw) } {
            let (mut source, mut function) = (ptr::null_mut(), ptr::null_mut());
            let (mut line, mut column) = (0, 0);
            check(unsafe {
                ffi::escargot_result_stack_frame(
                    raw,
                    index,
                    &mut source,
                    &mut function,
                    &mut line,
                    &mut column,
                )
            })?;
            let name = |text: ffi::escargot_value| -> Result<Option<String>> {
                if text.is_null() {
                    return Ok(None);
                }
                Ok(Some(String::from_utf16_lossy(
                    &self.root(text)?.to_utf16()?,
                )))
            };
            stack.push(StackFrame {
                source_name: name(source)?,
                function_name: name(function)?,
                line,
                column,
            });
        }
        Err(Error::JavaScript(Box::new(Exception { value, stack })))
    }

    fn create_value(&self, call: impl FnOnce(*mut ffi::escargot_value) -> i32) -> Result<Value> {
        let mut raw = ptr::null_mut();
        check(call(&mut raw))?;
        self.root(raw)
    }

    pub fn eval(&self, source: &str, source_name: &str) -> Result<Value> {
        self.execute(|out| unsafe {
            ffi::escargot_context_eval(
                self.raw(),
                source.as_ptr().cast(),
                source.len(),
                source_name.as_ptr().cast(),
                source_name.len(),
                out,
            )
        })
    }

    /// Drain this VM's pending jobs, stopping at the first uncaught exception.
    /// Jobs are never implicitly drained by `eval`, `call`, or promise settlement.
    pub fn run_jobs(&self) -> Result<Value> {
        self.execute(|out| unsafe { ffi::escargot_vm_run_jobs(self.0.vm.raw.as_ptr(), out) })
    }

    pub fn global(&self) -> Result<Value> {
        self.root(unsafe { ffi::escargot_context_get_global(self.raw()) })
    }
    pub fn undefined(&self) -> Result<Value> {
        self.root(unsafe { ffi::escargot_value_undefined() })
    }
    pub fn null(&self) -> Result<Value> {
        self.root(unsafe { ffi::escargot_value_null() })
    }
    pub fn boolean(&self, value: bool) -> Result<Value> {
        self.root(unsafe { ffi::escargot_value_boolean(value.into()) })
    }
    pub fn number(&self, value: f64) -> Result<Value> {
        self.create_value(|out| unsafe { ffi::escargot_value_number(value, out) })
    }
    pub fn string(&self, value: &str) -> Result<Value> {
        self.create_value(|out| unsafe {
            ffi::escargot_string_create_utf8(value.as_ptr().cast(), value.len(), out)
        })
    }
    pub fn string_utf16(&self, value: &[u16]) -> Result<Value> {
        self.create_value(|out| unsafe {
            ffi::escargot_string_create_utf16(value.as_ptr(), value.len(), out)
        })
    }
    pub fn object(&self) -> Result<Value> {
        self.execute(|out| unsafe { ffi::escargot_object_create(self.raw(), out) })
    }
    pub fn array(&self, length: u32) -> Result<Value> {
        self.execute(|out| unsafe { ffi::escargot_array_create(self.raw(), length, out) })
    }
    pub fn promise(&self) -> Result<Value> {
        self.execute(|out| unsafe { ffi::escargot_promise_create(self.raw(), out) })
    }

    pub fn get(&self, object: &Value, key: &Value) -> Result<Value> {
        self.require_vm(&[object, key])?;
        self.execute(|out| unsafe {
            ffi::escargot_object_get_property(self.raw(), object.raw(), key.raw(), out)
        })
    }
    pub fn set(&self, object: &Value, key: &Value, value: &Value) -> Result<bool> {
        self.require_vm(&[object, key, value])?;
        self.execute(|out| unsafe {
            ffi::escargot_object_set_property(self.raw(), object.raw(), key.raw(), value.raw(), out)
        })?
        .as_boolean()
    }
    pub fn has(&self, object: &Value, key: &Value) -> Result<bool> {
        self.require_vm(&[object, key])?;
        self.execute(|out| unsafe {
            ffi::escargot_object_has_property(self.raw(), object.raw(), key.raw(), out)
        })?
        .as_boolean()
    }
    pub fn delete(&self, object: &Value, key: &Value) -> Result<bool> {
        self.require_vm(&[object, key])?;
        self.execute(|out| unsafe {
            ffi::escargot_object_delete_property(self.raw(), object.raw(), key.raw(), out)
        })?
        .as_boolean()
    }
    pub fn define_property(
        &self,
        object: &Value,
        key: &Value,
        value: &Value,
        attributes: PropertyAttributes,
    ) -> Result<bool> {
        self.require_vm(&[object, key, value])?;
        self.execute(|out| unsafe {
            ffi::escargot_object_define_data_property(
                self.raw(),
                object.raw(),
                key.raw(),
                value.raw(),
                attributes.writable.into(),
                attributes.enumerable.into(),
                attributes.configurable.into(),
                out,
            )
        })?
        .as_boolean()
    }
    pub fn coerce_to_string(&self, value: &Value) -> Result<Value> {
        self.require_vm(&[value])?;
        self.execute(|out| unsafe { ffi::escargot_value_to_string(self.raw(), value.raw(), out) })
    }
    pub fn coerce_to_number(&self, value: &Value) -> Result<Value> {
        self.require_vm(&[value])?;
        self.execute(|out| unsafe { ffi::escargot_value_to_number(self.raw(), value.raw(), out) })
    }
    pub fn call(&self, function: &Value, receiver: &Value, arguments: &[Value]) -> Result<Value> {
        self.require_vm(&[function, receiver])?;
        let args = self.arguments(arguments)?;
        self.execute(|out| unsafe {
            ffi::escargot_function_call(
                self.raw(),
                function.raw(),
                receiver.raw(),
                args.len(),
                args.as_ptr(),
                out,
            )
        })
    }
    pub fn construct(&self, function: &Value, arguments: &[Value]) -> Result<Value> {
        self.require_vm(&[function])?;
        let args = self.arguments(arguments)?;
        self.execute(|out| unsafe {
            ffi::escargot_function_construct(
                self.raw(),
                function.raw(),
                args.len(),
                args.as_ptr(),
                out,
            )
        })
    }
    fn arguments(&self, arguments: &[Value]) -> Result<Vec<ffi::escargot_value>> {
        arguments
            .iter()
            .map(|value| {
                self.require_vm(&[value])?;
                Ok(value.raw())
            })
            .collect()
    }
    pub fn resolve(&self, promise: &Value, value: &Value) -> Result<Value> {
        self.require_vm(&[promise, value])?;
        self.execute(|out| unsafe {
            ffi::escargot_promise_resolve(self.raw(), promise.raw(), value.raw(), out)
        })
    }
    pub fn reject(&self, promise: &Value, reason: &Value) -> Result<Value> {
        self.require_vm(&[promise, reason])?;
        self.execute(|out| unsafe {
            ffi::escargot_promise_reject(self.raw(), promise.raw(), reason.raw(), out)
        })
    }
}

struct ValueInner {
    reference: NonNull<ffi::escargot_reference__>,
    context: Rc<ContextInner>,
}

impl Drop for ValueInner {
    fn drop(&mut self) {
        unsafe {
            ffi::escargot_reference_release(self.reference.as_ptr());
        }
    }
}

/// An owned, strong GC reference. Cloning shares the root; it never copies JS objects.
///
/// ```compile_fail
/// let value = escargot::Runtime::new().unwrap().vm().unwrap()
///     .context().unwrap().number(42.0).unwrap();
/// std::thread::spawn(move || drop(value));
/// ```
#[derive(Clone)]
pub struct Value(Rc<ValueInner>);

impl fmt::Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Value")
            .field("type", &self.value_type())
            .finish_non_exhaustive()
    }
}

impl Value {
    fn raw(&self) -> ffi::escargot_value {
        unsafe { ffi::escargot_reference_get(self.0.reference.as_ptr()) }
    }

    /// Borrow the rooted value for advanced `sys` operations. Keep this value
    /// alive while using the pointer, on this thread and within the same VM.
    pub fn as_raw(&self) -> ffi::escargot_value {
        self.raw()
    }

    pub fn value_type(&self) -> Result<ValueType> {
        let mut kind = 0;
        check(unsafe { ffi::escargot_value_get_type(self.raw(), &mut kind) })?;
        match kind {
            ffi::ESCARGOT_UNDEFINED => Ok(ValueType::Undefined),
            ffi::ESCARGOT_NULL => Ok(ValueType::Null),
            ffi::ESCARGOT_BOOLEAN => Ok(ValueType::Boolean),
            ffi::ESCARGOT_NUMBER => Ok(ValueType::Number),
            ffi::ESCARGOT_STRING => Ok(ValueType::String),
            ffi::ESCARGOT_SYMBOL => Ok(ValueType::Symbol),
            ffi::ESCARGOT_BIGINT => Ok(ValueType::BigInt),
            ffi::ESCARGOT_OBJECT => Ok(ValueType::Object),
            _ => Err(Error::Api(ffi::ESCARGOT_INTERNAL_ERROR)),
        }
    }

    pub fn as_number(&self) -> Result<f64> {
        let mut value = 0.0;
        check(unsafe { ffi::escargot_value_get_number(self.raw(), &mut value) })?;
        Ok(value)
    }
    pub fn as_boolean(&self) -> Result<bool> {
        let mut value = 0;
        check(unsafe { ffi::escargot_value_get_boolean(self.raw(), &mut value) })?;
        Ok(value != 0)
    }
    pub fn to_utf8(&self) -> Result<String> {
        let mut length = 0;
        check(unsafe {
            ffi::escargot_string_copy_utf8(self.raw(), ptr::null_mut(), 0, &mut length)
        })?;
        let mut bytes = vec![0u8; length];
        check(unsafe {
            ffi::escargot_string_copy_utf8(
                self.raw(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
                &mut length,
            )
        })?;
        String::from_utf8(bytes).map_err(|_| Error::InvalidUtf8)
    }
    /// Preserve every UTF-16 code unit, including unpaired surrogates and NUL.
    pub fn to_utf16(&self) -> Result<Vec<u16>> {
        let mut length = 0;
        check(unsafe {
            ffi::escargot_string_copy_utf16(self.raw(), ptr::null_mut(), 0, &mut length)
        })?;
        let mut units = vec![0u16; length];
        check(unsafe {
            ffi::escargot_string_copy_utf16(
                self.raw(),
                units.as_mut_ptr(),
                units.len(),
                &mut length,
            )
        })?;
        Ok(units)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Test threads share the process-wide native platform. The public API
    // rejects concurrent owners rather than blocking or racing initialization.
    static TEST_LOCK: Mutex<()> = Mutex::new(());
    fn session() -> (MutexGuard<'static, ()>, Runtime, Context) {
        let lock = TEST_LOCK.lock().unwrap_or_else(|error| error.into_inner());
        let runtime = Runtime::new().unwrap();
        let context = runtime.vm().unwrap().context().unwrap();
        (lock, runtime, context)
    }
    fn exception(error: Error) -> Exception {
        match error {
            Error::JavaScript(exception) => *exception,
            other => panic!("{other:?}"),
        }
    }

    #[test]
    fn evaluates_and_distinguishes_value_types() {
        let (_lock, _runtime, context) = session();
        assert_eq!(
            context
                .eval("21 * 2", "math.js")
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
        assert_eq!(
            context.undefined().unwrap().value_type().unwrap(),
            ValueType::Undefined
        );
        assert_eq!(
            context.null().unwrap().value_type().unwrap(),
            ValueType::Null
        );
        assert!(context.boolean(true).unwrap().as_boolean().unwrap());
        assert_eq!(
            context
                .eval("123n", "bigint.js")
                .unwrap()
                .value_type()
                .unwrap(),
            ValueType::BigInt
        );
        assert_eq!(
            context
                .eval("Symbol('x')", "symbol.js")
                .unwrap()
                .value_type()
                .unwrap(),
            ValueType::Symbol
        );
        assert!(context
            .number(f64::NAN)
            .unwrap()
            .as_number()
            .unwrap()
            .is_nan());
        assert!(matches!(
            context.string("42").unwrap().as_number(),
            Err(Error::Api(ffi::ESCARGOT_TYPE_ERROR))
        ));
        assert_eq!(
            context
                .coerce_to_number(&context.string("42").unwrap())
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
    }

    #[test]
    fn strings_preserve_embedded_nul_and_utf16_surrogates() {
        let (_lock, _runtime, context) = session();
        for text in ["", "한글 🦀", "a\0b"] {
            assert_eq!(context.string(text).unwrap().to_utf8().unwrap(), text);
        }
        let units = [0, 0xd800, 65, 0xdc00, 0xd83e, 0xdd80];
        assert_eq!(
            context.string_utf16(&units).unwrap().to_utf16().unwrap(),
            units
        );
        assert_eq!(context.string_utf16(&[]).unwrap().to_utf16().unwrap(), []);
        assert_eq!(
            context
                .eval("'a\\0b'", "nul\0name.js")
                .unwrap()
                .to_utf16()
                .unwrap(),
            [97, 0, 98]
        );
    }

    #[test]
    fn properties_and_attributes_keep_false_separate_from_exceptions() {
        let (_lock, _runtime, context) = session();
        let object = context.object().unwrap();
        let key = context.string("answer").unwrap();
        let number = context.number(42.0).unwrap();
        assert!(context.set(&object, &key, &number).unwrap());
        assert!(context.has(&object, &key).unwrap());
        assert_eq!(
            context.get(&object, &key).unwrap().as_number().unwrap(),
            42.0
        );
        assert!(context.delete(&object, &key).unwrap());
        assert!(!context.has(&object, &key).unwrap());
        assert!(context
            .define_property(
                &object,
                &key,
                &number,
                PropertyAttributes {
                    writable: false,
                    enumerable: true,
                    configurable: false,
                }
            )
            .unwrap());
        assert!(!context
            .set(&object, &key, &context.number(7.0).unwrap())
            .unwrap());
        assert!(!context.delete(&object, &key).unwrap());
        assert_eq!(
            context
                .get(
                    &context.array(3).unwrap(),
                    &context.string("length").unwrap()
                )
                .unwrap()
                .as_number()
                .unwrap(),
            3.0
        );
    }

    #[test]
    fn calls_and_constructors_return_owned_values() {
        let (_lock, _runtime, context) = session();
        let function = context
            .eval("(function(x) { return this.base + x })", "call.js")
            .unwrap();
        let receiver = context.eval("({base: 40})", "receiver.js").unwrap();
        assert_eq!(
            context
                .call(&function, &receiver, &[context.number(2.0).unwrap()])
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
        let constructor = context
            .eval("(function Answer(x) { this.answer = x })", "new.js")
            .unwrap();
        let object = context
            .construct(&constructor, &[context.number(42.0).unwrap()])
            .unwrap();
        assert_eq!(
            context
                .get(&object, &context.string("answer").unwrap())
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
        assert!(matches!(
            context.call(&context.null().unwrap(), &receiver, &[]),
            Err(Error::JavaScript(_))
        ));
    }

    #[test]
    fn contexts_in_one_vm_share_values_and_keep_distinct_globals() {
        let (_lock, runtime, first) = session();
        let second = Vm(first.0.vm.clone()).context().unwrap();
        first
            .eval("globalThis.realm = 'first'", "first.js")
            .unwrap();
        second
            .eval("globalThis.realm = 'second'", "second.js")
            .unwrap();
        let object = first.eval("({answer: 42})", "value.js").unwrap();
        second
            .set(
                &second.global().unwrap(),
                &second.string("shared").unwrap(),
                &object,
            )
            .unwrap();
        assert_eq!(
            second
                .eval("realm + shared.answer", "read.js")
                .unwrap()
                .to_utf8()
                .unwrap(),
            "second42"
        );
        drop(first);
        drop(runtime);
        assert_eq!(
            second
                .get(&object, &second.string("answer").unwrap())
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
    }

    #[test]
    fn rejects_foreign_vm_values_before_native_calls() {
        let (_lock, runtime, context) = session();
        let foreign = runtime.vm().unwrap().context().unwrap();
        let foreign_value = foreign.number(9.0).unwrap();
        let object = context.object().unwrap();
        let key = context.string("x").unwrap();
        assert!(matches!(
            context.set(&object, &key, &foreign_value),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.get(&foreign.object().unwrap(), &key),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.has(&object, &foreign.string("x").unwrap()),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.coerce_to_string(&foreign_value),
            Err(Error::WrongVm)
        ));
        let function = context.eval("(x => x)", "function.js").unwrap();
        assert!(matches!(
            context.call(
                &function,
                &context.undefined().unwrap(),
                std::slice::from_ref(&foreign_value)
            ),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.construct(&function, std::slice::from_ref(&foreign_value)),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.resolve(&context.promise().unwrap(), &foreign_value),
            Err(Error::WrongVm)
        ));
        assert!(matches!(
            context.reject(&context.promise().unwrap(), &foreign_value),
            Err(Error::WrongVm)
        ));
    }

    #[test]
    fn exceptions_keep_original_values_and_independent_stacks() {
        let (_lock, _runtime, context) = session();
        let first = exception(
            context
                .eval("function fail() { throw {answer: 42}; } fail()", "first.js")
                .unwrap_err(),
        );
        let second = exception(context.eval("throw 7", "second.js").unwrap_err());
        assert_eq!(second.value.as_number().unwrap(), 7.0);
        assert_eq!(
            context
                .get(&first.value, &context.string("answer").unwrap())
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
        assert!(first
            .stack
            .iter()
            .any(|frame| frame.source_name.as_deref() == Some("first.js")));
        assert!(second
            .stack
            .iter()
            .any(|frame| frame.source_name.as_deref() == Some("second.js")));
        assert_eq!(
            context.eval("42", "after.js").unwrap().as_number().unwrap(),
            42.0
        );
    }

    #[test]
    fn primitive_throws_and_parse_errors_are_reported() {
        let (_lock, _runtime, context) = session();
        for (source, kind) in [
            ("throw undefined", ValueType::Undefined),
            ("throw null", ValueType::Null),
            ("throw false", ValueType::Boolean),
            ("throw 'message'", ValueType::String),
        ] {
            assert_eq!(
                exception(context.eval(source, "throw.js").unwrap_err())
                    .value
                    .value_type()
                    .unwrap(),
                kind
            );
        }
        assert!(matches!(
            context.eval("const =", "invalid.js"),
            Err(Error::JavaScript(_))
        ));
    }

    #[test]
    fn property_and_conversion_exceptions_use_the_same_error_path() {
        let (_lock, _runtime, context) = session();
        let object = context
            .eval(
                "({ get x() { throw 11; }, set x(v) { throw 12; }, valueOf() { throw 13; } })",
                "accessors.js",
            )
            .unwrap();
        let key = context.string("x").unwrap();
        assert_eq!(
            exception(context.get(&object, &key).unwrap_err())
                .value
                .as_number()
                .unwrap(),
            11.0
        );
        assert_eq!(
            exception(
                context
                    .set(&object, &key, &context.null().unwrap())
                    .unwrap_err()
            )
            .value
            .as_number()
            .unwrap(),
            12.0
        );
        assert_eq!(
            exception(context.coerce_to_number(&object).unwrap_err())
                .value
                .as_number()
                .unwrap(),
            13.0
        );
    }

    #[test]
    fn promise_jobs_require_explicit_pumping() {
        let (_lock, _runtime, context) = session();
        let promise = context.promise().unwrap();
        context
            .set(
                &context.global().unwrap(),
                &context.string("pending").unwrap(),
                &promise,
            )
            .unwrap();
        context
            .eval(
                "var answer = 0; pending.then(x => answer = x)",
                "promise.js",
            )
            .unwrap();
        context
            .resolve(&promise, &context.number(42.0).unwrap())
            .unwrap();
        assert!(Vm(context.0.vm.clone()).has_pending_job());
        assert_eq!(
            context
                .eval("answer", "before.js")
                .unwrap()
                .as_number()
                .unwrap(),
            0.0
        );
        context.run_jobs().unwrap();
        assert!(!Vm(context.0.vm.clone()).has_pending_job());
        assert_eq!(
            context
                .eval("answer", "after.js")
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
        context.run_jobs().unwrap();
    }

    #[test]
    fn promise_rejection_preserves_reason() {
        let (_lock, _runtime, context) = session();
        let promise = context.promise().unwrap();
        context
            .set(
                &context.global().unwrap(),
                &context.string("pending").unwrap(),
                &promise,
            )
            .unwrap();
        context
            .eval("var reason; pending.catch(x => reason = x)", "catch.js")
            .unwrap();
        context
            .reject(&promise, &context.string("rejected\0reason").unwrap())
            .unwrap();
        context.run_jobs().unwrap();
        assert_eq!(
            context
                .eval("reason", "reason.js")
                .unwrap()
                .to_utf8()
                .unwrap(),
            "rejected\0reason"
        );
    }

    #[test]
    fn rust_heap_roots_survive_native_allocation_pressure() {
        let (_lock, _runtime, context) = session();
        let values: Vec<_> = (0..128)
            .map(|i| {
                context
                    .eval(&format!("({{answer: {i}}})"), "root.js")
                    .unwrap()
            })
            .collect();
        context
            .eval(
                "for (let i = 0; i < 200000; i++) { let x = new Array(64).fill({i}); }",
                "gc.js",
            )
            .unwrap();
        let key = context.string("answer").unwrap();
        for (i, value) in values.iter().enumerate() {
            assert_eq!(
                context.get(value, &key).unwrap().as_number().unwrap(),
                i as f64
            );
        }
        let clone = values[42].clone();
        drop(values);
        assert_eq!(
            context.get(&clone, &key).unwrap().as_number().unwrap(),
            42.0
        );
    }

    #[test]
    fn values_keep_the_runtime_alive_after_owners_are_dropped() {
        let (lock, runtime, context) = session();
        let value = context.string("retained").unwrap();
        let weak = Rc::downgrade(&runtime.0);
        drop(context);
        drop(runtime);
        assert!(weak.upgrade().is_some());
        assert_eq!(value.to_utf8().unwrap(), "retained");
        let shared = Runtime::new().unwrap();
        assert!(Rc::ptr_eq(&shared.0, &weak.upgrade().unwrap()));
        drop(shared);
        drop(value);
        assert!(weak.upgrade().is_none());
        // A successful new session also checks that all native roots were freed.
        drop(Runtime::new().unwrap());
        drop(lock);
    }

    #[test]
    fn repeated_sessions_do_not_exhaust_gc_kinds() {
        let _lock = TEST_LOCK.lock().unwrap_or_else(|error| error.into_inner());
        for _ in 0..64 {
            let runtime = Runtime::new().unwrap();
            let context = runtime.vm().unwrap().context().unwrap();
            assert_eq!(
                context
                    .eval("Function('return 42')()", "reinitialize.js")
                    .unwrap()
                    .as_number()
                    .unwrap(),
                42.0
            );
        }
    }

    #[test]
    fn concurrent_runtime_owners_are_rejected() {
        let (_lock, _runtime, _context) = session();
        assert!(
            std::thread::spawn(|| matches!(Runtime::new(), Err(Error::RuntimeBusy)))
                .join()
                .unwrap()
        );
    }

    #[test]
    fn unsafe_bridge_roots_values_before_releasing_native_results() {
        let (_lock, _runtime, context) = session();
        let source = c"({answer: 42})";
        let mut result = ptr::null_mut();
        let value = unsafe {
            assert_eq!(
                ffi::escargot_context_eval(
                    context.as_raw(),
                    source.as_ptr(),
                    source.to_bytes().len(),
                    c"raw.js".as_ptr(),
                    6,
                    &mut result
                ),
                ffi::ESCARGOT_OK
            );
            let value = context
                .root_value(ffi::escargot_result_value(result))
                .unwrap();
            ffi::escargot_result_release(result);
            value
        };
        context
            .eval(
                "for (let i = 0; i < 100000; i++) new Array(64).fill(i)",
                "gc.js",
            )
            .unwrap();
        assert_eq!(
            context
                .get(&value, &context.string("answer").unwrap())
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
    }

    #[test]
    fn unwinding_releases_roots_and_allows_reinitialization() {
        let _lock = TEST_LOCK.lock().unwrap_or_else(|error| error.into_inner());
        assert!(std::panic::catch_unwind(|| {
            let runtime = Runtime::new().unwrap();
            let context = runtime.vm().unwrap().context().unwrap();
            let _value = context.object().unwrap();
            panic!("test unwind");
        })
        .is_err());
        let runtime = Runtime::new().unwrap();
        assert_eq!(
            runtime
                .vm()
                .unwrap()
                .context()
                .unwrap()
                .eval("42", "recovered.js")
                .unwrap()
                .as_number()
                .unwrap(),
            42.0
        );
    }
}
