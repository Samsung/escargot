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
#include "Object.h"
#include "Context.h"
#include "VMInstance.h"
#include "ErrorObject.h"
#include "FunctionObject.h"
#include "ArrayObject.h"
#include "BoundFunctionObject.h"
#include "StringObject.h"
#include "NumberObject.h"
#include "BooleanObject.h"
#include "SymbolObject.h"
#include "BigIntObject.h"
#include "ProxyObject.h"
#include "PrototypeObject.h"
#include "ScriptClassConstructorFunctionObject.h"

#include "Global.h"

namespace Escargot {

COMPILE_ASSERT(OBJECT_PROPERTY_NAME_UINT32_VIAS == (OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS << 1), "");
COMPILE_ASSERT(OBJECT_PROPERTY_NAME_UINT32_VIAS <= (1 << 3), "");

ObjectStructurePropertyName::ObjectStructurePropertyName()
    : m_data(((size_t)AtomicString().string()) | OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS)
{
}

ObjectStructurePropertyName::ObjectStructurePropertyName(const Value& value)
{
    // accept string or symbol value only
    ASSERT(value.isString() || value.isSymbol());
    if (value.isString()) {
        m_data = reinterpret_cast<size_t>(value.asString());
    } else {
        m_data = reinterpret_cast<size_t>(value.asSymbol());
    }
}

ObjectStructurePropertyName::ObjectStructurePropertyName(ExecutionState& state, const Value& valueIn)
{
    Value value = valueIn.toPrimitive(state, Value::PreferString);
    RETURN_IF_PENDING_EXCEPTION
    if (UNLIKELY(value.isSymbol())) {
        m_data = (size_t)value.asSymbol();
        return;
    }

    String* string = value.toString(state);
    size_t v = string->getTypeTag();
    if (v > POINTER_VALUE_STRING_TAG_IN_DATA) {
        ASSERT(v == ((v & ~POINTER_VALUE_STRING_TAG_IN_DATA) | OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS));
        m_data = v;
        return;
    }

    const auto& data = string->bufferAccessData();
    if (data.length == 0) {
        m_data = ((size_t)AtomicString().string()) | OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS;
        return;
    }
    bool needsRemainNormalString = false;
    char16_t c = data.charAt(0);
    if ((c == '.' || (c >= '0' && c <= '9')) && data.length > 16) {
        needsRemainNormalString = true;
    }

    if (UNLIKELY(needsRemainNormalString)) {
        m_data = (size_t)string;
    } else {
        if (c < ESCARGOT_ASCII_TABLE_MAX && (data.length == 1)) {
            m_data = ((size_t)state.context()->staticStrings().asciiTable[c].string()) | OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS;
        } else {
            m_data = ((size_t)AtomicString(state, string).string()) | OBJECT_PROPERTY_NAME_ATOMIC_STRING_VIAS;
        }
    }
}

ObjectStructurePropertyName ObjectPropertyName::toObjectStructurePropertyNameUintCase(ExecutionState& state) const
{
    ASSERT(isUIntType());

    auto uint = uintValue();
    if (uint < ESCARGOT_STRINGS_NUMBERS_MAX) {
        return ObjectStructurePropertyName(state.context()->staticStrings().numbers[uint]);
    }

    return ObjectStructurePropertyName(state, String::fromDouble(uint));
}

ObjectRareData::ObjectRareData(Object* obj)
    : m_isExtensible(true)
    , m_isEverSetAsPrototypeObject(false)
    , m_isArrayObjectLengthWritable(true)
    , m_isSpreadArrayObject(false)
    , m_isFinalizerRegistered(false)
    , m_isInlineCacheable(true)
    , m_hasExtendedExtraData(false)
#if defined(ESCARGOT_ENABLE_TEST)
    , m_isHTMLDDA(false)
#endif
    , m_arrayObjectFastModeBufferExpandCount(0)
    , m_extraData(nullptr)
    , m_prototype(obj ? obj->m_prototype : nullptr)
    , m_internalSlot(nullptr)
{
}

void* ObjectRareData::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectRareData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_extraData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_internalSlot));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectRareData));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Value ObjectGetResult::valueSlowCase(ExecutionState& state, const Value& receiver) const
{
    if (LIKELY(isDataProperty())) {
        return m_nativeGetterSetterData.m_nativeGetterSetterData->m_getter(state, m_nativeGetterSetterData.m_object, receiver, m_nativeGetterSetterData.m_internalData);
    }
    if (m_jsGetterSetter->hasGetter() && m_jsGetterSetter->getter().isCallable()) {
        return Object::call(state, m_jsGetterSetter->getter(), receiver, 0, nullptr);
    }
    return Value();
}

ObjectPropertyDescriptor ObjectGetResult::convertToPropertyDescriptor(ExecutionState& state, const Value& receiver)
{
    ASSERT(hasValue());
    ObjectPropertyDescriptor desc;

    if (isDataProperty()) {
        desc = ObjectPropertyDescriptor(this->value(state, receiver));
        desc.setWritable(isWritable());
    } else {
        desc = ObjectPropertyDescriptor(*this->jsGetterSetter(), ObjectPropertyDescriptor::NotPresent);
    }

    desc.setEnumerable(isEnumerable());
    desc.setConfigurable(isConfigurable());

    ASSERT(desc.checkProperty());
    return desc;
}

// https://262.ecma-international.org/#sec-frompropertydescriptor
Value ObjectGetResult::fromPropertyDescriptor(ExecutionState& state, const Value& receiver)
{
    // If Desc is undefined, then return undefined.
    if (!hasValue()) {
        return Value();
    }

    // Let obj be the result of creating a new object as if by the expression new Object() where Object is the standard built-in constructor with that name.
    Object* obj = new Object(state);

    // If IsDataDescriptor(Desc) is true, then
    if (isDataProperty()) {
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "value", Property Descriptor {[[Value]]: Desc.[[Value]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(this->value(state, receiver), ObjectPropertyDescriptor::AllPresent));
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "writable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().writable), ObjectPropertyDescriptor(Value(isWritable()), ObjectPropertyDescriptor::AllPresent));
    } else {
        Value get;
        if (jsGetterSetter()->hasGetter())
            get = jsGetterSetter()->getter();

        Value set;
        if (jsGetterSetter()->hasSetter())
            set = jsGetterSetter()->setter();

        // Call the [[DefineOwnProperty]] internal method of obj with arguments "get", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().get), ObjectPropertyDescriptor(get, ObjectPropertyDescriptor::AllPresent));
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "set", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().set), ObjectPropertyDescriptor(set, ObjectPropertyDescriptor::AllPresent));
    }

    // Call the [[DefineOwnProperty]] internal method of obj with arguments "enumerable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().enumerable), ObjectPropertyDescriptor(Value(isEnumerable()), ObjectPropertyDescriptor::AllPresent));

    // Call the [[DefineOwnProperty]] internal method of obj with arguments "configurable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().configurable), ObjectPropertyDescriptor(Value(isConfigurable()), ObjectPropertyDescriptor::AllPresent));

    return obj;
}

ObjectPropertyDescriptor::ObjectPropertyDescriptor(ExecutionState& state, Object* obj)
    : m_isDataProperty(true)
{
    m_property = NotPresent;
    const StaticStrings* strings = &state.context()->staticStrings();
    auto desc = obj->get(state, ObjectPropertyName(strings->enumerable));
    if (desc.hasValue()) {
        Value val = desc.value(state, obj);
        RETURN_IF_PENDING_EXCEPTION
        setEnumerable(val.toBoolean(state));
    }
    desc = obj->get(state, ObjectPropertyName(strings->configurable));
    if (desc.hasValue())
        setConfigurable(desc.value(state, obj).toBoolean(state));

    bool hasValue = false;
    desc = obj->get(state, ObjectPropertyName(strings->value));
    if (desc.hasValue()) {
        setValue(desc.value(state, obj));
        hasValue = true;
    }

    bool hasWritable = false;
    desc = obj->get(state, ObjectPropertyName(strings->writable));
    if (desc.hasValue()) {
        setWritable(desc.value(state, obj).toBoolean(state));
        hasWritable = true;
    }

    desc = obj->get(state, ObjectPropertyName(strings->get));
    if (desc.hasValue()) {
        Value getter = desc.value(state, obj);
        if (!getter.isCallable() && !getter.isUndefined()) {
            THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "Getter must be a function or undefined");
        } else {
            m_isDataProperty = false;
            m_getterSetter = JSGetterSetter(getter, Value(Value::EmptyValue));
        }
    }

    desc = obj->get(state, ObjectPropertyName(strings->set));
    if (desc.hasValue()) {
        Value setter = desc.value(state, obj);
        if (!setter.isCallable() && !setter.isUndefined()) {
            THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "Setter must be a function or undefined");
        } else {
            if (m_isDataProperty) {
                m_isDataProperty = false;
                m_getterSetter = JSGetterSetter(Value(Value::EmptyValue), setter);
            } else {
                m_getterSetter.m_setter = setter;
            }
        }
    }

    if (!m_isDataProperty && (hasWritable || hasValue)) {
        THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "Invalid property descriptor. Cannot both specify accessors and a value or writable attribute");
    }

    ASSERT(checkProperty());
}

void ObjectPropertyDescriptor::setEnumerable(bool enumerable)
{
    if (enumerable) {
        m_property = (PresentAttribute)(m_property | EnumerablePresent);
        m_property = (PresentAttribute)(m_property & ~NonEnumerablePresent);
    } else {
        m_property = (PresentAttribute)(m_property | NonEnumerablePresent);
        m_property = (PresentAttribute)(m_property & ~EnumerablePresent);
    }
}

void ObjectPropertyDescriptor::setConfigurable(bool configurable)
{
    if (configurable) {
        m_property = (PresentAttribute)(m_property | ConfigurablePresent);
        m_property = (PresentAttribute)(m_property & ~NonConfigurablePresent);
    } else {
        m_property = (PresentAttribute)(m_property | NonConfigurablePresent);
        m_property = (PresentAttribute)(m_property & ~ConfigurablePresent);
    }
}

void ObjectPropertyDescriptor::setWritable(bool writable)
{
    if (writable) {
        m_property = (PresentAttribute)(m_property | WritablePresent);
        m_property = (PresentAttribute)(m_property & ~NonWritablePresent);
    } else {
        m_property = (PresentAttribute)(m_property | NonWritablePresent);
        m_property = (PresentAttribute)(m_property & ~WritablePresent);
    }
}

void ObjectPropertyDescriptor::setValue(const Value& v)
{
    m_property = (PresentAttribute)(m_property | ValuePresent);
    m_value = v;
}

