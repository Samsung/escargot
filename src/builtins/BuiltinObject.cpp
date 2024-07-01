/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "runtime/ArrayObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

typedef VectorWithInlineStorage<48, std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>> ObjectStructurePropertyVector;

static Value builtinObject__proto__Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return thisValue.toObject(state)->getPrototype(state);
}

static Value builtinObject__proto__Setter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value value = argv[0];
    Object* thisObject = thisValue.toObject(state);
    // Setting __proto__ to a non-object, non-null value is ignored
    if (!value.isObject() && !value.isNull()) {
        return Value();
    }
    if (!thisObject->setPrototype(state, value)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "can't set prototype of this object");
    }
    return Value();
}

static Value builtinObjectConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is neither undefined nor the active function, then
    if (newTarget.hasValue() && newTarget.value() != state.resolveCallee()) {
        // Return ? OrdinaryCreateFromConstructor(NewTarget, "%ObjectPrototype%").
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
            return constructorRealm->globalObject()->objectPrototype();
        });
        return new Object(state, proto);
    }

    Value value = argv[0];
    if (value.isUndefined() || value.isNull()) {
        // If value is null, undefined or not supplied, return ObjectCreate(%ObjectPrototype%).
        return new Object(state);
    } else {
        // Return ! ToObject(value).
        return value.toObject(state);
    }
}

static Value builtinObjectValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return thisValue.toObject(state);
}

static Value builtinObjectPreventExtensions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return argv[0];
    }
    Object* o = argv[0].asObject();
    if (!o->preventExtensions(state)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().preventExtensions.string(), "PreventExtensions is false");
    }
    return o;
}

static Value builtinObjectToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    StaticStrings* strings = &state.context()->staticStrings();

    if (thisValue.isUndefined()) {
        return strings->lazyObjectUndefinedToString().string();
    } else if (thisValue.isNull()) {
        return strings->lazyObjectNullToString().string();
    }

    Object* thisObject = thisValue.toObject(state);

    // check isArray first
    bool isArray = thisObject->isArray(state);

    Value toStringTag = thisObject->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag)).value(state, thisObject);
    if (toStringTag.isString()) {
        String* tag = toStringTag.asString();
        auto bad = tag->bufferAccessData();
        if (bad.has8BitContent && bad.length < 256) {
            LChar* buf = (LChar*)alloca(bad.length + 10);
            size_t len = 0;

            buf[0] = '[';
            buf[1] = 'o';
            buf[2] = 'b';
            buf[3] = 'j';
            buf[4] = 'e';
            buf[5] = 'c';
            buf[6] = 't';
            buf[7] = ' ';
            memcpy(buf + 8, bad.bufferAs8Bit, bad.length);
            buf[bad.length + 8] = ']';
            buf[bad.length + 9] = 0;
            len = 9 + bad.length;

            return AtomicString(state.context(), buf, len).string();
        }

        StringBuilder builder;
        builder.appendString("[object ");
        builder.appendString(tag);
        builder.appendString("]");
        return AtomicString(state, builder.finalize()).string();
    }

    if (isArray) {
        return strings->lazyObjectArrayToString().string();
    } else if (thisObject->isArgumentsObject()) {
        return strings->lazyObjectArgumentsToString().string();
    } else if (thisObject->isCallable()) {
        return strings->lazyObjectFunctionToString().string();
    } else if (thisObject->isErrorObject()) {
        return strings->lazyObjectErrorToString().string();
    } else if (thisObject->isBooleanObject()) {
        return strings->lazyObjectBooleanToString().string();
    } else if (thisObject->isNumberObject()) {
        return strings->lazyObjectNumberToString().string();
    } else if (thisObject->isStringObject()) {
        return strings->lazyObjectStringToString().string();
    } else if (thisObject->isDateObject()) {
        return strings->lazyObjectDateToString().string();
    } else if (thisObject->isRegExpObject()) {
        return strings->lazyObjectRegExpToString().string();
    }

    return strings->lazyObjectObjectToString().string();
}

