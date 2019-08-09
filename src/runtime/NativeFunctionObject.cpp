/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "NativeFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

// function for derived classes. derived class MUST initlize member variable of FunctionObject.
NativeFunctionObject::NativeFunctionObject(ExecutionState& state, size_t defaultSpace)
    : FunctionObject(state, defaultSpace)
{
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock)
    : FunctionObject(state, codeBlock->isNativeFunctionConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = codeBlock;
    initStructureAndValues(state, m_codeBlock->isNativeFunctionConstructor(), false);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    if (NativeFunctionObject::isConstructor())
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(FunctionObject::codeBlock()->hasCallNativeFunctionCode());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : FunctionObject(state, info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new CodeBlock(state.context(), info);
    initStructureAndValues(state, m_codeBlock->isNativeFunctionConstructor(), false);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin)
    : FunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = codeBlock;
    ASSERT(!NativeFunctionObject::isConstructor());
    initStructureAndValues(state, m_codeBlock->isNativeFunctionConstructor(), false);
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltinConstructor)
    : FunctionObject(state, codeBlock->isNativeFunctionConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = codeBlock;

    initStructureAndValues(state, codeBlock->isNativeFunctionConstructor(), false);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    if (NativeFunctionObject::isConstructor())
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(FunctionObject::codeBlock()->hasCallNativeFunctionCode());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinConstructor)
    : FunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3)
{
    m_codeBlock = new CodeBlock(state.context(), info);

    initStructureAndValues(state, m_codeBlock->isNativeFunctionConstructor(), false);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(NativeFunctionObject::isConstructor());
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinProxyConstructor)
    : FunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = new CodeBlock(state.context(), info);

    // The Proxy constructor does not have a prototype property
    m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->parameterCount()));
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());

    ASSERT(NativeFunctionObject::isConstructor());
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
}

bool NativeFunctionObject::isConstructor() const
{
    return m_codeBlock->isNativeFunctionConstructor();
}

template <bool isConstruct>
Value NativeFunctionObject::processNativeFunctionCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, Value* argv, Object* newTarget)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY((state.stackBase() - currentStackBase) > STACK_LIMIT_FROM_BASE)) {
#else
    if (UNLIKELY((currentStackBase - state.stackBase()) > STACK_LIMIT_FROM_BASE)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    Context* ctx = m_codeBlock->context();
    bool isStrict = m_codeBlock->isStrict();

    CallNativeFunctionData* code = m_codeBlock->nativeFunctionData();

    size_t len = m_codeBlock->parameterCount();
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
    ExecutionState newState(ctx, &state, nullptr, this, argc, argv, isStrict);

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

    try {
        Value result = code->m_fn(newState, receiver, argc, argv, isConstruct);
        if (isConstruct) {
            if (UNLIKELY(!result.isObject())) {
                ErrorObject::throwBuiltinError(newState, ErrorObject::TypeError, "Native Constructor must returns constructed new object");
            }
            // if newTarget is differ with this function object, ex) class A extends Array{}; new A();
            if (newTarget != this) {
                // set prototype for native object
                Object* thisArgument = result.asObject();
                // Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
                // OrdinaryCreateFromConstructor -> Let proto be GetPrototypeFromConstructor(constructor, intrinsicDefaultProto).
                // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let proto be Get(constructor, "prototype").
                Value proto = newTarget->get(newState, ObjectPropertyName(newState.context()->staticStrings().prototype)).value(newState, newTarget);
                // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> If Type(proto) is not Object, then
                // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let realm be GetFunctionRealm(constructor).
                // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> ReturnIfAbrupt(realm).
                // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let proto be realm’s intrinsic object named intrinsicDefaultProto.
                if (!proto.isObject()) {
                    proto = ctx->globalObject()->objectPrototype();
                }
                thisArgument->setPrototype(state, proto);
            }
            return result;
        }
        return result;
    } catch (const Value& v) {
        ByteCodeInterpreter::processException(newState, v, m_codeBlock, SIZE_MAX);
        return Value();
    }
}

Value NativeFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
    return processNativeFunctionCall<false>(state, thisValue, argc, argv, nullptr);
}

Object* NativeFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget->isObject());
    ASSERT(newTarget->isConstructor());
    ASSERT(codeBlock()->hasCallNativeFunctionCode());

    Value result = processNativeFunctionCall<true>(state, Value(), argc, argv, newTarget);
    return result.asObject();
}
}
