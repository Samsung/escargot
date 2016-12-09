#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ArrayObject.h"

namespace Escargot {

static Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    bool interpretArgumentsAsElements = false;
    size_t size = 0;
    if (argc > 1) {
        size = argc;
        interpretArgumentsAsElements = true;
    } else if (argc == 1) {
        Value& val = argv[0];
        if (val.isNumber()) {
            if (val.equalsTo(state, Value(val.toUint32(state)))) {
                size = val.toNumber(state);
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
            }
        } else {
            size = 1;
            interpretArgumentsAsElements = true;
        }
    }
    ArrayObject* array;
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isArrayObject()) {
        array = thisValue.asPointerValue()->asObject()->asArrayObject();
    } else {
        array = new ArrayObject(state);
    }

    array->setLength(state, size);
    if (interpretArgumentsAsElements) {
        Value val = argv[0];
        if (argc > 1 || !val.isInt32()) {
            for (size_t idx = 0; idx < argc; idx++) {
                array->defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), Object::ObjectPropertyDescriptorForDefineOwnProperty(val));
                val = argv[idx + 1];
            }
        }
    }
    return array;
}


static Value builtinArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, Array, join);
    uint32_t len = thisBinded->length(state);
    Value separator = argv[0];
    // TODO
    // size_t lenMax = ESString::maxLength();
    size_t lenMax = std::numeric_limits<uint32_t>::max();
    String* sep;

    if (separator.isUndefined()) {
        sep = state.context()->staticStrings().asciiTable[(size_t)','].string();
    } else {
        sep = separator.toString(state);
    }
    // TODO
    /*
    StringRecursionChecker checker(thisBinded);
    if (checker.recursionCheck()) {
        return ESValue(strings->emptyString.string());
    }
*/
    StringBuilder builder;
    double prevIndex = 0;
    double curIndex = 0;
    while (curIndex < len) {
        if (curIndex != 0) {
            if (sep->length() > 0) {
                if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - (curIndex - prevIndex - 1) * sep->length())) {
                    RELEASE_ASSERT_NOT_REACHED();
                    // TODO
                    // instance->throwOOMError();
                }
                while (curIndex - prevIndex > 1) {
                    builder.appendString(sep);
                    prevIndex++;
                }
                builder.appendString(sep);
            }
        }
        Value elem = thisBinded->get(state, ObjectPropertyName(state, Value(curIndex))).value();

        if (!elem.isUndefinedOrNull()) {
            builder.appendString(elem.toString(state));
        }
        prevIndex = curIndex;
        if (elem.isUndefined()) {
            curIndex = Object::nextIndexForward(state, thisBinded, prevIndex, len, true);
        } else {
            curIndex++;
        }
    }
    if (sep->length() > 0) {
        if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - (curIndex - prevIndex - 1) * sep->length())) {
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
            // instance->throwOOMError();
        }
        while (curIndex - prevIndex > 1) {
            builder.appendString(sep);
            prevIndex++;
        }
    }
    return builder.finalize();
}

static Value builtinArrayToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, toString);
    Value toString = thisObject->get(state, state.context()->staticStrings().join).value();
    if (!toString.isFunction()) {
        toString = state.context()->globalObject()->objectPrototypeToString();
    }
    return FunctionObject::call(toString, state, thisObject, 0, nullptr);
}

void GlobalObject::installArray(ExecutionState& state)
{
    m_array = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                     return new ArrayObject(state);
                                 }));
    m_array->markThisObjectDontNeedStructureTransitionTable(state);
    m_array->setPrototype(state, m_functionPrototype);
    // TODO m_array->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_arrayPrototype = m_objectPrototype;
    m_arrayPrototype = new ArrayObject(state);
    m_arrayPrototype->setPrototype(state, m_objectPrototype);

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().join),
                                                       Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().join, builtinArrayJoin, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                       Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinArrayToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