static Value builtinObjectHasOwn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* obj = argv[0].toObject(state);
    return Value(obj->hasOwnProperty(state, ObjectPropertyName(state, argv[1])));
}

static Value builtinObjectHasOwnProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ObjectPropertyName key(state, argv[0]);
    Object* obj = thisValue.toObject(state);
    return Value(obj->hasOwnProperty(state, key));
}

static Value objectDefineProperties(ExecutionState& state, Value object, Value properties)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!object.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Object.string(), false, strings->defineProperty.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotObject);
    }

    Object* O = object.asObject();
    // Let props be ? ToObject(Properties).
    Object* props = properties.toObject(state);

    // Let keys be ? props.[[OwnPropertyKeys]]().
    Object::OwnPropertyKeyVector keys = props->ownPropertyKeys(state);
    // Let descriptors be a new empty List.
    VectorWithInlineStorage<32, std::pair<ObjectPropertyName, ObjectPropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>>> descriptors;

    for (size_t i = 0; i < keys.size(); i++) {
        // Let propDesc be ? props.[[GetOwnProperty]](nextKey).
        ObjectPropertyName nextKey(state, keys[i]);
        ObjectGetResult propDesc = props->getOwnProperty(state, nextKey);
        // If propDesc is not undefined and propDesc.[[Enumerable]] is true, then
        if (propDesc.hasValue() && propDesc.isEnumerable()) {
            // Let descObj be ? Get(props, nextKey).
            Value descVal = propDesc.value(state, props);
            if (!descVal.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().defineProperty.string(), ErrorObject::Messages::GlobalObject_DescriptorNotObject);
            }

            // Let desc be ? ToPropertyDescriptor(descObj).
            ObjectPropertyDescriptor desc(state, descVal.asObject());
            // Append the pair (a two element List) consisting of nextKey and desc to the end of descriptors.
            descriptors.push_back(std::make_pair(nextKey, desc));
        }
    }

    // For each pair from descriptors in list order, do
    for (auto& it : descriptors) {
        // Let P be the first element of pair.
        // Let desc be the second element of pair.
        // Perform ? DefinePropertyOrThrow(O, P, desc).
        O->defineOwnPropertyThrowsException(state, it.first, it.second);
    }

    return O;
}

static Value builtinObjectCreate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject() && !argv[0].isNull())
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().create.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotObjectAndNotNull);
    Object* obj;
    if (argv[0].isNull()) {
        obj = new Object(state, Object::PrototypeIsNull);
    } else {
        obj = new Object(state);
        obj->setPrototype(state, argv[0]);
    }

    if (!argv[1].isUndefined())
        return objectDefineProperties(state, obj, argv[1]);
    return obj;
}

static Value builtinObjectDefineProperties(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return objectDefineProperties(state, argv[0], argv[1]);
}

static Value builtinObjectDefineProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Object.defineProperty ( O, P, Attributes )
    // If Type(O) is not Object, throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().defineProperty.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // Let key be ToPropertyKey(P).
    ObjectPropertyName key = ObjectPropertyName(state, argv[1]);

    // Let desc be ToPropertyDescriptor(Attributes).
    if (!argv[2].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Property description must be an object");
    }

    ObjectPropertyDescriptor desc(state, argv[2].asObject());

    O->defineOwnPropertyThrowsException(state, key, desc);
    return O;
}

static Value builtinObjectIsPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Object.prototype.isPrototypeOf (V)
    // If V is not an object, return false.
    if (!argv[0].isObject()) {
        return Value(false);
    }
    Value V = argv[0];

    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Repeat
    while (true) {
        // Let V be the value of the [[Prototype]] internal property of V.
        V = V.toObject(state)->getPrototype(state);
        // if V is null, return false
        if (!V.isObject())
            return Value(false);
        // If O and V refer to the same object, return true.
        if (V.asObject() == O) {
            return Value(true);
        }
    }
}

