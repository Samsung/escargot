#include "Escargot.h"
#include "Object.h"
#include "ExecutionContext.h"
#include "Context.h"
#include "ErrorObject.h"
#include "ArrayObject.h"

namespace Escargot {

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
    m_values[0] = Value(state.context()->globalObject()->objectPrototype());
}

Object* Object::createBuiltinObjectPrototype(ExecutionState& state)
{
    Object* obj = new Object(state, 1, false);
    obj->m_structure = state.context()->defaultStructureForObject();
    obj->m_values[0] = Value(Value::Null);
    return obj;
}

Object* Object::createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function)
{
    Object* obj = new Object(state, 2, false);
    obj->m_structure = state.context()->defaultStructureForFunctionPrototypeObject();
    obj->m_values[0] = Value(state.context()->globalObject()->objectPrototype());
    obj->m_values[1] = Value(function);

    return obj;
}

Value Object::getPrototypeSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().__proto__)).value();
}

bool Object::setPrototypeSlowCase(ExecutionState& state, const Value& value)
{
    return defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().__proto__), ObjectPropertyDescriptorForDefineOwnProperty(value));
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
                return ObjectGetResult(data->m_getter(state, this), item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
            }
        } else {
            // TODO
            // implement js getter, setter
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    return ObjectGetResult();
}

bool Object::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (isEverSetAsPrototypeObject()) {
        if (P.toValue(state).toIndex(state) != Value::InvalidIndexValue) {
            // TODO
            // implement bad time
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    // TODO Return true, if every field in Desc is absent.
    // TODO Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value as the corresponding field in current when compared using the SameValue algorithm (9.12).

    PropertyName propertyName = P.toPropertyName(state);
    size_t oldIdx = m_structure->findProperty(state, propertyName);
    if (oldIdx == SIZE_MAX) {
        if (checkPropertyAlreadyDefinedWithNonWritableInPrototype(state, P)) {
            return false;
        }

        // 3. If current is undefined and extensible is false, then Reject.
        if (UNLIKELY(!isExtensible()))
            return false;

        // TODO implement JS getter setter
        RELEASE_ASSERT(desc.isDataProperty());

        m_structure = m_structure->addProperty(state, propertyName, desc.toObjectPropertyDescriptor());

        m_values.pushBack(desc.value());
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

        // TODO If IsGenericDescriptor(Desc) is true, then no further validation is required.
        // if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        if (item.m_descriptor.isDataProperty() != desc.isDataProperty()) {
            // Reject, if the [[Configurable]] field of current is false.
            if (!item.m_descriptor.isConfigurable()) {
                return false;
            }
            RELEASE_ASSERT_NOT_REACHED();
            // TODO
            // If IsDataDescriptor(current) is true, then
            // Convert the property named P of object O from a data property to an accessor property. Preserve the existing values of the converted property’s [[Configurable]] and [[Enumerable]] attributes and set the rest of the property’s attributes to their default values.
            // Else,
            // Convert the property named P of object O from an accessor property to a data property. Preserve the existing values of the converted property’s [[Configurable]] and [[Enumerable]] attributes and set the rest of the property’s attributes to their default values.
        }
        // Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
        else if (item.m_descriptor.isDataProperty() && desc.isDataProperty()) {
            // If the [[Configurable]] field of current is false, then
            if (!item.m_descriptor.isConfigurable()) {
                // Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (item.m_descriptor.isConfigurable() && !desc.isWritable()) {
                    return false;
                }
                // If the [[Writable]] field of current is false, then
                if (!item.m_descriptor.isWritable()) {
                    // Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                    if (!desc.value().equalsTo(state, getOwnDataPropertyUtilForObject(state, idx, this))) {
                        return false;
                    }
                }
            }
            // else, the [[Configurable]] field of current is true, so any change is acceptable.
        } else {
            RELEASE_ASSERT_NOT_REACHED();
            // TODO
            // Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so,
            // If the [[Configurable]] field of current is false, then
            // Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
            // Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
        }
        // For each attribute field of Desc that is present, set the correspondingly named attribute of the property named P of object O to the value of the field.
        bool shouldDelete = false;

        if (desc.isWritablePresent()) {
            if (desc.isWritable() != item.m_descriptor.isWritable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete && desc.isEnumerablePresent()) {
            if (desc.isEnumerable() != item.m_descriptor.isEnumerable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete && desc.isConfigurablePresent()) {
            if (desc.isConfigurable() != item.m_descriptor.isConfigurable()) {
                shouldDelete = true;
            }
        }

        if (!shouldDelete) {
            if (item.m_descriptor.isDataProperty()) {
                if (LIKELY(!item.m_descriptor.isNativeAccessorProperty())) {
                    m_values[idx] = desc.value();
                    return true;
                } else {
                    ObjectPropertyNativeGetterSetterData* data = item.m_descriptor.nativeGetterSetterData();
                    return data->m_setter(state, this, desc.value());
                }
            } else {
                // TODO
                // implement js getter, setter
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            deleteOwnProperty(state, P);
            defineOwnProperty(state, P, desc);
        }

        return true;
    }
}

void Object::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ASSERT(getOwnProperty(state, P).hasValue());
    ASSERT(getOwnProperty(state, P).isConfigurable());
    deleteOwnProperty(state, m_structure->findProperty(state, P.toPropertyName(state)));
}