ObjectStructurePropertyDescriptor ObjectPropertyDescriptor::toObjectStructurePropertyDescriptor() const
{
    if (isDataProperty()) {
        int f = 0;

        if (isWritable()) {
            f = ObjectStructurePropertyDescriptor::WritablePresent;
        }

        if (isConfigurable()) {
            f |= ObjectStructurePropertyDescriptor::ConfigurablePresent;
        }

        if (isEnumerable()) {
            f |= ObjectStructurePropertyDescriptor::EnumerablePresent;
        }

        return ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)f);
    } else {
        int f = 0;

        if (isConfigurable()) {
            f |= ObjectStructurePropertyDescriptor::ConfigurablePresent;
        }

        if (isEnumerable()) {
            f |= ObjectStructurePropertyDescriptor::EnumerablePresent;
        }

        if (hasJSGetter()) {
            f |= ObjectStructurePropertyDescriptor::HasJSGetter;
        }

        if (hasJSSetter()) {
            f |= ObjectStructurePropertyDescriptor::HasJSSetter;
        }

        return ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)f);
    }
}

ObjectPropertyDescriptor ObjectPropertyDescriptor::fromObjectStructurePropertyDescriptor(const ObjectStructurePropertyDescriptor& desc, const Value& value)
{
    const ObjectPropertyDescriptor::PresentAttribute flag = ObjectPropertyDescriptor::NotPresent;

    // If IsDataDescriptor(Desc) is true, then
    if (desc.isDataProperty()) {
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "value", Property Descriptor {[[Value]]: Desc.[[Value]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ObjectPropertyDescriptor ret(value, flag);
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "writable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ret.setWritable(desc.isWritable());

        // Call the [[DefineOwnProperty]] internal method of obj with arguments "enumerable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ret.setEnumerable(desc.isEnumerable());
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "configurable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ret.setConfigurable(desc.isConfigurable());

        return ret;
    } else {
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "get", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "set", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ObjectPropertyDescriptor ret(*value.asPointerValue()->asJSGetterSetter(), flag);

        // Call the [[DefineOwnProperty]] internal method of obj with arguments "enumerable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ret.setEnumerable(desc.isEnumerable());
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "configurable", Property Descriptor {[[Value]]: Desc.[[Writable]], [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        ret.setConfigurable(desc.isConfigurable());

        return ret;
    }
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-frompropertydescriptor
Object* ObjectPropertyDescriptor::fromObjectPropertyDescriptor(ExecutionState& state, const ObjectPropertyDescriptor& desc)
{
    auto strings = &state.context()->staticStrings();
    // 1. If Desc is undefined, return undefined.
    // 2. Let obj be ObjectCreate(%ObjectPrototype%).
    Object* obj = new Object(state);
    // 4. If Desc has a [[Value]] field, then
    if (!desc.isAccessorDescriptor()) {
        // 4.a Perform CreateDataProperty(obj, "value", Desc.[[Value]]).
        // 5. If Desc has a [[Writable]] field, then
        // 5.a Perform CreateDataProperty(obj, "writable", Desc.[[Writable]]).
        if (desc.isValuePresent()) {
            obj->defineOwnProperty(state, ObjectPropertyName(state, strings->value.string()), ObjectPropertyDescriptor(desc.value(), ObjectPropertyDescriptor::AllPresent));
        }
        if (desc.isWritablePresent()) {
            obj->defineOwnProperty(state, ObjectPropertyName(state, strings->writable.string()), ObjectPropertyDescriptor(Value(desc.isWritable()), ObjectPropertyDescriptor::AllPresent));
        }
    } else {
        ASSERT(desc.hasJSGetter() || desc.hasJSSetter());
        // 6. If Desc has a [[Get]] field, then
        if (desc.hasJSGetter()) {
            // 6.a. Perform CreateDataProperty(obj, "get", Desc.[[Get]]).
            obj->defineOwnProperty(state, ObjectPropertyName(state, strings->get.string()), ObjectPropertyDescriptor(desc.getterSetter().getter(), ObjectPropertyDescriptor::AllPresent));
        }
        // 7. If Desc has a [[Set]] field, then
        if (desc.hasJSSetter()) {
            // 7.a. Perform CreateDataProperty(obj, "set", Desc.[[Set]])
            obj->defineOwnProperty(state, ObjectPropertyName(state, strings->set.string()), ObjectPropertyDescriptor(desc.getterSetter().setter(), ObjectPropertyDescriptor::AllPresent));
        }
    }
    // 8. If Desc has an [[Enumerable]] field, then
    // 8.a. Perform CreateDataProperty(obj, "enumerable", Desc.[[Enumerable]]).
    if (desc.isEnumerablePresent()) {
        obj->defineOwnProperty(state, ObjectPropertyName(state, strings->enumerable.string()), ObjectPropertyDescriptor(Value(desc.isEnumerable()), ObjectPropertyDescriptor::AllPresent));
    }
    // 9. If Desc has a [[Configurable]] field, then
    // 9.a. Perform CreateDataProperty(obj , "configurable", Desc.[[Configurable]]).
    if (desc.isConfigurablePresent()) {
        obj->defineOwnProperty(state, ObjectPropertyName(state, strings->configurable.string()), ObjectPropertyDescriptor(Value(desc.isConfigurable()), ObjectPropertyDescriptor::AllPresent));
    }
    // 10. Assert: all of the above CreateDataProperty operations return true.
    // 11. Return obj.
    return obj;
}

void ObjectPropertyDescriptor::completePropertyDescriptor(ObjectPropertyDescriptor& desc)
{
    // 1. ReturnIfAbrupt(Desc).
    // 2. Assert: Desc is a Property Descriptor
    // 3. Let like be Record{[[Value]]: undefined, [[Writable]]: false, [[Get]]: undefined, [[Set]]: undefined, [[Enumerable]]: false, [[Configurable]]: false}.

    // 4. If either IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then
    if (desc.isGenericDescriptor() || desc.isDataDescriptor()) {
        // a. If Desc does not have a [[Value]] field, set Desc.[[Value]] to like.[[Value]].
        if (!desc.isValuePresent()) {
            desc.setValue(Value());
        }
        // If Desc does not have a [[Writable]] field, set Desc.[[Writable]] to like.[[Writable]].
        if (!desc.isWritablePresent()) {
            desc.setWritable(false);
        }
    } else {
        // a. If Desc does not have a [[Get]] field, set Desc.[[Get]] to like.[[Get]].
        if (!desc.hasJSGetter()) {
            desc.m_getterSetter.m_getter = Value();
        }
        // b. If Desc does not have a [[Set]] field, set Desc.[[Set]] to like.[[Set]].
        if (!desc.hasJSSetter()) {
            desc.m_getterSetter.m_setter = Value();
        }
    }
    // 6. If Desc does not have an [[Enumerable]] field, set Desc.[[Enumerable]] to like.[[Enumerable]].
    if (!desc.isEnumerablePresent()) {
        desc.setEnumerable(false);
    }
    // 7. If Desc does not have a [[Configurable]] field, set Desc.[[Configurable]] to like.[[Configurable]].
    if (!desc.isConfigurablePresent()) {
        desc.setConfigurable(false);
    }
    // 8. Return Desc.
}

Object::Object(ExecutionState& state)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(state.context()->globalObject()->objectPrototype())
{
    ASSERT(!!m_prototype);
    m_values.resizeWithUninitializedValues(0, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-objectcreate
Object::Object(ExecutionState& state, Object* proto)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(proto)
{
    // proto has been marked as a prototype object
    ASSERT(!!proto);
    ASSERT(proto->isEverSetAsPrototypeObject());
    // create a new ordinary object
    m_values.resizeWithUninitializedValues(0, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER);
}

Object::Object(ExecutionState& state, Object::PrototypeIsNullTag)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(nullptr)
{
    // create a new ordinary object
    m_values.resizeWithUninitializedValues(0, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER);
}

Object::Object(ExecutionState& state, Object* proto, size_t defaultSpace)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(proto)
{
    // proto has been marked as a prototype object
    ASSERT(!!proto);
    ASSERT(proto->isEverSetAsPrototypeObject());
    m_values.resizeWithUninitializedValues(0, defaultSpace);
}

Object::Object(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto)
    : m_structure(structure)
    , m_prototype(proto)
    , m_values(std::move(values))
{
    ASSERT(!!structure);
    ASSERT(!!proto);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-isconcatspreadable
bool Object::isConcatSpreadable(ExecutionState& state)
{
    // If Type(O) is not Object, return false.
    if (!isObject()) {
        return false;
    }
    // Let spreadable be Get(O, @@isConcatSpreadable).
    Value spreadable = get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().isConcatSpreadable)).value(state, this);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    // If spreadable is not undefined, return ToBoolean(spreadable).
    if (!spreadable.isUndefined()) {
        return spreadable.toBoolean(state);
    }
    // Return IsArray(O).
    return isArray(state);
}

Object* Object::createBuiltinObjectPrototype(ExecutionState& state)
{
    Object* obj = new PrototypeObject(state, PrototypeObject::__ForGlobalBuiltin__);
    obj->markThisObjectDontNeedStructureTransitionTable();
    return obj;
}

class FunctionPrototypeObject : public PrototypeObject {
public:
    explicit FunctionPrototypeObject(ExecutionState& state, Object* proto, size_t defaultSpace)
        : PrototypeObject(state, proto, defaultSpace)
    {
    }

    virtual bool isFunctionPrototypeObject() const override
    {
        return true;
    }
};

Object* Object::createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function)
{
    auto ctx = function->codeBlock()->context();
    Object* obj = new FunctionPrototypeObject(state, ctx->globalObject()->objectPrototype()->asObject(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1);
    obj->m_structure = ctx->defaultStructureForFunctionPrototypeObject();
    obj->m_values[0] = Value(function);

    return obj;
}

void Object::setGlobalIntrinsicObject(ExecutionState& state, bool isPrototype)
{
    // For initialization of GlobalObject's intrinsic objects
    // These objects have fixed properties, so transition table is not used for memory optimization
    ASSERT(m_prototype);
    ASSERT(!structure()->hasIndexPropertyName());

    markThisObjectDontNeedStructureTransitionTable();

    if (isPrototype) {
        markAsPrototypeObject(state);
    }
}

String* Object::constructorName(ExecutionState& state)
{
    Optional<FunctionObject*> ctor;
    if (hasExtendedExtraData() && extendedExtraData()->m_meaningfulConstructor) {
        ctor = extendedExtraData()->m_meaningfulConstructor;
    } else {
        Optional<Object*> object = getPrototypeObject(state);
        if (object) {
            Value c = object->getOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, object.value());
            if (c.isFunction()) {
                ctor = c.asFunction();
            }
        }
    }

    Value name;
    if (ctor) {
        if (ctor->isScriptClassConstructorFunctionObject()) {
            name = ctor->getOwnProperty(state, state.context()->staticStrings().name).value(state, ctor.value());
        } else {
            name = ctor->codeBlock()->functionName().string();
        }
    }

    if (name.isString()) {
        return name.asString();
    } else {
        return state.context()->staticStrings().Object.string();
    }
}

bool Object::setPrototype(ExecutionState& state, const Value& proto)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-setprototypeof-v
    // [[SetPrototypeOf]] (V)

    // 1. Assert: Either Type(V) is Object or Type(V) is Null.
    ASSERT(proto.isObject() || proto.isNull());

    // 3. Let current be the value of the [[Prototype]] internal slot of O.
    // 4. If SameValue(V, current), return true.
    Value lastPrototype = this->getPrototype(state);
    if (proto == lastPrototype) {
        return true;
    }

    // 2. Let extensible be the value of the [[Extensible]] internal slot of O.
    // 5. If extensible is false, return false.
    if (!isExtensible(state)) {
        return false;
    }

    // 6. Let p be V.
    Value p = proto;
    // 7. Let done be false.
    bool done = false;
    // 8. Repeat while done is false,
    while (done == false) {
        if (p.isNull()) { // If p is null, let done be true.
            done = true;
        } else if (p.isObject() && p.asObject() == this) { // Else, if SameValue(p, O) is true, return false.
            return false;
        } else { // Else,
            // i. If the [[GetPrototypeOf]] internal method of p is not the ordinary object internal method defined in 9.1.1, let done be true.
            if (UNLIKELY(!p.isObject() || !p.asObject()->isOrdinary())) {
                done = true;
            } else { // ii. Else, let p be the value of p’s [[Prototype]] internal slot.
                p = p.asObject()->getPrototype(state);
            }
        }
    }

    //9. Set the value of the [[Prototype]] internal slot of O to V.
    Object* o = nullptr;
    if (LIKELY(proto.isObject())) {
        o = proto.asObject();
        o->markAsPrototypeObject(state);
    }

    if (hasRareData()) {
        rareData()->m_prototype = o;
    } else {
        m_prototype = o;
    }

    bool hadPrototypeAsObject = lastPrototype.isObject();

    if (UNLIKELY(
            // if new prototype is null and last prototype was object
            (!o && hadPrototypeAsObject)
            ||
            // if last prototype is function's prototype
            (hadPrototypeAsObject && lastPrototype.asObject()->isFunctionPrototypeObject())
            ||
            // if last prototype is class's prototype
            (hadPrototypeAsObject && lastPrototype.asObject()->isScriptClassConstructorPrototypeObject()))) {
        if (!hasRareData() || !rareData()->m_hasExtendedExtraData || !rareData()->m_extendedExtraData->m_meaningfulConstructor.hasValue()) {
            if (lastPrototype.asObject()->isScriptClassConstructorPrototypeObject()) {
                ensureExtendedExtraData()->m_meaningfulConstructor = lastPrototype.asObject()->asScriptClassConstructorPrototypeObject()->constructor();
            } else {
                auto ctor = lastPrototype.asObject()->readConstructorSlotWithoutState();
                if (ctor && ctor.value().isFunction()) {
                    ensureExtendedExtraData()->m_meaningfulConstructor = ctor.value().asFunction();
                }
            }
        }
    }

    // 10. Return true.
    return true;
}

void Object::markAsPrototypeObject(ExecutionState& state)
{
    if (hasVTag(g_objectTag)) {
        writeVTag(g_prototypeObjectTag);
    } else {
        ensureRareData()->m_isEverSetAsPrototypeObject = true;
    }

    if (UNLIKELY(!state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && (structure()->hasIndexPropertyName() || isProxyObject()))) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
    }
}

ObjectGetResult Object::getOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    if (propertyName.isUIntType() && !m_structure->hasIndexPropertyName()) {
        return ObjectGetResult();
    }
    ObjectStructurePropertyName P = propertyName.toObjectStructurePropertyName(state);
    auto findResult = m_structure->findProperty(P);
    if (LIKELY(findResult.first != SIZE_MAX)) {
        const ObjectStructureItem* item = findResult.second.value();
        const auto& desc = item->m_descriptor;
        auto presentAttributes = desc.descriptorData().presentAttributes();
        if (desc.isDataProperty()) {
            if (LIKELY(!desc.isNativeAccessorProperty())) {
                return ObjectGetResult(m_values[findResult.first], presentAttributes & ObjectStructurePropertyDescriptor::WritablePresent, presentAttributes & ObjectStructurePropertyDescriptor::EnumerablePresent, presentAttributes & ObjectStructurePropertyDescriptor::ConfigurablePresent);
            } else {
                ObjectPropertyNativeGetterSetterData* data = desc.nativeGetterSetterData();
                return ObjectGetResult(data, this, m_values[findResult.first], presentAttributes & ObjectStructurePropertyDescriptor::WritablePresent, presentAttributes & ObjectStructurePropertyDescriptor::EnumerablePresent, presentAttributes & ObjectStructurePropertyDescriptor::ConfigurablePresent);
            }
        } else {
            const Value& v = m_values[findResult.first];
            ASSERT(v.isPointerValue() && v.asPointerValue()->isJSGetterSetter());
            return ObjectGetResult(v.asPointerValue()->asJSGetterSetter(), presentAttributes & ObjectStructurePropertyDescriptor::EnumerablePresent, presentAttributes & ObjectStructurePropertyDescriptor::ConfigurablePresent);
        }
    }
    return ObjectGetResult();
}

bool Object::defineOwnPropertyMethod(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    // TODO Return true, if every field in Desc is absent.
    // TODO Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value as the corresponding field in current when compared using the SameValue algorithm (9.12).

    ObjectStructurePropertyName propertyName = P.toObjectStructurePropertyName(state);
    auto findResult = m_structure->findProperty(propertyName);
    if (findResult.first == SIZE_MAX) {
        // 3. If current is undefined and extensible is false, then Reject.
        if (UNLIKELY(!isExtensible(state))) {
            return false;
        }

        auto structureBefore = m_structure;
        m_structure = m_structure->addProperty(propertyName, desc.toObjectStructurePropertyDescriptor());
        ASSERT(structureBefore != m_structure);
        if (LIKELY(desc.isDataProperty())) {
            const Value& val = desc.isValuePresent() ? desc.value() : Value();
            m_values.pushBack(val, m_structure->propertyCount());
        } else {
            m_values.pushBack(Value(new JSGetterSetter(desc.getterSetter())), m_structure->propertyCount());
        }

        // ASSERT(m_values.size() == m_structure->propertyCount());
        return true;
    } else {
        size_t idx = findResult.first;
        const ObjectStructureItem* item = findResult.second.value();
        auto current = item->m_descriptor;

        // If the [[Configurable]] field of current is false then
        if (!current.isConfigurable()) {
            // Reject, if the [[Configurable]] field of Desc is true.
            if (desc.isConfigurable()) {
                return false;
            }
            // Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and Desc are the Boolean negation of each other.
            if (desc.isEnumerablePresent() && desc.isEnumerable() != current.isEnumerable()) {
                return false;
            }
        }

        bool shouldDelete = false;
        Value v = current.isNativeAccessorProperty() ? this->get(state, ObjectPropertyName(state, propertyName)).value(state, this) : Value(m_values[idx]);
        ObjectPropertyDescriptor newDesc = ObjectPropertyDescriptor::fromObjectStructurePropertyDescriptor(current, v);

        // If IsGenericDescriptor(Desc) is true, then
        if (desc.isGenericDescriptor()) {
            // no further validation is required.
        }
        // If IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        else if (current.isDataProperty() != desc.isDataDescriptor()) {
            // Reject, if the [[Configurable]] field of current is false.
            if (!current.isConfigurable()) {
                return false;
            }

            shouldDelete = true;

            int f = current.isConfigurable() ? ObjectPropertyDescriptor::ConfigurablePresent : ObjectPropertyDescriptor::NonConfigurablePresent;
            f |= current.isEnumerable() ? ObjectPropertyDescriptor::EnumerablePresent : ObjectPropertyDescriptor::NonEnumerablePresent;

            // If IsDataDescriptor(current) is true, then
            if (current.isDataProperty()) {
                // Convert the property named P of object O from a data property to an accessor property.
                // Preserve the existing values of the converted property’s [[Configurable]] and [[Enumerable]] attributes
                // and set the rest of the property’s attributes to their default values.
                newDesc = ObjectPropertyDescriptor(desc.getterSetter(), (ObjectPropertyDescriptor::PresentAttribute)f);
            } else {
                // Else,
                // Convert the property named P of object O from an accessor property to a data property.
                // Preserve the existing values of the converted property's [[Configurable]] and [[Enumerable]] attributes
                // and set the rest of the property's attributes to their default values.
                newDesc = ObjectPropertyDescriptor(desc.isValuePresent() ? desc.value() : Value(), (ObjectPropertyDescriptor::PresentAttribute)f);
            }
            // Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
        } else if (item->m_descriptor.isDataProperty() && desc.isDataDescriptor()) {
            // If the [[Configurable]] field of current is false, then
            if (!item->m_descriptor.isConfigurable()) {
                // Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (!item->m_descriptor.isWritable() && desc.isWritable()) {
                    return false;
                }
                // If the [[Writable]] field of current is false, then
                // Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                if (!item->m_descriptor.isWritable() && desc.isValuePresent() && !desc.value().equalsToByTheSameValueAlgorithm(state, getOwnDataPropertyUtilForObject(state, idx, this))) {
                    return false;
                }
            }
        }
        // Else, the [[Configurable]] field of current is true, so any change is acceptable.
        else {
            // Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so,
            ASSERT(!current.isDataProperty() && !desc.isDataDescriptor());

            // If the [[Configurable]] field of current is false, then
            if (!current.isConfigurable()) {
                JSGetterSetter* currgs = Value(m_values[idx]).asPointerValue()->asJSGetterSetter();

                // Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
                if (desc.getterSetter().hasSetter() && (desc.getterSetter().setter() != (currgs->hasSetter() ? currgs->setter() : Value()))) {
                    return false;
                }
                // Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
                if (desc.getterSetter().hasGetter() && (desc.getterSetter().getter() != (currgs->hasGetter() ? currgs->getter() : Value()))) {
                    return false;
                }
            }
        }

        // For each attribute field of Desc that is present, set the correspondingly named attribute of the property named P of object O to the value of the field.
        if (newDesc.isDataDescriptor()) {
            if (desc.isValuePresent()) {
                newDesc.setValue(desc.value());
            } else if (!newDesc.isValuePresent()) {
                newDesc.setValue(Value());
            }
            if (desc.isWritablePresent()) {
                newDesc.setWritable(desc.isWritable());
            }
        } else {
            if (desc.isAccessorDescriptor()) {
                if (desc.hasJSGetter()) {
                    newDesc.m_getterSetter.m_getter = desc.getterSetter().getter();
                }
                if (desc.hasJSSetter()) {
                    newDesc.m_getterSetter.m_setter = desc.getterSetter().setter();
                }
            }
        }
        if (desc.isConfigurablePresent()) {
            newDesc.setConfigurable(desc.isConfigurable());
        }
        if (desc.isEnumerablePresent()) {
            newDesc.setEnumerable(desc.isEnumerable());
        }

        if (!shouldDelete) {
            if (newDesc.isDataDescriptor() && newDesc.isWritablePresent() && newDesc.isWritable() != current.isWritable()) {
                shouldDelete = true;
            } else if (newDesc.isEnumerablePresent() && newDesc.isEnumerable() != current.isEnumerable()) {
                shouldDelete = true;
            } else if (newDesc.isConfigurablePresent() && newDesc.isConfigurable() != current.isConfigurable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete) {
            if (newDesc.isDataDescriptor()) {
                if (desc.isValuePresent()) {
                    return setOwnDataPropertyUtilForObjectInner(state, findResult.first, *item, newDesc.value(), Value(this));
                }
            } else {
                m_values[idx] = Value(new JSGetterSetter(newDesc.getterSetter()));
            }
        } else {
            auto oldDesc = findResult.second.value();
            if (newDesc.isDataDescriptor() && oldDesc->m_descriptor.isNativeAccessorProperty()) {
                auto newNative = new ObjectPropertyNativeGetterSetterData(newDesc.isWritable(), newDesc.isEnumerable(), newDesc.isConfigurable(),
                                                                          oldDesc->m_descriptor.nativeGetterSetterData()->m_getter, oldDesc->m_descriptor.nativeGetterSetterData()->m_setter);
                m_structure = m_structure->replacePropertyDescriptor(idx, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(newNative));
            } else {
                m_structure = m_structure->replacePropertyDescriptor(idx, newDesc.toObjectStructurePropertyDescriptor());
            }

            if (newDesc.isDataDescriptor()) {
                return setOwnDataPropertyUtilForObjectInner(state, idx, m_structure->readProperty(idx), newDesc.value(), Value(this));
            } else {
                m_values[idx] = Value(new JSGetterSetter(newDesc.getterSetter()));
            }
        }

        return true;
    }
}

