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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "NativeFunctionObject.h"
#include "runtime/BoundFunctionObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ScriptClassConstructorFunctionObject.h"
#include "parser/Lexer.h"

namespace Escargot {

static Value builtinFunctionEmptyFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value();
}

static Value builtinFunctionConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (newTarget.hasValue() && UNLIKELY((bool)state.context()->securityPolicyCheckCallback())) {
        Value checkMSG = state.context()->securityPolicyCheckCallback()(state, false);
        if (!checkMSG.isEmpty()) {
            ASSERT(checkMSG.isString());
            ErrorObject* err = ErrorObject::createError(state, ErrorObject::Code::EvalError, checkMSG.asString());
            state.throwException(err);
            return Value();
        }
    }

    size_t argumentVectorCount = argc > 1 ? argc - 1 : 0;
    Value sourceValue = argc >= 1 ? argv[argc - 1] : Value(String::emptyString);
    auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().anonymous, argumentVectorCount, argv, sourceValue, false, false, false, false);

    // Let proto be ? GetPrototypeFromConstructor(newTarget, fallbackProto).
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->functionPrototype();
    });

    ScriptFunctionObject* result = new ScriptFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment, true, false, false);

    return result;
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#sec-function.prototype.tostring
static Value builtinFunctionToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (LIKELY(thisValue.isObject())) {
        Object* func = thisValue.asObject();

        if (LIKELY(func->isFunctionObject())) {
            FunctionObject* fn = func->asFunctionObject();

            if (fn->isScriptClassConstructorFunctionObject()) {
                return fn->asScriptFunctionObject()->asScriptClassConstructorFunctionObject()->classSourceCode();
            } else {
                StringBuilder builder;
                if (fn->codeBlock()->isInterpretedCodeBlock() && fn->codeBlock()->asInterpretedCodeBlock()->script() != nullptr) {
                    StringView src = fn->codeBlock()->asInterpretedCodeBlock()->src();
                    size_t length = src.length();
                    while (length > 0 && EscargotLexer::isWhiteSpaceOrLineTerminator(src[length - 1])) {
                        length--;
                    }
                    builder.appendString(new StringView(src, 0, length));
                } else {
                    builder.appendString("function ");
                    builder.appendString(fn->codeBlock()->functionName().string());
                    builder.appendString("() { [native code] }");
                }

                return builder.finalize(&state);
            }
        }

        if (func->isBoundFunctionObject() || func->isCallable()) {
            StringBuilder builder;
            builder.appendString("function () { [native code] }");
            return builder.finalize(&state);
        }
    }

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_ThisNotFunctionObject);
    return Value();
}

static Value builtinFunctionApply(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), ErrorObject::Messages::GlobalObject_ThisNotFunctionObject);
    }
    Value thisArg = argv[0];
    Value argArray = argv[1];
    size_t arrlen = 0;
    Value* arguments = nullptr;
    if (argArray.isUndefinedOrNull()) {
        // do nothing
    } else if (argArray.isObject()) {
        Object* obj = argArray.asObject();
        arrlen = obj->length(state);
        arguments = ALLOCA(sizeof(Value) * arrlen, Value, state);
        for (size_t i = 0; i < arrlen; i++) {
            auto re = obj->getIndexedProperty(state, Value(i));
            if (re.hasValue()) {
                arguments[i] = re.value(state, obj);
            } else {
                arguments[i] = Value();
            }
        }
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), ErrorObject::Messages::GlobalObject_SecondArgumentNotObject);
    }

    return Object::call(state, thisValue, thisArg, arrlen, arguments);
}

static Value builtinFunctionCall(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), ErrorObject::Messages::GlobalObject_ThisNotFunctionObject);
    }
    Value thisArg = argv[0];
    size_t arrlen = argc > 0 ? argc - 1 : 0;
    Value* arguments = ALLOCA(sizeof(Value) * arrlen, Value, state);
    for (size_t i = 0; i < arrlen; i++) {
        arguments[i] = argv[i + 1];
    }

    return Object::call(state, thisValue, thisArg, arrlen, arguments);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-function.prototype.bind
static Value builtinFunctionBind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If IsCallable(Target) is false, throw a TypeError exception.
    if (!thisValue.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().bind.string(), ErrorObject::Messages::GlobalObject_ThisNotFunctionObject);
    }

    // Let Target be the this value.
    Object* target = thisValue.asObject();

    // Let args be a new (possibly empty) List consisting of all of the argument values provided after thisArg in order.
    Value boundThis = argv[0];
    size_t boundArgc = (argc > 0) ? argc - 1 : 0;
    Value* boundArgv = (boundArgc > 0) ? argv + 1 : nullptr;
    //BoundFunctionObject* F = new BoundFunctionObject(state, thisValue, boundThis, boundArgc, boundArgv);

    // Let targetHasLength be HasOwnProperty(Target, "length").
    bool targetHasLength = target->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().length));
    double length = 0;
    // If targetHasLength is true, then
    if (targetHasLength) {
        // Let targetLen be Get(Target, "length").
        Value targetLen = target->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, target);
        // If Type(targetLen) is not Number, let L be 0.
        // Else Let targetLen be ToInteger(targetLen).
        // Let L be the larger of 0 and the result of targetLen minus the number of elements of args.
        if (targetLen.isNumber()) {
            length = std::max(0.0, targetLen.toInteger(state) - boundArgc);
        }
    }

    // F->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length),
    //                                    ObjectPropertyDescriptor(Value(length), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // Let targetName be Get(Target, "name").
    Value targetName = target->get(state, ObjectPropertyName(state.context()->staticStrings().name)).value(state, target);
    // If Type(targetName) is not String, let targetName be the empty string.
    if (!targetName.isString()) {
        targetName = String::emptyString;
    }

    StringBuilder builder;
    builder.appendString("bound ");
    builder.appendString(targetName.asString());
    //F->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().name),
    //                                    ObjectPropertyDescriptor(Value(builder.finalize(&state)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // Let F be BoundFunctionCreate(Target, thisArg, args).
    // Let status be DefinePropertyOrThrow(F, "length", PropertyDescriptor {[[Value]]: L, [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}).
    // Perform SetFunctionName(F, targetName, "bound").
    // Return F.
    return new BoundFunctionObject(state, target, boundThis, boundArgc, boundArgv, Value(length), Value(builder.finalize(&state)));
}

static Value builtinFunctionHasInstanceOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject()) {
        return Value(false);
    }
    return Value(thisValue.asObject()->hasInstance(state, argv[0]));
}

void GlobalObject::installFunction(ExecutionState& state)
{
    m_functionPrototype = new NativeFunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(AtomicString(), builtinFunctionEmptyFunction, 0, NativeFunctionInfo::Strict)),
                                                   NativeFunctionObject::__ForGlobalBuiltin__);
    m_functionPrototype->setGlobalIntrinsicObject(state);

    m_function = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_function->setGlobalIntrinsicObject(state);
    m_function->setFunctionPrototype(state, m_functionPrototype);

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                                          ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinFunctionToString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionApply = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().apply, builtinFunctionApply, 2, NativeFunctionInfo::Strict));
    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().apply),
                                                          ObjectPropertyDescriptor(m_functionApply, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().call),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().call, builtinFunctionCall, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().bind),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().bind, builtinFunctionBind, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().hasInstance)),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.hasInstance]")), builtinFunctionHasInstanceOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Function),
                      ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