static Value builtinObjectPropertyIsEnumerable(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let P be toPropertyKey(V).
    ObjectPropertyName P(state, argv[0]);

    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name.
    ObjectGetResult desc = O->getOwnProperty(state, P);

    // If desc is undefined, return false.
    if (!desc.hasValue())
        return Value(false);

    // Return the value of desc.[[Enumerable]].
    return Value(desc.isEnumerable());
}

static Value builtinObjectToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/#sec-object.prototype.tolocalestring
    // Let O be the this value.
    // Return ? Invoke(O, "toString").
    Object* O = thisValue.toObject(state);
    Value toString = O->get(state, ObjectPropertyName(state.context()->staticStrings().toString)).value(state, thisValue);
    return Object::call(state, toString, thisValue, 0, nullptr);
}

static Value builtinObjectGetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return argv[0].toObject(state)->getPrototype(state);
}

static Value builtinObjectSetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-setprototypeof-v
    // 19.1.2.18 Object.setPrototypeOf ( O, proto )
    Value object = argv[0];
    Value proto = argv[1];

    // 1. Let O be RequireObjectCoercible(O).
    // 2. ReturnIfAbrupt(O).
    if (object.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    // 3. If Type(proto) is neither Object nor Null, throw a TypeError exception.
    if (!proto.isObject() && !proto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    // 4. If Type(O) is not Object, return O.
    if (!object.isObject()) {
        return object;
    }

    // 5. Let status be O.[[SetPrototypeOf]](proto).
    Object* obj = object.toObject(state);

    // 7. If status is false, throw a TypeError exception.
    if (!obj->setPrototype(state, proto)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "can't set prototype of this object");
        return Value();
    }

    return object;
}

static Value builtinObjectFreeze(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        // If Type(O) is not Object, return O.
        return argv[0];
    }

    Object* O = argv[0].asObject();

    // Let status be ? SetIntegrityLevel(O, frozen).
    // If status is false, throw a TypeError exception.
    if (!Object::setIntegrityLevel(state, O, false)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().freeze.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Return O.
    return O;
}

static Value builtinObjectFromEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argv[0].isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().fromEntries.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull);
    }
    Value iterable = argv[0];
    Object* obj = new Object(state);

    auto iteratorRecord = IteratorObject::getIterator(state, iterable);
    while (true) {
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            return obj;
        }

        Value nextItem = IteratorObject::iteratorValue(state, next.value());
        if (!nextItem.isObject()) {
            ErrorObject* errorobj = ErrorObject::createError(state, ErrorCode::TypeError, new ASCIIString("TypeError"));
            return IteratorObject::iteratorClose(state, iteratorRecord, errorobj, true);
        }

        try {
            // Let k be Get(nextItem, "0").
            // If k is an abrupt completion, return ? IteratorClose(iter, k).
            Value k = nextItem.asObject()->getIndexedProperty(state, Value(0)).value(state, nextItem);
            // Let v be Get(nextItem, "1").
            // If v is an abrupt completion, return ? IteratorClose(iter, v).
            Value v = nextItem.asObject()->getIndexedProperty(state, Value(1)).value(state, nextItem);

            ObjectPropertyName key(state, k);
            obj->defineOwnPropertyThrowsException(state, key,
                                                  ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        } catch (const Value& v) {
            // we should save thrown value bdwgc cannot track thrown value
            Value exceptionValue = v;
            // If status is an abrupt completion, return ? IteratorClose(iteratorRecord, status).
            return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
        }
    }

    return obj;
}

// https://262.ecma-international.org/#sec-object.getownpropertydescriptor
static Value builtinObjectGetOwnPropertyDescriptor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let obj be ToObject(O).
    Object* O = argv[0].toObject(state);

    // Let desc be ? obj.[[GetOwnProperty]](key).
    // Return FromPropertyDescriptor(desc).
    return O->getOwnPropertyDescriptor(state, ObjectPropertyName(state, argv[1]));
}

