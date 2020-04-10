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
#include "ObjectStructure.h"
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
#include "ProxyObject.h"
#include "util/Util.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

COMPILE_ASSERT(OBJECT_PROPERTY_NAME_UINT32_VIAS == (PROPERTY_NAME_ATOMIC_STRING_VIAS << 1), "");
COMPILE_ASSERT(OBJECT_PROPERTY_NAME_UINT32_VIAS <= (1 << 3), "");

ObjectStructurePropertyName::ObjectStructurePropertyName(ExecutionState& state, const Value& valueIn)
{
    Value value = valueIn.toPrimitive(state, Value::PreferString);
    if (UNLIKELY(value.isSymbol())) {
        m_data = (size_t)value.asSymbol();
        return;
    }
    String* string = value.toString(state);
    const auto& data = string->bufferAccessData();
    if (data.length == 0) {
        m_data = ((size_t)AtomicString().string()) | PROPERTY_NAME_ATOMIC_STRING_VIAS;
        return;
    }
    bool needsRemainNormalString = false;
    char16_t c = data.has8BitContent ? ((LChar*)data.buffer)[0] : ((char16_t*)data.buffer)[0];
    if ((c == '.' || (c >= '0' && c <= '9')) && data.length > 16) {
        needsRemainNormalString = true;
    }

    if (UNLIKELY(needsRemainNormalString)) {
        m_data = (size_t)string;
    } else {
        if (c < ESCARGOT_ASCII_TABLE_MAX && (data.length == 1)) {
            m_data = ((size_t)state.context()->staticStrings().asciiTable[c].string()) | PROPERTY_NAME_ATOMIC_STRING_VIAS;
        } else {
            m_data = ((size_t)AtomicString(state, string).string()) | PROPERTY_NAME_ATOMIC_STRING_VIAS;
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
{
    if (obj)
        m_prototype = obj->m_prototype;
    else
        m_prototype = nullptr;
    m_isExtensible = true;
    m_isEverSetAsPrototypeObject = false;
    m_isFastModeArrayObject = true;
    m_isArrayObjectLengthWritable = true;
    m_isSpreadArrayObject = false;
    m_shouldUpdateEnumerateObject = false;
    m_hasNonWritableLastIndexRegexpObject = false;
    m_arrayObjectFastModeBufferExpandCount = 0;
    m_extraData = nullptr;
    m_internalSlot = nullptr;
}

void* ObjectRareData::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
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
#ifdef ESCARGOT_32
    if (m_jsGetterSetter->getter().isCallable()) {
#else
    if (m_jsGetterSetter->hasGetter() && m_jsGetterSetter->getter().isCallable()) {
#endif
        return Object::call(state, m_jsGetterSetter->getter(), receiver, 0, nullptr);
    }
    return Value();
}

Value ObjectGetResult::toPropertyDescriptor(ExecutionState& state, const Value& receiver)
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
    if (desc.hasValue())
        setEnumerable(desc.value(state, obj).toBoolean(state));
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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Getter must be a function or undefined");
        } else {
            m_isDataProperty = false;
            m_getterSetter = JSGetterSetter(getter, Value(Value::EmptyValue));
        }
    }

    desc = obj->get(state, ObjectPropertyName(strings->set));
    if (desc.hasValue()) {
        Value setter = desc.value(state, obj);
        if (!setter.isCallable() && !setter.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Setter must be a function or undefined");
        } else {
            if (m_isDataProperty) {
                m_isDataProperty = false;
                m_getterSetter = JSGetterSetter(Value(Value::EmptyValue), setter);
            } else {
                m_getterSetter.m_setter = setter;
            }
        }
    }

    if (!m_isDataProperty && (hasWritable | hasValue)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid property descriptor. Cannot both specify accessors and a value or writable attribute");
    }

    checkProperty();
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
    ASSERT(proto->hasRareData() && proto->rareData()->m_isEverSetAsPrototypeObject);
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
    ASSERT(proto->hasRareData() && proto->rareData()->m_isEverSetAsPrototypeObject);
    m_values.resizeWithUninitializedValues(0, defaultSpace);
}

// this constructor is used only for initialization of GlobalObject
Object::Object(ExecutionState& state, size_t defaultSpace, ForGlobalBuiltin)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(nullptr)
{
    m_values.resizeWithUninitializedValues(0, defaultSpace);
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
    // If spreadable is not undefined, return ToBoolean(spreadable).
    if (!spreadable.isUndefined()) {
        return spreadable.toBoolean(state);
    }
    // Return IsArray(O).
    return isArray(state);
}

Object* Object::createBuiltinObjectPrototype(ExecutionState& state)
{
    Object* obj = new Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, Object::__ForGlobalBuiltin__);
    obj->setPrototype(state, Value(Value::Null));
    obj->markThisObjectDontNeedStructureTransitionTable();
    obj->ensureRareData()->m_isEverSetAsPrototypeObject = true;

    return obj;
}

