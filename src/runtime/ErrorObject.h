/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

extern const char* errorMessage_NotImplemented; // FIXME to be removed
extern const char* errorMessage_IsNotDefined;
extern const char* errorMessage_IsNotInitialized;
extern const char* errorMessage_DuplicatedIdentifier;
extern const char* errorMessage_AssignmentToConstantVariable;
extern const char* errorMessage_DefineProperty_Default;
extern const char* errorMessage_DefineProperty_LengthNotWritable;
extern const char* errorMessage_DefineProperty_NotWritable;
extern const char* errorMessage_DefineProperty_RedefineNotConfigurable;
extern const char* errorMessage_DefineProperty_NotExtensible;
extern const char* errorMessage_DefineProperty_NotConfigurable;
extern const char* errorMessage_ObjectToPrimitiveValue;
extern const char* errorMessage_NullToObject;
extern const char* errorMessage_UndefinedToObject;
extern const char* errorMessage_NOT_Callable;
extern const char* errorMessage_Not_Constructor;
extern const char* errorMessage_Not_Constructor_Function;
extern const char* errorMessage_Constructor_Return_Undefined;
extern const char* errorMessage_Get_FromUndefined;
extern const char* errorMessage_Get_FromNull;
extern const char* errorMessage_Set_ToUndefined;
extern const char* errorMessage_Set_ToNull;
extern const char* errorMessage_New_Target_Is_Undefined;
extern const char* errorMessage_Class_Prototype_Is_Not_Object_Nor_Null;
extern const char* errorMessage_Class_Extends_Value_Is_Not_Object_Nor_Null;
extern const char* errorMessage_Initialized_This_Binding;
extern const char* errorMessage_UnInitialized_This_Binding;
extern const char* errorMessage_No_Super_Binding;
extern const char* errorMessage_InvalidDerivedConstructorReturnValue;
extern const char* errorMessage_InstanceOf_NotFunction;
extern const char* errorMessage_InstanceOf_InvalidPrototypeProperty;
extern const char* errorMessage_ArgumentsOrCaller_InStrictMode;
extern const char* errorMessage_GlobalObject_ThisUndefinedOrNull;
extern const char* errorMessage_GlobalObject_ThisNotObject;
extern const char* errorMessage_GlobalObject_ThisNotRegExpObject;
extern const char* errorMessage_GlobalObject_ThisNotConstructor;
extern const char* errorMessage_GlobalObject_ThisNotDateObject;
extern const char* errorMessage_GlobalObject_ThisNotFunctionObject;
extern const char* errorMessage_GlobalObject_ThisNotBoolean;
extern const char* errorMessage_GlobalObject_ThisNotNumber;
extern const char* errorMessage_GlobalObject_ThisNotString;
extern const char* errorMessage_GlobalObject_ThisNotTypedArrayObject;
extern const char* errorMessage_GlobalObject_ThisNotArrayBufferObject;
extern const char* errorMessage_GlobalObject_ThisNotDataViewObject;
extern const char* errorMessage_GlobalObject_MalformedURI;
extern const char* errorMessage_GlobalObject_RangeError;
extern const char* errorMessage_GlobalObject_FileNotExist;
extern const char* errorMessage_GlobalObject_NotExecutable;
extern const char* errorMessage_GlobalObject_FirstArgumentNotObject;
extern const char* errorMessage_GlobalObject_SecondArgumentNotObject;
extern const char* errorMessage_GlobalObject_DescriptorNotObject;
extern const char* errorMessage_GlobalObject_ToLocaleStringNotCallable;
extern const char* errorMessage_GlobalObject_ToISOStringNotCallable;
extern const char* errorMessage_GlobalObject_CallbackNotCallable;
extern const char* errorMessage_GlobalObject_InvalidDate;
extern const char* errorMessage_GlobalObject_JAError;
extern const char* errorMessage_GlobalObject_JOError;
extern const char* errorMessage_GlobalObject_RadixInvalidRange;
extern const char* errorMessage_GlobalObject_NotDefineable;
extern const char* errorMessage_GlobalObject_FirstArgumentNotObjectAndNotNull;
extern const char* errorMessage_GlobalObject_ReduceError;
extern const char* errorMessage_GlobalObject_FirstArgumentNotCallable;
extern const char* errorMessage_GlobalObject_FirstArgumentNotString;
extern const char* errorMessage_GlobalObject_FirstArgumentInvalidLength;
extern const char* errorMessage_GlobalObject_InvalidArrayBufferOffset;
extern const char* errorMessage_GlobalObject_InvalidArrayBufferSize;
extern const char* errorMessage_GlobalObject_NotExistNewInArrayBufferConstructor;
extern const char* errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor;
extern const char* errorMessage_GlobalObject_NotExistNewInDataViewConstructor;
extern const char* errorMessage_GlobalObject_InvalidArrayLength;
extern const char* errorMessage_GlobalObject_DetachedBuffer;
extern const char* errorMessage_GlobalObject_ConstructorRequiresNew;
extern const char* errorMessage_GlobalObject_CalledOnIncompatibleReceiver;
extern const char* errorMessage_GlobalObject_IllegalFirstArgument;
extern const char* errorMessage_String_InvalidStringLength;

class ErrorObject : public Object {
public:
    enum Code {
        None,
        ReferenceError,
        TypeError,
        SyntaxError,
        RangeError,
        URIError,
        EvalError
    };
    static void throwBuiltinError(ExecutionState& state, Code code, const char* templateString)
    {
        throwBuiltinError(state, code, String::emptyString, false, String::emptyString, templateString);
    }
    static void throwBuiltinError(ExecutionState& state, Code code, const char* templateString, AtomicString templateDataString)
    {
        throwBuiltinError(state, code, templateDataString.string(), false, String::emptyString, templateString);
    }
    static void throwBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString);
    static ErrorObject* createError(ExecutionState& state, ErrorObject::Code code, String* errorMessage);
    ErrorObject(ExecutionState& state, String* errorMessage);
    virtual bool isErrorObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Error";
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
        TightVector<StackTraceGCData, GCUtil::gc_malloc_ignore_off_page_allocator<StackTraceGCData>> gcValues;
        TightVector<StackTraceNonGCData, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<StackTraceNonGCData>> nonGCValues;
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
    ReferenceErrorObject(ExecutionState& state, String* errorMessage);
};

class TypeErrorObject : public ErrorObject {
public:
    TypeErrorObject(ExecutionState& state, String* errorMessage);
};

class SyntaxErrorObject : public ErrorObject {
public:
    SyntaxErrorObject(ExecutionState& state, String* errorMessage);
};

class RangeErrorObject : public ErrorObject {
public:
    RangeErrorObject(ExecutionState& state, String* errorMessage);
};

class URIErrorObject : public ErrorObject {
public:
    URIErrorObject(ExecutionState& state, String* errorMessage);
};

class EvalErrorObject : public ErrorObject {
public:
    EvalErrorObject(ExecutionState& state, String* errorMessage);
};
}

#endif
