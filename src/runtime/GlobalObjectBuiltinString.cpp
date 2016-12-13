#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"
#include "ErrorObject.h"

namespace Escargot {

static Value builtinStringConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (isNewExpression) {
        Object* thisObject = thisValue.toObject(state);
        StringObject* stringObject = thisObject->asStringObject();
        if (argc == 0) {
            stringObject->setStringData(state, String::emptyString);
        } else {
            stringObject->setStringData(state, argv[0].toString(state));
        }
        return stringObject;
    } else {
        // called as function
        if (argc == 0)
            return String::emptyString;
        Value value = argv[0];
        return value.toString(state);
    }
}

static Value builtinStringToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isObject() && thisValue.asObject()->isStringObject()) {
        return thisValue.asObject()->asStringObject()->primitiveValue();
    }

    if (thisValue.isString())
        return thisValue.toString(state);

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().String.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotString);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinStringIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
    String* searchStr = argv[0].toString(state);

    Value val;
    if (argc > 1) {
        val = argv[1];
    }
    size_t pos;
    if (val.isUndefined()) {
        pos = 0;
    } else {
        pos = val.toInteger(state);
    }

    size_t len = str->length();
    size_t start = std::min(std::max(pos, (size_t)0), len);
    size_t result = str->find(searchStr, start);
    if (result == SIZE_MAX)
        return Value(-1);
    else
        return Value(result);
}

static Value builtinStringSubstring(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_STRING(str, String, indexOf);
    if (argc == 0) {
        return str;
    } else {
        int len = str->length();
        double doubleStart = argv[0].toNumber(state);
        Value end = argv[1];
        double doubleEnd = (argc < 2 || end.isUndefined()) ? len : end.toNumber(state);
        doubleStart = (std::isnan(doubleStart)) ? 0 : doubleStart;
        doubleEnd = (std::isnan(doubleEnd)) ? 0 : doubleEnd;

        double finalStart = (int)trunc(std::min(std::max(doubleStart, 0.0), (double)len));
        double finalEnd = (int)trunc(std::min(std::max(doubleEnd, 0.0), (double)len));
        size_t from = std::min(finalStart, finalEnd);
        size_t to = std::max(finalStart, finalEnd);
        ASSERT(from <= to);
        if (to - from == 1) {
            char16_t c = str->charAt(from);
            if (c < ESCARGOT_ASCII_TABLE_MAX) {
                return state.context()->staticStrings().asciiTable[c].string();
            }
        }
        return str->subString(from, to - from);
    }
}


void GlobalObject::installString(ExecutionState& state)
{
    m_string = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().String, builtinStringConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new StringObject(state);
                                  }));
    m_string->markThisObjectDontNeedStructureTransitionTable(state);
    m_string->setPrototype(state, m_functionPrototype);
    // TODO m_string->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_stringPrototype = m_objectPrototype;
    m_stringPrototype = new StringObject(state, String::emptyString);
    m_stringPrototype->setPrototype(state, m_objectPrototype);

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                        Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinStringToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().indexOf),
                                                        Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    m_stringPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().substring),
                                                        Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinStringSubstring, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));


    m_string->setFunctionPrototype(state, m_stringPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().String),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_string, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
