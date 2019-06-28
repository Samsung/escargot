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
#include "Object.h"
#include "ObjectStructure.h"
#include "Context.h"
#include "VMInstance.h"
#include "ErrorObject.h"
#include "FunctionObject.h"
#include "ArrayObject.h"
#include "BoundFunctionObject.h"
#include "util/Util.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

PropertyName::PropertyName(ExecutionState& state, const Value& valueIn)
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

PropertyName ObjectPropertyName::toPropertyNameUintCase(ExecutionState& state) const
{
    ASSERT(isUIntType());

    auto uint = uintValue();
    if (uint < ESCARGOT_STRINGS_NUMBERS_MAX) {
        return PropertyName(state.context()->staticStrings().numbers[uint]);
    }

    return PropertyName(state, String::fromDouble(uint));
}

size_t g_objectRareDataTag;

ObjectRareData::ObjectRareData(Object* obj)
{
    if (obj)
        m_prototype = obj->m_prototype;
    else
        m_prototype = nullptr;
    m_isExtensible = true;
    m_isEverSetAsPrototypeObject = false;
    m_isFastModeArrayObject = true;
    m_shouldUpdateEnumerateObjectData = false;
    m_isInArrayObjectDefineOwnProperty = false;
    m_hasNonWritableLastIndexRegexpObject = false;
    m_extraData = nullptr;
#ifdef ESCARGOT_ENABLE_PROMISE
    m_internalSlot = nullptr;
#endif
}

void* ObjectRareData::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectRareData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_extraData));
#ifdef ESCARGOT_ENABLE_PROMISE
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectRareData, m_internalSlot));
#endif
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
        obj->defineOwnProperty(state, ObjectPropertyName(state, strings->value.string()), ObjectPropertyDescriptor(desc.isValuePresent() ? desc.value() : Value(), ObjectPropertyDescriptor::AllPresent));
        obj->defineOwnProperty(state, ObjectPropertyName(state, strings->writable.string()), ObjectPropertyDescriptor(Value(desc.isWritable()), ObjectPropertyDescriptor::AllPresent));
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
    obj->defineOwnProperty(state, ObjectPropertyName(state, strings->enumerable.string()), ObjectPropertyDescriptor(Value(desc.isEnumerable()), ObjectPropertyDescriptor::AllPresent));
    // 9. If Desc has a [[Configurable]] field, then
    // 9.a. Perform CreateDataProperty(obj , "configurable", Desc.[[Configurable]]).
    obj->defineOwnProperty(state, ObjectPropertyName(state, strings->configurable.string()), ObjectPropertyDescriptor(Value(desc.isConfigurable()), ObjectPropertyDescriptor::AllPresent));
    // 10. Assert: all of the above CreateDataProperty operations return true.
    // 11. Return obj.
    return obj;
}

size_t g_objectTag;

Object::Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea)
    : m_structure(state.context()->defaultStructureForObject())
    , m_prototype(nullptr)
{
    m_values.resizeWithUninitializedValues(0, defaultSpace);
    if (initPlainArea) {
        initPlainObject(state);
    }
}

Object::Object(ExecutionState& state)
    : m_structure(state.context()->defaultStructureForObject())
{
    m_values.resizeWithUninitializedValues(0, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER);
    initPlainObject(state);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-isconcatspreadable
bool Object::isConcatSpreadable(ExecutionState& state)
{
    // If Type(O) is not Object, return false.
    if (!isObject()) {
        return false;
    }
    // Let spreadable be Get(O, @@isConcatSpreadable).
    ObjectGetResult spreadable = get(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().isConcatSpreadable));
    Value val = spreadable.value(state, Value());
    // If spreadable is not undefined, return ToBoolean(spreadable).
    if (!val.isUndefined()) {
        return val.toBoolean(state);
    }
    // Return IsArray(O).
    return isArrayObject();
}

void Object::initPlainObject(ExecutionState& state)
{
    m_prototype = state.context()->globalObject()->objectPrototype()->asObject();
}

Object* Object::createBuiltinObjectPrototype(ExecutionState& state)
{
    Object* obj = new Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, false);
    obj->m_structure = state.context()->defaultStructureForObject();
    obj->m_prototype = nullptr;
    g_objectTag = *((size_t*)obj);
    return obj;
}

Object* Object::createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function)
{
    Object* obj = new Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, false);
    obj->m_structure = state.context()->defaultStructureForFunctionPrototypeObject();
    obj->m_prototype = state.context()->globalObject()->objectPrototype()->asObject();
    obj->m_values[0] = Value(function);

    return obj;
}

