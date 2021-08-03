/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/MapObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ToStringRecursionPreventer.h"

namespace Escargot {

static Value builtinMapConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    // Let map be ? OrdinaryCreateFromConstructor(NewTarget, "%MapPrototype%", « [[MapData]] »).
    // Set map's [[MapData]] internal slot to a new empty List.
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->mapPrototype();
    });
    MapObject* map = new MapObject(state, proto);

    // If iterable is not present, or is either undefined or null, return map.
    if (argc == 0 || argv[0].isUndefinedOrNull()) {
        return map;
    }

    Value iterable = argv[0];

    // Let adder be ? Get(map, "set").
    Value adder = map->Object::get(state, ObjectPropertyName(state.context()->staticStrings().set)).value(state, map);
    // If IsCallable(adder) is false, throw a TypeError exception.
    if (!adder.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
    }

    // Let iteratorRecord be ? GetIterator(iterable).
    auto iteratorRecord = IteratorObject::getIterator(state, iterable);
    while (true) {
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            return map;
        }

        Value nextItem = IteratorObject::iteratorValue(state, next.value());
        if (!nextItem.isObject()) {
            ErrorObject* errorobj = ErrorObject::createError(state, ErrorObject::TypeError, new ASCIIString("TypeError"));
            return IteratorObject::iteratorClose(state, iteratorRecord, errorobj, true);
        }

        try {
            // Let k be Get(nextItem, "0").
            // If k is an abrupt completion, return ? IteratorClose(iter, k).
            Value k = nextItem.asObject()->getIndexedProperty(state, Value(0)).value(state, nextItem);
            // Let v be Get(nextItem, "1").
            // If v is an abrupt completion, return ? IteratorClose(iter, v).
            Value v = nextItem.asObject()->getIndexedProperty(state, Value(1)).value(state, nextItem);

            // Let status be Call(adder, map, « k.[[Value]], v.[[Value]] »).
            Value argv[2] = { k, v };
            Object::call(state, adder, map, 2, argv);
        } catch (const Value& v) {
            // we should save thrown value bdwgc cannot track thrown value
            Value exceptionValue = v;
            // If status is an abrupt completion, return ? IteratorClose(iteratorRecord, status).
            return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
        }
    }

    return map;
}

#define RESOLVE_THIS_BINDING_TO_MAP(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                          \
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {                                                                                                                                                                                 \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                    \
    MapObject* NAME = thisValue.asObject()->asMapObject();

static Value builtinMapClear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, clear);
    M->clear(state);
    return Value();
}

static Value builtinMapDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, stringDelete);
    return Value(M->deleteOperation(state, argv[0]));
}

static Value builtinMapGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, get);
    return M->get(state, argv[0]);
}

static Value builtinMapHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, has);
    return Value(M->has(state, argv[0]));
}

static Value builtinMapSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, set);
    M->set(state, argv[0], argv[1]);
    return M;
}

static Value builtinMapForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, forEach);
    // Let M be the this value.
    // If Type(M) is not Object, throw a TypeError exception.
    // If M does not have a [[MapData]] internal slot, throw a TypeError exception.

    Value callbackfn = argv[0];
    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Map.string(), true, state.context()->staticStrings().forEach.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
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
            Object::call(state, callbackfn, T, 3, argv);
        }
    }

    return Value();
}

static Value builtinMapKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, keys);
    return M->keys(state);
}

static Value builtinMapValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, values);
    return M->values(state);
}

static Value builtinMapEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, entries);
    return M->entries(state);
}

static Value builtinMapSizeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, Map, size);
    return Value(M->size(state));
}

static Value builtinMapIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isMapIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().MapIterator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }
    MapIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asMapIteratorObject();
    return iter->next(state);
}

void GlobalObject::initializeMap(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->map();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Map), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installMap(ExecutionState& state)
{
    ASSERT(!!m_iteratorPrototype);

    m_map = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Map, builtinMapConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_map->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_map->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_mapPrototype = new MapObject(state, m_objectPrototype);
    m_mapPrototype->setGlobalIntrinsicObject(state, true);
    m_mapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_map, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().clear),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clear, builtinMapClear, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinMapDelete, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().get),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinMapGet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinMapHas, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().set),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinMapSet, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinMapForEach, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().keys),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().keys, builtinMapKeys, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().values),
                                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinMapValues, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto entFn = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinMapEntries, 0, NativeFunctionInfo::Strict));
    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                     ObjectPropertyDescriptor(entFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                     ObjectPropertyDescriptor(entFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                     ObjectPropertyDescriptor(Value(state.context()->staticStrings().Map.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSize, builtinMapSizeGetter, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_mapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().size), desc);

    m_mapIteratorPrototype = new Object(state, m_iteratorPrototype);
    m_mapIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_mapIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinMapIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_mapIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                             ObjectPropertyDescriptor(Value(String::fromASCII("Map Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    m_map->setFunctionPrototype(state, m_mapPrototype);
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Map),
                        ObjectPropertyDescriptor(m_map, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
