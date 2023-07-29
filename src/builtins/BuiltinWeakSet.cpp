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
    if (!newTarget.hasValue()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->weakSetPrototype();
    });
    RETURN_VALUE_IF_PENDING_EXCEPTION

    WeakSetObject* weakSet = new WeakSetObject(state, proto);

    Value iterable;
    if (argc > 0) {
        iterable = argv[0];
    }
    if (iterable.isUndefinedOrNull()) {
        return weakSet;
    }
    Value add = weakSet->get(state, ObjectPropertyName(state.context()->staticStrings().add)).value(state, weakSet);
    RETURN_VALUE_IF_PENDING_EXCEPTION
    if (!add.isCallable()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }

    auto iteratorRecord = IteratorObject::getIterator(state, iterable);
    RETURN_VALUE_IF_PENDING_EXCEPTION

    while (true) {
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        if (!next.hasValue()) {
            return weakSet;
        }

        Value nextValue = IteratorObject::iteratorValue(state, next.value());
        RETURN_VALUE_IF_PENDING_EXCEPTION

        Value argv[1] = { nextValue };
        Object::call(state, add, weakSet, 1, argv);
        if (UNLIKELY(state.hasPendingException())) {
            Value exceptionValue = state.detachPendingException();
            return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
        }
    }

    return weakSet;
}

#define RESOLVE_THIS_BINDING_TO_WEAKSET(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                      \
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakSetObject()) {                                                                                                                                                                             \
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                    \
    WeakSetObject* NAME = thisValue.asObject()->asWeakSetObject();

static Value builtinWeakSetAdd(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKSET(S, WeakSet, add);
    if (!argv[0].isObject()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, "Invalid value used as weak set key");
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
