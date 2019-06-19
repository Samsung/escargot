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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "parser/ScriptParser.h"
#include "parser/esprima_cpp/esprima.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/BoundFunctionObject.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/ProgramNode.h"

namespace Escargot {

static Value builtinFunctionEmptyFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Value builtinFunctionConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression && UNLIKELY((bool)state.context()->securityPolicyCheckCallback())) {
        Value checkMSG = state.context()->securityPolicyCheckCallback()(state, false);
        if (!checkMSG.isEmpty()) {
            ASSERT(checkMSG.isString());
            ErrorObject* err = ErrorObject::createError(state, ErrorObject::Code::EvalError, checkMSG.asString());
            state.throwException(err);
            return Value();
        }
    }

    StringBuilder src, srcToTest;
    src.appendString("function anonymous(");
    srcToTest.appendString("function anonymous(");

    for (size_t i = 1; i < argc; i++) {
        String* p = argv[i - 1].toString(state);
        src.appendString(p);
        srcToTest.appendString(p);
        if (i != argc - 1) {
            src.appendString(",");
            srcToTest.appendString(",");

            for (size_t j = 0; j < p->length(); j++) {
                char16_t c = p->charAt(j);
                if (c == '}' || c == '{' || c == ')' || c == '(') {
                    ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
                }
            }
        }
    }

    try {
        srcToTest.appendString("\r\n){ }");
        String* cur = srcToTest.finalize(&state);
        state.context()->vmInstance()->parsedSourceCodes().push_back(cur);
        esprima::parseProgram(state.context(), StringView(cur, 0, cur->length()), false, SIZE_MAX);
    } catch (esprima::Error& orgError) {
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
    }

    Value sourceValue = argc >= 1 ? argv[argc - 1] : Value();
    String* source = sourceValue.toString(state);

    auto data = source->bufferAccessData();

    for (size_t i = 0; i < data.length; i++) {
        char16_t c;
        if (data.has8BitContent) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c == '{') {
            break;
        } else if (c == '}') {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is unbalanced braces(}) in Function Constructor input");
        }
    }
    src.appendString("\n){\n");
    if (argc > 0) {
        src.appendString(source);
    }
    src.appendString("\n}");

    ScriptParser parser(state.context());
    auto parserResult = parser.parse(src.finalize(&state), new ASCIIString("Function Constructor input"));

    if (parserResult.m_error) {
        ErrorObject* err = ErrorObject::createError(state, parserResult.m_error->errorCode, parserResult.m_error->message);
        state.throwException(err);
    }

    parserResult.m_script->topCodeBlock()->cachedASTNode()->deref();
    parserResult.m_script->topCodeBlock()->clearCachedASTNode();

    InterpretedCodeBlock* cb = parserResult.m_script->topCodeBlock()->childBlocks()[0];
    cb->updateSourceElementStart(3, 1);
    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, parserResult.m_script->topCodeBlock(), state.context()->globalObject()), nullptr);
    return new FunctionObject(state, cb, globalEnvironment);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-function.prototype.tostring
static Value builtinFunctionToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // FIXME: If Type(func) is Object and is either a built-in function object or has an [[ECMAScriptCode]] internal slot, then
    if (thisValue.isFunction() == true) {
        FunctionObject* fn = thisValue.asFunction();
        StringBuilder builder;
        builder.appendString("function ");
        builder.appendString(fn->codeBlock()->functionName().string());
        builder.appendString("(");

        if (fn->codeBlock()->isInterpretedCodeBlock() == true) {
            for (size_t i = 0; i < fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation().size(); i++) {
                builder.appendString(fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation()[i].m_name.string());
                if (i < (fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation().size() - 1)) {
                    builder.appendString(", ");
                }
            }
        }

        builder.appendString(") ");
        if (fn->codeBlock()->isInterpretedCodeBlock() == true && fn->codeBlock()->asInterpretedCodeBlock()->script() != nullptr) {
            StringView src = fn->codeBlock()->asInterpretedCodeBlock()->src();
            while (src[src.length() - 1] != '}') {
                src = StringView(src, 0, src.length() - 1);
            }
            builder.appendString(new StringView(src));
        } else {
            builder.appendString("{ [native code] }");
        }

        return builder.finalize(&state);
    }

    if (thisValue.isObject() == true && thisValue.asObject()->isBoundFunctionObject() == true) {
        StringBuilder builder;
        builder.appendString("function () { [native code] }");
        return builder.finalize(&state);
    }

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
    return Value();
}

static Value builtinFunctionApply(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isCallable() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_SecondArgumentNotObject);
    }

    return Object::call(state, thisValue, thisArg, arrlen, arguments);
}

static Value builtinFunctionCall(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isCallable() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
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
static Value builtinFunctionBind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If IsCallable(Target) is false, throw a TypeError exception.
    if (thisValue.isCallable() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().bind.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
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
    if (targetHasLength == true) {
        // Let targetLen be Get(Target, "length").
        Value targetLen = target->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, target);
        // If Type(targetLen) is not Number, let L be 0.
        // Else Let targetLen be ToInteger(targetLen).
        // Let L be the larger of 0 and the result of targetLen minus the number of elements of args.
        if (targetLen.isNumber() == true) {
            length = std::max(0.0, targetLen.toInteger(state) - boundArgc);
        }
    }

    // F->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length),
    //                                    ObjectPropertyDescriptor(Value(length), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // Let targetName be Get(Target, "name").
    Value targetName = target->get(state, ObjectPropertyName(state.context()->staticStrings().name)).value(state, target);
    // If Type(targetName) is not String, let targetName be the empty string.
    if (targetName.isString() != true) {
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
    return new BoundFunctionObject(state, thisValue, boundThis, boundArgc, boundArgv, Value(length), Value(builder.finalize(&state)));
}

static Value builtinFunctionHasInstanceOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
#ifndef ESCARGOT_ENABLE_ES2015
    if (thisValue.isFunction() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
#endif
    return Value(Object::hasInstance(state, thisValue, argv[0]));
}

void GlobalObject::installFunction(ExecutionState& state)
{
    FunctionObject* emptyFunction = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionEmptyFunction, 0, nullptr, 0)),
                                                       FunctionObject::__ForGlobalBuiltin__);

    g_functionObjectTag = *((size_t*)emptyFunction);

    m_functionPrototype = emptyFunction;
    m_functionPrototype->setPrototype(state, m_objectPrototype);
    m_functionPrototype->markThisObjectDontNeedStructureTransitionTable(state);

    m_function = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                        // create dummy object.
                                        // this object is replaced in function ctor
                                        return new FunctionObject(state, NativeFunctionInfo(AtomicString(), builtinFunctionConstructor, 0, nullptr, 0));
                                    }),
                                    FunctionObject::__ForBuiltin__);
    m_function->markThisObjectDontNeedStructureTransitionTable(state);

    m_function->setPrototype(state, emptyFunction);
    m_function->setFunctionPrototype(state, emptyFunction);
    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                                          ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinFunctionToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().apply),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().apply, builtinFunctionApply, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().call),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().call, builtinFunctionCall, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().bind),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().bind, builtinFunctionBind, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().hasInstance)),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.hasInstance]")), builtinFunctionHasInstanceOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Function),
                      ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