bool Object::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    // this method should be called for pure Object,
    // not for derived Objects like PrototypeObject
    ASSERT(hasVTag(g_objectTag));
    ASSERT(!isEverSetAsPrototypeObject());

    return defineOwnPropertyMethod(state, P, desc);
}

void Object::directDefineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    // directly add a property
    ASSERT(isOrdinary());
    ASSERT(!hasOwnProperty(state, P));
    ASSERT(isExtensible(state));

    ObjectStructurePropertyName propertyName = P.toObjectStructurePropertyName(state);
    m_structure = m_structure->addProperty(propertyName, desc.toObjectStructurePropertyDescriptor());
    if (LIKELY(desc.isDataProperty())) {
        const Value& val = desc.isValuePresent() ? desc.value() : Value();
        m_values.pushBack(val, m_structure->propertyCount());
    } else {
        m_values.pushBack(Value(new JSGetterSetter(desc.getterSetter())), m_structure->propertyCount());
    }
}

bool Object::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto result = getOwnProperty(state, P);
    if (result.hasValue() && result.isConfigurable()) {
        deleteOwnProperty(state, m_structure->findProperty(P.toObjectStructurePropertyName(state)).first);
        return true;
    } else if (result.hasValue() && !result.isConfigurable()) {
        return false;
    }
    return true;
}

