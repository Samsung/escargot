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

namespace Escargot {

ErrorObject* ErrorObject::createError(ExecutionState& state, ErrorObject::Code code, String* errorMessage)
{
    switch (code) {
    case ReferenceError:
        return new ReferenceErrorObject(state, state.context()->globalObject()->referenceErrorPrototype(), errorMessage);
    case TypeError:
        return new TypeErrorObject(state, state.context()->globalObject()->typeErrorPrototype(), errorMessage);
    case SyntaxError:
        return new SyntaxErrorObject(state, state.context()->globalObject()->syntaxErrorPrototype(), errorMessage);
    case RangeError:
        return new RangeErrorObject(state, state.context()->globalObject()->rangeErrorPrototype(), errorMessage);
    case URIError:
        return new URIErrorObject(state, state.context()->globalObject()->uriErrorPrototype(), errorMessage);
    case EvalError:
        return new EvalErrorObject(state, state.context()->globalObject()->evalErrorPrototype(), errorMessage);
    case AggregateError:
        return new AggregateErrorObject(state, state.context()->globalObject()->aggregateErrorPrototype(), errorMessage);
#if defined(ENABLE_WASM)
    case WASMCompileError:
        return new WASMCompileErrorObject(state, state.context()->globalObject()->wasmCompileErrorPrototype(), errorMessage);
    case WASMLinkError:
        return new WASMLinkErrorObject(state, state.context()->globalObject()->wasmLinkErrorPrototype(), errorMessage);
    case WASMRuntimeError:
        return new WASMRuntimeErrorObject(state, state.context()->globalObject()->wasmRuntimeErrorPrototype(), errorMessage);
#endif
    default:
        return new ErrorObject(state, state.context()->globalObject()->errorPrototype(), errorMessage);
    }
}

ErrorObject* ErrorObject::createBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString)
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
    case ReferenceError:
        return new ReferenceErrorObject(state, state.context()->globalObject()->referenceErrorPrototype(), errorMessage);
        break;
    case TypeError:
        return new TypeErrorObject(state, state.context()->globalObject()->typeErrorPrototype(), errorMessage);
        break;
    case SyntaxError:
        return new SyntaxErrorObject(state, state.context()->globalObject()->syntaxErrorPrototype(), errorMessage);
        break;
    case RangeError:
        return new RangeErrorObject(state, state.context()->globalObject()->rangeErrorPrototype(), errorMessage);
        break;
    case URIError:
        return new URIErrorObject(state, state.context()->globalObject()->uriErrorPrototype(), errorMessage);
        break;
    case EvalError:
        return new EvalErrorObject(state, state.context()->globalObject()->evalErrorPrototype(), errorMessage);
        break;
    case AggregateError:
        return new AggregateErrorObject(state, state.context()->globalObject()->aggregateErrorPrototype(), errorMessage);
        break;
#if defined(ENABLE_WASM)
    case WASMCompileError:
        return new WASMCompileErrorObject(state, state.context()->globalObject()->wasmCompileErrorPrototype(), errorMessage);
    case WASMLinkError:
        return new WASMLinkErrorObject(state, state.context()->globalObject()->wasmLinkErrorPrototype(), errorMessage);
    case WASMRuntimeError:
        return new WASMRuntimeErrorObject(state, state.context()->globalObject()->wasmRuntimeErrorPrototype(), errorMessage);
#endif
    default:
        return new ErrorObject(state, state.context()->globalObject()->errorPrototype(), errorMessage);
        break;
    }
}

void ErrorObject::throwBuiltinError(ExecutionState& state, Code code, String* objectName, bool prototype, String* functionName, const char* templateString)
{
    state.throwException(Value(ErrorObject::createBuiltinError(state, code, objectName, prototype, functionName, templateString)));
}

void ErrorObject::throwBuiltinError(ExecutionState& state, Code code, String* errorMessage)
{
    state.throwException(Value(ErrorObject::createError(state, code, errorMessage)));
}

ErrorObject::ErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : Object(state, proto)
    , m_stackTraceData(nullptr)
{
    if (errorMessage->length()) {
        defineOwnPropertyThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message,
                                                       ObjectPropertyDescriptor(errorMessage, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    }
}

ReferenceErrorObject::ReferenceErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

TypeErrorObject::TypeErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

RangeErrorObject::RangeErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

SyntaxErrorObject::SyntaxErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

URIErrorObject::URIErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

EvalErrorObject::EvalErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

AggregateErrorObject::AggregateErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

#if defined(ENABLE_WASM)
WASMCompileErrorObject::WASMCompileErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

WASMLinkErrorObject::WASMLinkErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}

WASMRuntimeErrorObject::WASMRuntimeErrorObject(ExecutionState& state, Object* proto, String* errorMessage)
    : ErrorObject(state, proto, errorMessage)
{
}
#endif
} // namespace Escargot
