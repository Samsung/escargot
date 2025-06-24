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
#include "runtime/SetObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

static Value builtinSetConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
        return Value();
    }

    // Let set be ? OrdinaryCreateFromConstructor(NewTarget, "%SetPrototype%", « [[SetData]] »).
    // Set set's [[SetData]] internal slot to a new empty List.
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->setPrototypeObject();
    });
    SetObject* set = new SetObject(state, proto);

    // If iterable is not present, let iterable be undefined.
    Value iterable;
    if (argc >= 1) {
        iterable = argv[0];
    }

    // If iterable is either undefined or null, return set.
    if (iterable.isUndefinedOrNull()) {
        return set;
    }

    // Let adder be ? Get(set, "add").
    Value adder = set->get(state, ObjectPropertyName(state.context()->staticStrings().add)).value(state, set);
    // If IsCallable(adder) is false, throw a TypeError exception.
    if (!adder.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    // Let iteratorRecord be ? GetIterator(iterable).
    auto iteratorRecord = IteratorObject::getIterator(state, iterable);

    // Repeat
    while (true) {
        // Let next be ? IteratorStep(iteratorRecord).
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        // If next is false, return set.
        if (!next.hasValue()) {
            return set;
        }
        // Let nextValue be ? IteratorValue(next).
        Value nextValue = IteratorObject::iteratorValue(state, next.value());

        // Let status be Call(adder, set, « nextValue »).
        try {
            Value argv[1] = { nextValue };
            Object::call(state, adder, set, 1, argv);
        } catch (const Value& v) {
            // we should save thrown value bdwgc cannot track thrown value
            Value exceptionValue = v;
            // If status is an abrupt completion, return ? IteratorClose(iteratorRecord, status).
            return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
        }
    }

    return set;
}

#define RESOLVE_THIS_BINDING_TO_SET(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                        \
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {                                                                                                                                                                               \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    SetObject* NAME = thisValue.asObject()->asSetObject();

static Value builtinSetAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, add);
    S->add(state, argv[0]);
    return S;
}

static Value builtinSetClear(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, clear);
    S->clear(state);
    return Value();
}

static Value builtinSetDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, stringDelete);
    return Value(S->deleteOperation(state, argv[0]));
}

static Value builtinSetHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, has);
    return Value(S->has(state, argv[0]));
}

static Value builtinSetForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let S be the this value.
    // If Type(S) is not Object, throw a TypeError exception.
    // If S does not have a [[SetData]] internal slot, throw a TypeError exception.
    RESOLVE_THIS_BINDING_TO_SET(S, Set, forEach);

    Value callbackfn = argv[0];
    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Set.string(), true, state.context()->staticStrings().forEach.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
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
            Object::call(state, callbackfn, T, 3, argv);
        }
    }

    return Value();
}

// https://tc39.es/proposal-set-methods/#sec-getsetrecord
struct SetRecord {
    Object* set;
    size_t size;
    Value has;
    Value keys;
};

static SetRecord getSetRecord(ExecutionState& state, const Value& objInput)
{
    // 1. If obj is not an Object, throw a TypeError exception.
    if (!objInput.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_FirstArgumentNotObject);
    }
    Object* obj = objInput.asObject();
    // 2. Let rawSize be ? Get(obj, "size").
    auto rawSize = obj->get(state, ObjectPropertyName(state.context()->staticStrings().size)).value(state, obj);
    // 3. Let numSize be ? ToNumber(rawSize).
    // 4. NOTE: If rawSize is undefined, then numSize will be NaN.
    auto numSize = rawSize.toNumber(state);
    // 5. If numSize is NaN, throw a TypeError exception.
    if (std::isnan(numSize)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid obj.size");
    }
    // 6. Let intSize be ! ToIntegerOrInfinity(numSize).
    auto intSize = Value(Value::DoubleToIntConvertibleTestNeeds, numSize).toInteger(state);
    // 7. If intSize < 0, throw a RangeError exception.
    if (intSize < 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Invalid obj.size");
    }
    // 8. Let has be ? Get(obj, "has").
    auto has = obj->get(state, ObjectPropertyName(state.context()->staticStrings().has)).value(state, obj);
    // 9. If IsCallable(has) is false, throw a TypeError exception.
    if (!has.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "obj.has is not callable");
    }
    // 10. Let keys be ? Get(obj, "keys").
    auto keys = obj->get(state, ObjectPropertyName(state.context()->staticStrings().keys)).value(state, obj);
    // 11. If IsCallable(keys) is false, throw a TypeError exception.
    if (!keys.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "obj.keys is not callable");
    }
    // 12. Return a new Set Record { [[Set]]: obj, [[Size]]: intSize, [[Has]]: has, [[Keys]]: keys }.
    return { obj, static_cast<size_t>(intSize), has, keys };
}

