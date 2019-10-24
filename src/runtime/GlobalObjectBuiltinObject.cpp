/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "ArrayObject.h"
#include "NativeFunctionObject.h"
#include "IteratorObject.h"

namespace Escargot {

typedef VectorWithInlineStorage<48, std::pair<ObjectPropertyName, ObjectPropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>>> ObjectPropertyVector;
typedef VectorWithInlineStorage<48, std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>> ObjectStructurePropertyVector;

static Value builtinObject__proto__Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return thisValue.toObject(state)->getPrototype(state);
}

static Value builtinObject__proto__Setter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value value = argv[0];
    Object* thisObject = thisValue.toObject(state);
    // Setting __proto__ to a non-object, non-null value is ignored
    if (!value.isObject() && !value.isNull()) {
        return Value();
    }
    if (!thisObject->setPrototype(state, value)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "can't set prototype of this object");
    }
    return Value();
}

static Value builtinObjectConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value value = argv[0];
    if (value.isUndefined() || value.isNull()) {
        return new Object(state);
    } else {
        return value.toObject(state);
    }
}

static Value builtinObjectValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(ret, Object, valueOf);
    return ret;
}

static Value builtinObjectPreventExtensions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return argv[0];
    }
    Object* o = argv[0].asObject();
    o->preventExtensions(state);
    return o;
}

static Value builtinObjectToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isUndefined()) {
        return new ASCIIString("[object Undefined]");
    } else if (thisValue.isNull()) {
        return new ASCIIString("[object Null]");
    }

    Object* thisObject = thisValue.toObject(state);

    StringBuilder builder;
    builder.appendString("[object ");

    if (thisObject->isArray(state)) {
        builder.appendString("Array");
    } else {
        Value toStringTag = thisObject->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag)).value(state, thisObject);
        if (toStringTag.isString()) {
            builder.appendString(toStringTag.asString());
        } else {
            builder.appendString(thisObject->internalClassProperty(state));
        }
    }

    builder.appendString("]");
    return AtomicString(state, builder.finalize()).string();
}

static Value builtinObjectHasOwnProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value key = argv[0].toPrimitive(state, Value::PrimitiveTypeHint::PreferString);
    Object* obj = thisValue.toObject(state);
    return Value(obj->hasOwnProperty(state, ObjectPropertyName(state, key)));
}

static Value objectDefineProperties(ExecutionState& state, Value object, Value properties)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!object.isObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Object.string(), false, strings->defineProperty.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    Object* props = properties.toObject(state);
    ObjectPropertyVector descriptors;
    props->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        auto propDesc = self->getOwnProperty(state, name);
        auto descriptors = (ObjectPropertyVector*)data;
        if (propDesc.hasValue() && desc.isEnumerable()) {
            Value propVal = propDesc.value(state, self);
            if (!propVal.isObject())
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().defineProperty.string(), errorMessage_GlobalObject_DescriptorNotObject);
            descriptors->push_back(std::make_pair(name, ObjectPropertyDescriptor(state, propVal.toObject(state))));
        }
        return true;
    },
                       &descriptors, false);
    for (auto it : descriptors) {
        object.asObject()->defineOwnPropertyThrowsException(state, it.first, it.second);
    }
    return object;
}

static Value builtinObjectCreate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject() && !argv[0].isNull())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().create.string(), errorMessage_GlobalObject_FirstArgumentNotObjectAndNotNull);
    Object* obj = new Object(state);
    if (argv[0].isNull())
        obj->setPrototype(state, Value(Value::Null));
    else
        obj->setPrototype(state, argv[0]);

    if (!argv[1].isUndefined())
        return objectDefineProperties(state, obj, argv[1]);
    return obj;
}

static Value builtinObjectDefineProperties(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return objectDefineProperties(state, argv[0], argv[1]);
}

static Value builtinObjectDefineProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Object.defineProperty ( O, P, Attributes )
    // If Type(O) is not Object, throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().defineProperty.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // Let key be ToPropertyKey(P).
    ObjectPropertyName key = ObjectPropertyName(state, argv[1]);

    // Let desc be ToPropertyDescriptor(Attributes).
    if (!argv[2].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Property description must be an object");
    }

    ObjectPropertyDescriptor desc(state, argv[2].asObject());

    O->defineOwnPropertyThrowsException(state, key, desc);
    return O;
}

static Value builtinObjectIsPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Object.prototype.isPrototypeOf (V)
    // If V is not an object, return false.
    if (!argv[0].isObject()) {
        return Value(false);
    }
    Value V = argv[0];

    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Object, isPrototypeOf);

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

