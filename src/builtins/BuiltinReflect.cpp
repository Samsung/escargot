/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "runtime/Object.h"
#include "runtime/ProxyObject.h"
#include "runtime/NativeFunctionObject.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"

namespace Escargot {

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.apply
static Value builtinReflectApply(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];
    Value thisArgument = argv[1];
    Value argList = argv[2];

    // 1. If IsCallable(target) is false, throw a TypeError exception.
    if (!target.isObject() || !target.asObject()->isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: calling a not-callable target in apply function is forbidden");
    }

    // 2. Let args be CreateListFromArrayLike(argumentsList).
    auto args = Object::createListFromArrayLike(state, argList);

    // 5. Return Call(target, thisArgument, args).
    return Object::call(state, target, thisArgument, args.size(), args.data());
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.construct
static Value builtinReflectConstruct(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];
    Value argList = argv[1];
    Value newTargetArg = argc > 2 ? argv[2] : target;

    // 1. If IsConstructor(target) is false, throw a TypeError exception.
    if (!target.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.construct should has a construct method");
    }

    // 2. If newTarget is not present, let newTarget be target.
    // 3. Else, if IsConstructor(newTarget) is false, throw a TypeError exception.
    if (!newTargetArg.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The new target of Reflect.construct should be a constructor");
    }

    // 4. Let args be CreateListFromArrayLike(argumentsList).
    auto args = Object::createListFromArrayLike(state, argList);
    // 6. Return Construct(target, args, newTarget).
    return Object::construct(state, target, args.size(), args.data(), newTargetArg.asObject());
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.defineproperty
static Value builtinReflectDefineProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.defineProperty should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 3. Let desc be ToPropertyDescriptor(attributes).
    if (!argv[2].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The attributes of Reflect.defineProperty should be an Object");
    }
    ObjectPropertyDescriptor desc(state, argv[2].asObject());

    // 6. Return target.[[DefineOwnProperty]](key, desc).
    return Value(target.asObject()->defineOwnProperty(state, key, desc));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.deleteproperty
static Value builtinReflectDeleteProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.deleteProperty should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 4. Return target.[[Delete]](key).
    return Value(target.asObject()->deleteOwnProperty(state, key));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.get
static Value builtinReflectGet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.get should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 4. If receiver is not present, then
    // 4.a. Let receiver be target.
    Value receiver = argc > 2 ? argv[2] : target;

    // 5. Return target.[[Get]](key, receiver).
    return target.asObject()->get(state, key).value(state, receiver);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.getownpropertydescriptor
static Value builtinReflectGetOwnPropertyDescriptor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.getOwnPropertyDescriptor should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 4. Let desc be target.[[GetOwnProperty]](key).
    ObjectGetResult desc = target.asObject()->getOwnProperty(state, key);

    // 6. Return FromPropertyDescriptor(desc).
    return desc.fromPropertyDescriptor(state, target.asObject());
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.getprototypeof
static Value builtinReflectGetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.getPrototypeOf should be an Object");
    }

    // 2. Return target.[[GetPrototypeOf]]().
    return target.asObject()->getPrototype(state);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.has
static Value builtinReflectHas(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.has should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 4. Return target.[[HasProperty]](key).
    return Value(target.asObject()->hasProperty(state, key));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.isextensible
static Value builtinReflectIsExtensible(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.isExtensible should be an Object");
    }

    // 2. Return target.[[IsExtensible]]().
    return Value(target.asObject()->isExtensible(state));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.preventextensions
static Value builtinReflectPreventExtensions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.preventExtension should be an Object");
    }

    // 2. Return target.[[PreventExtensions]]().
    return Value(target.asObject()->preventExtensions(state));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.ownkeys
static Value builtinReflectOwnKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.preventExtension should be an Object");
    }
    // 2. Let keys be target.[[OwnPropertyKeys]]().
    // 3. ReturnIfAbrupt(keys).
    auto keys = target.asObject()->ownPropertyKeys(state);
    // 4. Return CreateArrayFromList(keys).
    return Object::createArrayFromList(state, keys.size(), keys.data());
}


// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.set
static Value builtinReflectSet(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.set should be an Object");
    }

    // 2. Let key be ToPropertyKey(propertyKey).
    ObjectPropertyName key(state, argv[1]);

    // 4. If receiver is not present, then
    // 4.a. Let receiver be target.
    Value receiver = argc > 3 ? argv[3] : target;

    // 5. Return target.[[Set]](key, V, receiver).
    return Value(target.asObject()->set(state, key, argv[2], receiver));
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-reflect.setprototypeof
static Value builtinReflectSetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Value target = argv[0];
    Value proto = argv[1];

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The target of Reflect.setPrototypeOf should be an Object");
    }

    // 2. If Type(proto) is not Object and proto is not null, throw a TypeError exception
    if (!proto.isObject() && !proto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Reflect.string(), false, String::emptyString(), "%s: The proto of Reflect.setPrototypeOf should be an Object or Null");
    }

    // 3. Return target.[[SetPrototypeOf]](proto).
    return Value(target.asObject()->setPrototype(state, proto));
}

void GlobalObject::initializeReflect(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->reflect();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Reflect), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installReflect(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_reflect = new Object(state);
    m_reflect->setGlobalIntrinsicObject(state);

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().apply),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().apply, builtinReflectApply, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().construct),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().construct, builtinReflectConstruct, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().defineProperty),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().defineProperty, builtinReflectDefineProperty, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().deleteProperty),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().deleteProperty, builtinReflectDeleteProperty, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().get, builtinReflectGet, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getOwnPropertyDescriptor),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getOwnPropertyDescriptor, builtinReflectGetOwnPropertyDescriptor, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getPrototypeOf),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getPrototypeOf, builtinReflectGetPrototypeOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().has),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().has, builtinReflectHas, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isExtensible),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isExtensible, builtinReflectIsExtensible, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().preventExtensions),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().preventExtensions, builtinReflectPreventExtensions, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().ownKeys),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ownKeys, builtinReflectOwnKeys, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().set, builtinReflectSet, 3, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().setPrototypeOf),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().setPrototypeOf, builtinReflectSetPrototypeOf, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_reflect->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                       ObjectPropertyDescriptor(state.context()->staticStrings().Reflect.string(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(strings->Reflect),
                        ObjectPropertyDescriptor(m_reflect, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
