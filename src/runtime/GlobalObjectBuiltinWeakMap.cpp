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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "WeakMapObject.h"
#include "IteratorObject.h"
#include "NativeFunctionObject.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinWeakMapConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    // Let map be ? OrdinaryCreateFromConstructor(NewTarget, "%MapPrototype%", « [[MapData]] »).
    // Set map's [[MapData]] internal slot to a new empty List.
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->weakMapPrototype();
    });
    WeakMapObject* map = new WeakMapObject(state, proto);

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

#define RESOLVE_THIS_BINDING_TO_WEAKMAP(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                      \
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {                                                                                                                                                                             \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                    \
    WeakMapObject* NAME = thisValue.asObject()->asWeakMapObject();

static Value builtinWeakMapDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, stringDelete);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(M->deleteOperation(state, argv[0].asObject()));
}

static Value builtinWeakMapGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, get);
    if (!argv[0].isObject()) {
        return Value();
    }

    return M->get(state, argv[0].asObject());
}

static Value builtinWeakMapHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, has);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(M->has(state, argv[0].asObject()));
}

static Value builtinWeakMapSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, set);
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid value used as weak map key");
    }

    M->set(state, argv[0].asObject(), argv[1]);
    return M;
}

void GlobalObject::installWeakMap(ExecutionState& state)
{
    m_weakMap = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakMap, builtinWeakMapConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_weakMap->setGlobalIntrinsicObject(state);

    m_weakMapPrototype = new Object(state, m_objectPrototype);
    m_weakMapPrototype->setGlobalIntrinsicObject(state, true);
    m_weakMapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinWeakMapDelete, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().get),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinWeakMapGet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinWeakMapHas, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().set),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinWeakMapSet, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakMap.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMap->setFunctionPrototype(state, m_weakMapPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakMap),
                      ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
