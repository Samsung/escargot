/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotErrorObject__
#define __EscargotErrorObject__

#include "runtime/Object.h"

namespace Escargot {

class ByteCodeBlock;
class SandBox;

class ErrorObject : public Object {
public:
    class Messages {
    public:
        static constexpr const char* NotImplemented = "Not implemented";
        static constexpr const char* IsNotDefined = "%s is not defined";
        static constexpr const char* IsNotInitialized = "Cannot access '%s' before initialization";
        static constexpr const char* DuplicatedIdentifier = "Identifier '%s' has already been declared";
        static constexpr const char* AssignmentToConstantVariable = "Assignment to constant variable '%s'";
        static constexpr const char* DefineProperty_Default = "Cannot define property '%s'";
        static constexpr const char* DefineProperty_LengthNotWritable = "Cannot modify property '%s': 'length' is not writable";
        static constexpr const char* DefineProperty_NotWritable = "Cannot modify non-writable property '%s'";
        static constexpr const char* DefineProperty_RedefineNotConfigurable = "Cannot redefine non-configurable property '%s'";
        static constexpr const char* DefineProperty_NotExtensible = "Cannot define property '%s': object is not extensible";
        static constexpr const char* DefineProperty_NotConfigurable = "Cannot delete property '%s': property is not configurable";
        static constexpr const char* ObjectToPrimitiveValue = "Cannot convert object to primitive value";
        static constexpr const char* NullToObject = "cannot convert null into object";
        static constexpr const char* UndefinedToObject = "cannot convert undefined into object";
        static constexpr const char* NOT_Callable = "Callee is not a function object";
        static constexpr const char* Not_Constructor = "Callee is not a constructor";
        static constexpr const char* Not_Constructor_Function = "%s is not a constructor";
        static constexpr const char* Not_Invoked_With_New = "%s must be invoked with \'new\'";
        static constexpr const char* Can_Not_Be_Destructed = "Right side of assignment cannot be destructured";
        static constexpr const char* Constructor_Return_Undefined = "constructor returns undefined";
        static constexpr const char* Get_FromUndefined = "Cannot get property '%s' of undefined";
        static constexpr const char* Get_FromNull = "Cannot get property '%s' of null";
        static constexpr const char* Set_ToUndefined = "Cannot set property '%s' of undefined";
        static constexpr const char* Set_ToNull = "Cannot set property '%s' of null";
        static constexpr const char* New_Target_Is_Undefined = "NewTarget is undefined";
        static constexpr const char* Class_Prototype_Is_Not_Object_Nor_Null = "Class extends object prototype property is not object nor null";
        static constexpr const char* Class_Prototype_Is_Not_Static_Generator = "Classes may not have a static property named 'prototype'";
        static constexpr const char* Class_Extends_Value_Is_Not_Object_Nor_Null = "Class extends value is not object nor null";
        static constexpr const char* Initialized_This_Binding = "Super constructor may only be called once";
        static constexpr const char* UnInitialized_This_Binding = "Must call super constructor in derived class before accessing 'this' or returning from derived constructor";
        static constexpr const char* No_Super_Binding = "Invalid super binding";
        static constexpr const char* InvalidDerivedConstructorReturnValue = "Derived constructors may only return object or undefined";
        static constexpr const char* InstanceOf_NotFunction = "Invalid operand to 'instanceof': right expr is not a function object";
        static constexpr const char* InstanceOf_InvalidPrototypeProperty = "instanceof called on an object with an invalid prototype property";
        static constexpr const char* ArgumentsOrCaller_InStrictMode = "'caller' and 'arguments' are restricted function properties and cannot be accessed in this context.";
        static constexpr const char* FailedToLoadModule = "Failed to load module %s";
        static constexpr const char* CanNotMixBigIntWithOtherTypes = "Cannot mix BigInt and other types, use explicit conversions";
        static constexpr const char* CanNotConvertValueToIndex = "Cannot convert value to index";
        static constexpr const char* DivisionByZero = "Division by zero";
        static constexpr const char* Overflow = "overflow occurred";
        static constexpr const char* OutOfMemory = "out of memory";
        static constexpr const char* ExponentByNegative = "Exponent must be positive";
        static constexpr const char* GlobalObject_ThisUndefinedOrNull = "%s: this value is undefined or null";
        static constexpr const char* GlobalObject_ThisNotObject = "%s: this value is not an object";
        static constexpr const char* GlobalObject_ThisNotRegExpObject = "%s: this value is not a RegExp object";
        static constexpr const char* GlobalObject_ThisNotConstructor = "%s: this value is not a Constructor";
        static constexpr const char* GlobalObject_ThisNotDateObject = "%s: this value is not a Date object";
        static constexpr const char* GlobalObject_ThisNotFunctionObject = "%s: this value is not a Function object";
        static constexpr const char* GlobalObject_ThisNotBoolean = "%s: this value is not Boolean nor Boolean object";
        static constexpr const char* GlobalObject_ThisNotNumber = "%s: this value is not Number nor Number object";
        static constexpr const char* GlobalObject_ThisNotString = "%s: this value is not String nor String object";
        static constexpr const char* GlobalObject_ThisNotTypedArrayObject = "%s: this value is not a Typed Array object";
        static constexpr const char* GlobalObject_ThisNotArrayBufferObject = "%s: this value is not an ArrayBuffer object";
        static constexpr const char* GlobalObject_ThisNotDataViewObject = "%s: this value is not a DataView object";
        static constexpr const char* GlobalObject_MalformedURI = "%s: malformed URI";
        static constexpr const char* GlobalObject_RangeError = "%s: invalid range";
        static constexpr const char* GlobalObject_FileNotExist = "%s: cannot load file";
        static constexpr const char* GlobalObject_NotExecutable = "%s: cannot run";
        static constexpr const char* GlobalObject_FirstArgumentNotObject = "%s: first argument is not an object";
        static constexpr const char* GlobalObject_SecondArgumentNotObject = "%s: second argument is not an object";
        static constexpr const char* GlobalObject_DescriptorNotObject = "%s: descriptor is not an object";
        static constexpr const char* GlobalObject_ToLocaleStringNotCallable = "%s: toLocaleString is not callable";
        static constexpr const char* GlobalObject_ToISOStringNotCallable = "%s: toISOString is not callable";
        static constexpr const char* GlobalObject_CallbackNotCallable = "%s: callback is not callable";
        static constexpr const char* GlobalObject_InvalidDate = "%s: Invalid Date";
        static constexpr const char* GlobalObject_JAError = "%s: JA error";
        static constexpr const char* GlobalObject_JOError = "%s: JO error";
        static constexpr const char* GlobalObject_RadixInvalidRange = "%s: radix is invalid range";
        static constexpr const char* GlobalObject_NotDefineable = "%s: cannot define property";
        static constexpr const char* GlobalObject_FirstArgumentNotObjectAndNotNull = "%s: first argument is not an object and not null";
        static constexpr const char* GlobalObject_ReduceError = "%s: reduce of empty array with no initial value";
        static constexpr const char* GlobalObject_FirstArgumentNotCallable = "%s: first argument is not callable";
        static constexpr const char* GlobalObject_FirstArgumentNotString = "%s: first argument is not a string";
        static constexpr const char* GlobalObject_FirstArgumentInvalidLength = "%s: first arugment is an invalid length value";
        static constexpr const char* GlobalObject_InvalidArrayBufferOffset = "%s: ArrayBuffer length minus the byteOffset is not a multiple of the element size";
        static constexpr const char* GlobalObject_InvalidArrayBufferSize = "%s: ArrayBuffer buffer allocation failed";
        static constexpr const char* GlobalObject_InvalidArrayLength = "Invalid array length";
        static constexpr const char* GlobalObject_DetachedBuffer = "%s: Detached buffer cannot be used here";
        static constexpr const char* GlobalObject_ConstructorRequiresNew = "Constructor requires 'new'";
        static constexpr const char* GlobalObject_CalledOnIncompatibleReceiver = "%s: called on incompatible receiver";
        static constexpr const char* GlobalObject_IllegalFirstArgument = "%s: illegal first argument";
        static constexpr const char* String_InvalidStringLength = "Invalid string length";
        static constexpr const char* CanNotReadPrivateMember = "Cannot read private member %s from an object whose class did not declare it";
        static constexpr const char* CanNotWritePrivateMember = "Cannot write private member %s from an object whose class did not declare it";
#if defined(ENABLE_CODE_CACHE)
        static constexpr const char* CodeCache_Loaded_StaticError = "[CodeCache] Default Error Message of ThrowStaticError: %s";
#endif
#if defined(ENABLE_WASM)
        static constexpr const char* WASM_CompileError = "[WASM] Module compile error";
        static constexpr const char* WASM_FuncCallError = "[WASM] Function call error";
        static constexpr const char* WASM_ReadImportsError = "[WASM] Error while reading each import element of module with importObject";
        static constexpr const char* WASM_InstantiateModuleError = "[WASM] Instantiate WebAssembly module error";
        static constexpr const char* WASM_SetToGlobalConstValue = "%s: [WASM] Set to Global constant value";
#endif
    };

