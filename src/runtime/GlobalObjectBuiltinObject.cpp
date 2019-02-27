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

namespace Escargot {

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
    thisObject->setPrototype(state, value);
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
    // If Type(O) is not Object, throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().defineProperty.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* o = argv[0].asObject();
    o->preventExtensions(state);
    return o;
}

static Value builtinObjectToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isUndefined()) {
        return new Char8String("[object Undefined]");
    } else if (thisValue.isNull()) {
        return new Char8String("[object Null]");
    }

    Object* thisObject = thisValue.toObject(state);
    StringBuilder builder;
    builder.appendString("[object ");
    Value toStringTag = thisObject->get(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().toStringTag)).value(state, thisObject);
    if (toStringTag.isString()) {
        builder.appendString(toStringTag.asString());
    } else {
        builder.appendString(thisObject->internalClassProperty());
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
    std::vector<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>, gc_allocator_ignore_off_page<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>>> descriptors;
    props->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        auto propDesc = self->getOwnProperty(state, name);
        std::vector<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>, gc_allocator_ignore_off_page<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>>>* descriptors = (std::vector<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>, gc_allocator_ignore_off_page<std::pair<ObjectPropertyName, ObjectPropertyDescriptor>>>*)data;
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
    if (!toString.isFunction())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);

    // Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments.
    return FunctionObject::call(state, toString, Value(O), 0, nullptr);
}

static Value builtinObjectGetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Object.getPrototypeOf ( O )
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().getPrototypeOf.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    return argv[0].asObject()->getPrototype(state);
}

static Value builtinObjectSetPrototypeOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value object = argv[0];
    Value proto = argv[1];

    // 1. Let O be ? RequireObjectCoercible(O).
    if (object.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    // 2. If Type(proto) is neither Object nor Null, throw a TypeError exception.
    if (!proto.isObject() && !proto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().setPrototypeOf.string(), "");
        return Value();
    }

    Object* obj = object.toObject(state);
    obj->setPrototype(state, proto);

    return object;
}

static Value builtinObjectFreeze(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
#if defined(ESCARGOT_ENABLE_VENDORTEST) || defined(ESCARGOT_SHELL)
    // If Type(O) is not Object throw a TypeError exception. (ES5)
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().freeze.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
#else
    // If Type(O) is not Object, return O. (ES6)
    if (!argv[0].isObject()) {
        return argv[0];
    }
#endif

    Object* O = argv[0].asObject();
    //O->markThisObjectDontNeedStructureTransitionTable(state);

    // For each named own property name P of O,
    std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>> descriptors;
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>* descriptors = (std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>*)data;
        descriptors->push_back(std::make_pair(P, desc));
        return true;
    },
                   &descriptors);
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

static Value builtinObjectGetOwnPropertyDescriptor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Object.getOwnPropertyDescriptor ( O, P )
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().getOwnPropertyDescriptor.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // Let name be ToString(P).
    Value name = argv[1];

    // Let desc be the result of calling the [[GetOwnProperty]] internal method of O with argument name.
    ObjectGetResult desc = O->getOwnProperty(state, ObjectPropertyName(state, name));

    // Return the result of calling FromPropertyDescriptor(desc) (8.10.4).
    return desc.toPropertyDescriptor(state, O);
}

static Value builtinObjectGetOwnPropertyNames(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().getOwnPropertyNames.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // Let array be the result of creating a new object as if by the expression new Array () where Array is the standard built-in constructor with that name.
    ArrayObject* array = new ArrayObject(state);

    // Let n be 0.
    size_t n = 0;
    struct Data {
        ArrayObject* array;
        size_t* n;
    } data;
    data.array = array;
    data.n = &n;
    // For each named own property P of O
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        Data* a = (Data*)data;
        // Let name be the String value that is the name of P.
        Value name = P.toPlainValue(state).toString(state);
        // Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        a->array->defineOwnProperty(state, ObjectPropertyName(state, Value((*a->n))), ObjectPropertyDescriptor(name, ObjectPropertyDescriptor::AllPresent));
        // Increment n by 1.
        (*a->n)++;
        return true;
    },
                   &data);

    // Return array.
    return array;
}

