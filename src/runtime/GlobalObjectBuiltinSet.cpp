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
#include "SetObject.h"
#include "IteratorOperations.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinSetConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ConstructorRequiresNew);
    }

    // Let set be ? OrdinaryCreateFromConstructor(NewTarget, "%SetPrototype%", « [[SetData]] »).
    // Set set's [[SetData]] internal slot to a new empty List.
    SetObject* set = thisValue.asObject()->asSetObject();

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
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {                                                                                                                                                                       \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                          \
    SetObject* NAME = thisValue.asObject()->asSetObject();

static Value builtinSetAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, add);
    S->add(state, argv[0]);
    return S;
}

static Value builtinSetClear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, clear);
    S->clear(state);
    return Value();
}

static Value builtinSetDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, stringDelete);
    return Value(S->deleteOperation(state, argv[0]));
}

static Value builtinSetHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, has);
    return Value(S->has(state, argv[0]));
}

static Value builtinSetForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let S be the this value.
    // If Type(S) is not Object, throw a TypeError exception.
    // If S does not have a [[SetData]] internal slot, throw a TypeError exception.
    RESOLVE_THIS_BINDING_TO_SET(S, Set, forEach);

    Value callbackfn = argv[0];
    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Set.string(), true, state.context()->staticStrings().forEach.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc >= 2) {
        T = argv[1];
    }
    // Let entries be the List that is the value of S's [[SetData]] internal slot.
    const SetObject::SetObjectData& entries = S->storage();
    // Repeat for each e that is an element of entries, in original insertion order
    for (size_t i = 0; i < entries.size(); i++) {
        Value e = entries[i];
        // If e is not empty, then
        if (!e.isEmpty()) {
            // If e.[[Key]] is not empty, then
            // Perform ? Call(callbackfn, T, « e, e, S »).
            Value argv[3] = { Value(e), Value(e), Value(S) };
            callbackfn.asFunction()->call(state, T, 3, argv);
        }
    }

    return Value();
}

static Value builtinSetValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, values);
    return S->values(state);
}

static Value builtinSetEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, entries);
    return S->entries(state);
}

static Value builtinSetSizeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, size);
    return Value(S->size(state));
}

static Value builtinSetIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorObject() || !thisValue.asObject()->asIteratorObject()->isSetIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().SetIterator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }
    SetIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asSetIteratorObject();
    return iter->next(state);
}

void GlobalObject::installSet(ExecutionState& state)
{
    m_set = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Set, builtinSetConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                   return new SetObject(state);
                               }),
                               FunctionObject::__ForBuiltin__);
    m_set->markThisObjectDontNeedStructureTransitionTable(state);
    m_set->setPrototype(state, m_functionPrototype);

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, nullptr, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_set->defineOwnProperty(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_setPrototype = m_objectPrototype;
    m_setPrototype = new SetObject(state);
    m_setPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_setPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_set, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().clear),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clear, builtinSetClear, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinSetDelete, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinSetHas, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().add),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinSetAdd, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinSetForEach, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto valuesFn = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinSetValues, 0, nullptr, NativeFunctionInfo::Strict));
    auto values = ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().values), values);
    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().keys), values);

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinSetEntries, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                     ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                     ObjectPropertyDescriptor(Value(state.context()->staticStrings().Set.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    JSGetterSetter gs(
        new FunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("get size")), builtinSetSizeGetter, 0, nullptr, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_setPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().size), desc);

    m_setIteratorPrototype = m_iteratorPrototype;
    m_setIteratorPrototype = new SetIteratorObject(state, nullptr, SetIteratorObject::TypeKey);

    m_setIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                             ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinSetIteratorNext, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                             ObjectPropertyDescriptor(Value(String::fromASCII("Set Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_set->setFunctionPrototype(state, m_setPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Set),
                      ObjectPropertyDescriptor(m_set, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
