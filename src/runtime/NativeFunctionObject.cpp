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
    : FunctionObject(state, codeBlock->isConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = codeBlock;
    m_homeObject = nullptr;

    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    if (isConstructor())
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();

    ASSERT(FunctionObject::codeBlock()->hasCallNativeFunctionCode());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : FunctionObject(state, info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2))
{
    m_codeBlock = new CodeBlock(state.context(), info);
    m_homeObject = nullptr;

    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

NativeFunctionObject::NativeFunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin)
    : FunctionObject(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)
{
    m_codeBlock = codeBlock;
    m_homeObject = nullptr;

    ASSERT(!isConstructor());
    initStructureAndValues(state);
}

Value NativeFunctionObject::processNativeFunctionCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, Value* argv, bool isNewExpression, const Value& newTarget)
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
    bool isSuperCall = state.isOnGoingSuperCall();

    if (UNLIKELY(isSuperCall && isBuiltin() && !isNewExpression)) {
        Value returnValue = Object::construct(state, this, argc, argv);
        returnValue.asObject()->setPrototype(state, receiverSrc.toObject(state)->getPrototype(state));
        return returnValue;
    }

    CallNativeFunctionData* code = m_codeBlock->nativeFunctionData();
    FunctionEnvironmentRecordSimple record(this);
    LexicalEnvironment env(&record, nullptr);

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
    // prepare receiver
    if (UNLIKELY(!isStrict)) {
        if (receiver.isUndefinedOrNull()) {
            receiver = ctx->globalObject();
        } else {
            receiver = receiver.toObject(state);
        }
    }

    // FIXME bind this value and newTarget
    record.bindThisValue(state, receiver);
    if (isNewExpression) {
        record.setNewTarget(newTarget);
    }

    ExecutionState newState(ctx, &state, &env, isStrict, &receiver);
    try {
        Value returnValue = code->m_fn(newState, receiver, argc, argv, isNewExpression);
        if (UNLIKELY(isSuperCall)) {
            state.getThisEnvironment()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->bindThisValue(state, returnValue);
            state.setOnGoingSuperCall(false);
            if (returnValue.isObject() && !isBuiltin()) {
                returnValue.asObject()->setPrototype(state, receiver.toObject(state)->getPrototype(state));
            }
        }
        return returnValue;
    } catch (const Value& v) {
        ByteCodeInterpreter::processException(newState, v, SIZE_MAX);
        return Value();
    }
}

Value NativeFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
    return processNativeFunctionCall(state, thisValue, argc, argv, false, Value());
}

Object* NativeFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget)
{
    ASSERT(codeBlock()->hasCallNativeFunctionCode());
    // FIXME: newTarget
    CodeBlock* cb = codeBlock();
    FunctionObject* constructor = this;

    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget.isObject());
    ASSERT(newTarget.isConstructor());

    Value result = processNativeFunctionCall(state, Value(Value::EmptyValue), argc, argv, true, newTarget);
    return result.asObject();
}
}