// https://262.ecma-international.org/#sec-object.getownpropertydescriptors
static Value builtinObjectGetOwnPropertyDescriptors(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* obj = argv[0].toObject(state);
    auto ownKeys = obj->ownPropertyKeys(state);
    Object* descriptors = new Object(state);

    for (uint64_t i = 0; i < ownKeys.size(); i++) {
        Value descriptor = obj->getOwnPropertyDescriptor(state, ObjectPropertyName(state, ownKeys[i]));
        if (!descriptor.isUndefined()) {
            descriptors->defineOwnProperty(state, ObjectPropertyName(state, ownKeys[i]), ObjectPropertyDescriptor(descriptor, ObjectPropertyDescriptor::AllPresent));
        }
    }
    return descriptors;
}

enum class GetOwnPropertyKeysType : unsigned {
    String,
    Symbol
};

static ArrayObject* getOwnPropertyKeys(ExecutionState& state, Value o, GetOwnPropertyKeysType type)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-getownpropertykeys

    // 1. Let obj be ToObject(O).
    // 2. ReturnIfAbrupt(obj)
    auto obj = o.asObject();
    // 3. Let keys be obj.[[OwnPropertyKeys]]().
    // 4. ReturnIfAbrupt(keys).
    auto keys = obj->ownPropertyKeys(state);

    // 5. Let nameList be a new empty List.
    ValueVectorWithInlineStorage nameList;

    // 6. Repeat for each element nextKey of keys in List order,
    //  a. If Type(nextKey) is Type, then
    //      i. Append nextKey as the last element of nameList.
    bool (Escargot::Value::*func)() const;
    if (type == GetOwnPropertyKeysType::String) {
        func = &Value::isString;
    } else {
        func = &Value::isSymbol;
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        if ((keys[i].*func)()) {
            nameList.pushBack(keys[i]);
        }
    }

    // 7. Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

static Value builtinObjectGetOwnPropertyNames(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-object.getownpropertynames
    Object* O = argv[0].toObject(state);
    return getOwnPropertyKeys(state, O, GetOwnPropertyKeysType::String);
}

static Value builtinObjectGetOwnPropertySymbols(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-object.getownpropertysymbols
    Object* O = argv[0].toObject(state);
    return getOwnPropertyKeys(state, O, GetOwnPropertyKeysType::Symbol);
}

static Value builtinObjectIsExtensible(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return Value(Value::False);
    }
    // Return the Boolean value of the [[Extensible]] internal property of O.
    return Value(argv[0].asObject()->isExtensible(state));
}

static Value builtinObjectIsFrozen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return Value(Value::True);
    }

    bool result = Object::testIntegrityLevel(state, argv[0].asObject(), false);
    return Value(result);
}

static Value builtinObjectIsSealed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return Value(Value::True);
    }

    bool result = Object::testIntegrityLevel(state, argv[0].asObject(), true);
    return Value(result);
}

static Value builtinObjectSeal(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isObject()) {
        return argv[0];
    }
    Object* O = argv[0].asObject();

    // Let status be ? SetIntegrityLevel(O, sealed).
    // If status is false, throw a TypeError exception.
    if (!Object::setIntegrityLevel(state, O, true)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().seal.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Return O.
    return O;
}

