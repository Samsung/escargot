/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_WASM_INTERPRETER)

#include "Escargot.h"
#include "wasm/runtime/WASMFunction.h"
#include "wasm/runtime/WASMModule.h"
#include "wasm/interpreter/WASMInterpreter.h"

#include "runtime/SandBox.h"
#include "runtime/Object.h"
#include "runtime/VMInstance.h"

namespace Escargot {

void WASMFunction::call(const uint32_t argc, WASMValue* argv, WASMValue* result)
{
    uint8_t* sp = reinterpret_cast<uint8_t*>(alloca(m_moduleFunction->requiredStackSize()));
    WASMInterpreter::interpret(m_instance, this, reinterpret_cast<size_t>(m_moduleFunction->byteCode()), sp);

    WASMFunctionType* ft = functionType();
    const WASMFunctionType::WASMFunctionTypeVector& resultTypeInfo = ft->result();

    sp = sp - ft->resultStackSize();
    uint8_t* resultStackPointer = sp;
    for (size_t i = 0; i < resultTypeInfo.size(); i++) {
        result[i] = WASMValue(resultTypeInfo[i], resultStackPointer);
        resultStackPointer += wasmValueSizeInStack(resultTypeInfo[i]);
    }
}

void WASMImportedFunction::call(const uint32_t argc, WASMValue* argv, WASMValue* result)
{
    // Let realm be func's associated Realm.
    // Let relevant settings be realm?s settings object.
    SandBox sb(m_relatedContext);
    struct Sender {
        WASMImportedFunction* self;
        const uint32_t argc;
        WASMValue* argv;
        WASMValue* result;
    } s = { this, argc, argv, result };

    auto sbResult = sb.run([](ExecutionState& state, void* data) -> Value {
        WASMImportedFunction* self = reinterpret_cast<Sender*>(data)->self;
        const uint32_t argc = reinterpret_cast<Sender*>(data)->argc;
        WASMValue* argv = reinterpret_cast<Sender*>(data)->argv;
        WASMValue* result = reinterpret_cast<Sender*>(data)->result;

        // Let [parameters] ? [results] be functype.
        size_t argSize = self->functionType()->param().size();

        // Let jsArguments be << >>
        Value* jsArguments = ALLOCA(sizeof(Value) * argSize, Value, state);

        // For each arg of arguments,
        for (size_t i = 0; i < argSize; i++) {
            // Append ! ToJSValue(arg) to jsArguments.
            jsArguments[i] = argv[i].toValue();
        }

        // Let ret be ? Call(func, undefined, jsArguments).
        Value ret = Object::call(state, self->m_importedFunction, Value(), argSize, jsArguments);

        // Let resultsSize be results size.
        size_t resultsSize = self->functionType()->result().size();
        if (resultsSize == 0) {
            // If resultsSize is 0, return << >>
            return Value();
        }

        if (resultsSize == 1) {
            // Otherwise, if resultsSize is 1, return << ToWebAssemblyValue(ret, results[0]) >>
            result[0] = WASMValue(state, self->m_functionType->result()[0], ret);
        } else {
            // Otherwise,
            // Let method be ? GetMethod(ret, @@iterator).
            Value method = Object::getMethod(state, ret, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
            // If method is undefined, throw a TypeError.
            if (method.isUndefined()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_FuncCallError);
            }

            // Let values be ? IterableToList(ret, method).
            ValueVectorWithInlineStorage values = IteratorObject::iterableToList(state, ret, method);
            // If values's size is not resultsSize, throw a TypeError exception.
            if (values.size() != resultsSize) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_FuncCallError);
            }

            // For each value and resultType in values and results, paired linearly,
            // Append ToWebAssemblyValue(value, resultType) to wasmValues.
            for (size_t i = 0; i < resultsSize; i++) {
                result[i] = WASMValue(state, self->m_functionType->result()[i], ret);
            }
        }

        return Value();
    },
                           &s);

    // TODO
    // Assert: result.[[Type]] is throw or normal.
    // If result.[[Type]] is throw, then trigger a WebAssembly trap, and propagate result.[[Value]] to the enclosing JavaScript.
    // Otherwise, return result.[[Value]].
}

} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