static Value builtinObjectGetOwnPropertySymbols(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Object* O = argv[0].toObject(state);

    // Let array be the result of creating a new object as if by the expression new Array () where Array is the standard built-in constructor with that name.
    ArrayObject* array = new ArrayObject(state);

    // Let n be 0.
    size_t n = 0;
    struct Data {
        ArrayObject* array;
        size_t* n;
    } data;
    data.array = array;
    data.n = &n;
    // For each named own property P of O
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        Data* a = (Data*)data;
        // Let name be the String value that is the name of P.
        Value name = P.toPlainValue(state);
        if (name.isSymbol()) {
            // Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            a->array->defineOwnProperty(state, ObjectPropertyName(state, Value((*a->n))), ObjectPropertyDescriptor(name, ObjectPropertyDescriptor::AllPresent));
            // Increment n by 1.
            (*a->n)++;
        }
        return true;
    },
                   &data, false);

    // Return array.
    return array;
}

static Value builtinObjectIsExtensible(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().seal.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    // Return the Boolean value of the [[Extensible]] internal property of O.
    return Value(argv[0].asObject()->isExtensible(state));
}

static Value builtinObjectIsFrozen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().seal.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
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
                   &hasNonFrozenProperty);
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
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().seal.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
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
                   &hasNonSealedProperty);
    if (hasNonSealedProperty)
        return Value(false);

    // If the [[Extensible]] internal property of O is false, then return true.
    if (!O->isExtensible(state))
        return Value(true);

    // Otherwise, return false.
    return Value(false);
}

static Value builtinObjectKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().getOwnPropertyNames.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // Let array be the result of creating a new object as if by the expression new Array(n) where Array is the standard built-in constructor with that name.
    ArrayObject* array = new ArrayObject(state);

    // Let index be 0.
    size_t index = 0;

    struct Data {
        ArrayObject* array;
        size_t* index;
    } data;
    data.array = array;
    data.index = &index;

    // For each own enumerable property of O whose name String is P
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        if (desc.isEnumerable()) {
            Data* aData = (Data*)data;
            // Call the [[DefineOwnProperty]] internal method of array with arguments ToString(index), the PropertyDescriptor {[[Value]]: P, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            aData->array->defineOwnProperty(state, ObjectPropertyName(state, Value(*aData->index)), ObjectPropertyDescriptor(Value(P.toPlainValue(state).toString(state)), ObjectPropertyDescriptor::AllPresent));
            // Increment index by 1.
            (*aData->index)++;
        }
        return true;
    },
                   &data);

    // Return array.
    return array;
}

static Value builtinObjectSeal(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // If Type(O) is not Object throw a TypeError exception.
    if (!argv[0].isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().seal.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    }
    Object* O = argv[0].asObject();

    // For each named own property name P of O,
    std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>> descriptors;
    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>>* descriptors = (std::vector<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<ObjectPropertyName, ObjectStructurePropertyDescriptor>>>*)data;
        descriptors->push_back(std::make_pair(P, desc));
        return true;
    },
                   &descriptors);
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
        ValueVector keys;
        Object* from = nullptr;
        if (nextSource.isUndefinedOrNull()) {
        } else {
            // Let from be ! ToObject(nextSource).
            from = nextSource.toObject(state);
            // Let keys be ? from.[[OwnPropertyKeys]]().
            keys = from->getOwnPropertyKeys(state);
        }

        // For each element nextKey of keys in List order, do
        for (size_t i = 0; i < keys.size(); i++) {
            Value nextKey = keys[i];
            // Let desc be ? from.[[GetOwnProperty]](nextKey).
            auto desc = from->getOwnProperty(state, ObjectPropertyName(state, nextKey));
            // If desc is not undefined and desc.[[Enumerable]] is true, then
            if (desc.hasValue() && desc.isEnumerable()) {
                // Let propValue be ? Get(from, nextKey).
                Value propValue = desc.value(state, from);
                // Perform ? Set(to, nextKey, propValue, true).
                to->setThrowsException(state, ObjectPropertyName(state, nextKey), propValue, to);
            }
        }
    }
    return to;
}

static ArrayObject* createArrayFromList(ExecutionState& state, ValueVector& elements)
{
    // Let array be ! ArrayCreate(0).
    // Let n be 0.
    // For each element e of elements, do
    // Let status be CreateDataProperty(array, ! ToString(n), e).
    // Assert: status is true.
    // Increment n by 1.
    // Return array.
    ArrayObject* array = new ArrayObject(state);
    for (size_t n = 0; n < elements.size(); n++) {
        array->defineOwnProperty(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(elements[n], ObjectPropertyDescriptor::AllPresent));
    }
    return array;
}

