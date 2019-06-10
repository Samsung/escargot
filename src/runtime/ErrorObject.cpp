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

#include "Escargot.h"
#include "ErrorObject.h"
#include "Context.h"

namespace Escargot {
const char* errorMessage_NotImplemented = "Not implemented";
const char* errorMessage_IsNotDefined = "%s is not defined";
const char* errorMessage_AssignmentToConstantVariable = "Assignment to constant variable";
const char* errorMessage_DefineProperty_Default = "Cannot define property '%s'";
const char* errorMessage_DefineProperty_LengthNotWritable = "Cannot modify property '%s': 'length' is not writable";
const char* errorMessage_DefineProperty_NotWritable = "Cannot modify non-writable property '%s'";
const char* errorMessage_DefineProperty_RedefineNotConfigurable = "Cannot redefine non-configurable property '%s'";
const char* errorMessage_DefineProperty_NotExtensible = "Cannot define property '%s': object is not extensible";
const char* errorMessage_DefineProperty_NotConfigurable = "Cannot delete property '%s': property is not configurable";
const char* errorMessage_ObjectToPrimitiveValue = "Cannot convert object to primitive value";
const char* errorMessage_NullToObject = "cannot convert null into object";
const char* errorMessage_UndefinedToObject = "cannot convert undefined into object";
const char* errorMessage_NOT_Callable = "Callee is not a function object";
const char* errorMessage_Not_Constructor = "%s is not a constructor";
const char* errorMessage_Constructor_Return_Undefined = "constructor returns undefined";
const char* errorMessage_Get_FromUndefined = "Cannot get property '%s' of undefined";
const char* errorMessage_Get_FromNull = "Cannot get property '%s' of null";
const char* errorMessage_Set_ToUndefined = "Cannot set property '%s' of undefined";
const char* errorMessage_Set_ToNull = "Cannot set property '%s' of null";
const char* errorMessage_New_Target_Is_Undefined = "NewTarget is undefined";
const char* errorMessage_Class_Prototype_Is_Not_Object_Nor_Null = "Class extends object prototype property is not object nor null";
const char* errorMessage_Class_Extends_Value_Is_Not_Object_Nor_Null = "Class extends value is not object nor null";
const char* errorMessage_Initialized_This_Binding = "Super constructor may only be called once";
const char* errorMessage_UnInitialized_This_Binding = "Must call super constructor in derived class before accessing 'this' or returning from derived constructor";
const char* errorMessage_No_Super_Binding = "Invalid super binding";
const char* errorMessage_InvalidDerivedConstructorReturnValue = "Derived constructors may only return object or undefined";
const char* errorMessage_InstanceOf_NotFunction = "Invalid operand to 'instanceof': right expr is not a function object";
const char* errorMessage_InstanceOf_InvalidPrototypeProperty = "instanceof called on an object with an invalid prototype property";
const char* errorMessage_ArgumentsOrCaller_InStrictMode = "'caller' and 'arguments' are restricted function properties and cannot be accessed in this context.";
const char* errorMessage_GlobalObject_ThisUndefinedOrNull = "%s: this value is undefined or null";
const char* errorMessage_GlobalObject_ThisNotObject = "%s: this value is not an object";
const char* errorMessage_GlobalObject_ThisNotRegExpObject = "%s: this value is not a RegExp object";
const char* errorMessage_GlobalObject_ThisNotConstructor = "%s: this value is not a Constructor";
const char* errorMessage_GlobalObject_ThisNotDateObject = "%s: this value is not a Date object";
const char* errorMessage_GlobalObject_ThisNotFunctionObject = "%s: this value is not a Function object";
const char* errorMessage_GlobalObject_ThisNotBoolean = "%s: this value is not Boolean nor Boolean object";
const char* errorMessage_GlobalObject_ThisNotNumber = "%s: this value is not Number nor Number object";
const char* errorMessage_GlobalObject_ThisNotString = "%s: this value is not String nor String object";
const char* errorMessage_GlobalObject_ThisNotTypedArrayObject = "%s: this value is not a Typed Array object";
const char* errorMessage_GlobalObject_ThisNotArrayBufferObject = "%s: this value is not an ArrayBuffer object";
const char* errorMessage_GlobalObject_ThisNotDataViewObject = "%s: this value is not a DataView object";
const char* errorMessage_GlobalObject_MalformedURI = "%s: malformed URI";
const char* errorMessage_GlobalObject_RangeError = "%s: invalid range";
const char* errorMessage_GlobalObject_FileNotExist = "%s: cannot load file";
const char* errorMessage_GlobalObject_NotExecutable = "%s: cannot run";
const char* errorMessage_GlobalObject_FirstArgumentNotObject = "%s: first argument is not an object";
const char* errorMessage_GlobalObject_SecondArgumentNotObject = "%s: second argument is not an object";
const char* errorMessage_GlobalObject_DescriptorNotObject = "%s: descriptor is not an object";
const char* errorMessage_GlobalObject_ToLocaleStringNotCallable = "%s: toLocaleString is not callable";
const char* errorMessage_GlobalObject_ToISOStringNotCallable = "%s: toISOString is not callable";
const char* errorMessage_GlobalObject_CallbackNotCallable = "%s: callback is not callable";
const char* errorMessage_GlobalObject_InvalidDate = "%s: Invalid Date";
const char* errorMessage_GlobalObject_JAError = "%s: JA error";
const char* errorMessage_GlobalObject_JOError = "%s: JO error";
const char* errorMessage_GlobalObject_RadixInvalidRange = "%s: radix is invalid range";
const char* errorMessage_GlobalObject_NotDefineable = "%s: cannot define property";
const char* errorMessage_GlobalObject_FirstArgumentNotObjectAndNotNull = "%s: first argument is not an object and not null";
const char* errorMessage_GlobalObject_ReduceError = "%s: reduce of empty array with no initial value";
const char* errorMessage_GlobalObject_FirstArgumentNotCallable = "%s: first argument is not callable";
const char* errorMessage_GlobalObject_FirstArgumentNotString = "%s: first argument is not a string";
const char* errorMessage_GlobalObject_FirstArgumentInvalidLength = "%s: first arugment is an invalid length value";
const char* errorMessage_GlobalObject_InvalidArrayBufferOffset = "%s: ArrayBuffer length minus the byteOffset is not a multiple of the element size";
const char* errorMessage_GlobalObject_InvalidArrayBufferSize = "%s: ArrayBuffer buffer allocation failed";
const char* errorMessage_GlobalObject_NotExistNewInArrayBufferConstructor = "%s: Constructor ArrayBuffer requires \'new\'";
const char* errorMessage_GlobalObject_NotExistNewInTypedArrayConstructor = "%s: Constructor TypedArray requires \'new\'";
const char* errorMessage_GlobalObject_NotExistNewInDataViewConstructor = "%s: Constructor DataView requires \'new\'";
const char* errorMessage_GlobalObject_InvalidArrayLength = "Invalid array length";
const char* errorMessage_GlobalObject_DetachedBuffer = "%s: Detached buffer cannot be used here";
const char* errorMessage_GlobalObject_ConstructorRequiresNew = "Constructor requires 'new'";
const char* errorMessage_GlobalObject_CalledOnIncompatibleReceiver = "%s: called on incompatible receiver";
const char* errorMessage_GlobalObject_IllegalFirstArgument = "%s: illegal first argument";
const char* errorMessage_String_InvalidStringLength = "Invalid string length";


void ErrorObject::throwBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString)
{
    StringBuilder replacerBuilder;
    if (objectName->length()) {
        replacerBuilder.appendString(objectName);
    }
    if (prototype) {
        replacerBuilder.appendChar('.');
        replacerBuilder.appendString(state.context()->staticStrings().prototype.string());
    }
    if (functionName->length()) {
        replacerBuilder.appendChar('.');
        replacerBuilder.appendString(functionName);
    }

    String* errorMessage;
    String* replacer = replacerBuilder.finalize();

    size_t len1 = strlen(templateString);
    size_t len2 = replacer->length();
    std::basic_string<char16_t> buf;
    buf.resize(len1);
    for (size_t i = 0; i < len1; i++) {
        buf[i] = templateString[i];
    }
    UTF16StringDataNonGCStd str(buf.data(), len1);
    size_t idx;
    if ((idx = str.find(u"%s")) != SIZE_MAX) {
        str.replace(str.begin() + idx, str.begin() + idx + 2, replacer->toUTF16StringData().data());
    }
    errorMessage = new UTF16String(str.data(), str.length());
    if (code == ReferenceError)
        state.throwException(new ReferenceErrorObject(state, errorMessage));
    else if (code == TypeError)
        state.throwException(new TypeErrorObject(state, errorMessage));
    else if (code == SyntaxError)
        state.throwException(new SyntaxErrorObject(state, errorMessage));
    else if (code == RangeError)
        state.throwException(new RangeErrorObject(state, errorMessage));
    else if (code == URIError)
        state.throwException(new URIErrorObject(state, errorMessage));
    else if (code == EvalError)
        state.throwException(new EvalErrorObject(state, errorMessage));
    else
        state.throwException(new ErrorObject(state, errorMessage));
}