void Object::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    const ObjectStructureItem* propertiesVector;
    propertiesVector = m_structure->properties();
    size_t cnt = m_structure->propertyCount();
    bool inTransitionMode = m_structure->inTransitionMode();
    if (!inTransitionMode) {
        auto newData = ALLOCA(sizeof(ObjectStructureItem) * cnt, ObjectStructureItem);
        memcpy(newData, propertiesVector, sizeof(ObjectStructureItem) * cnt);
        propertiesVector = newData;
    }
    for (size_t i = 0; i < cnt; i++) {
        const ObjectStructureItem& item = propertiesVector[i];
        if (shouldSkipSymbolKey && item.m_propertyName.isSymbol()) {
            continue;
        }
        if (!callback(state, this, ObjectPropertyName(state, item.m_propertyName), item.m_descriptor, data)) {
            break;
        }
    }
}

ObjectHasPropertyResult Object::hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-hasproperty-p
    // 1. Assert: IsPropertyKey(P) is true.
    // 2. Let hasOwn be OrdinaryGetOwnProperty(O, P).
    auto hasOwn = getOwnProperty(state, propertyName);
    // 3. If hasOwn is not undefined, return true.
    if (hasOwn.hasValue()) {
        return ObjectHasPropertyResult(hasOwn);
    }
    // 4. Let parent be O.[[GetPrototypeOf]]().
    // 5. ReturnIfAbrupt(parent).
    auto parent = getPrototypeObject(state);
    // 6. If parent is not null, then
    if (parent) {
        // a. Return parent.[[HasProperty]](P).
        return parent->hasProperty(state, propertyName);
    }
    // 7. Return false.
    return ObjectHasPropertyResult();
}

ObjectHasPropertyResult Object::hasIndexedProperty(ExecutionState& state, const Value& propertyName)
{
    return hasProperty(state, ObjectPropertyName(state, propertyName));
}

typedef std::pair<Value::ValueIndex, ObjectStructurePropertyDescriptor> IndexItem;
struct CompareIndexItem {
    bool operator()(const IndexItem& a, const IndexItem& b) const
    {
        return a.first < b.first;
    }
};

template <typename ResultType, typename ResultBinder>
static ResultType objectOwnPropertyKeys(ExecutionState& state, Object* self)
{
    // TODO turn-on gc if replace std::multiset to another gc-well-supported type
    GCDisabler disableGC;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-ownpropertykeys
    struct Properties {
        std::multiset<IndexItem, CompareIndexItem, GCUtil::gc_malloc_allocator<IndexItem>> indexes;
        VectorWithInlineStorage<32, std::pair<String*, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<String*, ObjectStructurePropertyDescriptor>>> strings;
        VectorWithInlineStorage<4, std::pair<Symbol*, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<Symbol*, ObjectStructurePropertyDescriptor>>> symbols;
    } properties;

    self->enumeration(
        state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
            auto properties = (Properties*)data;

            auto indexProperty = name.tryToUseAsIndexProperty();
            if (indexProperty != Value::InvalidIndexPropertyValue) {
                properties->indexes.insert(std::make_pair(indexProperty, desc));
            } else {
                const ObjectStructurePropertyName& propertyName = name.objectStructurePropertyName();

                if (propertyName.isSymbol()) {
                    properties->symbols.push_back(std::make_pair(propertyName.symbol(), desc));
                } else {
                    ASSERT(propertyName.isPlainString());
                    String* name = propertyName.plainString();
                    properties->strings.push_back(std::make_pair(name, desc));
                }
            }
            return true;
        },
        &properties, false);

    ResultType result(properties.indexes.size() + properties.strings.size() + properties.symbols.size());

    ResultBinder b;
    size_t idx = 0;
    for (auto& v : properties.indexes) {
        result[idx++] = b(Value(v.first).toString(state), v.second);
    }
    for (auto& v : properties.strings) {
        result[idx++] = b(v.first, v.second);
    }
    for (auto& v : properties.symbols) {
        result[idx++] = b(v.first, v.second);
    }

    return result;
}

class OwnPropertyKeyResultResultBinder {
public:
    Value operator()(PointerValue* s, const ObjectStructurePropertyDescriptor& desc)
    {
        return Value(s);
    }
};

class OwnPropertyKeyAndDescResultResultBinder {
public:
    std::pair<Value, ObjectStructurePropertyDescriptor> operator()(PointerValue* s, const ObjectStructurePropertyDescriptor& desc)
    {
        return std::make_pair(Value(s), desc);
    }
};

Object::OwnPropertyKeyAndDescVector Object::ownPropertyKeysFastPath(ExecutionState& state)
{
    ASSERT(canUseOwnPropertyKeysFastPath());
    return objectOwnPropertyKeys<Object::OwnPropertyKeyAndDescVector, OwnPropertyKeyAndDescResultResultBinder>(state, this);
}