enum EnumerableOwnPropertiesType {
    EnumerableOwnPropertiesTypeKey,
    EnumerableOwnPropertiesTypeValue,
    EnumerableOwnPropertiesTypeKeyValue
};

// https://www.ecma-international.org/ecma-262/8.0/#sec-enumerableownproperties
static ValueVector enumerableOwnProperties(ExecutionState& state, Object* O, EnumerableOwnPropertiesType kind)
{
    ValueVector properties;
    // Let ownKeys be ? O.[[OwnPropertyKeys]]().
    // Let properties be a new empty List.
    // For each element key of ownKeys in List order, do
    struct Data {
        ValueVector& properties;
        EnumerableOwnPropertiesType kind;
        Data(ValueVector& properties, EnumerableOwnPropertiesType kind)
            : properties(properties)
            , kind(kind)
        {
        }
    };
    Data sender(properties, kind);

    double k = 0;

    // Order the elements of properties so they are in the same relative order as would be produced by the Iterator that would be returned
    // if the EnumerateObjectProperties internal method were invoked with O.
    while (true) {
        // Let exists be the result of calling the [[HasProperty]] internal method of E with P.
        ObjectGetResult exists = O->getIndexedProperty(state, Value(k));
        if (exists.hasValue() && exists.isEnumerable()) {
            // If kind is "key", append key to properties.
            if (sender.kind == EnumerableOwnPropertiesType::EnumerableOwnPropertiesTypeKey) {
                sender.properties.push_back(Value(k));
            } else {
                // Else,
                // Let value be ? Get(O, key).
                Value value = exists.value(state, O);
                // If kind is "value", append value to properties.
                if (sender.kind == EnumerableOwnPropertiesType::EnumerableOwnPropertiesTypeValue) {
                    sender.properties.push_back(value);
                } else {
                    // Else,
                    // Assert: kind is "key+value".
                    // Let entry be CreateArrayFromList(« key, value »).
                    ArrayObject* entry = new ArrayObject(state);
                    entry->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(Value(k), ObjectPropertyDescriptor::AllPresent));
                    entry->defineOwnProperty(state, ObjectPropertyName(state, Value(1)), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
                    // Append entry to properties.
                    sender.properties.push_back(entry);
                }
            }
            k++;
        } else {
            Object::nextIndexForward(state, O, k, std::numeric_limits<double>::max(), false, k);
            if (k == std::numeric_limits<double>::max()) {
                break;
            }
        }
    }

    O->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& keyName, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        Data* d = (Data*)data;
        Value vv = keyName.toPlainValue(state);
        if (vv.toArrayIndex(state) != Value::InvalidArrayIndexValue) {
            return true;
        }
        // If Type(key) is String, then
        // Let desc be ? O.[[GetOwnProperty]](key).
        // If desc is not undefined and desc.[[Enumerable]] is true, then
        if (desc.isEnumerable()) {
            // If kind is "key", append key to properties.
            if (d->kind == EnumerableOwnPropertiesType::EnumerableOwnPropertiesTypeKey) {
                d->properties.push_back(keyName.toPlainValue(state));
            } else {
                // Else,
                // Let value be ? Get(O, key).
                Value value = self->get(state, keyName).value(state, self);
                // If kind is "value", append value to properties.
                if (d->kind == EnumerableOwnPropertiesType::EnumerableOwnPropertiesTypeValue) {
                    d->properties.push_back(value);
                } else {
                    // Else,
                    // Assert: kind is "key+value".
                    // Let entry be CreateArrayFromList(« key, value »).
                    ArrayObject* entry = new ArrayObject(state);
                    entry->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(keyName.toPlainValue(state), ObjectPropertyDescriptor::AllPresent));
                    entry->defineOwnProperty(state, ObjectPropertyName(state, Value(1)), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
                    // Append entry to properties.
                    d->properties.push_back(entry);
                }
            }
        }
        return true;
    },
                   &sender, true);

    // Return properties.
    return properties;
}

static Value builtinObjectValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "value").
    auto nameList = enumerableOwnProperties(state, obj, EnumerableOwnPropertiesTypeValue);
    // Return CreateArrayFromList(nameList).
    return createArrayFromList(state, nameList);
}

