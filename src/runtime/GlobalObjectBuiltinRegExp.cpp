#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "RegExpObject.h"

namespace Escargot {

static Value builtinRegExpConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    bool patternIsRegExp = argc >= 1 && argv[0].isObject() && argv[0].asObject()->isRegExpObject();
    RegExpObject* regexp;
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isRegExpObject()) {
        regexp = thisValue.asPointerValue()->asObject()->asRegExpObject();
    } else {
        if (patternIsRegExp) {
            if (argc == 1 || (argc >= 2 && argv[1].isUndefined()))
                return argv[0];
#ifndef USE_ES6_MODE
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot supply flags when constructing one RegExp from another");
#endif
        }
        regexp = new RegExpObject(state);
    }
    // TODO(ES6): consider the case that patternIsRegExp is true
    const StaticStrings* strings = &state.context()->staticStrings();
    String* patternStr = (argc < 1 || argv[0].isUndefined()) ? strings->defaultRegExpString.string() : argv[0].toString(state);
    if (patternStr->length() == 0)
        patternStr = strings->defaultRegExpString.string();

    String* optionStr = (argc < 2 || argv[1].isUndefined()) ? String::emptyString : argv[1].toString(state);
    regexp->setSource(state, patternStr);
    regexp->setOption(state, optionStr);
    return regexp;
}

static Value builtinRegExpTest(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // TODO
    return Value();
}

void GlobalObject::installRegExp(ExecutionState& state)
{
    m_regexp = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().RegExp, builtinRegExpConstructor, 2, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new RegExpObject(state, String::emptyString, RegExpObject::Option::None);
                                  }));
    m_regexp->markThisObjectDontNeedStructureTransitionTable(state);
    m_regexp->setPrototype(state, m_functionPrototype);
    // TODO m_regexp->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);

    m_regexpPrototype = m_objectPrototype;
    m_regexpPrototype = new Object(state);
    m_regexpPrototype->setPrototype(state, m_objectPrototype);

    m_regexp->setFunctionPrototype(state, m_regexpPrototype);

    const StaticStrings* strings = &state.context()->staticStrings();
    // $21.2.5.2 RegExp.prototype.exec
    // $21.2.5.13 RegExp.prototype.test
    m_regexp->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->test),
                                               Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(strings->test, builtinRegExpTest, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    // $21.2.5.14 RegExp.prototype.toString
    // $B.2.5.1 RegExp.prototype.compile

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().RegExp),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_regexp, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