void Object::enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectPropertyDescriptor& desc)> fn) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    size_t cnt = m_structure->propertyCount();
    for (size_t i = 0; i < cnt; i++) {
        const ObjectStructureItem& item = m_structure->readProperty(state, i);
        if (!fn(ObjectPropertyName(state, item.m_propertyName), item.m_descriptor)) {
            break;
        }
    }
}

ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName, Object* receiver)
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
bool Object::set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, Object* receiver)
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
        ObjectPropertyDescriptorForDefineOwnProperty desc(v, ObjectPropertyDescriptorForDefineOwnProperty::AllPresent);
        return defineOwnProperty(state, propertyName, desc);
    } else {
        // If IsDataDescriptor(ownDesc) is true, then
        if (desc.isDataProperty()) {
            // If ownDesc.[[Writable]] is false, return false.
            if (!desc.isWritable()) {
                return false;
            }
            // TODO If Type(Receiver) is not Object, return false.
            // Let existingDescriptor be Receiver.[[GetOwnProperty]](P).
            auto receiverDesc = receiver->getOwnProperty(state, propertyName);
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
                ObjectPropertyDescriptorForDefineOwnProperty desc(v);
                return receiver->defineOwnProperty(state, propertyName, desc);
            } else {
                // Else Receiver does not currently have a property P,
                // Return CreateDataProperty(Receiver, P, V).
                ObjectPropertyDescriptorForDefineOwnProperty desc(v);
                return defineOwnProperty(state, propertyName, desc);
            }
        } else {
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}

void Object::setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, Object* receiver)
{
    if (UNLIKELY(!set(state, P, v, receiver))) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, P.string(state), false, String::emptyString, errorMessage_DefineProperty_NotWritable);
    }
}

void Object::setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, Object* receiver)
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

bool Object::checkPropertyAlreadyDefinedWithNonWritableInPrototype(ExecutionState& state, const ObjectPropertyName& P)
{
    Value __proto__Value = getPrototype(state);
    while (true) {
        if (!__proto__Value.isObject()) {
            break;
        }
        Object* targetObj = __proto__Value.asObject();
        auto t = targetObj->getOwnProperty(state, P);
        if (t.hasValue()) {
            // http://www.ecma-international.org/ecma-262/5.1/#sec-8.12.5
            // If IsAccessorDescriptor(desc) is true, then
            // Let setter be desc.[[Set]] which cannot be undefined.
            // Call the [[Call]] internal method of setter providing O as the this value and providing V as the sole argument.
            if (!t.isDataProperty()) {
                // TODO
                // implement js getter, setter
                RELEASE_ASSERT_NOT_REACHED();
                /*
                if (!foundInPrototype) {
                    ESPropertyAccessorData* data = targetObj->accessorData(t);
                    if (data->isAccessorDescriptor()) {
                        if (data->getJSSetter()) {
                            ESValue receiverVal(this);
                            if (receiver)
                                receiverVal = *receiver;
                            ESValue args[] = {val};
                            ESFunctionObject::call(ESVMInstance::currentInstance(), data->getJSSetter(), receiverVal, args, 1, false);
                            return true;
                        }
                        return false;
                    }
                    if (data->getNativeSetter()) {
                        if (!targetObj->hiddenClass()->m_propertyInfo[t].writable()) {
                            return false;
                        }
                        foundInPrototype = true;
                        break;
                    } else {
                        return false;
                    }
                }*/
            } else {
                if (!t.isWritable()) {
                    return true;
                }
            }
        }
        __proto__Value = targetObj->getPrototype(state);
    }
    return false;
}


void Object::deleteOwnProperty(ExecutionState& state, size_t idx)
{
    if (isPlainObject()) {
        const ObjectStructureItem& ownDesc = m_structure->readProperty(state, idx);
        if (ownDesc.m_descriptor.isNativeAccessorProperty()) {
            ensureObjectRareData();
            // TODO modify every native accessor!
            RELEASE_ASSERT_NOT_REACHED();
            m_rareData->m_isPlainObject = false;
        }
    }

    m_structure = m_structure->removeProperty(state, idx);
    m_values.erase(idx);
}

uint32_t Object::length(ExecutionState& state)
{
    return get(state, state.context()->staticStrings().length, this).value().toUint32(state);
}

double Object::nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined)
{
    Value ptr = obj;
    double ret = end;
    while (ptr.isObject()) {
        ptr.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectPropertyDescriptor& desc) {
            uint32_t index = Value::InvalidArrayIndexValue;
            Value key = name.toValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (skipUndefined && ptr.asObject()->get(state, name).value().isUndefined()) {
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
        ptr.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectPropertyDescriptor& desc) {
            uint32_t index = Value::InvalidArrayIndexValue;
            Value key = name.toValue(state);
            if ((index = key.toArrayIndex(state)) != Value::InvalidArrayIndexValue) {
                if (skipUndefined && ptr.asObject()->get(state, name).value().isUndefined()) {
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
            selected.push_back(getOwnProperty(state, ObjectPropertyName(state, idx)).value());
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
}