bool Object::setPrototype(ExecutionState& state, const Value& proto)
{
    if (!proto.isObject() && !proto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't set prototype of this object");
        return false;
    }

    Value object = this;
    Value current = this->getPrototype(state);

    if (proto == current) {
        return true;
    }

    if (UNLIKELY(!isOrdinary() || !isExtensible(state))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't set prototype of this object");
        return false;
    }

    Value nextProto = proto;
    while (nextProto && nextProto.isObject()) {
        if (nextProto.asObject() == this) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "cyclic __proto__");
            return false;
        }
        if (UNLIKELY(!nextProto.asObject()->isOrdinary())) {
            break;
        }
        nextProto = nextProto.asObject()->getPrototype(state);
    }

    Object* o = nullptr;
    if (LIKELY(proto.isObject())) {
        proto.asObject()->markAsPrototypeObject(state);
        o = proto.asObject();
    }

    if (rareData()) {
        rareData()->m_prototype = o;
    } else {
        m_prototype = o;
    }

    return true;
}

void Object::markAsPrototypeObject(ExecutionState& state)
{
    ensureObjectRareData();
    rareData()->m_isEverSetAsPrototypeObject = true;

    if (!state.context()->vmInstance()->didSomePrototypeObjectDefineIndexedProperty() && structure()->hasIndexPropertyName()) {
        state.context()->vmInstance()->somePrototypeObjectDefineIndexedProperty(state);
    }
}

ObjectGetResult Object::getOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (propertyName.isUIntType() && !m_structure->hasIndexPropertyName()) {
        return ObjectGetResult();
    }
    PropertyName P = propertyName.toPropertyName(state);
    size_t idx = m_structure->findProperty(state, P);
    if (LIKELY(idx != SIZE_MAX)) {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            if (LIKELY(!item.m_descriptor.isNativeAccessorProperty())) {
                return ObjectGetResult(m_values[idx], item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
            } else {
                ObjectPropertyNativeGetterSetterData* data = item.m_descriptor.nativeGetterSetterData();
                return ObjectGetResult(data->m_getter(state, this, m_values[idx]), item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
            }
        } else {
            Value v = m_values[idx];
            ASSERT(v.isPointerValue() && v.asPointerValue()->isJSGetterSetter());
            return ObjectGetResult(v.asPointerValue()->asJSGetterSetter(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
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

    PropertyName propertyName = P.toPropertyName(state);
    size_t oldIdx = m_structure->findProperty(state, propertyName);
    if (oldIdx == SIZE_MAX) {
        // 3. If current is undefined and extensible is false, then Reject.
        if (UNLIKELY(!isExtensible(state))) {
            return false;
        }

        auto structureBefore = m_structure;
        m_structure = m_structure->addProperty(state, propertyName, desc.toObjectStructurePropertyDescriptor());
        ASSERT(structureBefore != m_structure);
        if (LIKELY(desc.isDataProperty())) {
            const Value& val = desc.isValuePresent() ? desc.value() : Value();
            m_values.pushBack(val, m_structure->propertyCount());

            if (val.isObject() && val.asObject()->isFunctionObject()) {
                val.asObject()->asFunctionObject()->setHomeObject(this);
            }
        } else {
            m_values.pushBack(Value(new JSGetterSetter(desc.getterSetter())), m_structure->propertyCount());
        }

        // ASSERT(m_values.size() == m_structure->propertyCount());
        return true;
    } else {
        size_t idx = oldIdx;
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        auto current = item.m_descriptor;

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
        } else if (item.m_descriptor.isDataProperty() && desc.isDataDescriptor()) {
            // If the [[Configurable]] field of current is false, then
            if (!item.m_descriptor.isConfigurable()) {
                // Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (!item.m_descriptor.isWritable() && desc.isWritable()) {
                    return false;
                }
                // If the [[Writable]] field of current is false, then
                // Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                if (!item.m_descriptor.isWritable() && desc.isValuePresent() && !desc.value().equalsToByTheSameValueAlgorithm(state, getOwnDataPropertyUtilForObject(state, idx, this))) {
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
                    return setOwnDataPropertyUtilForObjectInner(state, idx, item, newDesc.value());
                }
            } else {
                m_values[idx] = Value(new JSGetterSetter(newDesc.getterSetter()));
            }
        } else {
            auto structureBefore = m_structure;
            if (!structure()->isStructureWithFastAccess()) {
                m_structure = structure()->convertToWithFastAccess(state);
            }

            if (newDesc.isDataDescriptor() && m_structure->m_properties[idx].m_descriptor.isNativeAccessorProperty()) {
                auto newNative = new ObjectPropertyNativeGetterSetterData(newDesc.isWritable(), newDesc.isEnumerable(), newDesc.isConfigurable(),
                                                                          m_structure->m_properties[idx].m_descriptor.nativeGetterSetterData()->m_getter, m_structure->m_properties[idx].m_descriptor.nativeGetterSetterData()->m_setter);
                m_structure->m_properties[idx].m_descriptor = ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(newNative);
            } else {
                m_structure->m_properties[idx].m_descriptor = newDesc.toObjectStructurePropertyDescriptor();
            }

            m_structure = new ObjectStructureWithFastAccess(state, *((ObjectStructureWithFastAccess*)m_structure));

            ASSERT(structureBefore != m_structure);
            if (newDesc.isDataDescriptor()) {
                return setOwnDataPropertyUtilForObjectInner(state, idx, m_structure->m_properties[idx], newDesc.value());
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
        deleteOwnProperty(state, m_structure->findProperty(state, P.toPropertyName(state)));
        return true;
    } else if (result.hasValue() && !result.isConfigurable()) {
        return false;
    }
    return true;
}

void Object::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ObjectStructure structure(*m_structure);
    size_t cnt = structure.propertyCount();
    for (size_t i = 0; i < cnt; i++) {
        const ObjectStructureItem& item = structure.readProperty(state, i);
        if (shouldSkipSymbolKey && item.m_propertyName.isSymbol()) {
            continue;
        }
        if (!callback(state, this, ObjectPropertyName(state, item.m_propertyName), item.m_descriptor, data)) {
            break;
        }
    }
}

ValueVector Object::getOwnPropertyKeys(ExecutionState& state)
{
    ValueVector result;
    enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        ValueVector* result = (ValueVector*)data;
        result->pushBack(name.toPlainValue(state));
        return true;
    },
                &result, false);
    return result;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-get-p-receiver
ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    Object* temp = this;
    auto desc = this->getOwnProperty(state, propertyName);
    while (true) {
        if (!desc.hasValue()) {
            Object* parent = temp->getPrototypeObject(state);
            if (!parent) {
                return ObjectGetResult();
            }
            desc = parent->getOwnProperty(state, propertyName);
            temp = parent;
        } else {
            break;
        }
    }
    return desc;
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
Value Object::getMethod(ExecutionState& state, const Value& object, const ObjectPropertyName& propertyName)
{
    // 2. Let func be GetV(O, P).
    Object* obj = object.toObject(state);
    Value func = obj->get(state, propertyName).value(state, object);
    // 4. If func is either undefined or null, return undefined.
    if (func.isUndefinedOrNull()) {
        return Value();
    }
    // 5. If IsCallable(func) is false, throw a TypeError exception.
    if (!func.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, String::emptyString, false, String::emptyString, "%s: return value of getMethod is not callable");
    }
    // 6. Return func.
    return func;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-call
Value Object::call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    // If IsCallable(F) is false, throw a TypeError exception.
    if (!callee.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_NOT_Callable);
    }
    // Return F.[[Call]](V, argumentsList).
    return callee.asObject()->call(state, thisValue, argc, argv);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-construct