static Value builtinObjectPropertyIsEnumerable(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let P be toPropertyKey(V).
    Value P = argv[0].toPropertyKey(state);

    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Object, propertyIsEnumerable);

    // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name.
    ObjectGetResult desc = O->getOwnProperty(state, ObjectPropertyName(state, P));

    // If desc is undefined, return false.
    if (!desc.hasValue())
        return Value(false);

    // Return the value of desc.[[Enumerable]].
    return Value(desc.isEnumerable());
}

static Value builtinObjectToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Object, propertyIsEnumerable);

    // Let toString be the result of calling the [[Get]] internal method of O passing "toString" as the argument.
    Value toString = O->get(state, ObjectPropertyName(state.context()->staticStrings().toString)).value(state, O);

    // If IsCallable(toString) is false, throw a TypeError exception.
    if (!toString.isCallable())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);

    // Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments.
    return Object::call(state, toString, Value(O), 0, nullptr);
}

static Value builtinObjectGetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return argv[0].toObject(state)->getPrototype(state);
}

static Value builtinObjectSetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-setprototypeof-v
    // 19.1.2.18 Object.setPrototypeOf ( O, proto )
    Value object = argv[0];
    Value proto = argv[1];

    // 1. Let O be RequireObjectCoercible(O).
    // 2. ReturnIfAbrupt(O).
    if (object.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    // 3. If Type(proto) is neither Object nor Null, throw a TypeError exception.
    if (!proto.isObject() && !proto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    // 4. If Type(O) is not Object, return O.
    if (!object.isObject()) {
        return object;
    }

    // 5. Let status be O.[[SetPrototypeOf]](proto).
    Object* obj = object.toObject(state);
    bool status = obj->setPrototype(state, proto);

    // 7. If status is false, throw a TypeError exception.
    if (!status) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "can't set prototype of this object");
        return Value();
    }

    return object;
}

static Value builtinObjectFreeze(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        // If Type(O) is not Object, return O. (ES6)
        return argv[0];
    }

    Object* O = argv[0].asObject();

    // For each named own property name P of O,
    ObjectStructurePropertyVector descriptors;
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        auto descriptors = (ObjectStructurePropertyVector*)data;
        descriptors->push_back(std::make_pair(P, desc));
        return true;
    },
                   &descriptors, false);
    for (const auto& it : descriptors) {
        const ObjectPropertyName& P = it.first;
        const ObjectStructurePropertyDescriptor& desc = it.second;

        // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        Object* newDesc = nullptr;

        // If IsDataDescriptor(desc) is true, then
        // If desc.[[Writable]] is true, set desc.[[Writable]] to false.
        if (desc.isDataProperty() && desc.isWritable()) {
            newDesc = O->getOwnProperty(state, P).toPropertyDescriptor(state, O).asObject();
            newDesc->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().writable), Value(false), newDesc);
        }
        // If desc.[[Configurable]] is true, set desc.[[Configurable]] to false.
        if (desc.isConfigurable()) {
            if (!newDesc) {
                newDesc = O->getOwnProperty(state, P).toPropertyDescriptor(state, O).asObject();
            }
            newDesc->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().configurable), Value(false), newDesc);
        }
        // Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments.
        if (newDesc) {
            O->defineOwnProperty(state, P, ObjectPropertyDescriptor(state, newDesc));
        }
    }

    // Set the [[Extensible]] internal property of O to false.
    O->preventExtensions(state);
    // Return O.
    return O;
}

