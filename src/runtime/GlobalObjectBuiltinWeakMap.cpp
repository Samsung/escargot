/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "WeakMapObject.h"
#include "IteratorObject.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinWeakMapConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ConstructorRequiresNew);
    }

    // Let map be ? OrdinaryCreateFromConstructor(NewTarget, "%MapPrototype%", « [[MapData]] »).
    // Set map's [[MapData]] internal slot to a new empty List.
    WeakMapObject* map = thisValue.asObject()->asWeakMapObject();
    // If iterable is not present, let iterable be undefined.
    Value iterable;
    if (argc >= 1) {
        iterable = argv[0];
    }
    // If iterable is either undefined or null, let iter be undefined.
    Value adder;
    Object* iter = nullptr;
    if (iterable.isUndefinedOrNull()) {
        iterable = Value();
    } else {
        // Else,
        // Let adder be ? Get(map, "set").
        adder = map->Object::get(state, ObjectPropertyName(state.context()->staticStrings().set)).value(state, map);
        // If IsCallable(adder) is false, throw a TypeError exception.
        if (!adder.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
        }
        // Let iter be ? GetIterator(iterable).
        iterable = iterable.toObject(state);
        iter = iterable.asObject()->iterator(state);
        if (iter == nullptr) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "object is not iterable");
        }
    }
    // If iter is undefined, return map.
    if (!iter) {
        return map;
    }

    // Repeat
    while (true) {
        // Let next be ? IteratorStep(iter).
        auto next = iter->iteratorNext(state);
        // If next is false(done is true), return map.
        if (next.second) {
            return map;
        }
        // Let nextItem be ? IteratorValue(next).
        Value nextItem = next.first;

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
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {                                                                                                                                                                   \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                          \
    WeakMapObject* NAME = thisValue.asObject()->asWeakMapObject();

static Value builtinWeakMapDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, WeakMap, stringDelete);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(M->deleteOperation(state, argv[0].asObject()));
}

static Value builtinWeakMapGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, WeakMap, get);
    if (!argv[0].isObject()) {
        return Value();
    }

    return M->get(state, argv[0].asObject());
}

static Value builtinWeakMapHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, WeakMap, has);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(M->has(state, argv[0].asObject()));
}

static Value builtinWeakMapSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_MAP(M, WeakMap, set);
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid value used as weak map key");
    }

    M->set(state, argv[0].asObject(), argv[1]);
    return M;
}

void GlobalObject::installWeakMap(ExecutionState& state)
{
    m_weakMap = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakMap, builtinWeakMapConstructor, 0, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                       return new WeakMapObject(state);
                                   }),
                                   FunctionObject::__ForBuiltin__);
    m_weakMap->markThisObjectDontNeedStructureTransitionTable(state);
    m_weakMap->setPrototype(state, m_functionPrototype);
    m_weakMapPrototype = m_objectPrototype;
    m_weakMapPrototype = new WeakMapObject(state);
    m_weakMapPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_weakMapPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinWeakMapDelete, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().get),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinWeakMapGet, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinWeakMapHas, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().set),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinWeakMapSet, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakMap.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMap->setFunctionPrototype(state, m_weakMapPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakMap),
                      ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
