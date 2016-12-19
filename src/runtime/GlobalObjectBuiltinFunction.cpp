#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"

namespace Escargot {

static Value builtinFunctionEmptyFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Value builtinFunctionConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // TODO
    RELEASE_ASSERT_NOT_REACHED();
    return Value();
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
                arguments[i] = re.value();
        }
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Function.string(), true, state.context()->staticStrings().apply.string(), errorMessage_GlobalObject_SecondArgumentNotObject);
    }

    return thisVal->call(state, thisArg, arrlen, arguments);
}

void GlobalObject::installFunction(ExecutionState& state)
{
    FunctionObject* emptyFunction = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionEmptyFunction, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                                                                // TODO
                                                                                RELEASE_ASSERT_NOT_REACHED();
                                                                            })),
                                                       FunctionObject::__ForBuiltin__);
    m_functionPrototype = emptyFunction;
    m_functionPrototype->setPrototype(state, m_objectPrototype);
    // TODO convert into defineOwnProperty
    // m_functionPrototype->setFunctionPrototype(state, emptyFunction);

    m_function = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                        // TODO
                                        RELEASE_ASSERT_NOT_REACHED();
                                    }));
    m_function->markThisObjectDontNeedStructureTransitionTable(state);
    // TODO m_function->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);

    m_function->setPrototype(state, emptyFunction);
    m_function->setFunctionPrototype(state, emptyFunction);
    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                                          ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_functionPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().apply),
                                                          ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().apply, builtinFunctionApply, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Function),
                      ObjectPropertyDescriptor(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
