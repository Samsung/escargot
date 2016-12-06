#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "BooleanObject.h"

namespace Escargot {

static Value builtinBooleanConstructor(ExecutionState& state, Value thisValue, Value* argv, bool isNewExpression)
{
    // TODO
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installBoolean(ExecutionState& state)
{
    m_boolean = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Boolean, builtinBooleanConstructor, 1)));
    m_boolean->markThisObjectDontNeedStructureTransitionTable(state);
    m_boolean->setPrototype(state, m_functionPrototype);
    // TODO m_boolean->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_booleanPrototype = m_objectPrototype;
    m_booleanPrototype = new BooleanObject(state, false);
    m_booleanPrototype->setPrototype(state, m_objectPrototype);

    m_boolean->setFunctionPrototype(state, m_booleanPrototype);

    defineOwnProperty(state, PropertyName(state.context()->staticStrings().Boolean),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_boolean, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}

}
