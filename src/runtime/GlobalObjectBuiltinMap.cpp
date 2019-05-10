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
#include "MapObject.h"
#include "IteratorOperations.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinMapConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ConstructorRequiresNew);
    }

    // Let map be ? OrdinaryCreateFromConstructor(NewTarget, "%MapPrototype%", « [[MapData]] »).
    // Set map's [[MapData]] internal slot to a new empty List.
    MapObject* map = thisValue.asObject()->asMapObject();
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
        // Let adder be ? Get(map, "set").
        adder = map->Object::get(state, ObjectPropertyName(state.context()->staticStrings().set)).value(state, map);
        // If IsCallable(adder) is false, throw a TypeError exception.
        if (!adder.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
        }
        // Let iter be ? GetIterator(iterable).
        iter = getIterator(state, iterable);
    }
    // If iter is undefined, return map.
    if (iter.isUndefined()) {
        return map;
    }

    // Repeat
    while (true) {
        // Let next be ? IteratorStep(iter).
        Value next = iteratorStep(state, iter);
        // If next is false(done is true), return map.
        if (next.isFalse()) {
            return map;
        }
        // Let nextItem be ? IteratorValue(next).
        Value nextItem = iteratorValue(state, next);

        // If Type(nextItem) is not Object, then
        if (!nextItem.isObject()) {
            // Let error be Completion{[[Type]]: throw, [[Value]]: a newly created TypeError object, [[Target]]: empty}.
            // Return ? IteratorClose(iter, error).
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Iterator value a is not an entry object");
        }

        // Let k be Get(nextItem, "0").
        // If k is an abrupt completion, return ? IteratorClose(iter, k).
        Value k = nextItem.asObject()->getIndexedProperty(state, Value(0)).value(state, nextItem);
        // Let v be Get(nextItem, "1").
        // If v is an abrupt completion, return ? IteratorClose(iter, v).
        Value v = nextItem.asObject()->getIndexedProperty(state, Value(1)).value(state, nextItem);

        // Let status be Call(adder, map, « k.[[Value]], v.[[Value]] »).
        Value argv[2] = { k, v };
        // TODO If status is an abrupt completion, return ? IteratorClose(iter, status).
        adder.asFunction()->call(state, map, 2, argv);
    }
    return map;
}

#define RESOLVE_THIS_BINDING_TO_MAP(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                \
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {                                                                                                                                                                       \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                          \
    MapObject* NAME = thisValue.asObject()->asMapObject();

static Value builtinMapClear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, clear);
    M->clear(state);
    return Value();
}

static Value builtinMapDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, stringDelete);
    return Value(M->deleteOperation(state, argv[0]));
}

static Value builtinMapGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, get);
    return M->get(state, argv[0]);
}

static Value builtinMapHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, has);
    return Value(M->has(state, argv[0]));
}

static Value builtinMapSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, set);
    M->set(state, argv[0], argv[1]);
    return M;
}

static Value builtinMapForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, forEach);
    // Let M be the this value.
    // If Type(M) is not Object, throw a TypeError exception.
    // If M does not have a [[MapData]] internal slot, throw a TypeError exception.

    Value callbackfn = argv[0];
    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Map.string(), true, state.context()->staticStrings().forEach.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc >= 2) {
        T = argv[1];
    }
    // Let entries be the List that is the value of M's [[MapData]] internal slot.
    const MapObject::MapObjectData& entries = M->storage();
    // Repeat for each Record {[[Key]], [[Value]]} e that is an element of entries, in original key insertion order
    for (size_t i = 0; i < entries.size(); i++) {
        // If e.[[Key]] is not empty, then
        if (!entries[i].first.isEmpty()) {
            // Perform ? Call(callbackfn, T, « e.[[Value]], e.[[Key]], M »).
            Value argv[3] = { Value(entries[i].second), Value(entries[i].first), Value(M) };
            callbackfn.asFunction()->call(state, T, 3, argv);
        }
    }

    return Value();
}

static Value builtinMapKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, keys);
    return M->keys(state);
}

static Value builtinMapValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, values);
    return M->values(state);
}

static Value builtinMapEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, entries);
    return M->entries(state);
}

static Value builtinMapSizeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, size);
    return Value(M->size(state));
}

static Value builtinMapIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorObject() || !thisValue.asObject()->asIteratorObject()->isMapIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().MapIterator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }
    MapIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asMapIteratorObject();
    return iter->next(state);
}

void GlobalObject::installMap(ExecutionState& state)
{
    m_map = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Map, builtinMapConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                   return new MapObject(state);
                               }),
                               FunctionObject::__ForBuiltin__);
    m_map->markThisObjectDontNeedStructureTransitionTable(state);
    m_map->setPrototype(state, m_functionPrototype);

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, nullptr, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_map->defineOwnProperty(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_mapPrototype = m_objectPrototype;
    m_mapPrototype = new MapObject(state);
    m_mapPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_mapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_map, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().clear),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clear, builtinMapClear, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinMapDelete, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().get),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinMapGet, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinMapHas, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().set),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinMapSet, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinMapForEach, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().keys),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().keys, builtinMapKeys, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().values),
                                                     ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinMapValues, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto entFn = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinMapEntries, 0, nullptr, NativeFunctionInfo::Strict));
    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                     ObjectPropertyDescriptor(entFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                     ObjectPropertyDescriptor(entFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                     ObjectPropertyDescriptor(Value(state.context()->staticStrings().Map.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    JSGetterSetter gs(
        new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSize, builtinMapSizeGetter, 0, nullptr, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_mapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().size), desc);

    m_mapIteratorPrototype = m_iteratorPrototype;
    m_mapIteratorPrototype = new MapIteratorObject(state, nullptr, MapIteratorObject::TypeKey);

    m_mapIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                             ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinMapIteratorNext, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                             ObjectPropertyDescriptor(Value(String::fromASCII("Map Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    m_map->setFunctionPrototype(state, m_mapPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Map),
                      ObjectPropertyDescriptor(m_map, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