Object::OwnPropertyKeyVector Object::ownPropertyKeys(ExecutionState& state)
{
    return objectOwnPropertyKeys<Object::OwnPropertyKeyVector, OwnPropertyKeyResultResultBinder>(state, this);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-get-p-receiver
ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& receiver)
{
    Object* iter = this;

    while (true) {
        ObjectGetResult desc = iter->getOwnProperty(state, propertyName);
        if (desc.hasValue()) {
            return desc;
        }

        iter = iter->getPrototypeObject(state);

        if (iter == nullptr) {
            break;
        }

        if (UNLIKELY(iter->isProxyObject())) {
            return iter->get(state, propertyName, receiver);
        }
    }
    return ObjectGetResult();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-set-p-v-receiver
bool Object::set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver)
{
    // 2. Let ownDesc be O.[[GetOwnProperty]](P).
    auto ownDesc = this->getOwnProperty(state, propertyName);
    // 4. If ownDesc is undefined, then
    bool hasValue = ownDesc.hasValue();
    if (!hasValue) {
        // FIXME need to optimize prototype iteration
        // 4.a. Let parent be O.[[GetPrototypeOf]]().
        Value parent = this->getPrototype(state);
        // 4.c. If parent is not null, then
        if (parent.isObject()) {
            // 4.c.i. Return parent.[[Set]](P, V, Receiver).
            return parent.asObject()->set(state, propertyName, v, receiver);
        } else {
            // 4.d.i. Let ownDesc be the PropertyDescriptor{[[Value]]: undefined, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}.
            ownDesc = ObjectGetResult(Value(), true, true, true);
        }
    }
    // 5. If IsDataDescriptor(ownDesc) is true, then
    if (ownDesc.isDataProperty()) {
        // 5.a. If ownDesc.[[Writable]] is false, return false.
        if (!ownDesc.isWritable()) {
            return false;
        }
        // 5.b. If Type(Receiver) is not Object, return false.
        if (!receiver.isObject()) {
            return false;
        }

        if (UNLIKELY(ownDesc.isDataAccessorProperty() && ownDesc.nativeGetterSetterData()->m_actsLikeJSGetterSetter)) {
            if (UNLIKELY(isEverSetAsPrototypeObject() && !state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && propertyName.isIndexString())) {
                state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
            }
            ObjectStructurePropertyName propertyStructureName = propertyName.toObjectStructurePropertyName(state);
            auto findResult = m_structure->findProperty(propertyStructureName);
            ASSERT(findResult.first != SIZE_MAX);
            const ObjectStructureItem* item = findResult.second.value();
            return setOwnDataPropertyUtilForObjectInner(state, findResult.first, *item, v, receiver.asObject());
        }

        // 5.c. Let existingDescriptor be Receiver.[[GetOwnProperty]](P).
        Object* receiverObj = receiver.asObject();

        ObjectGetResult existingDesc;
        if (hasValue && LIKELY(this == receiverObj)) {
            existingDesc = ownDesc;
            ObjectPropertyDescriptor propertyDesc(v);
            // 5.e.iv. Return Receiver.[[DefineOwnProperty]](P, valueDesc).
            return receiverObj->defineOwnProperty(state, propertyName, propertyDesc);
        }

        existingDesc = receiverObj->getOwnProperty(state, propertyName);

        // 5.e. If existingDescriptor is not undefined, then
        if (existingDesc.hasValue()) {
            // 5.e.i. If IsAccessorDescriptor(existingDescriptor) is true, return false.
            if (!existingDesc.isDataProperty()) {
                return false;
            }
            // 5.e.ii. If existingDescriptor.[[Writable]] is false, return false.
            if (!existingDesc.isWritable()) {
                return false;
            }
            // 5.e.iii. Let valueDesc be the PropertyDescriptor{[[Value]]: V}.
            ObjectPropertyDescriptor propertyDesc(v);
            // 5.e.iv. Return Receiver.[[DefineOwnProperty]](P, valueDesc).
            return receiverObj->defineOwnProperty(state, propertyName, propertyDesc);
        }
        // 5.f. Else Receiver does not currently have a property P,
        // 5.f.i Return CreateDataProperty(Receiver, P, V).
        return receiverObj->defineOwnProperty(state, propertyName, ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
    }
    // 6. Assert: IsAccessorDescriptor(ownDesc) is true.
    ASSERT(!ownDesc.isDataProperty());
    // 7. Let setter be ownDesc.[[Set]].
    Value setter = ownDesc.jsGetterSetter()->hasSetter() ? ownDesc.jsGetterSetter()->setter() : Value();
    // 8. If setter is undefined, return false.
    if (setter.isUndefined()) {
        return false;
    }
    // 9. Let setterResult be Call(setter, Receiver, «V»).
    Value argv[] = { v };
    Object::call(state, setter, receiver, 1, argv);
    return true;
}

static Object* internalFastToObjectForGetMethodGetV(ExecutionState& state, const Value& O)
{
    // below method use fake object instead of Value::toObject when O is isPrimitive.
    // we can use proxy object instead of Value::toObject because converted object only used on getting method
    Object* obj;

    if (O.isObject()) {
        obj = O.asObject();
    } else if (O.isString()) {
        obj = state.context()->globalObject()->stringProxyObject();
    } else if (O.isNumber()) {
        obj = state.context()->globalObject()->numberProxyObject();
    } else if (O.isBoolean()) {
        obj = state.context()->globalObject()->booleanProxyObject();
    } else if (O.isSymbol()) {
        obj = state.context()->globalObject()->symbolProxyObject();
    } else if (O.isBigInt()) {
        obj = state.context()->globalObject()->bigIntProxyObject();
    } else {
        obj = O.toObject(state); // this always cause type error
    }

    return obj;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-getmethod
Value Object::getMethod(ExecutionState& state, const Value& O, const ObjectPropertyName& propertyName)
{
    Object* obj = internalFastToObjectForGetMethodGetV(state, O);
    RETURN_VALUE_IF_PENDING_EXCEPTION
    auto r = obj->getMethod(state, propertyName);
    if (r) {
        return Value(r.value());
    }
    return Value();
}

Optional<Object*> Object::getMethod(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    // 2. Let func be GetV(O, P).
    Value func = get(state, propertyName).value(state, this);
    RETURN_NULL_IF_PENDING_EXCEPTION
    // 4. If func is either undefined or null, return undefined.
    if (func.isUndefinedOrNull()) {
        return nullptr;
    }
    // 5. If IsCallable(func) is false, throw a TypeError exception.
    if (!func.isCallable()) {
        THROW_BUILTIN_ERROR_RETURN_NULL(state, ErrorCode::TypeError, String::emptyString, false, String::emptyString, "%s: return value of getMethod is not callable");
    }
    // 6. Return func.
    return Optional<Object*>(func.asObject());
}

Value Object::getV(ExecutionState& state, const Value& O, const ObjectPropertyName& propertyName)
{
    Object* obj = internalFastToObjectForGetMethodGetV(state, O);
    return obj->get(state, propertyName).value(state, obj);
}

Object* Object::getPrototypeFromConstructor(ExecutionState& state, Object* constructor, Object* (*intrinsicDefaultProtoGetter)(ExecutionState& state, Context* context))
{
    // Assert: intrinsicDefaultProto is a String value that is this specification's name of an intrinsic object. The corresponding object must be an intrinsic that is intended to be used as the [[Prototype]] value of an object.
    // Assert: IsCallable(constructor) is true.
    ASSERT(constructor->isCallable());

    Value proto;
    if (LIKELY(constructor->isFunctionObject())) {
        proto = constructor->asFunctionObject()->getFunctionPrototype(state);
    } else {
        // Let proto be ? Get(constructor, "prototype").
        proto = constructor->get(state, ObjectPropertyName(state.context()->staticStrings().prototype)).value(state, constructor);
        RETURN_NULL_IF_PENDING_EXCEPTION
    }

    // If Type(proto) is not Object, then
    if (!proto.isObject()) {
        // Let realm be ? GetFunctionRealm(constructor).
        // Set proto to realm's intrinsic object named intrinsicDefaultProto.
        Context* c = constructor->getFunctionRealm(state);
        RETURN_NULL_IF_PENDING_EXCEPTION
        proto = intrinsicDefaultProtoGetter(state, c);
        RETURN_NULL_IF_PENDING_EXCEPTION
    } else {
        proto.asObject()->markAsPrototypeObject(state);
    }
    // Return proto.
    return proto.asObject();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-call
Value Object::call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, Value* argv)
{
    if (!callee.isPointerValue()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    // Return F.[[Call]](V, argumentsList).
    return callee.asPointerValue()->call(state, thisValue, argc, argv);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-construct
Value Object::construct(ExecutionState& state, const Value& constructor, const size_t argc, Value* argv, Object* newTarget)
{
    // If newTarget was not passed, let newTarget be F.
    if (newTarget == nullptr) {
        newTarget = constructor.asObject();
    }

    ASSERT(constructor.isConstructor());
    ASSERT(newTarget->isConstructor());

    return constructor.asPointerValue()->construct(state, argc, argv, newTarget);
}

// https://www.ecma-international.org/ecma-262/#sec-setintegritylevel
bool Object::setIntegrityLevel(ExecutionState& state, Object* O, bool isSealed)
{
    // Let status be ? O.[[PreventExtensions]]().
    bool status = O->preventExtensions(state);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    // If status is false, return false.
    if (!status) {
        return false;
    }

    // Let keys be ? O.[[OwnPropertyKeys]]().
    Object::OwnPropertyKeyVector keys = O->ownPropertyKeys(state);

    // If level is sealed, then
    if (isSealed) {
        // For each element k of keys, do
        for (size_t i = 0; i < keys.size(); i++) {
            ObjectGetResult currentDesc = O->getOwnProperty(state, ObjectPropertyName(state, keys[i]));
            if (currentDesc.isConfigurable()) {
                // Perform ? DefinePropertyOrThrow(O, k, PropertyDescriptor { [[Configurable]]: false }).
                ObjectPropertyDescriptor newDesc = currentDesc.convertToPropertyDescriptor(state, O);
                newDesc.setConfigurable(false);

                O->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, keys[i]), newDesc);
                RETURN_ZERO_IF_PENDING_EXCEPTION
            }
        }
    } else {
        // level is frozen
        // For each element k of keys, do
        for (size_t i = 0; i < keys.size(); i++) {
            // Let currentDesc be ? O.[[GetOwnProperty]](k).
            ObjectGetResult currentDesc = O->getOwnProperty(state, ObjectPropertyName(state, keys[i]));
            // If currentDesc is not undefined, then
            if (currentDesc.hasValue()) {
                if (currentDesc.isConfigurable() || (currentDesc.isDataProperty() && currentDesc.isWritable())) {
                    // Let desc be the PropertyDescriptor { [[Configurable]]: false, [[Writable]]: false }.
                    ObjectPropertyDescriptor newDesc = currentDesc.convertToPropertyDescriptor(state, O);
                    newDesc.setConfigurable(false);
                    if (currentDesc.isDataProperty()) {
                        newDesc.setWritable(false);
                    }
                    // Perform ? DefinePropertyOrThrow(O, k, desc).
                    O->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, keys[i]), newDesc);
                    RETURN_ZERO_IF_PENDING_EXCEPTION
                }
            }
        }
    }

    return true;
}

