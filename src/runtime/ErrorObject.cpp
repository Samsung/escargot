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

#include "Escargot.h"
#include "ErrorObject.h"
#include "Context.h"
#include "SandBox.h"
#include "NativeFunctionObject.h"

namespace Escargot {

ErrorObject* ErrorObject::createError(ExecutionState& state, ErrorCode code, String* errorMessage, bool fillStackInfo)
{
    switch (code) {
    case ErrorCode::ReferenceError:
        return new ReferenceErrorObject(state, state.context()->globalObject()->referenceErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::TypeError:
        return new TypeErrorObject(state, state.context()->globalObject()->typeErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::SyntaxError:
        return new SyntaxErrorObject(state, state.context()->globalObject()->syntaxErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::RangeError:
        return new RangeErrorObject(state, state.context()->globalObject()->rangeErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::URIError:
        return new URIErrorObject(state, state.context()->globalObject()->uriErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::EvalError:
        return new EvalErrorObject(state, state.context()->globalObject()->evalErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::AggregateError:
        return new AggregateErrorObject(state, state.context()->globalObject()->aggregateErrorPrototype(), errorMessage, fillStackInfo);
#if defined(ENABLE_WASM)
    case ErrorCode::WASMCompileError:
        return new WASMCompileErrorObject(state, state.context()->globalObject()->wasmCompileErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::WASMLinkError:
        return new WASMLinkErrorObject(state, state.context()->globalObject()->wasmLinkErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::WASMRuntimeError:
        return new WASMRuntimeErrorObject(state, state.context()->globalObject()->wasmRuntimeErrorPrototype(), errorMessage, fillStackInfo);
#endif
    default:
        return new ErrorObject(state, state.context()->globalObject()->errorPrototype(), errorMessage, fillStackInfo);
    }
}

ErrorObject* ErrorObject::createBuiltinError(ExecutionState& state, ErrorCode code, String* objectName, bool prototype, String* functionName, const char* templateString, bool fillStackInfo)
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

    size_t len = strlen(templateString);
    std::basic_string<char16_t> buf;
    buf.resize(len);
    for (size_t i = 0; i < len; i++) {
        buf[i] = templateString[i];
    }
    UTF16StringDataNonGCStd str(buf.data(), len);
    size_t idx;
    if ((idx = str.find(u"%s")) != SIZE_MAX) {
        str.replace(str.begin() + idx, str.begin() + idx + 2, replacer->toUTF16StringData().data());
    }
    errorMessage = new UTF16String(str.data(), str.length());
    switch (code) {
    case ErrorCode::ReferenceError:
        return new ReferenceErrorObject(state, state.context()->globalObject()->referenceErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::TypeError:
        return new TypeErrorObject(state, state.context()->globalObject()->typeErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::SyntaxError:
        return new SyntaxErrorObject(state, state.context()->globalObject()->syntaxErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::RangeError:
        return new RangeErrorObject(state, state.context()->globalObject()->rangeErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::URIError:
        return new URIErrorObject(state, state.context()->globalObject()->uriErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::EvalError:
        return new EvalErrorObject(state, state.context()->globalObject()->evalErrorPrototype(), errorMessage, fillStackInfo);
        break;
    case ErrorCode::AggregateError:
        return new AggregateErrorObject(state, state.context()->globalObject()->aggregateErrorPrototype(), errorMessage, fillStackInfo);
        break;
#if defined(ENABLE_WASM)
    case ErrorCode::WASMCompileError:
        return new WASMCompileErrorObject(state, state.context()->globalObject()->wasmCompileErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::WASMLinkError:
        return new WASMLinkErrorObject(state, state.context()->globalObject()->wasmLinkErrorPrototype(), errorMessage, fillStackInfo);
    case ErrorCode::WASMRuntimeError:
        return new WASMRuntimeErrorObject(state, state.context()->globalObject()->wasmRuntimeErrorPrototype(), errorMessage, fillStackInfo);
#endif
    default:
        return new ErrorObject(state, state.context()->globalObject()->errorPrototype(), errorMessage, fillStackInfo);
        break;
    }
}

void ErrorObject::throwBuiltinError(ExecutionState& state, ErrorCode code, String* objectName, bool prototype, String* functionName, const char* templateString)
{
    state.throwException(Value(ErrorObject::createBuiltinError(state, code, objectName, prototype, functionName, templateString, false)));
}

void ErrorObject::throwBuiltinError(ExecutionState& state, ErrorCode code, String* errorMessage)
{
    state.throwException(Value(ErrorObject::createError(state, code, errorMessage, false)));
}

static Value builtinErrorObjectStackInfoGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!(LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isErrorObject()))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "get Error.prototype.stack called on incompatible receiver");
    }

    ErrorObject* obj = thisValue.asObject()->asErrorObject();
    if (!obj->stackTraceData()) {
        return String::emptyString;
    }

    auto stackTraceData = obj->stackTraceData().value();
    StringBuilder builder;
    stackTraceData->buildStackTrace(state.context(), builder);
    return builder.finalize();
}

static Value builtinErrorObjectStackInfoSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!(LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isErrorObject()))) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "set Error.prototype.stack called on incompatible receiver");
    }

    // Do noting
    return Value();
}

ErrorObject::ErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : DerivedObject(state, proto)
    , m_stackTraceData(nullptr)
{
    if (errorMessage->length()) {
        defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,
                                         ObjectPropertyDescriptor(errorMessage, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    }

    Context* context = state.context();
    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(context->staticStrings().stack, builtinErrorObjectStackInfoGet, 0, NativeFunctionInfo::Strict)),
        new NativeFunctionObject(state, NativeFunctionInfo(context->staticStrings().stack, builtinErrorObjectStackInfoSet, 0, NativeFunctionInfo::Strict)));
    ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    defineOwnProperty(state, ObjectPropertyName(context->staticStrings().stack), desc);
    if (fillStackInfo) {
        updateStackTraceData(state);
    }
}

void ErrorObject::updateStackTraceData(ExecutionState& state)
{
    StackTraceDataOnStackVector stackTraceDataVector;
    SandBox::createStackTrace(stackTraceDataVector, state);
    setStackTraceData(StackTraceData::create(stackTraceDataVector));
}

ReferenceErrorObject::ReferenceErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

TypeErrorObject::TypeErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

RangeErrorObject::RangeErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

SyntaxErrorObject::SyntaxErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

URIErrorObject::URIErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

EvalErrorObject::EvalErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

AggregateErrorObject::AggregateErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

#if defined(ENABLE_WASM)
WASMCompileErrorObject::WASMCompileErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

WASMLinkErrorObject::WASMLinkErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}

WASMRuntimeErrorObject::WASMRuntimeErrorObject(ExecutionState& state, Object* proto, String* errorMessage, bool fillStackInfo)
    : ErrorObject(state, proto, errorMessage, fillStackInfo)
{
}
#endif
} // namespace Escargot
