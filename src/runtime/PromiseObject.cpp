/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "PromiseObject.h"
#include "Context.h"
#include "JobQueue.h"
#include "NativeFunctionObject.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

PromiseObject::PromiseObject(ExecutionState& state)
    : Object(state)
{
    Object::setPrototype(state, state.context()->globalObject()->promisePrototype());
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

    Object* resolveFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, promiseResolveFunction, 1, NativeFunctionInfo::Strict));
    Object* rejectFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, promiseRejectFunction, 1, NativeFunctionInfo::Strict));

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

    if (!constructor->isConstructor())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Constructor is not a function object")));

    // FIXME: Let executor be a new built-in function object as defined in GetCapabilitiesExecutor Functions (25.4.1.5.1).
    Object* executor = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, getCapabilitiesExecutorFunction, 2, NativeFunctionInfo::Strict));
    Object* internalSlot = executor->ensureInternalSlot(state);

    Value arguments[] = { executor };
    Value promise = Object::construct(state, constructor, 1, arguments);
    ASSERT(internalSlot == executor->internalSlot());

    Value resolveFunction = internalSlot->get(state, strings->resolve).value(state, internalSlot);
    Value rejectFunction = internalSlot->get(state, strings->reject).value(state, internalSlot);

    if (!resolveFunction.isCallable() || !rejectFunction.isCallable())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Promise resolve or reject function is not callable")));

    return PromiseReaction::Capability(promise, resolveFunction.asObject(), rejectFunction.asObject());
}

Object* PromiseObject::resolvingFunctionAlreadyResolved(ExecutionState& state, Object* callee)
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