ErrorObject::ErrorObject(ExecutionState& state, String* errorMessage)
    : Object(state)
    , m_stackTraceData(nullptr)
{
    if (errorMessage->length()) {
        defineOwnPropertyThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message,
                                                       ObjectPropertyDescriptor(errorMessage, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    }
    Object::setPrototype(state, state.context()->globalObject()->errorPrototype());
}

ErrorObject* ErrorObject::createError(ExecutionState& state, ErrorObject::Code code, String* errorMessage)
{
    if (code == ReferenceError)
        return new ReferenceErrorObject(state, errorMessage);
    else if (code == TypeError)
        return new TypeErrorObject(state, errorMessage);
    else if (code == SyntaxError)
        return new SyntaxErrorObject(state, errorMessage);
    else if (code == RangeError)
        return new RangeErrorObject(state, errorMessage);
    else if (code == URIError)
        return new URIErrorObject(state, errorMessage);
    else if (code == EvalError)
        return new EvalErrorObject(state, errorMessage);
    else
        return new ErrorObject(state, errorMessage);
}

ReferenceErrorObject::ReferenceErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->referenceErrorPrototype());
}

TypeErrorObject::TypeErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->typeErrorPrototype());
}

RangeErrorObject::RangeErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->rangeErrorPrototype());
}

SyntaxErrorObject::SyntaxErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->syntaxErrorPrototype());
}

URIErrorObject::URIErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->uriErrorPrototype());
}

EvalErrorObject::EvalErrorObject(ExecutionState& state, String* errorMessage)
    : ErrorObject(state, errorMessage)
{
    Object::setPrototype(state, state.context()->globalObject()->evalErrorPrototype());
}
} // namespace Escargot
