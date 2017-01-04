#if ESCARGOT_ENABLE_PROMISE

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "PromiseObject.h"

namespace Escargot {

static Value builtinPromiseConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return new PromiseObject(state);
}

static Value builtinPromiseToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value(state.context()->staticStrings().Promise.string());
}

void GlobalObject::installPromise(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_promise = new FunctionObject(state, NativeFunctionInfo(strings->Promise, builtinPromiseConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                       return new PromiseObject(state);
                                   }),
                                   FunctionObject::__ForBuiltin__);
    m_promise->markThisObjectDontNeedStructureTransitionTable(state);
    m_promise->setPrototype(state, m_functionPrototype);
    m_promisePrototype = m_objectPrototype;
    m_promisePrototype = new PromiseObject(state);
    m_promisePrototype->setPrototype(state, m_objectPrototype);
    m_promisePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.3.3.2 Promise.prototype.toString
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinPromiseToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_promise->setFunctionPrototype(state, m_promisePrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Promise),
                      ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}

#endif // ESCARGOT_ENABLE_PROMISE