Object* Object::createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function)
{
    auto ctx = function->codeBlock()->context();
    Object* obj = new Object(state, ctx->globalObject()->objectPrototype()->asObject(), ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1);
    obj->m_structure = ctx->defaultStructureForFunctionPrototypeObject();
    obj->m_values[0] = Value(function);

    return obj;
}

void Object::setGlobalIntrinsicObject(ExecutionState& state, bool isPrototype)
{
    // For initialization of GlobalObject's intrinsic objects
    // These objects have fixed properties, so transition table is not used for memory optimization
    ASSERT(m_prototype);
    ASSERT(!hasRareData());
    ASSERT(!state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty());
    ASSERT(!structure()->hasIndexPropertyName());

    markThisObjectDontNeedStructureTransitionTable();

    if (isPrototype) {
        ensureRareData()->m_isEverSetAsPrototypeObject = true;
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
    if (proto == this->getPrototype(state)) {
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

    // 10. Return true.
    return true;
}

void Object::markAsPrototypeObject(ExecutionState& state)
{
    ensureRareData()->m_isEverSetAsPrototypeObject = true;

    if (UNLIKELY(!state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && (structure()->hasIndexPropertyName() || isProxyObject()))) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
    }
}

ObjectGetResult Object::getOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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
                return ObjectGetResult(data->m_getter(state, this, m_values[findResult.first]), presentAttributes & ObjectStructurePropertyDescriptor::WritablePresent, presentAttributes & ObjectStructurePropertyDescriptor::EnumerablePresent, presentAttributes & ObjectStructurePropertyDescriptor::ConfigurablePresent);
            }
        } else {
            const Value& v = m_values[findResult.first];
            ASSERT(v.isPointerValue() && v.asPointerValue()->isJSGetterSetter());
            return ObjectGetResult(v.asPointerValue()->asJSGetterSetter(), presentAttributes & ObjectStructurePropertyDescriptor::EnumerablePresent, presentAttributes & ObjectStructurePropertyDescriptor::ConfigurablePresent);
        }
    }
    return ObjectGetResult();
}

bool Object::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (UNLIKELY(isEverSetAsPrototypeObject() && !state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && P.isIndexString())) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
    }

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
                    return setOwnDataPropertyUtilForObjectInner(state, findResult.first, *item, newDesc.value());
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
                return setOwnDataPropertyUtilForObjectInner(state, idx, m_structure->readProperty(idx), newDesc.value());
            } else {
                m_values[idx] = Value(new JSGetterSetter(newDesc.getterSetter()));
            }
        }

        return true;
    }
}

bool Object::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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

void Object::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    const ObjectStructureItem* propertiesVector;
    propertiesVector = m_structure->properties();
    size_t cnt = m_structure->propertyCount();
    bool inTransitionMode = m_structure->inTransitionMode();
    if (!inTransitionMode) {
        auto newData = ALLOCA(sizeof(ObjectStructureItem) * cnt, ObjectStructureItem, state);
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

template <typename ResultType, typename ResultBinder>
static ResultType objectOwnPropertyKeys(ExecutionState& state, Object* self)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-ownpropertykeys
    struct Properties {
        VectorWithInlineStorage<32, std::pair<Value::ValueIndex, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<Value::ValueIndex, ObjectStructurePropertyDescriptor>>> indexes;
        VectorWithInlineStorage<32, std::pair<String*, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<String*, ObjectStructurePropertyDescriptor>>> strings;
        VectorWithInlineStorage<4, std::pair<Symbol*, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<Symbol*, ObjectStructurePropertyDescriptor>>> symbols;
    } properties;

    self->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        auto properties = (Properties*)data;

        if (name.isUIntType()) {
            properties->indexes.push_back(std::make_pair(name.uintValue(), desc));
        } else {
            const ObjectStructurePropertyName& propertyName = name.objectStructurePropertyName();

            if (propertyName.isSymbol()) {
                properties->symbols.push_back(std::make_pair(propertyName.symbol(), desc));
            } else {
                String* name = propertyName.plainString();
                auto idx = name->tryToUseAsArrayIndex();
                if (idx == Value::InvalidArrayIndexValue) {
                    properties->strings.push_back(std::make_pair(name, desc));
                } else {
                    properties->indexes.push_back(std::make_pair(idx, desc));
                }
            }
        }
        return true;
    },
                      &properties, false);

    std::sort(properties.indexes.begin(), properties.indexes.end(),
              [](const std::pair<Value::ValueIndex, ObjectStructurePropertyDescriptor>& a, const std::pair<Value::ValueIndex, ObjectStructurePropertyDescriptor>& b) -> bool {
                  return a.first < b.first;
              });


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
ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName)
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
            return iter->get(state, propertyName);
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
    if (!ownDesc.hasValue()) {
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
        // 5.c. Let existingDescriptor be Receiver.[[GetOwnProperty]](P).
        Object* receiverObj = receiver.asObject();
        auto existingDesc = receiverObj->getOwnProperty(state, propertyName);
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

// https://www.ecma-international.org/ecma-262/6.0/#sec-getmethod
Value Object::getMethod(ExecutionState& state, const Value& O, const ObjectPropertyName& propertyName)
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
    } else {
        obj = O.toObject(state); // this always cause type error
    }

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
    // 4. If func is either undefined or null, return undefined.
    if (func.isUndefinedOrNull()) {
        return nullptr;
    }
    // 5. If IsCallable(func) is false, throw a TypeError exception.
    if (!func.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, String::emptyString, false, String::emptyString, "%s: return value of getMethod is not callable");
    }
    // 6. Return func.
    return Optional<Object*>(func.asObject());
}