// https://www.ecma-international.org/ecma-262/#sec-testintegritylevel
bool Object::testIntegrityLevel(ExecutionState& state, Object* O, bool isSealed)
{
    // Let extensible be ? IsExtensible(O).
    // If extensible is true, return false.
    if (O->isExtensible(state)) {
        return false;
    }

    // Let keys be ? O.[[OwnPropertyKeys]]().
    Object::OwnPropertyKeyVector keys = O->ownPropertyKeys(state);

    // For each element k of keys, do
    for (size_t i = 0; i < keys.size(); i++) {
        // Let currentDesc be ? O.[[GetOwnProperty]](k).
        ObjectGetResult currentDesc = O->getOwnProperty(state, ObjectPropertyName(state, keys[i]));
        // If currentDesc is not undefined, then
        if (currentDesc.hasValue()) {
            // If currentDesc.[[Configurable]] is true, return false.
            if (currentDesc.isConfigurable()) {
                return false;
            }

            // If level is frozen and IsDataDescriptor(currentDesc) is true, then
            // If currentDesc.[[Writable]] is true, return false.
            if (!isSealed && currentDesc.isDataProperty() && currentDesc.isWritable()) {
                return false;
            }
        }
    }

    return true;
}

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinaryhasinstance
bool Object::hasInstance(ExecutionState& state, Value O)
{
    Object* C = this;
    // If IsCallable(C) is false, return false.
    if (!C->isCallable()) {
        return false;
    }

    // If C has a [[BoundTargetFunction]] internal slot, then
    if (UNLIKELY(isBoundFunctionObject())) {
        // Let BC be the value of C’s [[BoundTargetFunction]] internal slot.
        Value BC = C->asBoundFunctionObject()->targetFunction();
        // Return InstanceofOperator(O,BC) (see 12.9.4).
        return O.instanceOf(state, BC);
    }
    // If Type(O) is not Object, return false.
    if (!O.isObject()) {
        return false;
    }
    // Let P be Get(C, "prototype").
    Value P = C->get(state, state.context()->staticStrings().prototype).value(state, C);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    // If Type(P) is not Object, throw a TypeError exception.
    if (!P.isObject()) {
        THROW_BUILTIN_ERROR_RETURN_ZERO(state, ErrorCode::TypeError, ErrorObject::Messages::InstanceOf_InvalidPrototypeProperty);
    }
    // Repeat
    O = O.asObject()->getPrototype(state);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    while (!O.isNull()) {
        // If O is null, return false.
        // If SameValue(P, O) is true, return true.
        if (P == O) {
            return true;
        }
        // Let O be O.[[GetPrototypeOf]]().
        O = O.asObject()->getPrototype(state);
        RETURN_ZERO_IF_PENDING_EXCEPTION
    }
    return false;
}

bool Object::isCompatiblePropertyDescriptor(ExecutionState& state, bool extensible, const ObjectPropertyDescriptor& desc, const ObjectGetResult& current)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-iscompatiblepropertydescriptor
    // https://www.ecma-international.org/ecma-262/6.0/#sec-validateandapplypropertydescriptor
    // Ref : https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarydefineownproperty

    // Note : This implementation is a simplified for the following reasons.
    //  1. isCompatiblePropertyDescriptor calls ValidateAndApplyPropertyDescriptor with the arguments O and P as undefined always
    //  2. Only proxy internal methods call isCompatiblePropertyDescriptor.
    // Therefore, excluded unnecessary parts of validateAndApplyPropertyDescriptor and implemented only the other parts at here
    // Furthermore, We decided to leave Object's [[DefineProperty]] (P, Desc) as it is.

    // 2. If current is undefined, then
    if (!current.hasValue()) {
        // a. If extensible is false, return false.
        if (!extensible) {
            return false;
        }
        // b. Assert: extensible is true.
        ASSERT(extensible);
        // e. Return true.
        return true;
    }
    // TDOO : 3. Return true, if every field in Desc is absent.
    // TDOO : 4. Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value

    // 5. If the [[Configurable]] field of current is false, then
    if (!current.isConfigurable()) {
        // a. Return false, if the [[Configurable]] field of Desc is true.
        if (desc.isConfigurable()) {
            return false;
        }
        // Return false, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and Desc are the Boolean negation of each other.
        if (desc.isEnumerablePresent() && desc.isEnumerable() != current.isEnumerable()) {
            return false;
        }
    }
    // 6. If IsGenericDescriptor(Desc) is true, no further validation is required.
    if (desc.isGenericDescriptor()) {
        return true;
    } else if (current.isDataProperty() != desc.isDataDescriptor()) { // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        // a. Return false, if the [[Configurable]] field of current is false.
        if (!current.isConfigurable()) {
            return false;
        }
    } else if (current.isDataProperty() && desc.isDataDescriptor()) { // 8. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
        // a. If the [[Configurable]] field of current is false, then
        if (!current.isConfigurable()) {
            // i. Return false, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
            if (!current.isWritable() && desc.isWritable()) {
                return false;
            }
            // ii. If the [[Writable]] field of current is false, then
            if (!current.isWritable()) {
                // 1. Return false, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                if (desc.isValuePresent() && !desc.value().equalsToByTheSameValueAlgorithm(state, current.value(state, Value()))) {
                    return false;
                }
            }
        }
    } else if (!current.isDataProperty() && desc.isAccessorDescriptor()) { // 9. Else IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true,
        // a. If the [[Configurable]] field of current is false, then
        if (!current.isConfigurable()) {
            JSGetterSetter* currgs = current.jsGetterSetter();
            // i. Return false, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
            if (desc.getterSetter().hasSetter() && (desc.getterSetter().setter() != (currgs->hasSetter() ? currgs->setter() : Value()))) {
                return false;
            }
            // ii. Return false, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
            if (desc.getterSetter().hasGetter() && (desc.getterSetter().getter() != (currgs->hasGetter() ? currgs->getter() : Value()))) {
                return false;
            }
        }
    }
    return true;
}

void Object::setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver))) {
        RETURN_IF_PENDING_EXCEPTION
        THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
    }
}

void Object::setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver)) && state.inStrictMode()) {
        RETURN_IF_PENDING_EXCEPTION
        THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
    }
}

void Object::throwCannotDefineError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_RedefineNotConfigurable);
}

void Object::throwCannotWriteError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
}

void Object::throwCannotDeleteError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotConfigurable);
}

ArrayObject* Object::createArrayFromList(ExecutionState& state, const uint64_t& size, const Value* buffer)
{
    return new ArrayObject(state, buffer, size);
}

ArrayObject* Object::createArrayFromList(ExecutionState& state, const ValueVector& elements)
{
    return Object::createArrayFromList(state, elements.size(), elements.data());
}

ValueVector Object::createListFromArrayLike(ExecutionState& state, Value obj, uint8_t elementTypes)
{
    // https://tc39.es/ecma262/#sec-createlistfromarraylike
    auto strings = &state.context()->staticStrings();

    // If elementTypes is not present, set elementTypes to « Undefined, Null, Boolean, String, Symbol, Number, BigInt, Object ».
    // If Type(obj) is not Object, throw a TypeError exception.
    if (!obj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_FirstArgumentNotObject);
        return ValueVector();
    }

    // Let len be ? LengthOfArrayLike(obj).
    Object* o = obj.asObject();
    auto len = o->length(state);
    if (UNLIKELY(state.hasPendingException())) {
        return ValueVector();
    }

    // Let list be an empty List.
    // Let index be 0.
    ValueVector list;
    uint64_t index = 0;
    bool allTypes = (elementTypes == static_cast<uint8_t>(ElementTypes::ALL));

    // Repeat while index < len
    while (index < len) {
        // Let next be Get(obj, indexName).
        Value next = o->getIndexedProperty(state, Value(index)).value(state, o);
        if (UNLIKELY(state.hasPendingException())) {
            return ValueVector();
        }

        // If Type(next) is not an element of elementTypes, throw a TypeError exception.
        bool validType = false;
        if (next.isUndefined()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Undefined));
        } else if (next.isNull()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Null));
        } else if (next.isBoolean()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Boolean));
        } else if (next.isString()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::String));
        } else if (next.isSymbol()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Symbol));
        } else if (next.isNumber()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Number));
        } else if (next.isBigInt()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::BigInt));
        } else if (next.isObject()) {
            validType = allTypes || (elementTypes & static_cast<uint8_t>(ElementTypes::Object));
        }

        if (UNLIKELY(!validType)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->object.string(), false, String::emptyString, "%s: Type(next) is not an element of elementTypes");
            return ValueVector();
        }

        // Append next as the last element of list.
        list.pushBack(next);
        // Set index to index + 1.
        index++;
    }

    // Return list.
    return list;
}

void Object::deleteOwnProperty(ExecutionState& state, size_t idx)
{
    m_structure = m_structure->removeProperty(idx);
    m_values.erase(idx, m_structure->propertyCount() + 1);

    // ASSERT(m_values.size() == m_structure->propertyCount());
}

void Object::markAsNonInlineCachable()
{
    ensureRareData()->m_isInlineCacheable = false;
}

void Object::redefineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    ASSERT(!P.isIndexString());
    ASSERT(desc.isDataProperty());
    ASSERT(desc.isWritable());
    ASSERT(desc.isConfigurable());

    ObjectStructurePropertyName propertyName = P.toObjectStructurePropertyName(state);
    auto findResult = m_structure->findProperty(propertyName);
    ASSERT(findResult.first != SIZE_MAX);

    size_t idx = findResult.first;
    const ObjectStructureItem* item = findResult.second.value();
    auto current = item->m_descriptor;
    ASSERT(current.isDataProperty());
    ASSERT(current.isWritable());
    ASSERT(current.isConfigurable());

    m_structure = m_structure->replacePropertyDescriptor(idx, desc.toObjectStructurePropertyDescriptor());
    m_values[idx] = desc.value();
}

uint64_t Object::length(ExecutionState& state)
{
    // ToLength(Get(obj, "length"))
    Value len = get(state, state.context()->staticStrings().length).value(state, this);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    return len.toLength(state);
}

bool Object::isArray(ExecutionState& state)
{
    if (isArrayObject()) {
        return true;
    }
    if (isProxyObject()) {
        ProxyObject* proxy = asProxyObject();
        if (proxy->handler() == nullptr) {
            THROW_BUILTIN_ERROR_RETURN_ZERO(state, ErrorCode::TypeError, state.context()->staticStrings().Proxy.string(), false, String::emptyString, "%s: Proxy handler should not null.");
        }
        if (proxy->target() == nullptr) {
            return false;
        }
        return proxy->target()->isArray(state);
    }
    return false;
}

void Object::nextIndexForward(ExecutionState& state, Object* obj, const int64_t cur, const int64_t end, int64_t& nextIndex)
{
    Value ptr = obj;
    int64_t ret = end;
    struct Data {
        bool* exists;
        const int64_t* cur;
        int64_t* ret;
    } data;
    data.cur = &cur;
    data.ret = &ret;

    while (ptr.isObject()) {
        if (UNLIKELY(!ptr.asObject()->isOrdinary())) {
            nextIndex = std::min(end, cur + 1);
            return;
        }

        ptr.asObject()->enumeration(
            state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
                int64_t index;
                Data* e = (Data*)data;
                int64_t* ret = e->ret;
                Value key = name.toPlainValue();
                index = key.toNumber(state);
                if ((uint64_t)index != Value::InvalidIndexValue) {
                    if (index > *e->cur && *ret > index) {
                        *ret = std::min(index, *ret);
                    }
                }
                return true;
            },
            &data);
        ptr = ptr.asObject()->getPrototype(state);
    }
    nextIndex = std::max(ret, cur + 1);
}

