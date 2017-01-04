#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ErrorObject.h"

namespace Escargot {

static Value builtinErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression) {
        Value message = argv[0];
        if (!message.isUndefined()) {
            thisValue.toObject(state)->asErrorObject()->defineOwnPropertyThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message,
                                                                                                       ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)));
        }
        return Value();
    } else {
        ErrorObject* obj = new ErrorObject(state, String::emptyString);
        Value message = argv[0];
        if (!message.isUndefined()) {
            obj->setThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message, message.toString(state), obj);
        }
        return obj;
    }
    return Value();
}

#define DEFINE_ERROR_CTOR(errorname)                                                                                                                                                                                                                                                          \
    static Value builtin##errorname##ErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                         \
        if (isNewExpression) {                                                                                                                                                                                                                                                                \
            Value message = argv[0];                                                                                                                                                                                                                                                          \
            if (!message.isUndefined()) {                                                                                                                                                                                                                                                     \
                thisValue.toObject(state)->defineOwnPropertyThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message,                                                                                                                                                    \
                                                                                          ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent))); \
            }                                                                                                                                                                                                                                                                                 \
            return Value();                                                                                                                                                                                                                                                                   \
        } else {                                                                                                                                                                                                                                                                              \
            ErrorObject* obj = new errorname##ErrorObject(state, String::emptyString);                                                                                                                                                                                                        \
            Value message = argv[0];                                                                                                                                                                                                                                                          \
            if (!message.isUndefined()) {                                                                                                                                                                                                                                                     \
                obj->setThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message, message.toString(state), obj);                                                                                                                                                         \
            }                                                                                                                                                                                                                                                                                 \
            return obj;                                                                                                                                                                                                                                                                       \
        }                                                                                                                                                                                                                                                                                     \
        return Value();                                                                                                                                                                                                                                                                       \
    }

DEFINE_ERROR_CTOR(Reference);
DEFINE_ERROR_CTOR(Type);
DEFINE_ERROR_CTOR(Syntax);
DEFINE_ERROR_CTOR(Range);
DEFINE_ERROR_CTOR(URI);
DEFINE_ERROR_CTOR(Eval);

static Value builtinErrorThrowTypeError(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new TypeErrorObject(state, String::fromUTF8("", 0)));
    return Value();
}

static Value builtinErrorToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, state.context()->staticStrings().Error.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotObject);

    Object* o = thisValue.toObject(state);

    // TODO
    /*
    StringRecursionChecker checker(o);
    if (checker.recursionCheck()) {
        return ESValue(strings->emptyString.string());
    }*/

    Value name = o->get(state, state.context()->staticStrings().name, o).value(state, o);
    String* nameStr;
    if (name.isUndefined()) {
        nameStr = state.context()->staticStrings().Error.string();
    } else {
        nameStr = name.toString(state);
    }
    Value message = o->get(state, state.context()->staticStrings().message, o).value(state, o);
    String* messageStr;
    if (message.isUndefined()) {
        messageStr = String::emptyString;
    } else {
        messageStr = message.toString(state);
    }

    if (nameStr->length() == 0) {
        return messageStr;
    }

    if (messageStr->length() == 0) {
        return nameStr;
    }

    StringBuilder builder;
    builder.appendString(nameStr);
    builder.appendString(": ");
    builder.appendString(messageStr);
    return builder.finalize();
}

void GlobalObject::installError(ExecutionState& state)
{
    m_error = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Error, builtinErrorConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                     return new ErrorObject(state, String::emptyString);
                                 }),
                                 FunctionObject::__ForBuiltin__);
    m_error->markThisObjectDontNeedStructureTransitionTable(state);

    m_error->setPrototype(state, m_functionPrototype);

    m_errorPrototype = m_objectPrototype;
    m_errorPrototype = new ErrorObject(state, String::emptyString);
    m_error->setFunctionPrototype(state, m_errorPrototype);
    m_errorPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_errorPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message, ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(state.context()->staticStrings().Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    auto errorToStringFn = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinErrorToString, 0, nullptr, NativeFunctionInfo::Strict));
    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().toString, ObjectPropertyDescriptor(errorToStringFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // http://www.ecma-international.org/ecma-262/5.1/#sec-13.2.3
    // 13.2.3 The [[ThrowTypeError]] Function Object
    m_throwTypeError = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ThrowTypeError, builtinErrorThrowTypeError, 0, nullptr, NativeFunctionInfo::Strict));


#define DEFINE_ERROR(errorname, bname)                                                                                                                                                                                                                                                                                                  \
    m_##errorname##Error = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().bname##Error, builtin##bname##ErrorConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {                                                                                                            \
                                                  return new bname##ErrorObject(state, String::emptyString);                                                                                                                                                                                                                            \
                                              }),                                                                                                                                                                                                                                                                                       \
                                              FunctionObject::__ForBuiltin__);                                                                                                                                                                                                                                                          \
    m_##errorname##Error->setPrototype(state, m_functionPrototype);                                                                                                                                                                                                                                                                     \
    m_##errorname##ErrorPrototype = m_errorPrototype;                                                                                                                                                                                                                                                                                   \
    m_##errorname##ErrorPrototype = new bname##ErrorObject(state, String::emptyString);                                                                                                                                                                                                                                                 \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().constructor, ObjectPropertyDescriptor(m_##errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                            \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().message, ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                                 \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(state.context()->staticStrings().bname##Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
    m_##errorname##Error->setFunctionPrototype(state, m_##errorname##ErrorPrototype);                                                                                                                                                                                                                                                   \
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().bname##Error),                                                                                                                                                                                                                                         \
                      ObjectPropertyDescriptor(m_##errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    DEFINE_ERROR(reference, Reference);
    DEFINE_ERROR(type, Type);
    DEFINE_ERROR(syntax, Syntax);
    DEFINE_ERROR(range, Range);
    DEFINE_ERROR(uri, URI);
    DEFINE_ERROR(eval, Eval);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Error),
                      ObjectPropertyDescriptor(m_error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
