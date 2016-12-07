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

void GlobalObject::installFunction(ExecutionState& state)
{
    FunctionObject* emptyFunction = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionEmptyFunction, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    })), FunctionObject::__ForBuiltin__);
    m_functionPrototype = emptyFunction;
    m_functionPrototype->setPrototype(state, m_objectPrototype);
    // TODO convert into defineOwnProperty
    // m_functionPrototype->setFunctionPrototype(state, emptyFunction);

    m_function = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Function, builtinFunctionConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    })));
    m_function->markThisObjectDontNeedStructureTransitionTable(state);
    // TODO m_function->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);

    m_function->setPrototype(state, emptyFunction);
    m_function->setFunctionPrototype(state, emptyFunction);
    m_functionPrototype->defineOwnProperty(state, PropertyName(state.context()->staticStrings().constructor),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    defineOwnProperty(state, PropertyName(state.context()->staticStrings().Function),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_function, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}


}
