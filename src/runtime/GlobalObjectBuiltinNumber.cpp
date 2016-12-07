#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "NumberObject.h"

namespace Escargot {

static Value builtinNumberConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // TODO
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installNumber(ExecutionState& state)
{
    m_number = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Number, builtinNumberConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
        return new NumberObject(state);
    })));
    m_number->markThisObjectDontNeedStructureTransitionTable(state);
    m_number->setPrototype(state, m_functionPrototype);
    // TODO m_number->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_numberPrototype = m_objectPrototype;
    m_numberPrototype = new NumberObject(state, 0);
    m_numberPrototype->setPrototype(state, m_objectPrototype);

    m_number->setFunctionPrototype(state, m_numberPrototype);

    defineOwnProperty(state, PropertyName(state.context()->staticStrings().Number),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}

}
