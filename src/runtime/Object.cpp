#include "Escargot.h"
#include "Object.h"
#include "ExecutionContext.h"
#include "Context.h"
#include "ErrorObject.h"

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
    obj->m_values[0] = Value();
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
Object::ObjectGetResult Object::getOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    PropertyName P = propertyName.toPropertyName(state);
    size_t idx = m_structure->findProperty(state, P);
    if (LIKELY(idx != SIZE_MAX)) {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            if (LIKELY(!item.m_descriptor.isNativeAccessorProperty())) {
                return Object::ObjectGetResult(m_values[idx], item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
            } else {
                ObjectPropertyNativeGetterSetterData* data = item.m_descriptor.nativeGetterSetterData();
                return Object::ObjectGetResult(data->m_getter(state, this), item.m_descriptor.isWritable(), item.m_descriptor.isEnumerable(), item.m_descriptor.isConfigurable());
            }
        } else {
            // TODO
            // implement js getter, setter
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    return Object::ObjectGetResult();
}

bool Object::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    if (isEverSetAsPrototypeObject()) {
        // TODO
        // implement bad time
        RELEASE_ASSERT_NOT_REACHED();
    }

    PropertyName propertyName = P.toPropertyName(state);
    size_t oldIdx = m_structure->findProperty(state, propertyName);
    if (oldIdx == SIZE_MAX) {
        if (checkPropertyAlreadyDefinedWithNonWritableInPrototype(state, P)) {
            return false;
        }

        if (UNLIKELY(!isExtensible()))
            return false;

        m_structure = m_structure->addProperty(state, propertyName, desc.descriptor());

        // TODO implement JS getter setter
        RELEASE_ASSERT(desc.descriptor().isDataProperty());

        m_values.pushBack(desc.value());
        return true;
    } else {
        size_t idx = oldIdx;
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (!item.m_descriptor.isWritable())
            return false;

        if (item.m_descriptor != desc.descriptor()) {
            if (!item.m_descriptor.isConfigurable()) {
                return false;
            }

            deleteOwnProperty(state, P);
            defineOwnProperty(state, P, desc);
            return true;
        }

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

        return true;
    }
}

void Object::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    ASSERT(getOwnProperty(state, P).hasValue());
    ASSERT(getOwnProperty(state, P).isConfigurable());
    deleteOwnProperty(state, m_structure->findProperty(state, P.toPropertyName(state)));
}

Object::ObjectGetResult Object::get(ExecutionState& state, const ObjectPropertyName& propertyName, Object* receiver)
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
            return Object::ObjectGetResult();
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
                return set(state, propertyName, v, receiver);
            }
            target = O->getPrototype(state);
        }
        ObjectPropertyDescriptorForDefineOwnProperty desc(v);
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
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
            m_rareData->m_isPlainObject = false;
        }
    }

    m_structure = m_structure->removeProperty(state, idx);
    m_values.erase(idx);
}
}
