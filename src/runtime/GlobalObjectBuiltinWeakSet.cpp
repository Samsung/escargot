/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "WeakSetObject.h"
#include "IteratorObject.h"
#include "IteratorOperations.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinWeakSetConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ConstructorRequiresNew);
    }

    // Let set be ? OrdinaryCreateFromConstructor(NewTarget, "%WeakSetPrototype%", « [[WeakSetData]] »).
    // Set set's [[WeakSetData]] internal slot to a new empty List.
    WeakSetObject* set = thisValue.asObject()->asWeakSetObject();

    // If iterable is not present, let iterable be undefined.
    Value iterable;
    if (argc >= 1) {
        iterable = argv[0];
    }
    // If iterable is either undefined or null, let iter be undefined.
    Value adder;
    Value iter;
    if (!iterable.isUndefinedOrNull()) {
        // Else,
        // Let adder be ? Get(set, "add").
        adder = set->Object::get(state, ObjectPropertyName(state.context()->staticStrings().add)).value(state, set);
        // If IsCallable(adder) is false, throw a TypeError exception.
        if (!adder.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
        }
        // Let iter be ? GetIterator(iterable).
        iter = getIterator(state, iterable);
    }
    // If iter is undefined, return Set.
    if (iter.isUndefined()) {
        return set;
    }

    // Repeat
    while (true) {
        // Let next be ? IteratorStep(iter).
        Value next = iteratorStep(state, iter);
        // If next is false, return set.
        if (next.isFalse()) {
            return set;
        }
        // Let nextValue be ? IteratorValue(next).
        Value nextValue = iteratorValue(state, next);

        // Let status be Call(adder, set, « nextValue.[[Value]] »).
        // TODO If status is an abrupt completion, return ? IteratorClose(iter, status).
        Value argv[1] = { nextValue };
        adder.asFunction()->call(state, set, 1, argv);
    }
    return set;
}

#define RESOLVE_THIS_BINDING_TO_SET(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                \
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakSetObject()) {                                                                                                                                                                   \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                          \
    WeakSetObject* NAME = thisValue.asObject()->asWeakSetObject();

static Value builtinWeakSetAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, WeakSet, add);
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid value used as weak set key");
    }

    S->add(state, argv[0].asObject());
    return S;
}

static Value builtinWeakSetDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, WeakSet, stringDelete);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(S->deleteOperation(state, argv[0].asObject()));
}

static Value builtinWeakSetHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, WeakSet, has);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(S->has(state, argv[0].asObject()));
}

void GlobalObject::installWeakSet(ExecutionState& state)
{
    m_weakSet = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakSet, builtinWeakSetConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                       return new WeakSetObject(state);
                                   }),
                                   FunctionObject::__ForBuiltin__);
    m_weakSet->markThisObjectDontNeedStructureTransitionTable(state);
    m_weakSet->setPrototype(state, m_functionPrototype);
    m_weakSetPrototype = m_objectPrototype;
    m_weakSetPrototype = new WeakSetObject(state);
    m_weakSetPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_weakSetPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakSet, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinWeakSetDelete, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinWeakSetHas, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().add),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinWeakSetAdd, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakSet.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSet->setFunctionPrototype(state, m_weakSetPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakSet),
                      ObjectPropertyDescriptor(m_weakSet, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
