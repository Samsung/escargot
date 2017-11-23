/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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

void* PromiseObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(PromiseObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_promiseResult));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_fulfillReactions));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_rejectReactions));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(PromiseObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
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
    for (size_t i = 0; i < reactions.size(); i++)
        state.context()->jobQueue()->enqueueJob(state, new PromiseReactionJob(state.context(), reactions[i], m_promiseResult));
}
}

#endif // ESCARGOT_ENABLE_PROMISE
