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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/ErrorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ToStringRecursionPreventer.h"

namespace Escargot {

static void installErrorCause(ExecutionState& state, Object* obj, const Value& options)
{
    const AtomicString& causeString = state.context()->staticStrings().cause;
    // 1. If Type(options) is Object and ? HasProperty(options, "cause") is true, then
    if (options.isObject()) {
        auto res = options.asObject()->hasProperty(state, ObjectPropertyName(causeString));
        if (res) {
            // Let cause be ? Get(options, "cause").
            Value cause = res.value(state, ObjectPropertyName(causeString), options);
            // Perform ! CreateNonEnumerableDataPropertyOrThrow(O, "cause", cause).
            obj->defineOwnPropertyThrowsException(state, causeString,
                                                  ObjectPropertyDescriptor(cause, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
        }
    }
}

static Value builtinErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->errorPrototype();
    });

#if defined(ENABLE_EXTENDED_API)
    ErrorObject* obj = new ErrorObject(state, proto, String::emptyString, true, state.context()->vmInstance()->isErrorCreationCallbackRegistered());
#else
    ErrorObject* obj = new ErrorObject(state, proto, String::emptyString);
#endif

    Value message = argv[0];
    if (!message.isUndefined()) {
        obj->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,
                                              ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    }

    Value options = argc > 1 ? argv[1] : Value();
    installErrorCause(state, obj, options);

    return obj;
}

#if defined(ENABLE_EXTENDED_API)
#define DEFINE_ERROR_CTOR(errorName, lowerCaseErrorName)                                                                                                                                                                                                \
    static Value builtin##errorName##ErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)                                                                                                    \
    {                                                                                                                                                                                                                                                   \
        if (!newTarget.hasValue()) {                                                                                                                                                                                                                    \
            newTarget = state.resolveCallee();                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                               \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {                                                                                                 \
            return constructorRealm->globalObject()->lowerCaseErrorName##ErrorPrototype();                                                                                                                                                              \
        });                                                                                                                                                                                                                                             \
        ErrorObject* obj = new errorName##ErrorObject(state, proto, String::emptyString, true, state.context()->vmInstance()->isErrorCreationCallbackRegistered());                                                                                     \
        Value message = argv[0];                                                                                                                                                                                                                        \
        if (!message.isUndefined()) {                                                                                                                                                                                                                   \
            obj->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,                                                                                                                                                      \
                                                  ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
        }                                                                                                                                                                                                                                               \
        Value options = argc > 1 ? argv[1] : Value();                                                                                                                                                                                                   \
        installErrorCause(state, obj, options);                                                                                                                                                                                                         \
        return obj;                                                                                                                                                                                                                                     \
    }
#else
#define DEFINE_ERROR_CTOR(errorName, lowerCaseErrorName)                                                                                                                                                                                                \
    static Value builtin##errorName##ErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)                                                                                                    \
    {                                                                                                                                                                                                                                                   \
        if (!newTarget.hasValue()) {                                                                                                                                                                                                                    \
            newTarget = state.resolveCallee();                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                               \
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {                                                                                                 \
            return constructorRealm->globalObject()->lowerCaseErrorName##ErrorPrototype();                                                                                                                                                              \
        });                                                                                                                                                                                                                                             \
        ErrorObject* obj = new errorName##ErrorObject(state, proto, String::emptyString);                                                                                                                                                               \
        Value message = argv[0];                                                                                                                                                                                                                        \
        if (!message.isUndefined()) {                                                                                                                                                                                                                   \
            obj->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,                                                                                                                                                      \
                                                  ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
        }                                                                                                                                                                                                                                               \
        Value options = argc > 1 ? argv[1] : Value();                                                                                                                                                                                                   \
        installErrorCause(state, obj, options);                                                                                                                                                                                                         \
        return obj;                                                                                                                                                                                                                                     \
    }
#endif

DEFINE_ERROR_CTOR(Reference, reference);
DEFINE_ERROR_CTOR(Type, type);
DEFINE_ERROR_CTOR(Syntax, syntax);
DEFINE_ERROR_CTOR(Range, range);
DEFINE_ERROR_CTOR(URI, uri);
DEFINE_ERROR_CTOR(Eval, eval);

static Value builtinAggregateErrorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, let newTarget be the active function object; else let newTarget be NewTarget.
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    // Let O be ? OrdinaryCreateFromConstructor(newTarget, "%AggregateError.prototype%", « [[ErrorData]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->aggregateErrorPrototype();
    });

#if defined(ENABLE_EXTENDED_API)
    ErrorObject* O = new AggregateErrorObject(state, proto, String::emptyString, true, state.context()->vmInstance()->isErrorCreationCallbackRegistered());
#else
    ErrorObject* O = new AggregateErrorObject(state, proto, String::emptyString);