// https://tc39.es/ecma262/#sec-setdataindex
static size_t setDataIndex(ExecutionState& state, const SetObject::SetObjectData& setData, Value value)
{
    // 1. Set value to CanonicalizeKeyedCollectionKey(value).
    value = value.toCanonicalizeKeyedCollectionKey(state);
    // 2. Let size be the number of elements in setData.
    size_t size = setData.size();
    // 3. Let index be 0.
    size_t index = 0;
    // 4. Repeat, while index < size,
    while (index < size) {
        // a. Let e be setData[index].
        auto e = setData[index];
        // b. If e is not empty and e is SameValueZero(e, value), then
        if (!e.isEmpty() && value.equalsToByTheSameValueZeroAlgorithm(state, Value(e))) {
            // i. Return index.
            return index;
        }
        // c. Set index to index + 1.
        index++;
    }
    // 5. Return not-found.
    return SIZE_MAX;
}

// https://tc39.es/ecma262/#sec-setdatahas
static bool setDataHas(ExecutionState& state, const SetObject::SetObjectData& setData, const Value& value)
{
    return setDataIndex(state, setData, value) != SIZE_MAX;
}

// https://tc39.es/ecma262/#sec-set.prototype.union
static Value builtinSetUnion(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 1. Let O be the this value.
    // 2. Perform ? RequireInternalSlot(O, [[SetData]]).
    RESOLVE_THIS_BINDING_TO_SET(O, Set, stringUnion);
    // 3. Let otherRec be ? GetSetRecord(other).
    SetRecord otherRec = getSetRecord(state, argv[0]);
    // 4. Let keysIter be ? GetIteratorFromMethod(otherRec.[[Set]], otherRec.[[Keys]]).
    auto keysIter = IteratorObject::getIteratorFromMethod(state, otherRec.set, otherRec.keys);
    // 5. Let resultSetData be a copy of O.[[SetData]].
    SetObject::SetObjectData resultSetData = O->asSetObject()->storage();
    // 6. Let next be true.
    // 7. Repeat, while next is not false,
    while (true) {
        // a. Set next to ? IteratorStep(keysIter).
        Optional<Object*> next = IteratorObject::iteratorStep(state, keysIter);
        // b. If next is not false, then
        if (next) {
            // i. Let nextValue be ? IteratorValue(next).
            auto nextValue = IteratorObject::iteratorValue(state, next.value());
            // Set nextValue to CanonicalizeKeyedCollectionKey(nextValue).
            nextValue = nextValue.toCanonicalizeKeyedCollectionKey(state);
            // ii. If SetDataHas(resultSetData, nextValue) is false, then
            if (!setDataHas(state, resultSetData, nextValue)) {
                // 1. Append nextValue to resultSetData.
                resultSetData.pushBack(nextValue);
            }
        } else {
            break;
        }
    }
    // 8. Let result be OrdinaryObjectCreate(%Set.prototype%, « [[SetData]] »).
    // 9. Set result.[[SetData]] to resultSetData.
    // 10. Return result.
    return new SetObject(state, state.context()->globalObject()->setPrototypeObject(), std::move(resultSetData));
}

static Value builtinSetValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, values);
    return S->values(state);
}

static Value builtinSetEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, entries);
    return S->entries(state);
}

static Value builtinSetSizeGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_SET(S, Set, size);
    return Value(S->size(state));
}

static Value builtinSetIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isSetIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().SetIterator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }
    SetIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asSetIteratorObject();
    return iter->next(state);
}

void GlobalObject::initializeSet(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->set();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Set), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installSet(ExecutionState& state)
{
    ASSERT(!!m_iteratorPrototype);

    m_set = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Set, builtinSetConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_set->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_set->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_setPrototypeObject = new PrototypeObject(state);
    m_setPrototypeObject->setGlobalIntrinsicObject(state, true);
    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_set, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().clear),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clear, builtinSetClear, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinSetDelete, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinSetHas, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().add),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinSetAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinSetForEach, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringUnion),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringUnion, builtinSetUnion, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto valuesFn = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinSetValues, 0, NativeFunctionInfo::Strict));
    auto values = ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent));
    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().values), values);
    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().keys), values);

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinSetEntries, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                  ObjectPropertyDescriptor(valuesFn, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                  ObjectPropertyDescriptor(Value(state.context()->staticStrings().Set.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));
    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("get size")), builtinSetSizeGetter, 0, NativeFunctionInfo::Strict)),
        Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_setPrototypeObject->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().size), desc);

    m_setIteratorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_setIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_setIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinSetIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_setIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                    ObjectPropertyDescriptor(Value(String::fromASCII("Set Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_set->setFunctionPrototype(state, m_setPrototypeObject);
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Set),
                        ObjectPropertyDescriptor(m_set, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