void Object::nextIndexBackward(ExecutionState& state, Object* obj, const int64_t cur, const int64_t end, int64_t& nextIndex)
{
    Value ptr = obj;
    int64_t ret = end;
    struct Data {
        const int64_t* cur;
        int64_t* ret;
    } data;
    data.cur = &cur;
    data.ret = &ret;

    while (ptr.isObject()) {
        if (UNLIKELY(!ptr.asObject()->isOrdinary())) {
            nextIndex = std::max(end, cur - 1);
            return;
        }
        ptr.asObject()->enumeration(
            state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
                int64_t index;
                Data* e = (Data*)data;
                int64_t* ret = e->ret;
                Value key = name.toPlainValue();
                index = key.toNumber(state);
                if ((uint64_t)index != Value::InvalidIndexValue) {
                    if (index < *e->cur) {
                        *ret = std::max(index, *ret);
                    }
                }
                return true;
            },
            &data);
        ptr = ptr.asObject()->getPrototype(state);
    }
    nextIndex = std::min(ret, cur - 1);
}

void Object::sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    ValueVectorWithInlineStorage64 selected;

    int64_t n = 0;
    int64_t k = 0;

    while (k < length) {
        Value idx = Value(k);
        if (hasOwnProperty(state, ObjectPropertyName(state, idx))) {
            selected.push_back(getOwnProperty(state, ObjectPropertyName(state, idx)).value(state, this));
            n++;
            k++;
        } else {
            int64_t result;
            nextIndexForward(state, this, k, length, result);
            k = result;
        }
    }


    if (selected.size()) {
        TightVector<Value, GCUtil::gc_malloc_allocator<Value>> tempSpace;
        tempSpace.resizeWithUninitializedValues(selected.size());

        mergeSort(selected.data(), selected.size(), tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = comp(a, b);
            return true;
        });
    }

    int64_t i;
    for (i = 0; i < n; i++) {
        setThrowsException(state, ObjectPropertyName(state, Value(i)), selected[i], this);
        RETURN_IF_PENDING_EXCEPTION
    }

    while (i < length) {
        Value idx = Value(i);
        if (hasOwnProperty(state, ObjectPropertyName(state, idx))) {
            deleteOwnProperty(state, ObjectPropertyName(state, Value(i)));
            i++;
        } else {
            int64_t result;
            nextIndexForward(state, this, i, length, result);
            i = result;
        }
    }
}

Value Object::getOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& receiver)
{
    Value v = m_values[idx];
    auto gs = v.asPointerValue()->asJSGetterSetter();
    if (gs->hasGetter() && gs->getter().isCallable()) {
        return Object::call(state, gs->getter(), receiver, 0, nullptr);
    }
    return Value();
}

bool Object::setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
{
    Value v = m_values[idx];
    auto gs = v.asPointerValue()->asJSGetterSetter();
    if (gs->hasSetter() && gs->setter().isCallable()) {
        Value arg = newValue;
        Object::call(state, gs->setter(), receiver, 1, &arg);
        return true;
    }
    return false;
}

bool Object::defineNativeDataAccessorProperty(ExecutionState& state, const ObjectPropertyName& P, ObjectPropertyNativeGetterSetterData* data, const Value& objectInternalData)
{
    if (hasOwnProperty(state, P))
        return false;
    if (!isExtensible(state))
        return false;

    ASSERT(!hasOwnProperty(state, P));
    ASSERT(isExtensible(state));

    m_structure = m_structure->addProperty(P.toObjectStructurePropertyName(state), ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(data));
    m_values.pushBack(objectInternalData, m_structure->propertyCount());

    if (UNLIKELY(data->m_actsLikeJSGetterSetter)) {
        markAsNonInlineCachable();
    }

    return true;
}

ObjectGetResult Object::getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver)
{
    return get(state, ObjectPropertyName(state, property), receiver);
}

bool Object::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver)
{
    return set(state, ObjectPropertyName(state, property), value, receiver);
}

static ObjectPrivateMemberDataChain* ensurePieceOnPrivateMemberChain(ExecutionState& state, ObjectExtendedExtraData* e, Object* contextObject)
{
    Optional<ObjectPrivateMemberDataChain*> piece = e->m_privateMemberChain;
    if (e->m_privateMemberChain) {
        while (true) {
            if (piece->m_contextObject == contextObject) {
                break;
            }
            if (!piece->m_nextPiece) {
                piece = piece->m_nextPiece = new ObjectPrivateMemberDataChain(contextObject,
                                                                              state.context()->defaultPrivateMemberStructure());
                break;
            }
            piece = piece->m_nextPiece;
        }
    } else {
        piece = e->m_privateMemberChain = new ObjectPrivateMemberDataChain(contextObject,
                                                                           state.context()->defaultPrivateMemberStructure());
    }
    return piece.value();
}

static void addPrivateMember(ExecutionState& state, ObjectExtendedExtraData* e, Object* contextObject, AtomicString propertyName,
                             ObjectPrivateMemberStructureItemKind kind, const EncodedValue& value)
{
    ObjectPrivateMemberDataChain* piece = ensurePieceOnPrivateMemberChain(state, e, contextObject);

    if (piece->m_privateMemberStructure->findProperty(propertyName)) {
        THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, ErrorObject::Messages::CanNotRedefinePrivateMember, propertyName.string());
    }

    piece->m_privateMemberStructure = piece->m_privateMemberStructure->addProperty(ObjectPrivateMemberStructureItem(propertyName, kind));
    piece->m_privateMemberValues.pushBack(value, piece->m_privateMemberStructure->propertyCount());
}

void Object::addPrivateField(ExecutionState& state, Object* contextObject, AtomicString propertyName, const Value& value)
{
    addPrivateMember(state, ensureExtendedExtraData(), contextObject, propertyName, ObjectPrivateMemberStructureItemKind::Field, value);
}

void Object::addPrivateMethod(ExecutionState& state, Object* contextObject, AtomicString propertyName, FunctionObject* fn)
{
    addPrivateMember(state, ensureExtendedExtraData(), contextObject, propertyName, ObjectPrivateMemberStructureItemKind::Method, EncodedValue(fn));
}

void Object::addPrivateAccessor(ExecutionState& state, Object* contextObject, AtomicString propertyName, FunctionObject* callback, bool isGetter, bool isSetter)
{
    auto e = ensureExtendedExtraData();
    ObjectPrivateMemberDataChain* piece = ensurePieceOnPrivateMemberChain(state, e, contextObject);

    auto r = piece->m_privateMemberStructure->findProperty(propertyName);
    if (r) {
        if (piece->m_privateMemberStructure->readProperty(r.value()).kind() == ObjectPrivateMemberStructureItemKind::GetterSetter) {
            Value val = piece->m_privateMemberValues[r.value()];
            JSGetterSetter* gs = val.asPointerValue()->asJSGetterSetter();
            if (isGetter) {
                if (!gs->hasGetter()) {
                    gs->setGetter(callback);
                    return;
                }
            }
            if (isSetter) {
                if (!gs->hasSetter()) {
                    gs->setSetter(callback);
                    return;
                }
            }
        }
        THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "Cannot add private field %s with same name twice", propertyName.string());
    } else {
        JSGetterSetter* gs = new JSGetterSetter(isGetter ? callback : Value(Value::EmptyValue), isSetter ? callback : Value(Value::EmptyValue));
        piece->m_privateMemberStructure = piece->m_privateMemberStructure->addProperty(
            ObjectPrivateMemberStructureItem(propertyName, ObjectPrivateMemberStructureItemKind::GetterSetter));
        piece->m_privateMemberValues.pushBack(EncodedValue(gs), piece->m_privateMemberStructure->propertyCount());
    }
}

static Optional<ObjectPrivateMemberDataChain*> findPieceOnPrivateMemberChain(ExecutionState& state, ObjectExtendedExtraData* e, Object* contextObject)
{
    Optional<ObjectPrivateMemberDataChain*> piece = e->m_privateMemberChain;
    while (piece) {
        if (piece->m_contextObject == contextObject) {
            return piece;
        }
        piece = piece->m_nextPiece;
    }
    return nullptr;
}

Value Object::getPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, bool shouldReferOuterClass)
{
    auto e = ensureExtendedExtraData();
    auto piece = findPieceOnPrivateMemberChain(state, e, contextObject);
    if (piece) {
        auto r = piece->m_privateMemberStructure->findProperty(propertyName);
        if (r) {
            auto desc = piece->m_privateMemberStructure->readProperty(r.value());
            if (desc.kind() == ObjectPrivateMemberStructureItemKind::GetterSetter) {
                JSGetterSetter* gs = Value(piece->m_privateMemberValues[r.value()]).asPointerValue()->asJSGetterSetter();
                if (!gs->hasGetter()) {
                    THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, "'%s' was defined without a getter", propertyName.string());
                } else {
                    return Object::call(state, gs->getter(), this, 0, nullptr);
                }
            } else {
                return piece->m_privateMemberValues[r.value()];
            }
        }
    } else if (shouldReferOuterClass && contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor()) {
        return getPrivateMember(state, contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor().value(), propertyName);
    }
    THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, ErrorObject::Messages::CanNotReadPrivateMember, propertyName.string());
}

bool Object::hasPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, bool shouldReferOuterClass)
{
    auto e = ensureExtendedExtraData();
    auto piece = findPieceOnPrivateMemberChain(state, e, contextObject);
    if (piece) {
        return piece->m_privateMemberStructure->findProperty(propertyName);
    } else if (shouldReferOuterClass && contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor()) {
        return hasPrivateMember(state, contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor().value(), propertyName);
    }
    return false;
}

