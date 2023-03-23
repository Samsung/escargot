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
#include "runtime/WeakSetObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

// https://www.ecma-international.org/ecma-262/10.0/#sec-weakset-constructor
static Value builtinWeakSetConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    // Let set be ? OrdinaryCreateFromConstructor(NewTarget, "%WeakSetPrototype%", « [[WeakSetData]] »).
    // Set set's [[WeakSetData]] internal slot to a new empty List.
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->weakSetPrototype();
    });
    WeakSetObject* set = new WeakSetObject(state, proto);

    // If iterable is not present, let iterable be undefined.
    Value iterable;
    if (argc > 0) {
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

#define RESOLVE_THIS_BINDING_TO_WEAKSET(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                    \
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakSetObject()) {                                                                                                                                                                           \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    WeakSetObject* NAME = thisValue.asObject()->asWeakSetObject();

static Value builtinWeakSetAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKSET(S, WeakSet, add);
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value used as weak set key");
    }

    S->add(state, argv[0].asObject());
    return S;
}

static Value builtinWeakSetDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKSET(S, WeakSet, stringDelete);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(S->deleteOperation(state, argv[0].asObject()));
}

static Value builtinWeakSetHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKSET(S, WeakSet, has);
    if (!argv[0].isObject()) {
        return Value(false);
    }

    return Value(S->has(state, argv[0].asObject()));
}

void GlobalObject::initializeWeakSet(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->weakSet();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakSet), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installWeakSet(ExecutionState& state)
{
    m_weakSet = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakSet, builtinWeakSetConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_weakSet->setGlobalIntrinsicObject(state);

    m_weakSetPrototype = new PrototypeObject(state);
    m_weakSetPrototype->setGlobalIntrinsicObject(state, true);

    m_weakSetPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakSet, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinWeakSetDelete, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinWeakSetHas, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().add),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().add, builtinWeakSetAdd, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSetPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakSet.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakSet->setFunctionPrototype(state, m_weakSetPrototype);
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakSet),
                        ObjectPropertyDescriptor(m_weakSet, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