static Value builtinObjectFromEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argv[0].isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().object.string(), true, state.context()->staticStrings().fromEntries.string(), errorMessage_GlobalObject_ThisUndefinedOrNull);
    }
    Value iterable = argv[0];
    Object* obj = new Object(state);

    Value iteratorRecord = IteratorObject::getIterator(state, iterable);
    while (true) {
        Value next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (next.isFalse()) {
            return obj;
        }

        Value nextItem = IteratorObject::iteratorValue(state, next);
        if (!nextItem.isObject()) {
            TypeErrorObject* errorobj = new TypeErrorObject(state, new ASCIIString("TypeError"));
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

static Value builtinObjectGetOwnPropertyDescriptor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Object.getOwnPropertyDescriptor ( O, P )

    // Let obj be ToObject(O).
    Object* O = argv[0].toObject(state);

    // Let name be ToString(P).
    Value name = argv[1];

    // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name.
    ObjectGetResult desc = O->getOwnProperty(state, ObjectPropertyName(state, name));

    // Return the result of calling FromPropertyDescriptor(desc) (8.10.4).
    return desc.toPropertyDescriptor(state, O);
}
// 19.1.2.9Object.getOwnPropertyDescriptors ( O )
//https://www.ecma-international.org/ecma-262/8.0/#sec-object.getownpropertydescriptors
static Value builtinObjectGetOwnPropertyDescriptors(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Object* obj = argv[0].toObject(state);
    auto ownKeys = obj->ownPropertyKeys(state);
    Object* descriptors = new Object(state);

    for (uint64_t i = 0; i < ownKeys.size(); i++) {
        ObjectGetResult desc = obj->getOwnProperty(state, ObjectPropertyName(state, ownKeys[i]));
        Value descriptor = desc.toPropertyDescriptor(state, obj);
        if (!descriptor.isUndefined()) {
            descriptors->defineOwnProperty(state, ObjectPropertyName(state, ownKeys[i]), ObjectPropertyDescriptor(descriptor, ObjectPropertyDescriptor::AllPresent));
        }
    }
    return descriptors;
}

enum class GetOwnPropertyKeysType {
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

static Value builtinObjectGetOwnPropertyNames(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-object.getownpropertynames
    Object* O = argv[0].toObject(state);
    return getOwnPropertyKeys(state, O, GetOwnPropertyKeysType::String);
}

static Value builtinObjectGetOwnPropertySymbols(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-object.getownpropertysymbols
    Object* O = argv[0].toObject(state);
    return getOwnPropertyKeys(state, O, GetOwnPropertyKeysType::Symbol);
}

static Value builtinObjectIsExtensible(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return Value(Value::False);
    }
    // Return the Boolean value of the [[Extensible]] internal property of O.
    return Value(argv[0].asObject()->isExtensible(state));
}

static Value builtinObjectIsFrozen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return Value(Value::True);
    }
    Object* O = argv[0].asObject();

    bool hasNonFrozenProperty = false;
    // For each named own property name P of O,
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        if ((desc.isDataProperty() && desc.isWritable()) || desc.isConfigurable()) {
            bool* hasNonFrozenProperty = (bool*)data;
            *hasNonFrozenProperty = true;
            return false;
        }
        return true;
    },
                   &hasNonFrozenProperty, false);
    if (hasNonFrozenProperty)
        return Value(false);

    // If the [[Extensible]] internal property of O is false, then return true.
    if (!O->isExtensible(state))
        return Value(true);

    // Otherwise, return false.
    return Value(false);
}

static Value builtinObjectIsSealed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return Value(Value::True);
    }
    Object* O = argv[0].asObject();

    bool hasNonSealedProperty = false;
    // For each named own property name P of O,
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        // If desc.[[Configurable]] is true, then return false.
        if (desc.isConfigurable()) {
            bool* hasNonSealedProperty = (bool*)data;
            *hasNonSealedProperty = true;
            return false;
        }
        return true;
    },
                   &hasNonSealedProperty, false);
    if (hasNonSealedProperty)
        return Value(false);

    // If the [[Extensible]] internal property of O is false, then return true.
    if (!O->isExtensible(state))
        return Value(true);

    // Otherwise, return false.
    return Value(false);
}

static Value builtinObjectSeal(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject()) {
        return argv[0];
    }
    Object* O = argv[0].asObject();

    // For each named own property name P of O,
    ObjectStructurePropertyVector descriptors;
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        auto descriptors = (ObjectStructurePropertyVector*)data;
        descriptors->push_back(std::make_pair(P, desc));
        return true;
    },
                   &descriptors, false);
    for (const auto& it : descriptors) {
        const ObjectPropertyName& P = it.first;
        const ObjectStructurePropertyDescriptor& desc = it.second;

        // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        // If desc.[[Configurable]] is true, set desc.[[Configurable]] to false.
        // Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments.
        if (desc.isConfigurable()) {
            Object* newDesc = O->getOwnProperty(state, P).toPropertyDescriptor(state, O).asObject();
            newDesc->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().configurable), Value(false), newDesc);
            O->defineOwnProperty(state, P, ObjectPropertyDescriptor(state, newDesc));
        }
    }

    // Set the [[Extensible]] internal property of O to false.
    O->preventExtensions(state);
    // Return O.
    return O;
}

static Value builtinObjectAssign(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

static Value builtinObjectIs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 19.1.2.10 Object.is ( value1, value2 )

    // Return SameValue(value1, value2).
    return Value(argv[0].equalsToByTheSameValueAlgorithm(state, argv[1]));
}

static Value builtinObjectKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 19.1.2.16 Object.keys ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "key").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::Key);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

static Value builtinObjectValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 19.1.2.21 Object.values ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "value").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::Value);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