Object* Object::construct(ExecutionState& state, const Value& constructor, const size_t argc, NULLABLE Value* argv, Value newTarget)
{
    // If newTarget was not passed, let newTarget be F.
    if (newTarget.isEmpty()) {
        newTarget = constructor;
    }
    // Assert: IsConstructor (F) is true.
    ASSERT(constructor.isConstructor());
    // Assert: IsConstructor (newTarget) is true.
    ASSERT(newTarget.isConstructor());
    // Return F.[[Construct]](argumentsList, newTarget).
    return constructor.asObject()->construct(state, argc, argv, newTarget);
}

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinaryhasinstance
bool Object::hasInstance(ExecutionState& state, const Value& C, Value O)
{
    // If IsCallable(C) is false, return false.
    if (!C.isCallable()) {
        return false;
    }

    // If C has a [[BoundTargetFunction]] internal slot, then
    if (UNLIKELY(C.isObject() && C.asObject()->isBoundFunctionObject())) {
        // Let BC be the value of C’s [[BoundTargetFunction]] internal slot.
        Value BC = C.asObject()->asBoundFunctionObject()->targetFunction();
        // Return InstanceofOperator(O,BC) (see 12.9.4).
        return ByteCodeInterpreter::instanceOfOperation(state, O, BC).toBoolean(state);
    }
    // If Type(O) is not Object, return false.
    if (!O.isObject()) {
        return false;
    }
    // Let P be Get(C, "prototype").
    ASSERT(C.isFunction());
    Value P = C.asFunction()->getFunctionPrototype(state);
    // If Type(P) is not Object, throw a TypeError exception.
    if (!P.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_InvalidPrototypeProperty);
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

void Object::setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
    }
}

void Object::setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver)) && state.inStrictMode()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
    }
}

void Object::throwCannotDefineError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, errorMessage_DefineProperty_RedefineNotConfigurable);
}

void Object::throwCannotWriteError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
}

void Object::throwCannotDeleteError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.toExceptionString(), false, String::emptyString, errorMessage_DefineProperty_NotConfigurable);
}