Object* Object::getPrototypeFromConstructor(ExecutionState& state, Object* constructor, Object* (*intrinsicDefaultProtoGetter)(ExecutionState& state, Context* context))
{
    // Assert: intrinsicDefaultProto is a String value that is this specification's name of an intrinsic object. The corresponding object must be an intrinsic that is intended to be used as the [[Prototype]] value of an object.
    // Assert: IsCallable(constructor) is true.
    ASSERT(constructor->isCallable());

    // Let proto be ? Get(constructor, "prototype").
    Value proto = constructor->get(state, ObjectPropertyName(state.context()->staticStrings().prototype)).value(state, constructor);

    // If Type(proto) is not Object, then
    if (!proto.isObject()) {
        // Let realm be ? GetFunctionRealm(constructor).
        // Set proto to realm's intrinsic object named intrinsicDefaultProto.
        proto = intrinsicDefaultProtoGetter(state, constructor->getFunctionRealm(state));
    } else {
        proto.asObject()->markAsPrototypeObject(state);
    }
    // Return proto.
    return proto.asObject();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-call
Value Object::call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    if (!callee.isPointerValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    // Return F.[[Call]](V, argumentsList).
    return callee.asPointerValue()->call(state, thisValue, argc, argv);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-construct
Object* Object::construct(ExecutionState& state, const Value& constructor, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    // If newTarget was not passed, let newTarget be F.
    if (newTarget == nullptr) {
        newTarget = constructor.asObject();
    }

    ASSERT(constructor.isConstructor());
    ASSERT(newTarget->isConstructor());

    return constructor.asPointerValue()->construct(state, argc, argv, newTarget);
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
    // If Type(P) is not Object, throw a TypeError exception.
    if (!P.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::InstanceOf_InvalidPrototypeProperty);
    }
    // Repeat
    O = O.asObject()->getPrototype(state);
    while (!O.isNull()) {
        // If O is null, return false.
        // If SameValue(P, O) is true, return true.
        if (P == O) {
            return true;
        }
        // Let O be O.[[GetPrototypeOf]]().
        O = O.asObject()->getPrototype(state);
    }
    return false;
}

bool Object::isCompatiblePropertyDescriptor(ExecutionState& state, bool extensible, const ObjectPropertyDescriptor& desc, const ObjectGetResult current)
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
            JSGetterSetter* currgs = current.value(state, Value()).asPointerValue()->asJSGetterSetter();
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
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
    }
}

void Object::setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver)) && state.inStrictMode()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
    }
}

void Object::throwCannotDefineError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_RedefineNotConfigurable);
}

void Object::throwCannotWriteError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
}