#endif

    Value message = argv[1];
    // If message is not undefined, then
    if (!message.isUndefined()) {
        // Let msg be ? ToString(message).
        // Let msgDesc be the PropertyDescriptor { [[Value]]: msg, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true }.
        // Perform ! DefinePropertyOrThrow(O, "message", msgDesc).
        O->defineOwnPropertyThrowsException(state, state.context()->staticStrings().message,
                                            ObjectPropertyDescriptor(message.toString(state), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    }

    // Perform ? InstallErrorCause(O, options).
    Value options = argc > 2 ? argv[2] : Value();
    installErrorCause(state, O, options);

    // Let errorsList be ? IterableToList(errors).
    auto errorsList = IteratorObject::iterableToList(state, argv[0]);
    // Perform ! DefinePropertyOrThrow(O, "errors", PropertyDescriptor { [[Configurable]]: true, [[Enumerable]]: false, [[Writable]]: true, [[Value]]: ! CreateArrayFromList(errorsList) }).
    O->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("errors")),
                                        ObjectPropertyDescriptor(Value(Object::createArrayFromList(state, errorsList.size(), errorsList.data())), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    // Return O.
    return O;
}

static Value builtinErrorThrowTypeError(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "");
    return Value();
}

static Value builtinErrorToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject())
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Error.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);

    Object* o = thisValue.toObject(state);

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(o)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, o);

    Value name = o->get(state, state.context()->staticStrings().name).value(state, o);
    String* nameStr;
    if (name.isUndefined()) {
        nameStr = state.context()->staticStrings().Error.string();
    } else {
        nameStr = name.toString(state);
    }
    Value message = o->get(state, state.context()->staticStrings().message).value(state, o);
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
    return builder.finalize(&state);
}

void GlobalObject::initializeError(ExecutionState& state)
{
#define DEFINE_ERROR_INIT(errorname, bname)                                                                                                                                                                                         \
    {                                                                                                                                                                                                                               \
        ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,                                                                                                              \
                                                                                                    [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value { \
                                                                                                        ASSERT(self->isGlobalObject());                                                                                             \
                                                                                                        return self->asGlobalObject()->errorname##Error();                                                                          \
                                                                                                    },                                                                                                                              \
                                                                                                    nullptr);                                                                                                                       \
        defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().bname##Error), nativeData, Value(Value::EmptyValue));                                                                           \
    }

    DEFINE_ERROR_INIT(reference, Reference);
    DEFINE_ERROR_INIT(type, Type);
    DEFINE_ERROR_INIT(syntax, Syntax);
    DEFINE_ERROR_INIT(range, Range);
    DEFINE_ERROR_INIT(uri, URI);
    DEFINE_ERROR_INIT(eval, Eval);
    DEFINE_ERROR_INIT(aggregate, Aggregate);

    {
        ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                    [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                        ASSERT(self->isGlobalObject());
                                                                                                        return self->asGlobalObject()->error();
                                                                                                    },
                                                                                                    nullptr);
        defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Error), nativeData, Value(Value::EmptyValue));
    }
}

void GlobalObject::installError(ExecutionState& state)
{
    m_error = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Error, builtinErrorConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_error->setGlobalIntrinsicObject(state);

    m_errorPrototype = new PrototypeObject(state);
    m_errorPrototype->setGlobalIntrinsicObject(state, true);

    m_error->setFunctionPrototype(state, m_errorPrototype);

    m_errorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_errorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().message, ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_errorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(state.context()->staticStrings().Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    auto errorToStringFn = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinErrorToString, 0, NativeFunctionInfo::Strict));
    m_errorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().toString, ObjectPropertyDescriptor(errorToStringFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // http://www.ecma-international.org/ecma-262/5.1/#sec-13.2.3
    // 13.2.3 The [[ThrowTypeError]] Function Object
    m_throwTypeError = new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(), builtinErrorThrowTypeError, 0, NativeFunctionInfo::Strict));
    m_throwTypeError->setGlobalIntrinsicObject(state, false);
    m_throwTypeError->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().length),
                                        ObjectPropertyDescriptor(Value(0), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    m_throwTypeError->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().name),
                                        ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    m_throwTypeError->preventExtensions(state);

    m_throwerGetterSetterData = new JSGetterSetter(m_throwTypeError, m_throwTypeError);

#define DEFINE_ERROR(errorname, bname, length)                                                                                                                                                                                                                                                                                                \
    m_##errorname##Error = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().bname##Error, builtin##bname##ErrorConstructor, length), NativeFunctionObject::__ForBuiltinConstructor__);                                                                                                                     \
    m_##errorname##Error->setPrototype(state, m_error);                                                                                                                                                                                                                                                                                       \
    m_##errorname##ErrorPrototype = new PrototypeObject(state, m_errorPrototype);                                                                                                                                                                                                                                                             \
    m_##errorname##ErrorPrototype->setGlobalIntrinsicObject(state, true);                                                                                                                                                                                                                                                                     \
    m_##errorname##ErrorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().constructor, ObjectPropertyDescriptor(m_##errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                            \
    m_##errorname##ErrorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().message, ObjectPropertyDescriptor(String::emptyString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));                                 \
    m_##errorname##ErrorPrototype->directDefineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(state.context()->staticStrings().bname##Error.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent))); \
    m_##errorname##Error->setFunctionPrototype(state, m_##errorname##ErrorPrototype);                                                                                                                                                                                                                                                         \
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().bname##Error),                                                                                                                                                                                                                                             \
                        ObjectPropertyDescriptor(m_##errorname##Error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    DEFINE_ERROR(reference, Reference, 1);
    DEFINE_ERROR(type, Type, 1);
    DEFINE_ERROR(syntax, Syntax, 1);
    DEFINE_ERROR(range, Range, 1);
    DEFINE_ERROR(uri, URI, 1);
    DEFINE_ERROR(eval, Eval, 1);
    DEFINE_ERROR(aggregate, Aggregate, 2);

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Error),
                        ObjectPropertyDescriptor(m_error, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