static Value builtinObjectEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let obj be ? ToObject(O).
    Object* obj = argv[0].toObject(state);
    // Let nameList be ? EnumerableOwnProperties(obj, "key+value").
    auto nameList = enumerableOwnProperties(state, obj, EnumerableOwnPropertiesTypeKeyValue);
    // Return CreateArrayFromList(nameList).
    return createArrayFromList(state, nameList);
}

void GlobalObject::installObject(ExecutionState& state)
{
    const StaticStrings& strings = state.context()->staticStrings();

    FunctionObject* emptyFunction = m_functionPrototype;
    m_object = new FunctionObject(state, NativeFunctionInfo(strings.Object, builtinObjectConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                      return new Object(state);
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_object->markThisObjectDontNeedStructureTransitionTable(state);
    m_object->setPrototype(state, emptyFunction);
    m_object->setFunctionPrototype(state, m_objectPrototype);
    // $19.1.2.2 Object.create (O [,Properties])
    m_objectCreate = new FunctionObject(state, NativeFunctionInfo(strings.create, builtinObjectCreate, 2, nullptr, NativeFunctionInfo::Strict));
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.create),
                                ObjectPropertyDescriptor(m_objectCreate,
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // 19.1.2.3 Object.defineProperties ( O, Properties )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.defineProperties),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.defineProperties, builtinObjectDefineProperties, 2, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.4 Object.defineProperty ( O, P, Attributes )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.defineProperty),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.defineProperty, builtinObjectDefineProperty, 3, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.5 Object.freeze ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.freeze),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.freeze, builtinObjectFreeze, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.6 Object.getOwnPropertyDescriptor ( O, P )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyDescriptor),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyDescriptor, builtinObjectGetOwnPropertyDescriptor, 2, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.7 Object.getOwnPropertyNames ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertyNames),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.getOwnPropertyNames, builtinObjectGetOwnPropertyNames, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getOwnPropertySymbols),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.getOwnPropertySymbols, builtinObjectGetOwnPropertySymbols, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.9 Object.getPrototypeOf ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.getPrototypeOf),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.getPrototypeOf, builtinObjectGetPrototypeOf, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.11 Object.isExtensible ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isExtensible),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.isExtensible, builtinObjectIsExtensible, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.12 Object.isFrozen ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isFrozen),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.isFrozen, builtinObjectIsFrozen, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.13 Object.isSealed ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.isSealed),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.isSealed, builtinObjectIsSealed, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.14 Object.keys ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.keys),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.keys, builtinObjectKeys, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.15 Object.preventExtensions ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.preventExtensions),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.preventExtensions, builtinObjectPreventExtensions, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.17 Object.seal ( O )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.seal),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.seal, builtinObjectSeal, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 19.1.2.20 Object.setPrototypeOf ( O, proto )
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.setPrototypeOf),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.setPrototypeOf, builtinObjectSetPrototypeOf, 2, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // ES6+ Object.assign
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.assign),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.assign, builtinObjectAssign, 2, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.constructor),
                                         ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.toString),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.valueOf),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.valueOf, builtinObjectValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // ES2017 Object.values
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.values),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.values, builtinObjectValues, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // ES2017 Object.entries
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.entries),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.entries, builtinObjectEntries, 1, nullptr, NativeFunctionInfo::Strict)),
                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.hasOwnProperty),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.hasOwnProperty, builtinObjectHasOwnProperty, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.isPrototypeOf(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.isPrototypeOf),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.isPrototypeOf, builtinObjectIsPrototypeOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.propertyIsEnumerable(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.propertyIsEnumerable),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.propertyIsEnumerable, builtinObjectPropertyIsEnumerable, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.5 Object.prototype.toLocaleString()
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.toLocaleString),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.toLocaleString, builtinObjectToLocaleString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.4 Object.prototype.propertyIsEnumerable(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().propertyIsEnumerable),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().propertyIsEnumerable, builtinObjectPropertyIsEnumerable, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    JSGetterSetter gs(
        new FunctionObject(state, NativeFunctionInfo(strings.get__proto__, builtinObject__proto__Getter, 0, nullptr, NativeFunctionInfo::Strict)),
        new FunctionObject(state, NativeFunctionInfo(strings.set__proto__, builtinObject__proto__Setter, 1, nullptr, NativeFunctionInfo::Strict)));
    ObjectPropertyDescriptor __proto__desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.__proto__), __proto__desc);
    defineOwnProperty(state, ObjectPropertyName(strings.Object),
                      ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototypeToString = new FunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, nullptr, NativeFunctionInfo::Strict));
}
}
