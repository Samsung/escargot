#include "Escargot.h"
#include "Object.h"
#include "ExecutionContext.h"
#include "Context.h"
#include "ErrorObject.h"
#include "FunctionObject.h"
#include "ArrayObject.h"

namespace Escargot {

ObjectRareData::ObjectRareData()
{
    m_isExtensible = true;
    m_isEverSetAsPrototypeObject = false;
    m_isFastModeArrayObject = true;
    m_internalClassName = nullptr;
    m_extraData = nullptr;
}

Value ObjectGetResult::valueSlowCase(ExecutionState& state, const Value& receiver) const
{
    if (m_jsGetterSetter->hasGetter() && m_jsGetterSetter->getter().isFunction()) {
        return m_jsGetterSetter->getter().asFunction()->call(state, receiver, 0, nullptr);
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

    bool hasWritable = false;
    desc = obj->get(state, ObjectPropertyName(strings->writable));
    if (desc.hasValue()) {
        setWritable(desc.value(state, obj).toBoolean(state));
        hasWritable = true;
    }

    bool hasValue = false;
    desc = obj->get(state, ObjectPropertyName(strings->value));
    if (desc.hasValue()) {
        setValue(desc.value(state, obj));
        hasValue = true;
    }

    desc = obj->get(state, ObjectPropertyName(strings->get));
    if (desc.hasValue()) {
        Value getter = desc.value(state, obj);
        if (!getter.isFunction() && !getter.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Getter must be a function or undefined");
        } else {
            m_isDataProperty = false;
            m_getterSetter = JSGetterSetter(getter, Value(Value::EmptyValue));
        }
    }

    desc = obj->get(state, ObjectPropertyName(strings->set));
    if (desc.hasValue()) {
        Value setter = desc.value(state, obj);
        if (!setter.isFunction() && !setter.isUndefined()) {
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

    if (!m_isDataProperty) {
        if (hasWritable | hasValue) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Invalid property descriptor. Cannot both specify accessors and a value or writable attribute");
        }
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

Object::Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea)
    : m_structure(state.context()->defaultStructureForObject())
    , m_rareData(nullptr)
{
    m_values.resizeWithUninitializedValues(defaultSpace);
    if (initPlainArea) {
        initPlainObject(state);
    }
}

Object::Object(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
{
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

void Object::setPrototype(ExecutionState& state, const Value& value)
{
    if (!isExtensible() && (value.isObject() || value.isUndefinedOrNull())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "can't set prototype of this object");
    }

    if (value.isObject()) {
        m_prototype = value.asObject();
        m_prototype->markAsPrototypeObject(state);
    } else if (value.isNull()) {
        m_prototype = nullptr;
    } else if (value.isUndefined()) {
        m_prototype = (Object*)1;
    } else {
    }
}

void Object::markAsPrototypeObject(ExecutionState& state)
{
    ensureObjectRareData();
    m_rareData->m_isEverSetAsPrototypeObject = true;

    if (!state.context()->didSomePrototypeObjectDefineIndexedProperty()) {
        if (structure()->hasIndexPropertyName()) {
            state.context()->somePrototypeObjectDefineIndexedProperty(state);
        }
    }
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-ordinarygetownproperty
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
                Value ret = data->m_getter(state, this);
                return ObjectGetResult(ret, item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
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
    if (isEverSetAsPrototypeObject()) {
        if (!state.context()->didSomePrototypeObjectDefineIndexedProperty() && isIndexString(P.string(state))) {
            state.context()->somePrototypeObjectDefineIndexedProperty(state);
        }
    }

    // TODO Return true, if every field in Desc is absent.
    // TODO Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value as the corresponding field in current when compared using the SameValue algorithm (9.12).

    PropertyName propertyName = P.toPropertyName(state);
    size_t oldIdx = m_structure->findProperty(state, propertyName);
    if (oldIdx == SIZE_MAX) {
        // 3. If current is undefined and extensible is false, then Reject.
        if (UNLIKELY(!isExtensible()))
            return false;

        m_structure = m_structure->addProperty(state, propertyName, desc.toObjectStructurePropertyDescriptor());
        if (LIKELY(desc.isDataProperty())) {
            if (LIKELY(desc.isValuePresent()))
                m_values.pushBack(desc.value());
            else
                m_values.pushBack(Value());
        } else {
            m_values.pushBack(Value(new JSGetterSetter(desc.getterSetter())));
        }

        ASSERT(m_values.size() == m_structure->propertyCount());
        return true;
    } else {
        size_t idx = oldIdx;
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);

        // If the [[Configurable]] field of current is false then
        if (!item.m_descriptor.isConfigurable()) {
            // Reject, if the [[Configurable]] field of Desc is true.
            if (desc.isConfigurable()) {
                return false;
            }
            // Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and Desc are the Boolean negation of each other.
            if (desc.isEnumerablePresent() && desc.isEnumerable() != item.m_descriptor.isEnumerable()) {
                return false;
            }
        }

        bool shouldDelete = false;
        ObjectPropertyDescriptor newDesc = ObjectPropertyDescriptor::fromObjectStructurePropertyDescriptor(item.m_descriptor, m_values[idx]);

        // If IsGenericDescriptor(Desc) is true, then
        if (desc.isGenericDescriptor()) {
            // no further validation is required.

            // if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        } else if (item.m_descriptor.isDataProperty() != desc.isDataDescriptor()) {
            // Reject, if the [[Configurable]] field of current is false.
            if (!item.m_descriptor.isConfigurable()) {
                return false;
            }

            auto current = item.m_descriptor;
            shouldDelete = true;

            int f = 0;
            if (current.isConfigurable()) {
                f = f | ObjectPropertyDescriptor::ConfigurablePresent;
            } else {
                f = f | ObjectPropertyDescriptor::NonConfigurablePresent;
            }

            if (current.isEnumerable()) {
                f = f | ObjectPropertyDescriptor::EnumerablePresent;
            } else {
                f = f | ObjectPropertyDescriptor::NonEnumerablePresent;
            }

            // If IsDataDescriptor(current) is true, then
            if (current.isDataProperty()) {
                // Convert the property named P of object O from a data property to an accessor property.
                // Preserve the existing values of the converted property’s [[Configurable]] and [[Enumerable]] attributes
                // and set the rest of the property’s attributes to their default values.
                newDesc = ObjectPropertyDescriptor(desc.getterSetter(), (ObjectPropertyDescriptor::PresentAttribute)f);
            } else {
                // Else,
                // Convert the property named P of object O from a data property to an accessor property.
                // Preserve the existing values of the converted property’s [[Configurable]] and [[Enumerable]] attributes
                // and set the rest of the property’s attributes to their default values.
                newDesc = ObjectPropertyDescriptor(desc.value(), (ObjectPropertyDescriptor::PresentAttribute)f);
            }
        }
        // Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
        else if (item.m_descriptor.isDataProperty() && desc.isDataDescriptor()) {
            // If the [[Configurable]] field of current is false, then
            if (!item.m_descriptor.isConfigurable()) {
                // Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (!item.m_descriptor.isWritable() && desc.isWritable()) {
                    return false;
                }
                // If the [[Writable]] field of current is false, then
                if (!item.m_descriptor.isWritable()) {
                    // Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                    Value val = desc.isValuePresent() ? desc.value() : Value();
                    if (!val.equalsToByTheSameValueAlgorithm(state, getOwnDataPropertyUtilForObject(state, idx, this))) {
                        return false;
                    }
                }
            }
            // else, the [[Configurable]] field of current is true, so any change is acceptable.
        } else {
            // Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so,
            ASSERT(!item.m_descriptor.isDataProperty() && !desc.isDataDescriptor());

            // If the [[Configurable]] field of current is false, then
            if (!item.m_descriptor.isConfigurable()) {
                JSGetterSetter* current = Value(m_values[idx]).asPointerValue()->asJSGetterSetter();

                // Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
                if (desc.getterSetter().hasSetter()) {
                    Value currentSetter = current->hasSetter() ? current->setter() : Value();
                    if (desc.getterSetter().setter() != currentSetter) {
                        return false;
                    }
                }

                // Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
                if (desc.getterSetter().hasGetter()) {
                    Value currentGetter = current->hasGetter() ? current->getter() : Value();
                    if (desc.getterSetter().getter() != currentGetter) {
                        return false;
                    }
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

        if (newDesc.isDataDescriptor()) {
            if (newDesc.isWritablePresent()) {
                if (newDesc.isWritable() != item.m_descriptor.isWritable()) {
                    shouldDelete = true;
                }
            }
        }

        if (!shouldDelete && newDesc.isEnumerablePresent()) {
            if (newDesc.isEnumerable() != item.m_descriptor.isEnumerable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete && newDesc.isConfigurablePresent()) {
            if (newDesc.isConfigurable() != item.m_descriptor.isConfigurable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete) {
            if (newDesc.isDataDescriptor()) {
                return setOwnDataPropertyUtilForObjectInner(state, idx, item, newDesc.value());
            } else {
                m_values[idx] = Value(new JSGetterSetter(newDesc.getterSetter()));
            }
        } else {
            if (!structure()->isStructureWithFastAccess())
                m_structure = structure()->convertToWithFastAccess(state);

            if (m_structure->m_properties[idx].m_descriptor.isNativeAccessorProperty()) {
                auto newNative = new ObjectPropertyNativeGetterSetterData(newDesc.isWritable(), newDesc.isEnumerable(), newDesc.isConfigurable(),
                                                                          m_structure->m_properties[idx].m_descriptor.nativeGetterSetterData()->m_getter, m_structure->m_properties[idx].m_descriptor.nativeGetterSetterData()->m_setter);
                m_structure->m_properties[idx].m_descriptor = ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(newNative);
            } else {
                m_structure->m_properties[idx].m_descriptor = newDesc.toObjectStructurePropertyDescriptor();
            }

            m_structure->m_version++;

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

void Object::enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> fn) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    size_t cnt = m_structure->propertyCount();
    for (size_t i = 0; i < cnt; i++) {
        const ObjectStructureItem& item = m_structure->readProperty(state, i);
        if (!fn(ObjectPropertyName(state, item.m_propertyName), item.m_descriptor)) {
            break;
        }
    }
}

ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& receiver)
{
    Object* target = this;
    while (true) {
        auto result = target->getOwnProperty(state, propertyName);
        if (result.hasValue()) {
            return result;
        }
        Value __proto__ = target->getPrototype(state);
        if (__proto__.isObject()) {
            target = __proto__.asObject();
        } else {
            return ObjectGetResult();
        }
    }
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-ordinary-object-internal-methods-and-internal-slots-set-p-v-receiver
bool Object::set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver)
{
    auto desc = getOwnProperty(state, propertyName);
    if (!desc.hasValue()) {
        Value target = this->getPrototype(state);
        while (target.isObject()) {
            Object* O = target.asObject();
            auto desc = O->getOwnProperty(state, propertyName);
            if (desc.hasValue()) {
                return O->set(state, propertyName, v, receiver);
            }
            target = O->getPrototype(state);
        }
        ObjectPropertyDescriptor desc(v, ObjectPropertyDescriptor::AllPresent);
        return defineOwnProperty(state, propertyName, desc);
    } else {
        // If IsDataDescriptor(ownDesc) is true, then
        if (desc.isDataProperty()) {
            // If ownDesc.[[Writable]] is false, return false.
            if (!desc.isWritable()) {
                return false;
            }

            if (!receiver.isObject())
                return false;
            Object* receiverObj = receiver.toObject(state);
            // Let existingDescriptor be Receiver.[[GetOwnProperty]](P).
            auto receiverDesc = receiverObj->getOwnProperty(state, propertyName);
            // If existingDescriptor is not undefined, then
            if (receiverDesc.hasValue()) {
                // If IsAccessorDescriptor(existingDescriptor) is true, return false.
                if (!receiverDesc.isDataProperty()) {
                    return false;
                }
                // If existingDescriptor.[[Writable]] is false, return false
                if (!receiverDesc.isWritable()) {
                    return false;
                }
                // Let valueDesc be the PropertyDescriptor{[[Value]]: V}.
                ObjectPropertyDescriptor desc(v);
                return receiverObj->defineOwnProperty(state, propertyName, desc);
            } else {
                // Else Receiver does not currently have a property P,
                // Return CreateDataProperty(Receiver, P, V).
                ObjectPropertyDescriptor newDesc(v, ObjectPropertyDescriptor::AllPresent);
                return receiverObj->defineOwnProperty(state, propertyName, newDesc);
            }
        } else {
            // Let setter be ownDesc.[[Set]].
            Value setter = desc.jsGetterSetter()->hasSetter() ? desc.jsGetterSetter()->setter() : Value();

            // If setter is undefined, return false.
            if (setter.isUndefined()) {
                return false;
            }

            // Let setterResult be Call(setter, Receiver, «V»).
            Value argv[] = { v };
            FunctionObject::call(setter, state, receiver, 1, argv);
            return true;
        }
    }
}

void Object::setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(state), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
    }
}

void Object::setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver)) && state.inStrictMode()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(state), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
    }
}

void Object::throwCannotDefineError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(), false, String::emptyString, errorMessage_DefineProperty_RedefineNotConfigurable);
}

void Object::throwCannotWriteError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
}

