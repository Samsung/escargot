#if ESCARGOT_ENABLE_PROMISE
#include "Escargot.h"
#include "PromiseObject.h"
#include "Context.h"
#include "JobQueue.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

PromiseObject::PromiseObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->promisePrototype());
}

PromiseReaction::Capability PromiseObject::createResolvingFunctions(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    FunctionObject* resolveFunction = new FunctionObject(state, NativeFunctionInfo(strings->Empty, promiseResolveFunction, 1, nullptr, NativeFunctionInfo::Strict));
    FunctionObject* rejectFunction = new FunctionObject(state, NativeFunctionInfo(strings->Empty, promiseRejectFunction, 1, nullptr, NativeFunctionInfo::Strict));

    resolveFunction->deleteOwnProperty(state, strings->name);
    rejectFunction->deleteOwnProperty(state, strings->name);

    Object* resolveFunctionInternalSlot = new Object(state);
    Object* rejectFunctionInternalSlot = new Object(state);

    Object* alreadyResolved = new Object(state);
    alreadyResolved->defineOwnProperty(state, ObjectPropertyName(strings->value), ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::AllPresent));

    ObjectPropertyDescriptor::PresentAttribute attrPromise = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent);
    ObjectPropertyDescriptor::PresentAttribute attrAlreadyResolved = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent);
    resolveFunctionInternalSlot->defineOwnProperty(state, strings->Promise, ObjectPropertyDescriptor(Value(this), attrPromise));
    resolveFunctionInternalSlot->defineOwnProperty(state, strings->alreadyResolved, ObjectPropertyDescriptor(Value(alreadyResolved), attrAlreadyResolved));
    rejectFunctionInternalSlot->defineOwnProperty(state, strings->Promise, ObjectPropertyDescriptor(Value(this), attrPromise));
    rejectFunctionInternalSlot->defineOwnProperty(state, strings->alreadyResolved, ObjectPropertyDescriptor(Value(alreadyResolved), attrAlreadyResolved));

    resolveFunction->setInternalSlot(resolveFunctionInternalSlot);
    rejectFunction->setInternalSlot(rejectFunctionInternalSlot);

    return PromiseReaction::Capability(this, resolveFunction, rejectFunction);
}

PromiseReaction::Capability PromiseObject::newPromiseCapability(ExecutionState& state, Object* constructor)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!constructor->isFunctionObject())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Constructor is not a function object")));

    FunctionObject* executor = new FunctionObject(state, NativeFunctionInfo(strings->Empty, getCapabilitiesExecutorFunction, 2, nullptr, NativeFunctionInfo::Strict));
    Object* internalSlot = executor->ensureInternalSlot(state);

    Value arguments[] = { executor };
    Value promise = ByteCodeInterpreter::newOperation(state, constructor, 1, arguments);
    ASSERT(internalSlot == executor->internalSlot());

    Value resolveFunction = internalSlot->get(state, strings->resolve).value(state, internalSlot);
    Value rejectFunction = internalSlot->get(state, strings->reject).value(state, internalSlot);

    if (!resolveFunction.isFunction() || !rejectFunction.isFunction())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Promise resolve or reject function is not callable")));

    return PromiseReaction::Capability(promise, resolveFunction.asFunction(), rejectFunction.asFunction());
}

Object* PromiseObject::resolvingFunctionAlreadyResolved(ExecutionState& state, FunctionObject* callee)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    Object* internalSlot = callee->internalSlot();
    Value alreadyResolved = internalSlot->get(state, strings->alreadyResolved).value(state, internalSlot);
    return alreadyResolved.asObject();
}

void PromiseObject::fulfillPromise(ExecutionState& state, Value value)
{
    m_state = PromiseState::FulFilled;
    m_promiseResult = value;
    triggerPromiseReactions(state, m_fulfillReactions);
    m_fulfillReactions.clear();
    m_rejectReactions.clear();
}

void PromiseObject::rejectPromise(ExecutionState& state, Value reason)
{
    m_state = PromiseState::Rejected;
    m_promiseResult = reason;
    triggerPromiseReactions(state, m_rejectReactions);

    m_fulfillReactions.clear();
    m_rejectReactions.clear();
}

void PromiseObject::triggerPromiseReactions(ExecutionState& state, PromiseObject::Reactions& reactions)
{
    for (auto it : reactions)
        state.context()->jobQueue()->enqueueJob(new PromiseReactionJob(it, m_promiseResult));
}
}

#endif // ESCARGOT_ENABLE_PROMISE
