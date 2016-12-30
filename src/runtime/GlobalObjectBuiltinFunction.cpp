#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "parser/ScriptParser.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

static Value builtinFunctionEmptyFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Value builtinFunctionConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    StringBuilder src;
    src.appendString("function anonymous(");

    for (size_t i = 1; i < argc; i++) {
        src.appendString(argv[i - 1].toString(state));
        if (i != argc - 1) {
            src.appendChar(',');
        }
    }

    src.appendString("){\n");
    if (argc > 0) {
        src.appendString(argv[argc - 1].toString(state));
    }
    src.appendString("\n}");

    // TODO user should get line - 1
    ScriptParser parser(state.context());
    auto parserResult = parser.parse(src.finalize(), new ASCIIString("Function Constructor input"));

    if (parserResult.m_error) {
        ErrorObject* err = ErrorObject::createError(state, parserResult.m_error->errorCode, parserResult.m_error->message);
        state.throwException(err);
    }

    CodeBlock* cb = parserResult.m_script->topCodeBlock()->childBlocks()[0];

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
    builder.appendString("() {");
    if (fn->codeBlock()->isNativeFunction())
        builder.appendString(" [native code] ");
    builder.appendString("}");

    return builder.finalize();
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
            auto re = obj->get(state, ObjectPropertyName(state, Value(i)), obj);
            if (re.hasValue())
                arguments[i] = re.value(state, obj);
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

    FunctionObject* target = thisValue.asFunction();
    Value boundThis = argv[0];

    CodeBlock* simpleCb = new CodeBlock(state.context());
    size_t boundArgc = (argc >= 1) ? argc - 1 : argc;
    Value* boundArgv = (boundArgc == 0) ? nullptr : argv + 1;
    simpleCb->initializeCodeBlockForCallBound(target, boundThis, boundArgc, boundArgv);

    FunctionObject* fn = new FunctionObject(state, simpleCb, nullptr);
    return fn;
}

void GlobalObject::installFunction(ExecutionState& state)
{
    FunctionObject* emptyFunction = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionEmptyFunction, 1, nullptr, 0)),
                                                       FunctionObject::__ForBuiltin__);
    m_functionPrototype = emptyFunction;
    m_functionPrototype->setPrototype(state, m_objectPrototype);

    m_function = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
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
