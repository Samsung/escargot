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
               Object::ObjectPropertyDescriptorForDefineOwnProperty(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
       }
       return Value();
    } else {
       ErrorObject* obj = new ErrorObject(state, String::emptyString);
       Value message = argv[0];
       if (message.isUndefined()) {
           obj->setThrowsExceptionWhenStrictMode(state, state.context()->staticStrings().message, message.toString(state), obj);
       }
       return obj;
    }
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

    Value name = o->get(state, state.context()->staticStrings().name).m_value;
    String* nameStr;
    if (name.isUndefined()) {
        nameStr = state.context()->staticStrings().Error.string();
    } else {
        nameStr = name.toString(state);
    }
    Value message = o->get(state, state.context()->staticStrings().message).m_value;
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
    m_error = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Error, builtinErrorConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
        return new ErrorObject(state, String::emptyString);
    })));
    m_error->markThisObjectDontNeedStructureTransitionTable(state);

    m_error->setPrototype(state, m_functionPrototype);
    // m_error->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);

    m_errorPrototype = new Object(state);
    m_error->setFunctionPrototype(state, m_errorPrototype);
    m_errorPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message, Object::ObjectPropertyDescriptorForDefineOwnProperty(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().name, Object::ObjectPropertyDescriptorForDefineOwnProperty(state.context()->staticStrings().Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    auto errorToStringFn = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinErrorToString, 0, nullptr, NativeFunctionInfo::Strict));
    m_errorPrototype->defineOwnPropertyThrowsException(state, state.context()->staticStrings().toString, Object::ObjectPropertyDescriptorForDefineOwnProperty(errorToStringFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    // m_##name##Error->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
#define DEFINE_ERROR(errorname, bname) \
    m_##errorname##Error = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().bname##Error, builtinErrorConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* { \
        return new bname##ErrorObject(state, String::emptyString); \
    }))); \
    m_##errorname##Error->setPrototype(state, m_functionPrototype); \
    m_##errorname##ErrorPrototype = new ErrorObject(state, String::emptyString); \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().constructor, Object::ObjectPropertyDescriptorForDefineOwnProperty(m_##errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent))); \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().message, Object::ObjectPropertyDescriptorForDefineOwnProperty(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent))); \
    m_##errorname##ErrorPrototype->defineOwnProperty(state, state.context()->staticStrings().name, Object::ObjectPropertyDescriptorForDefineOwnProperty(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent))); \
    m_##errorname##Error->setFunctionPrototype(state, m_##errorname##ErrorPrototype); \
    defineOwnProperty(state, PropertyName(state.context()->staticStrings().bname##Error), \
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    DEFINE_ERROR(reference, Reference);
    DEFINE_ERROR(type, Type);
    DEFINE_ERROR(syntax, Syntax);
    DEFINE_ERROR(range, Range);
    DEFINE_ERROR(uri, URI);
    DEFINE_ERROR(eval, Eval);

    defineOwnProperty(state, PropertyName(state.context()->staticStrings().Error),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

}


}