static Value builtinObjectAssign(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Object.assign ( target, ...sources )
    // Let to be ? ToObject(target).
    Object* to = argv[0].toObject(state);
    // If only one argument was passed, return to.
    if (argc == 1) {
        return to;
    }
    // Let sources be the List of argument values starting with the second argument.
    // For each element nextSource of sources, in ascending index order, do
    for (size_t i = 1; i < argc; i++) {
        Value nextSource = argv[i];
        // If nextSource is undefined or null, let keys be a new empty List.
        Object::OwnPropertyKeyVector keys;
        Object* from = nullptr;
        if (!nextSource.isUndefinedOrNull()) {
            // Let from be ! ToObject(nextSource).
            from = nextSource.toObject(state);
            // Let keys be ? from.[[OwnPropertyKeys]]().
            keys = from->ownPropertyKeys(state);
        }

        // For each element nextKey of keys in List order, do
        for (size_t i = 0; i < keys.size(); i++) {
            Value nextKey = keys[i];
            // Let desc be ? from.[[GetOwnProperty]](nextKey).
            auto desc = from->getOwnProperty(state, ObjectPropertyName(state, nextKey));
            // If desc is not undefined and desc.[[Enumerable]] is true, then
            if (desc.hasValue() && desc.isEnumerable()) {
                // Let propValue be ? Get(from, nextKey).
                Value propValue;
                if (from->isProxyObject()) {
                    propValue = from->get(state, ObjectPropertyName(state, Value(nextKey))).value(state, from);
                } else {
                    propValue = desc.value(state, from);
                }
                // Perform ? Set(to, nextKey, propValue, true).
                to->setThrowsException(state, ObjectPropertyName(state, nextKey), propValue, to);
            }
        }
    }
    return to;
}

static Value builtinObjectIs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 19.1.2.10 Object.is ( value1, value2 )

    // Return SameValue(value1, value2).
    return Value(argv[0].equalsToByTheSameValueAlgorithm(state, argv[1]));
}

static Value builtinObjectKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 19.1.2.16 Object.keys ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "key").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::Key);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

static Value builtinObjectValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 19.1.2.21 Object.values ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "value").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::Value);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

static Value builtinObjectEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 19.1.2.5 Object.entries ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "key+value").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::KeyAndValue);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

// Object.prototype.__defineGetter__ ( P, getter )
static Value builtinDefineGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // If IsCallable(getter) is false, throw a TypeError exception.
    if (!argv[1].isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, String::emptyString, true, state.context()->staticStrings().__defineGetter__.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    // Let desc be PropertyDescriptor{[[Get]]: getter, [[Enumerable]]: true, [[Configurable]]: true}.
    ObjectPropertyDescriptor desc(JSGetterSetter(argv[1].asObject(), Value(Value::EmptyValue)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));

    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

// Perform ? DefinePropertyOrThrow(O, key, desc).
#if defined(ENABLE_V8_LIKE_DEFINE_LOOKUP_GETTER_SETTER)
    O->defineOwnProperty(state, key, desc);
#else
    O->defineOwnPropertyThrowsException(state, key, desc);
#endif

    // Return undefined.
    return Value();
}

// Object.prototype.__defineSetter__ ( P, getter )
static Value builtinDefineSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // If IsCallable(getter) is false, throw a TypeError exception.
    if (!argv[1].isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, String::emptyString, true, state.context()->staticStrings().__defineSetter__.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    // Let desc be PropertyDescriptor{[[Get]]: getter, [[Enumerable]]: true, [[Configurable]]: true}.
    ObjectPropertyDescriptor desc(JSGetterSetter(Value(Value::EmptyValue), argv[1].asObject()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent));

    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

// Perform ? DefinePropertyOrThrow(O, key, desc).
#if defined(ENABLE_V8_LIKE_DEFINE_LOOKUP_GETTER_SETTER)
    O->defineOwnProperty(state, key, desc);
#else
    O->defineOwnPropertyThrowsException(state, key, desc);
#endif

    // Return undefined.
    return Value();
}