void Object::deleteOwnProperty(ExecutionState& state, size_t idx)
{
    m_structure = m_structure->removeProperty(state, idx);
    m_values.erase(idx, m_structure->propertyCount() + 1);

    // ASSERT(m_values.size() == m_structure->propertyCount());
}

uint64_t Object::length(ExecutionState& state)
{
    // ES6
    // return get(state, state.context()->staticStrings().length).value(state, this).toLength(state);
    // ES5
    return get(state, state.context()->staticStrings().length).value(state, this).toUint32(state);
}

double Object::lengthES6(ExecutionState& state)
{
    return get(state, state.context()->staticStrings().length).value(state, this).toLength(state);
}


bool Object::nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined, double& nextIndex)
{
    Value ptr = obj;
    double ret = end;
    bool exists = false;
    struct Data {
        bool* exists;
        const bool* skipUndefined;
        const double* cur;
        double* ret;
    } data;
    data.exists = &exists;
    data.skipUndefined = &skipUndefined;
    data.cur = &cur;
    data.ret = &ret;

    while (ptr.isObject()) {
        ptr.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
            uint64_t index;
            Data* e = (Data*)data;
            double* ret = e->ret;
            Value key = name.toPlainValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (*e->skipUndefined && self->get(state, name).value(state, self).isUndefined()) {
                    return true;
                }
                if (index > *e->cur && *ret > index) {
                    *ret = std::min(static_cast<double>(index), *ret);
                    *e->exists = true;
                }
            }
            return true;
        },
                                    &data);
        ptr = ptr.asObject()->getPrototype(state);
    }
    nextIndex = ret;
    return exists;
}

bool Object::nextIndexBackward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined, double& nextIndex)
{
    Value ptr = obj;
    double ret = end;
    bool exists = false;
    struct Data {
        bool* exists;
        const bool* skipUndefined;
        const double* cur;
        double* ret;
    } data;
    data.exists = &exists;
    data.skipUndefined = &skipUndefined;
    data.cur = &cur;
    data.ret = &ret;

    while (ptr.isObject()) {
        ptr.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
            uint64_t index;
            Data* e = (Data*)data;
            double* ret = e->ret;
            Value key = name.toPlainValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (*e->skipUndefined && self->get(state, name).value(state, self).isUndefined()) {
                    return true;
                }
                if (index < *e->cur) {
                    *ret = std::max(static_cast<double>(index), *ret);
                    *e->exists = true;
                }
            }
            return true;
        },
                                    &data);
        ptr = ptr.asObject()->getPrototype(state);
    }
    nextIndex = ret;
    return exists;
}

void Object::sort(ExecutionState& state, const std::function<bool(const Value& a, const Value& b)>& comp)
{
    std::vector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> selected;

    uint32_t len = length(state);
    uint32_t n = 0;
    uint32_t k = 0;

    while (k < len) {
        Value idx = Value(k);
        if (hasOwnProperty(state, ObjectPropertyName(state, idx))) {
            selected.push_back(getOwnProperty(state, ObjectPropertyName(state, idx)).value(state, this));
            n++;
            k++;
        } else {
            double result;
            nextIndexForward(state, this, k, len, false, result);
            k = result;
        }
    }


    if (selected.size()) {
        TightVector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> tempSpace;
        tempSpace.resizeWithUninitializedValues(selected.size());

        mergeSort(selected.data(), selected.size(), tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = comp(a, b);
            return true;
        });
    }

    uint32_t i;
    for (i = 0; i < n; i++) {
        setThrowsException(state, ObjectPropertyName(state, Value(i)), selected[i], this);
    }

    while (i < len) {
        Value idx = Value(i);
        if (hasOwnProperty(state, ObjectPropertyName(state, idx))) {
            deleteOwnProperty(state, ObjectPropertyName(state, Value(i)));
            i++;
        } else {
            double result;
            nextIndexForward(state, this, i, len, false, result);
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

    m_structure = m_structure->addProperty(state, P.toPropertyName(state), ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(data));
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

    Value S = C.asObject()->get(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().species)).value(state, C);

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
    char flags[6] = { 0 };
    int flags_idx = 0;

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().global)).value(state, this).toBoolean(state)) {
        flags[flags_idx++] = 'g';
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().ignoreCase)).value(state, this).toBoolean(state)) {
        flags[flags_idx++] = 'i';
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().multiline)).value(state, this).toBoolean(state)) {
        flags[flags_idx++] = 'm';
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, this).toBoolean(state)) {
        flags[flags_idx++] = 'u';
    }

    if (this->get(state, ObjectPropertyName(state, state.context()->staticStrings().sticky)).value(state, this).toBoolean(state)) {
        flags[flags_idx++] = 'y';
    }

    return new ASCIIString(flags);
}
}
