#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ArrayObject.h"

namespace Escargot {

static Value builtinObject__proto__Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return thisValue.asObject()->getPrototype(state);
}

static Value builtinObject__proto__Setter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    thisValue.asObject()->setPrototype(state, argv[0]);
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
    Object* o = argv[0].toObject(state);
    o->preventExtensions();
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
    builder.appendString(thisObject->internalClassProperty());
    builder.appendString("]");
    return AtomicString(state, builder.finalize()).string();
}

static Value builtinObjectHasOwnProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    String* keyString = argv[0].toPrimitive(state, Value::PrimitiveTypeHint::PreferString).toString(state);
    Object* obj = thisValue.toObject(state);
    return Value(obj->hasOwnProperty(state, ObjectPropertyName(state, keyString)));
}

inline Value objectDefineProperties(ExecutionState& state, Value object, Value properties)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!object.isObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Object.string(), false, strings->defineProperty.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    Object* props = properties.toObject(state);
    std::vector<std::pair<ObjectPropertyName, ObjectPropertyDescriptor> > descriptors;
    props->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) -> bool {
        auto propDesc = props->getOwnProperty(state, name);
        if (propDesc.hasValue() && desc.isEnumerable()) {
            Value propVal = propDesc.value(state, props);
            if (!propVal.isObject())
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Object.string(), false, strings->defineProperty.string(), errorMessage_GlobalObject_DescriptorNotObject);
            descriptors.push_back(std::make_pair(name, ObjectPropertyDescriptor(state, propVal.toObject(state))));
        }
        return true;
    });
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
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
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
    // Let P be ToString(V).
    Value P = argv[0].toString(state);

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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLoacleStringNotCallable);

    // Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments.
    return FunctionObject::call(toString, state, Value(O), 0, nullptr);
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

static Value builtinObjectFreeze(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
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
    Value name = argv[1].toString(state);

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
    // For each named own property P of O
    O->enumeration(state, [&](const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc) -> bool {
        // Let name be the String value that is the name of P.
        Value name = P.string(state);
        // Call the [[DefineOwnProperty]] internal method of array with arguments ToString(n), the PropertyDescriptor {[[Value]]: name, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        array->defineOwnProperty(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(name, ObjectPropertyDescriptor::AllPresent));
        // Increment n by 1.
        n++;
        return true;
    });

    // Return array.
    return array;
}

static Value builtinObjectIsExtensible(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinObjectIsFrozen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinObjectIsSealed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinObjectKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinObjectSeal(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

void GlobalObject::installObject(ExecutionState& state)
{
    const StaticStrings& strings = state.context()->staticStrings();

    FunctionObject* emptyFunction = m_functionPrototype;
    m_object = new FunctionObject(state, NativeFunctionInfo(strings.Object, builtinObjectConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new Object(state);
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_object->markThisObjectDontNeedStructureTransitionTable(state);
    m_object->setPrototype(state, emptyFunction);
    m_object->setFunctionPrototype(state, m_objectPrototype);
    // $19.1.2.2 Object.create (O [,Properties])
    m_object->defineOwnProperty(state, ObjectPropertyName(strings.create),
                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.create, builtinObjectCreate, 2, nullptr, NativeFunctionInfo::Strict)),
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

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.constructor),
                                         ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.toString),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.valueOf),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.valueOf, builtinObjectValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.hasOwnProperty),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings.toString, builtinObjectHasOwnProperty, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

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
        new FunctionObject(state, NativeFunctionInfo(strings.__proto__, builtinObject__proto__Getter, 0, nullptr, NativeFunctionInfo::Strict)),
        new FunctionObject(state, NativeFunctionInfo(strings.__proto__, builtinObject__proto__Setter, 1, nullptr, NativeFunctionInfo::Strict)));
    ObjectPropertyDescriptor __proto__desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(strings.__proto__), __proto__desc);
    defineOwnProperty(state, ObjectPropertyName(strings.Object),
                      ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