// Object.prototype.__lookupGetter__ ( P, getter )
static Value builtinLookupGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Repeat,
    while (O) {
        // Let desc be ? O.[[GetOwnProperty]](key).
        auto desc = O->getOwnProperty(state, key);

        // If desc is not undefined, then
        if (desc.hasValue()) {
            // If IsAccessorDescriptor(desc) is true, return desc.[[Get]].
            if (!desc.isDataProperty() && desc.jsGetterSetter()->hasGetter()) {
                return desc.jsGetterSetter()->getter();
            }
            // Return undefined.
            return Value();
        }
        // Set O to ? O.[[GetPrototypeOf]]().
        O = O->getPrototypeObject(state);
        // If O is null, return undefined.
    }
    return Value();
}

// Object.prototype.__lookupSetter__ ( P, getter )
static Value builtinLookupSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let key be ? ToPropertyKey(P).
    ObjectPropertyName key(state, argv[0]);

    // Repeat,
    while (O) {
        // Let desc be ? O.[[GetOwnProperty]](key).
        auto desc = O->getOwnProperty(state, key);

        // If desc is not undefined, then
        if (desc.hasValue()) {
            // If IsAccessorDescriptor(desc) is true, return desc.[[Set]].
            if (!desc.isDataProperty() && desc.jsGetterSetter()->hasSetter()) {
                return desc.jsGetterSetter()->setter();
            }
            // Return undefined.
            return Value();
        }
        // Set O to ? O.[[GetPrototypeOf]]().
        O = O->getPrototypeObject(state);
        // If O is null, return undefined.
    }
    return Value();
}

// https://tc39.es/ecma262/multipage/fundamental-objects.html#sec-object.groupby
static Value builtinObjectGroupBy(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let groups be ? GroupBy(items, callbackfn, PROPERTY).
    auto groups = IteratorObject::groupBy(state, argv[0], argv[1], IteratorObject::GroupByKeyCoercion::Property);
    // Let obj be OrdinaryObjectCreate(null).
    auto obj = new Object(state, Object::PrototypeIsNull);
    // For each Record { [[Key]], [[Elements]] } g of groups, do
    for (size_t i = 0; i < groups.size(); i++) {
        // Let elements be CreateArrayFromList(g.[[Elements]]).
        // Perform ! CreateDataPropertyOrThrow(obj, g.[[Key]], elements).
        obj->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, groups[i]->key),
                                              ObjectPropertyDescriptor(Object::createArrayFromList(state, groups[i]->elements), ObjectPropertyDescriptor::AllPresent));
    }
    // Return obj.
    return obj;
}

void GlobalObject::initializeObject(ExecutionState& state)
{
    // Object should be installed at the start time
    installObject(state);
}

