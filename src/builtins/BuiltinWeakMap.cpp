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
#include "runtime/WeakMapObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

static Value builtinWeakMapConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
        return Value();
    }

    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->weakMapPrototype();
    });

    WeakMapObject* weakMap = new WeakMapObject(state, proto);

    if (argc == 0 || argv[0].isUndefinedOrNull()) {
        return weakMap;
    }

    Value iterable = argv[0];
    Value add = weakMap->Object::get(state, ObjectPropertyName(state.context()->staticStrings().set)).value(state, weakMap);
    if (!add.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }

    auto iteratorRecord = IteratorObject::getIterator(state, iterable);
    while (true) {
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            return weakMap;
        }

        Value nextItem = IteratorObject::iteratorValue(state, next.value());
        if (!nextItem.isObject()) {
            ErrorObject* error = ErrorObject::createError(state, ErrorCode::TypeError,
                                                          new ASCIIStringFromExternalMemory("Invalid iterator value"));
            return IteratorObject::iteratorClose(state, iteratorRecord, error, true);
        }

        try {
            Value k = nextItem.asObject()->getIndexedProperty(state, Value(0)).value(state, nextItem);
            Value v = nextItem.asObject()->getIndexedProperty(state, Value(1)).value(state, nextItem);
            Value argv[2] = { k, v };
            Object::call(state, add, weakMap, 2, argv);
        } catch (const Value& v) {
            Value exception = v;
            return IteratorObject::iteratorClose(state, iteratorRecord, exception, true);
        }
    }
    return weakMap;
}

#define RESOLVE_THIS_BINDING_TO_WEAKMAP(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                                    \
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {                                                                                                                                                                           \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    WeakMapObject* NAME = thisValue.asObject()->asWeakMapObject();

static Value builtinWeakMapDelete(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, stringDelete);
    if (!argv[0].canBeHeldWeakly(state.context()->vmInstance())) {
        return Value(false);
    }

    return Value(M->deleteOperation(state, argv[0].asPointerValue()));
}

static Value builtinWeakMapGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, get);
    if (!argv[0].canBeHeldWeakly(state.context()->vmInstance())) {
        return Value();
    }

    return M->get(state, argv[0].asPointerValue());
}

static Value builtinWeakMapGetOrInsert(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, getOrInsert);
    if (!argv[0].canBeHeldWeakly(state.context()->vmInstance())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value used as weak map key");
    }

    return M->getOrInsert(state, argv[0].asPointerValue(), argv[1]);
}

static Value builtinWeakMapHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, has);
    if (!argv[0].canBeHeldWeakly(state.context()->vmInstance())) {
        return Value(false);
    }

    return Value(M->has(state, argv[0].asPointerValue()));
}

static Value builtinWeakMapSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    RESOLVE_THIS_BINDING_TO_WEAKMAP(M, WeakMap, set);
    if (!argv[0].canBeHeldWeakly(state.context()->vmInstance())) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Invalid value used as weak map key");
    }

    M->set(state, argv[0].asPointerValue(), argv[1]);
    return M;
}

void GlobalObject::initializeWeakMap(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->weakMap(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakMap), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installWeakMap(ExecutionState& state)
{
    m_weakMap = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().WeakMap, builtinWeakMapConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_weakMap->setGlobalIntrinsicObject(state);

    m_weakMapPrototype = new PrototypeObject(state, m_objectPrototype);
    m_weakMapPrototype->setGlobalIntrinsicObject(state, true);
    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringDelete),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringDelete, builtinWeakMapDelete, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinWeakMapGet, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getOrInsert),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getOrInsert, builtinWeakMapGetOrInsert, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().has),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinWeakMapHas, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinWeakMapSet, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMapPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                ObjectPropertyDescriptor(Value(state.context()->staticStrings().WeakMap.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_weakMap->setFunctionPrototype(state, m_weakMapPrototype);
    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().WeakMap),
                        ObjectPropertyDescriptor(m_weakMap, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