    enum Code {
        None,
        ReferenceError,
        TypeError,
        SyntaxError,
        RangeError,
        URIError,
        EvalError,
        AggregateError,
#if defined(ENABLE_WASM)
        WASMCompileError,
        WASMLinkError,
        WASMRuntimeError,
#endif
    };
    static void throwBuiltinError(ExecutionState& state, Code code, const char* templateString)
    {
        throwBuiltinError(state, code, String::emptyString, false, String::emptyString, templateString);
    }
    static void throwBuiltinError(ExecutionState& state, Code code, const char* templateString, AtomicString templateDataString)
    {
        throwBuiltinError(state, code, templateDataString.string(), false, String::emptyString, templateString);
    }
    static void throwBuiltinError(ExecutionState& state, Code code, const char* templateString, String* templateDataString)
    {
        throwBuiltinError(state, code, templateDataString, false, String::emptyString, templateString);
    }
    static ErrorObject* createBuiltinError(ExecutionState& state, Code code, const char* templateString)
    {
        return createBuiltinError(state, code, String::emptyString, false, String::emptyString, templateString);
    }
    static ErrorObject* createError(ExecutionState& state, ErrorObject::Code code, String* errorMessage);
    static ErrorObject* createBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString);
    static void throwBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString);
    static void throwBuiltinError(ExecutionState& state, Code code, String* errorMessage);

    ErrorObject(ExecutionState& state, Object* proto, String* errorMessage);

    virtual bool isErrorObject() const
    {
        return true;
    }

    struct StackTraceGCData {
        union {
            ByteCodeBlock* byteCodeBlock;
            String* infoString;
        };
    };
    struct StackTraceNonGCData {
        size_t byteCodePosition;
    };
    struct StackTraceData : public gc {
        TightVector<StackTraceGCData, GCUtil::gc_malloc_allocator<StackTraceGCData>> gcValues;
        TightVector<StackTraceNonGCData, GCUtil::gc_malloc_atomic_allocator<StackTraceNonGCData>> nonGCValues;
        Value exception;

        void buildStackTrace(Context* context, StringBuilder& builder);
        static StackTraceData* create(SandBox* sandBox);

    private:
        StackTraceData() {}
    };

    StackTraceData* stackTraceData()
    {
        return m_stackTraceData;
    }

    void setStackTraceData(StackTraceData* d)
    {
        m_stackTraceData = d;
    }

private:
    StackTraceData* m_stackTraceData;
};

class ReferenceErrorObject : public ErrorObject {
public:
    ReferenceErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class TypeErrorObject : public ErrorObject {
public:
    TypeErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class SyntaxErrorObject : public ErrorObject {
public:
    SyntaxErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class RangeErrorObject : public ErrorObject {
public:
    RangeErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class URIErrorObject : public ErrorObject {
public:
    URIErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class EvalErrorObject : public ErrorObject {
public:
    EvalErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class AggregateErrorObject : public ErrorObject {
public:
    AggregateErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

#if defined(ENABLE_WASM)
class WASMCompileErrorObject : public ErrorObject {
public:
    WASMCompileErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class WASMLinkErrorObject : public ErrorObject {
public:
    WASMLinkErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};

class WASMRuntimeErrorObject : public ErrorObject {
public:
    WASMRuntimeErrorObject(ExecutionState& state, Object* proto, String* errorMessage);
};
#endif
} // namespace Escargot

#endif