static Value builtinObjectEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // 19.1.2.5 Object.entries ( O )

    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "key+value").
    auto nameList = Object::enumerableOwnProperties(state, obj, EnumerableOwnPropertiesType::KeyAndValue);
    // Return CreateArrayFromList(nameList).
    return Object::createArrayFromList(state, nameList.size(), nameList.data());
}

void GlobalObject::installObject(ExecutionState& state)
{
    const StaticStrings& strings = state.context()->staticStrings();

    FunctionObject* emptyFunction = m_functionPrototype;
    m_object = new NativeFunctionObject(state, NativeFunctionInfo(strings.Object, builtinObjectConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_object->markThisObjectDontNeedStructureTransitionTable();
    m_object->setPrototype(state, emptyFunction);
    m_object->setFunctionPrototype(state, m_objectPrototype);
    // $19.1.2.2 Object.create (O [,Properties])
    m_objectCreate = new NativeFunctionObject(state, NativeFunctionInfo(strings.create, builtinObjectCreate, 2, NativeFunctionInfo::Strict));
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.create),
                                ObjectPropertyDescriptor(m_objectCreate,
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // 19.1.2.3 Object.defineProperties ( O, Properties )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.defineProperties),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.defineProperties, builtinObjectDefineProperties, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.4 Object.defineProperty ( O, P, Attributes )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.defineProperty),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.defineProperty, builtinObjectDefineProperty, 3, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.5 Object.freeze ( O )
    m_objectFreeze = new NativeFunctionObject(state, NativeFunctionInfo(strings.freeze, builtinObjectFreeze, 1, NativeFunctionInfo::Strict));
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.freeze),
                                ObjectPropertyDescriptor(m_objectFreeze,
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // 19.1.2.7 Object.fromEntries ( iterable )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.fromEntries),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.fromEntries, builtinObjectFromEntries, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyDescriptor),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyDescriptor, builtinObjectGetOwnPropertyDescriptor, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.7 Object.getOwnPropertyNames ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyNames),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyNames, builtinObjectGetOwnPropertyNames, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertySymbols),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertySymbols, builtinObjectGetOwnPropertySymbols, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyDescriptors),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyDescriptors, builtinObjectGetOwnPropertyDescriptors, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.9 Object.getPrototypeOf ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getPrototypeOf),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.getPrototypeOf, builtinObjectGetPrototypeOf, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.11 Object.isExtensible ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isExtensible),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isExtensible, builtinObjectIsExtensible, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.12 Object.isFrozen ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isFrozen),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isFrozen, builtinObjectIsFrozen, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.13 Object.isSealed ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isSealed),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isSealed, builtinObjectIsSealed, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.14 Object.keys ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.keys),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.keys, builtinObjectKeys, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.15 Object.preventExtensions ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.preventExtensions),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.preventExtensions, builtinObjectPreventExtensions, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.17 Object.seal ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.seal),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.seal, builtinObjectSeal, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.20 Object.setPrototypeOf ( O, proto )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.setPrototypeOf),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.setPrototypeOf, builtinObjectSetPrototypeOf, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // ES6+ Object.assign
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.assign),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.assign, builtinObjectAssign, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES6+ Object.is
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.is),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.is, builtinObjectIs, 2, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.constructor),
                                         ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.toString),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.valueOf),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.valueOf, builtinObjectValueOf, 0, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES2017 Object.values
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.values),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.values, builtinObjectValues, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // ES2017 Object.entries
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.entries),
                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.entries, builtinObjectEntries, 1, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.hasOwnProperty),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.hasOwnProperty, builtinObjectHasOwnProperty, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.isPrototypeOf(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.isPrototypeOf),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.isPrototypeOf, builtinObjectIsPrototypeOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.propertyIsEnumerable(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.propertyIsEnumerable),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.propertyIsEnumerable, builtinObjectPropertyIsEnumerable, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.5 Object.prototype.toLocaleString()
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.toLocaleString),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings.toLocaleString, builtinObjectToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.propertyIsEnumerable(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().propertyIsEnumerable),
                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().propertyIsEnumerable, builtinObjectPropertyIsEnumerable, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    JSGetterSetter gs(
        new NativeFunctionObject(state, NativeFunctionInfo(strings.get__proto__, builtinObject__proto__Getter, 0, NativeFunctionInfo::Strict)),
        new NativeFunctionObject(state, NativeFunctionInfo(strings.set__proto__, builtinObject__proto__Setter, 1, NativeFunctionInfo::Strict)));
    ObjectPropertyDescriptor __proto__desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.__proto__), __proto__desc);
    defineOwnProperty(state, ObjectPropertyName(strings.Object),
                      ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototypeToString = new NativeFunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, NativeFunctionInfo::Strict));
}
}
