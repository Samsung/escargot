/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "parser/ScriptParser.h"
#include "parser/esprima_cpp/esprima.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

static Value builtinFunctionEmptyFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Value builtinFunctionConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
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
        esprima::parseProgram(state.context(), StringView(cur, 0, cur->length()), nullptr, false, SIZE_MAX);
    } catch (esprima::Error* orgError) {
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

    InterpretedCodeBlock* cb = parserResult.m_script->topCodeBlock()->childBlocks()[0];
    cb->updateSourceElementStart(2, 1);

    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, parserResult.m_script->topCodeBlock(), state.context()->globalObject()), nullptr);
    return new FunctionObject(state, cb, globalEnvironment);
}

static Value builtinFunctionToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isFunction())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotFunctionObject);

    FunctionObject* fn = thisValue.asFunction();
    StringBuilder builder;
    builder.appendString("function ");
    builder.appendString(fn->codeBlock()->functionName().string());
    builder.appendString("(");

    if (fn->codeBlock()->isInterpretedCodeBlock()) {
        for (size_t i = 0; i < fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation().size(); i++) {
            builder.appendString(fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation()[i].m_name.string());
            if (i < (fn->codeBlock()->asInterpretedCodeBlock()->parametersInfomation().size() - 1)) {
                builder.appendString(", ");
            }
        }
    }

    builder.appendString(") ");
    if (fn->codeBlock()->isInterpretedCodeBlock() && fn->codeBlock()->asInterpretedCodeBlock()->script()) {
        StringView src = fn->codeBlock()->asInterpretedCodeBlock()->src();
        while (src[src.length() - 1] != '}') {
            src = StringView(src.string(), src.start(), src.end() - 1);
        }
        builder.appendString(&src);
    } else {
        builder.appendString("{ [native code] }");
    }

    return builder.finalize(&state);
}

static Value builtinFunctionApply(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isFunction())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
    FunctionObject* thisVal = thisValue.asFunction();
    Value thisArg = argv[0];
    Value argArray = argv[1];
    size_t arrlen;
    Value* arguments = nullptr;
    if (argArray.isUndefinedOrNull()) {
        // do nothing
        arrlen = 0;
        arguments = nullptr;
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

    return thisVal->call(state, thisArg, arrlen, arguments);
}

static Value builtinFunctionCall(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isFunction())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_ThisNotFunctionObject);
    FunctionObject* thisVal = thisValue.asFunction();
    Value thisArg = argv[0];
    size_t arrlen = argc > 0 ? argc - 1 : 0;
    Value* arguments = ALLOCA(sizeof(Value) * arrlen, Value, state);
    for (size_t i = 0; i < arrlen; i++) {
        arguments[i] = argv[i + 1];
    }

    return thisVal->call(state, thisArg, arrlen, arguments);
}

static Value builtinFunctionBind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isFunction())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().bind.string(), errorMessage_GlobalObject_ThisNotFunctionObject);

    FunctionObject* targetFunction = thisValue.asFunction();

    Value boundThis = argv[0];
    size_t boundArgc = (argc >= 1) ? argc - 1 : argc;
    Value* boundArgv = (boundArgc == 0) ? nullptr : argv + 1;
    CodeBlock* cb = new CodeBlock(state, targetFunction, boundThis, boundArgc, boundArgv);

    StringBuilder builder;
    builder.appendString("bound ");
    ObjectGetResult r = targetFunction->getOwnProperty(state, state.context()->staticStrings().name);
    Value fn;
    if (r.hasValue()) {
        fn = r.value(state, targetFunction);
    }
    if (fn.isString())
        builder.appendString(fn.asString());

    return new FunctionObject(state, cb, builder.finalize(&state), FunctionObject::ForBind::__ForBind__);
}

void GlobalObject::installFunction(ExecutionState& state)
{
    FunctionObject* emptyFunction = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionEmptyFunction, 1, nullptr, 0)),
                                                       FunctionObject::__ForBuiltin__);

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

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Function),
                      ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
