/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "NativeFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForGlobalBuiltin)
    : FunctionObject(state, state.context()->globalObject()->objectPrototype(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
    ASSERT(!NativeFunctionObject::isConstructor());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinConstructor)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    initStructureAndValues(state, info.m_isConstructor, false, false);
    if (info.m_isConstructor) {
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
    }
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinProxyConstructor)
    : FunctionObject(state, state.context()->globalObject()->functionPrototype(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new NativeCodeBlock(state.context(), info);
    // The Proxy constructor does not have a prototype property
    m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionLength()));

    ASSERT(NativeFunctionObject::isConstructor());
}

NativeFunctionObject::NativeFunctionObject(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info)
    : FunctionObject(structure, std::move(values), context->globalObject()->functionPrototype())
{
    m_codeBlock = new NativeCodeBlock(context, info);
}

bool NativeFunctionObject::isConstructor() const
{
    return nativeCodeBlock()->isNativeConstructor();
}

template <bool isConstruct>
Value NativeFunctionObject::processNativeFunctionCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, Value* argv, Optional<Object*> newTarget)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY(state.stackLimit() > currentStackBase)) {
#else
    if (UNLIKELY(state.stackLimit() < currentStackBase)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    NativeCodeBlock* codeBlock = nativeCodeBlock();
    Context* ctx = codeBlock->context();
    bool isStrict = codeBlock->isStrict();

    NativeFunctionPointer nativeFunc = codeBlock->nativeFunction();

    size_t len = codeBlock->functionLength();
    if (argc < len) {
        Value* newArgv = (Value*)alloca(sizeof(Value) * len);
        for (size_t i = 0; i < argc; i++) {
            newArgv[i] = argv[i];
        }
        for (size_t i = argc; i < len; i++) {
            newArgv[i] = Value();
        }
        argv = newArgv;
    }

    Value receiver = receiverSrc;
    ExecutionState newState(ctx, &state, this, argc, argv, isStrict);

    if (!isConstruct) {
        // prepare receiver
        if (UNLIKELY(!isStrict)) {
            if (receiver.isUndefinedOrNull()) {
                receiver = ctx->globalObject();
            } else {
                receiver = receiver.toObject(newState);
            }
        }
    }

    Value result;
    if (isConstruct) {
        result = nativeFunc(newState, receiver, argc, argv, newTarget);
        if (UNLIKELY(!result.isObject())) {
            ErrorObject::throwBuiltinError(newState, ErrorObject::TypeError, "Native Constructor must returns constructed new object");
        }
    } else {
        ASSERT(!newTarget);
        result = nativeFunc(newState, receiver, argc, argv, nullptr);
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state.context()->debugger(), &newState, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */
    return result;
}

Value NativeFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(codeBlock()->isNativeCodeBlock());
    return processNativeFunctionCall<false>(state, thisValue, argc, argv, nullptr);
}

Object* NativeFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget->isObject());
    ASSERT(newTarget->isConstructor());
    ASSERT(codeBlock()->isNativeCodeBlock());

    Value result = processNativeFunctionCall<true>(state, Value(), argc, argv, newTarget);
    return result.asObject();
}
}