void Object::throwCannotDeleteError(ExecutionState& state, const PropertyName& P)
{
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(), false, String::emptyString, errorMessage_DefineProperty_NotConfigurable);
}

void Object::deleteOwnProperty(ExecutionState& state, size_t idx)
{
    m_structure = m_structure->removeProperty(state, idx);
    m_values.erase(idx);

    ASSERT(m_values.size() == m_structure->propertyCount());
}

uint64_t Object::length(ExecutionState& state)
{
    return get(state, state.context()->staticStrings().length, this).value(state, this).toUint32(state);
}

double Object::nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined)
{
    Value ptr = obj;
    double ret = end;
    while (ptr.isObject()) {
        ptr.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) {
            uint64_t index;
            Value key = name.toValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (skipUndefined && ptr.asObject()->get(state, name).value(state, ptr.asObject()).isUndefined()) {
                    return true;
                }
                if (index > cur) {
                    ret = index;
                    return false;
                }
            }
            return true;
        });
        ptr = ptr.asObject()->getPrototype(state);
    }
    return ret;
}

double Object::nextIndexBackward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined)
{
    Value ptr = obj;
    double ret = end;
    while (ptr.isObject()) {
        ptr.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) {
            uint64_t index;
            Value key = name.toValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (skipUndefined && ptr.asObject()->get(state, name).value(state, ptr.asObject()).isUndefined()) {
                    return true;
                }
                if (index < cur) {
                    ret = index;
                    return false;
                }
            }
            return true;
        });
        ptr = ptr.asObject()->getPrototype(state);
    }
    return ret;
}

void Object::sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp)
{
    std::vector<Value, gc_malloc_ignore_off_page_allocator<Value>> selected;

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
            k = nextIndexForward(state, this, k, len, false);
        }
    }

    std::sort(selected.begin(), selected.end(), comp);

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
            i = nextIndexForward(state, this, i, len, false);
        }
    }
}

Value Object::getOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& receiver)
{
    Value v = m_values[idx];
    auto gs = v.asPointerValue()->asJSGetterSetter();
    if (gs->hasGetter() && gs->getter().isFunction()) {
        gs->getter().asFunction()->call(state, receiver, 0, nullptr);
    }
    return Value();
}

bool Object::setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue)
{
    Value v = m_values[idx];
    auto gs = v.asPointerValue()->asJSGetterSetter();
    if (gs->hasSetter() && gs->setter().isFunction()) {
        Value arg = newValue;
        gs->setter().asFunction()->call(state, this, 1, &arg);
        return true;
    }
    return false;
}
}