void Object::throwCannotDeleteError(ExecutionState& state, const ObjectStructurePropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotConfigurable);
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
    // https://www.ecma-international.org/ecma-262/6.0/#sec-createlistfromarraylike
    auto strings = &state.context()->staticStrings();

    // 1. ReturnIfAbrupt(obj).
    // 2. If elementTypes was not passed, let elementTypes be (Undefined, Null, Boolean, String, Symbol, Number, Object).

    // 3. If Type(obj) is not Object, throw a TypeError exception.
    if (!obj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->object.string(), false, String::emptyString, "%s: obj is not Object");
    }

    // 4. Let len be ToLength(Get(obj, "length")).
    // 5. ReturnIfAbrupt(len).
    Object* o = obj.asObject();
    auto len = o->length(state);

    // Honorate "length" property: If length>2^32-1, throw a RangeError exception.
    if (len > ((1LL << 32LL) - 1LL)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
    }

    // 6. Let list be an empty List.
    ValueVector list;
    // 7. Let index be 0.
    uint64_t index = 0;
    //8. Repeat while index < len
    while (index < len) {
        // a. Let indexName be ToString(index).
        auto indexName = Value(index).toString(state);
        // b. Let next be Get(obj, indexName).
        // c. ReturnIfAbrupt(next).
        auto next = o->get(state, ObjectPropertyName(state, indexName)).value(state, o);

        // d. If Type(next) is not an element of elementTypes, throw a TypeError exception.
        if (!(((elementTypes & (uint8_t)ElementTypes::Undefined) && next.isUndefined()) || ((elementTypes & (uint8_t)ElementTypes::Null) && next.isNull()) || ((elementTypes & (uint8_t)ElementTypes::Boolean) && next.isBoolean()) || ((elementTypes & (uint8_t)ElementTypes::String) && next.isString()) || ((elementTypes & (uint8_t)ElementTypes::Symbol) && next.isSymbol()) || ((elementTypes & (uint8_t)ElementTypes::Number) && next.isNumber()) || ((elementTypes & (uint8_t)ElementTypes::Object) && next.isObject()))) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->object.string(), false, String::emptyString, "%s: Type(next) is not an element of elementTypes");
        }
        // e. Append next as the last element of list.
        list.pushBack(next);
        // f. Set index to index + 1.
        index++;
    }

    // 9. Return list.
    return list;
}

void Object::deleteOwnProperty(ExecutionState& state, size_t idx)
{
    m_structure = m_structure->removeProperty(idx);
    m_values.erase(idx, m_structure->propertyCount() + 1);

    // ASSERT(m_values.size() == m_structure->propertyCount());
}

uint64_t Object::length(ExecutionState& state)
{
    // ToLength(Get(obj, "length"))
    return get(state, state.context()->staticStrings().length).value(state, this).toLength(state);
}

bool Object::isArray(ExecutionState& state)
{
    if (isArrayObject()) {
        return true;
    }
    if (isProxyObject()) {
        ProxyObject* proxy = asProxyObject();
        if (proxy->handler() == nullptr) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Proxy.string(), false, String::emptyString, "%s: Proxy handler should not null.");
            return false;
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

        ptr.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
            int64_t index;
            Data* e = (Data*)data;
            int64_t* ret = e->ret;
            Value key = name.toPlainValue(state);
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
        ptr.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
            int64_t index;
            Data* e = (Data*)data;
            int64_t* ret = e->ret;
            Value key = name.toPlainValue(state);
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
#ifdef ESCARGOT_32
    if (gs->getter().isCallable()) {
#else
    if (gs->hasGetter() && gs->getter().isCallable()) {
#endif
        return Object::call(state, gs->getter(), receiver, 0, nullptr);
    }
    return Value();
}

bool Object::setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
{
    Value v = m_values[idx];
    auto gs = v.asPointerValue()->asJSGetterSetter();
#ifdef ESCARGOT_32
    if (gs->setter().isCallable()) {
#else
    if (gs->hasSetter() && gs->setter().isCallable()) {
#endif
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

    return true;
}

ObjectGetResult Object::getIndexedProperty(ExecutionState& state, const Value& property)
{
    return get(state, ObjectPropertyName(state, property));
}

bool Object::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    return set(state, ObjectPropertyName(state, property), value, this);
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

    if (C.isUndefined()) {
        return defaultConstructor;
    }

    if (!C.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "constructor is not an object");
    }

    Value S = C.asObject()->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species)).value(state, C);

    if (S.isUndefinedOrNull()) {
        return defaultConstructor;
    }

    if (S.isConstructor()) {
        return S;
    }

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "invalid speciesConstructor return");
    return Value();
}

String* Object::optionString(ExecutionState& state)
{
    char flags[7] = { 0 };
    size_t flagsIdx = 0;
    size_t cacheIndex = 0;

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().global)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 'g';
        cacheIndex |= 1 << 0;
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().ignoreCase)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 'i';
        cacheIndex |= 1 << 1;
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().multiline)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 'm';
        cacheIndex |= 1 << 2;
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().dotAll)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 's';
        cacheIndex |= 1 << 3;
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 'u';
        cacheIndex |= 1 << 4;
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().sticky)).value(state, this).toBoolean(state)) {
        flags[flagsIdx++] = 'y';
        cacheIndex |= 1 << 5;
    }

    ASCIIString* result;
    auto cache = state.context()->vmInstance()->regexpOptionStringCache();
    if (cache[cacheIndex]) {
        result = cache[cacheIndex];
    } else {
        result = cache[cacheIndex] = new ASCIIString(flags, flagsIdx);
    }

    return result;
}

bool Object::isRegExp(ExecutionState& state)
{
    Value symbol = get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().match)).value(state, this);
    if (!symbol.isUndefined()) {
        return symbol.toBoolean(state);
    }
    return isRegExpObject();
}
}
