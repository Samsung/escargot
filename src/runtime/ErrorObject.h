#ifndef __EscargotErrorObject__
#define __EscargotErrorObject__

#include "runtime/Object.h"

namespace Escargot {

extern const char* errorMessage_IsNotDefined;
extern const char* errorMessage_DefineProperty_Default;
extern const char* errorMessage_DefineProperty_LengthNotWritable;
extern const char* errorMessage_DefineProperty_NotWritable;
extern const char* errorMessage_DefineProperty_RedefineNotConfigurable;
extern const char* errorMessage_DefineProperty_NotExtensible;
extern const char* errorMessage_ObjectToPrimitiveValue;
extern const char* errorMessage_NullToObject;
extern const char* errorMessage_UndefinedToObject;
extern const char* errorMessage_Call_NotFunction;
extern const char* errorMessage_Get_FromUndefined;
extern const char* errorMessage_Get_FromNull;
extern const char* errorMessage_Set_ToUndefined;
extern const char* errorMessage_Set_ToNull;
extern const char* errorMessage_New_NotConstructor;
extern const char* errorMessage_InstanceOf_NotFunction;
extern const char* errorMessage_InstanceOf_InvalidPrototypeProperty;
extern const char* errorMessage_ArgumentsOrCaller_InStrictMode;
extern const char* errorMessage_GlobalObject_ThisUndefinedOrNull;
extern const char* errorMessage_GlobalObject_ThisNotObject;
extern const char* errorMessage_GlobalObject_ThisNotRegExpObject;
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
extern const char* errorMessage_GlobalObject_ToLoacleStringNotCallable;
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
extern const char* errorMessage_GlobalObject_NotExistNewInArrayBufferConstructor;
extern const char* errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor;
extern const char* errorMessage_GlobalObject_NotExistNewInDataViewConstructor;
extern const char* errorMessage_GlobalObject_InvalidArrayLength;

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
    static void throwBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototoype, String* functionName, const char* templateString);
    ErrorObject(ExecutionState& state, String* errorMessage);
    virtual bool isErrorObject()
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Error";
    }

protected:
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
