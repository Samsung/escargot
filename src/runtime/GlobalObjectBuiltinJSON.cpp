#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"

namespace Escargot {

static Value builtinJSONParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinJSONStringify(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installJSON(ExecutionState& state)
{
    m_json = new Object(state);
    m_json->giveInternalClassProperty("JSON");
    m_json->markThisObjectDontNeedStructureTransitionTable(state);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().JSON),
                      ObjectPropertyDescriptor(m_json, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_json->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().parse),
                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().parse, builtinJSONParse, 1, nullptr, NativeFunctionInfo::Strict)),
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_json->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringify),
                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringify, builtinJSONStringify, 1, nullptr, NativeFunctionInfo::Strict)),
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