void Object::setPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, const Value& value, bool shouldReferOuterClass)
{
    auto e = ensureExtendedExtraData();
    auto piece = findPieceOnPrivateMemberChain(state, e, contextObject);
    if (piece) {
        auto r = piece->m_privateMemberStructure->findProperty(propertyName);
        if (r) {
            auto desc = piece->m_privateMemberStructure->readProperty(r.value());
            if (desc.kind() == ObjectPrivateMemberStructureItemKind::GetterSetter) {
                JSGetterSetter* gs = Value(piece->m_privateMemberValues[r.value()]).asPointerValue()->asJSGetterSetter();
                if (!gs->hasSetter()) {
                    THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "'%s' was defined without a setter", propertyName.string());
                } else {
                    Value argv = value;
                    Object::call(state, gs->setter(), this, 1, &argv);
                    return;
                }
            } else if (desc.kind() == ObjectPrivateMemberStructureItemKind::Method) {
                THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, "'%s' is non writable private property", propertyName.string());
            } else {
                piece->m_privateMemberValues[r.value()] = value;
                return;
            }
        }
    } else if (shouldReferOuterClass && contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor()) {
        return setPrivateMember(state, contextObject->asScriptClassConstructorFunctionObject()->outerClassConstructor().value(), propertyName, value);
    }
    THROW_BUILTIN_ERROR_RETURN(state, ErrorCode::TypeError, ErrorObject::Messages::CanNotWritePrivateMember, propertyName.string());
}

IteratorObject* Object::values(ExecutionState& state)
{
    return new ArrayIteratorObject(state, this, ArrayIteratorObject::TypeValue);
}

IteratorObject* Object::keys(ExecutionState& state)
{
    return new ArrayIteratorObject(state, this, ArrayIteratorObject::TypeKey);
}

IteratorObject* Object::entries(ExecutionState& state)
{
    return new ArrayIteratorObject(state, this, ArrayIteratorObject::TypeKeyValue);
}

ALWAYS_INLINE static void enumerableOwnPropertiesPushResult(ExecutionState& state, ValueVectorWithInlineStorage& properties, Object* object, const Value& key, EnumerableOwnPropertiesType kind)
{
    if (kind == EnumerableOwnPropertiesType::Key) {
        // 1. If kind is "key", append key to properties
        properties.pushBack(key);
    } else {
        // 2. Else
        // a. Let value be ? Get(O, key).
        ObjectPropertyName propertyName(state, key);
        Value value = object->get(state, propertyName).value(state, object);
        // b. If kind is "value", append value to properties.
        if (kind == EnumerableOwnPropertiesType::Value) {
            properties.pushBack(value);
        } else {
            // c. else
            // i. Assert: kind is "key+value".
            ASSERT(kind == EnumerableOwnPropertiesType::KeyAndValue);
            // ii. Let entry be CreateArrayFromList(« key, value »).
            Value v[2] = { key, value };
            auto entry = Object::createArrayFromList(state, 2, v);
            // iii. Append entry to properties.
            properties.pushBack(entry);
        }
    }
}

ValueVectorWithInlineStorage Object::enumerableOwnProperties(ExecutionState& state, Object* object, EnumerableOwnPropertiesType kind)
{
    // https://www.ecma-international.org/ecma-262/8.0/#sec-enumerableownproperties
    if (object->canUseOwnPropertyKeysFastPath()) {
        // FAST PATH
        Object::OwnPropertyKeyAndDescVector ownKeysAndDesc = object->ownPropertyKeysFastPath(state);

        bool seenAccessorProperty = false;
        ValueVectorWithInlineStorage properties;
        size_t size = ownKeysAndDesc.size();
        for (size_t i = 0; i < size; ++i) {
            const Value& key = ownKeysAndDesc[i].first;
            const ObjectStructurePropertyDescriptor& desc = ownKeysAndDesc[i].second;
            if (key.isSymbol()) {
                continue;
            }

            if (seenAccessorProperty || desc.isAccessorProperty()) {
                seenAccessorProperty = true; // we need to full evaluation after saw accessor. Accessors can delete properties
                ObjectPropertyName propertyName(state, key);
                auto desc = object->getOwnProperty(state, propertyName);
                if (desc.hasValue() && desc.isEnumerable()) {
                    enumerableOwnPropertiesPushResult(state, properties, object, key, kind);
                }
            } else {
                if (desc.isEnumerable()) {
                    enumerableOwnPropertiesPushResult(state, properties, object, key, kind);
                }
            }
        }
        return properties;
    }

    // 1. Assert: Type(O) is Object.
    // 2. Let ownKeys be ? O.[[OwnPropertyKeys]]().
    auto ownKeys = object->ownPropertyKeys(state);
    // 3. Let properties be a new empty List.
    ValueVectorWithInlineStorage properties;

    // 4. For each element key of ownKeys in List order, do
    for (size_t i = 0; i < ownKeys.size(); ++i) {
        auto& key = ownKeys[i];
        // a. If Type(key) is String, then
        if (key.isString()) {
            auto propertyName = ObjectPropertyName(state, key);

            // i. Let desc be ? O.[[GetOwnProperty]](key).
            auto desc = object->getOwnProperty(state, propertyName);
            // ii. If desc is not undefined and desc.[[Enumerable]] is true, then
            if (desc.hasValue() && desc.isEnumerable()) {
                enumerableOwnPropertiesPushResult(state, properties, object, key, kind);
            }
        }
    }
    // 5. Order the elements of properties so they are in the same relative order as would be produced by
    // the Iterator that would be returned if the EnumerateObjectProperties internal method were invoked with O.

    // 6. Return properties.
    return properties;
}

Value Object::speciesConstructor(ExecutionState& state, const Value& defaultConstructor)
{
    ASSERT(isObject());
    Value C = asObject()->get(state, state.context()->staticStrings().constructor).value(state, this);
    RETURN_VALUE_IF_PENDING_EXCEPTION

    if (C.isUndefined()) {
        return defaultConstructor;
    }

    if (!C.isObject()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, "constructor is not an object");
    }

    Value S = C.asObject()->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species)).value(state, C);
    RETURN_VALUE_IF_PENDING_EXCEPTION

    if (S.isUndefinedOrNull()) {
        return defaultConstructor;
    }

    if (S.isConstructor()) {
        return S;
    }

    THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, "invalid speciesConstructor return");
    return Value();
}

bool Object::isRegExp(ExecutionState& state)
{
    Value symbol = get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match)).value(state, this);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    if (!symbol.isUndefined()) {
        return symbol.toBoolean(state);
    }
    return isRegExpObject();
}

void Object::addFinalizer(ObjectFinalizer fn, void* data)
{
    auto r = ensureExtendedExtraData();
    if (!rareData()->m_isFinalizerRegistered) {
        rareData()->m_isFinalizerRegistered = true;

#define FINALIZER_CALLBACK()                                         \
    Object* self = (Object*)obj;                                     \
    auto r = self->extendedExtraData();                              \
    for (size_t i = 0; i < r->m_finalizer.size(); i++) {             \
        if (LIKELY(!!r->m_finalizer[i].first)) {                     \
            r->m_finalizer[i].first(self, r->m_finalizer[i].second); \
        }                                                            \
    }                                                                \
    r->m_finalizer.clear();

#ifndef NDEBUG
        GC_finalization_proc of = nullptr;
        void* od = nullptr;
        GC_REGISTER_FINALIZER_NO_ORDER(
            this, [](void* obj, void*) {
                FINALIZER_CALLBACK()
            },
            nullptr, &of, &od);
        ASSERT(!of);
        ASSERT(!od);
#else
        GC_REGISTER_FINALIZER_NO_ORDER(
            this, [](void* obj, void*) {
                FINALIZER_CALLBACK()
            },
            nullptr, nullptr, nullptr);
#endif
    }

    if (UNLIKELY(r->m_removedFinalizerCount)) {
        for (size_t i = 0; i < r->m_finalizer.size(); i++) {
            if (r->m_finalizer[i].first == nullptr) {
                ASSERT(r->m_finalizer[i].second == nullptr);
                r->m_finalizer[i].first = fn;
                r->m_finalizer[i].second = data;
                r->m_removedFinalizerCount--;
                return;
            }
        }
        ASSERT_NOT_REACHED();
    }

    r->m_finalizer.pushBack(std::make_pair(fn, data));
}

bool Object::removeFinalizer(ObjectFinalizer fn, void* data)
{
    auto r = extendedExtraData();
    for (size_t i = 0; i < r->m_finalizer.size(); i++) {
        if (r->m_finalizer[i].first == fn && r->m_finalizer[i].second == data) {
            r->m_finalizer[i].first = nullptr;
            r->m_finalizer[i].second = nullptr;
            r->m_removedFinalizerCount++;

            tryToShrinkFinalizers();
            return true;
        }
    }
    return false;
}

void Object::tryToShrinkFinalizers()
{
    auto r = extendedExtraData();
    auto oldFinalizer = r->m_finalizer;
    size_t oldSize = oldFinalizer.size();
    if (r->m_removedFinalizerCount > ((oldSize / 2) + 1)) {
        ASSERT(r->m_removedFinalizerCount <= oldSize);
        size_t newSize = oldSize - r->m_removedFinalizerCount;
        TightVector<std::pair<ObjectFinalizer, void*>, GCUtil::gc_malloc_atomic_allocator<std::pair<ObjectFinalizer, void*>>> newFinalizer;
        newFinalizer.resizeWithUninitializedValues(newSize);

        size_t j = 0;
        for (size_t i = 0; i < oldSize; i++) {
            if (oldFinalizer[i].first) {
                newFinalizer[j].first = oldFinalizer[i].first;
                newFinalizer[j].second = oldFinalizer[i].second;
                j++;
            }
        }
        ASSERT(j == newSize);

        r->m_finalizer = std::move(newFinalizer);
        r->m_removedFinalizerCount = 0;
    }
}

Object::FastLookupSymbolResult Object::fastLookupForSymbol(ExecutionState& state, Symbol* s, Optional<Object*> protochainSearchStopAt)
{
    ObjectStructurePropertyName name(s);
    Object* obj = this;
    while (true) {
        if (protochainSearchStopAt.unwrap() == obj) {
            return FastLookupSymbolResult(true, obj, SIZE_MAX);
        }

        if (!obj->isInlineCacheable()) {
            break;
        }

        auto structure = obj->structure();
        if (structure->hasSymbolPropertyName()) {
            auto ret = structure->findProperty(name).first;
            if (ret != SIZE_MAX) {
                return FastLookupSymbolResult(true, obj, ret);
            }
        }
        obj = obj->Object::getPrototypeObject(state);
        if (!obj) {
            return FastLookupSymbolResult(true, nullptr, SIZE_MAX);
        }
    }

    // fast path was failed
    return FastLookupSymbolResult();
}

bool DerivedObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    // check indexed property to confirm that there is no indexed property in prototype objects
    if (UNLIKELY(isEverSetAsPrototypeObject() && !state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && P.isIndexString())) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
    }

    return defineOwnPropertyMethod(state, P, desc);
}
} // namespace Escargot