void GlobalObject::installObject(ExecutionState& state)
{
    const StaticStrings& strings = state.context()->staticStrings();

    m_object = new NativeFunctionObject(state, NativeFunctionInfo(strings.Object, builtinObjectConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_object->setGlobalIntrinsicObject(state);
    m_object->setFunctionPrototype(state, m_objectPrototype);

    // $19.1.2.2 Object.create (O [,Properties])
    m_objectCreate = new NativeFunctionObject(state, NativeFunctionInfo(strings.create, builtinObjectCreate, 2, NativeFunctionInfo::Strict));
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.create),
                                      ObjectPropertyDescriptor(m_objectCreate,
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // 19.1.2.3 Object.defineProperties ( O, Properties )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.defineProperties),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.defineProperties, builtinObjectDefineProperties, 2, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.4 Object.defineProperty ( O, P, Attributes )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.defineProperty),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.defineProperty, builtinObjectDefineProperty, 3, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.5 Object.freeze ( O )
    m_objectFreeze = new NativeFunctionObject(state, NativeFunctionInfo(strings.freeze, builtinObjectFreeze, 1, NativeFunctionInfo::Strict));
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.freeze),
                                      ObjectPropertyDescriptor(m_objectFreeze,
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // 19.1.2.7 Object.fromEntries ( iterable )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.fromEntries),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.fromEntries, builtinObjectFromEntries, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyDescriptor),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyDescriptor, builtinObjectGetOwnPropertyDescriptor, 2, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.7 Object.getOwnPropertyNames ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyNames),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyNames, builtinObjectGetOwnPropertyNames, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertySymbols),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertySymbols, builtinObjectGetOwnPropertySymbols, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyDescriptors),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyDescriptors, builtinObjectGetOwnPropertyDescriptors, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.9 Object.getPrototypeOf ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.getPrototypeOf),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getPrototypeOf, builtinObjectGetPrototypeOf, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.11 Object.isExtensible ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.isExtensible),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isExtensible, builtinObjectIsExtensible, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.12 Object.isFrozen ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.isFrozen),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isFrozen, builtinObjectIsFrozen, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.13 Object.isSealed ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.isSealed),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isSealed, builtinObjectIsSealed, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.14 Object.keys ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.keys),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.keys, builtinObjectKeys, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.15 Object.preventExtensions ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.preventExtensions),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.preventExtensions, builtinObjectPreventExtensions, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.17 Object.seal ( O )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.seal),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.seal, builtinObjectSeal, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.20 Object.setPrototypeOf ( O, proto )
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.setPrototypeOf),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.setPrototypeOf, builtinObjectSetPrototypeOf, 2, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // ES6+ Object.assign
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.assign),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.assign, builtinObjectAssign, 2, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES6+ Object.is
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.is),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.is, builtinObjectIs, 2, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.constructor),
                                               ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.toString),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.valueOf),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.valueOf, builtinObjectValueOf, 0, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES2017 Object.values
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.values),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.values, builtinObjectValues, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // ES2017 Object.entries
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.entries),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.entries, builtinObjectEntries, 1, NativeFunctionInfo::Strict)),
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // Object.hasOwn
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.hasOwn),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.hasOwn, builtinObjectHasOwn, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // Object.groupBy
    m_object->directDefineOwnProperty(state, ObjectPropertyName(strings.groupBy),
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.groupBy, builtinObjectGroupBy, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.hasOwnProperty),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.hasOwnProperty, builtinObjectHasOwnProperty, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.isPrototypeOf(V)
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.isPrototypeOf),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isPrototypeOf, builtinObjectIsPrototypeOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.propertyIsEnumerable(V)
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.propertyIsEnumerable),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.propertyIsEnumerable, builtinObjectPropertyIsEnumerable, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.5 Object.prototype.toLocaleString()
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.toLocaleString),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.toLocaleString, builtinObjectToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto defineLookupGetterSetterNativeFunctionFlag =
#if defined(ENABLE_V8_LIKE_DEFINE_LOOKUP_GETTER_SETTER)
        0;
#else
        NativeFunctionInfo::Strict;
#endif
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.__defineGetter__),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings.__defineGetter__, builtinDefineGetter, 2, defineLookupGetterSetterNativeFunctionFlag)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.__defineSetter__),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings.__defineSetter__, builtinDefineSetter, 2, defineLookupGetterSetterNativeFunctionFlag)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.__lookupGetter__),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings.__lookupGetter__, builtinLookupGetter, 1, defineLookupGetterSetterNativeFunctionFlag)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.__lookupSetter__),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state,
                                                                                                 NativeFunctionInfo(strings.__lookupSetter__, builtinLookupSetter, 1, defineLookupGetterSetterNativeFunctionFlag)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(strings.get__proto__, builtinObject__proto__Getter, 0, NativeFunctionInfo::Strict)),
        new NativeFunctionObject(state, NativeFunctionInfo(strings.set__proto__, builtinObject__proto__Setter, 1, NativeFunctionInfo::Strict)));
    ObjectPropertyDescriptor __proto__desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_objectPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings.__proto__), __proto__desc);

    directDefineOwnProperty(state, ObjectPropertyName(strings.Object),
                            ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototypeToString = new NativeFunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, NativeFunctionInfo::Strict));
}
} // namespace Escargot
