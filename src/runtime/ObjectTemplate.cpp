/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "ObjectTemplate.h"
#include "runtime/Value.h"
#include "runtime/Object.h"
#include "runtime/Context.h"
#include "runtime/FunctionTemplate.h"
#include "runtime/SandBox.h"
#include "api/EscargotPublic.h"
#include "api/internal/ValueAdapter.h"

namespace Escargot {

class ObjectWithPropertyHandler : public Object {
public:
    virtual bool isValidPropertyName(const ObjectPropertyName& P) = 0;

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (isValidPropertyName(P) && m_propertyHandler->getter) {
            auto ret = m_propertyHandler->getter(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                 toRef(P.toPlainValue()));

            if (ret.hasValue()) {
                if (m_propertyHandler->query) {
                    auto attr = m_propertyHandler->query(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                         toRef(P.toPlainValue()));
                    return ObjectGetResult(toImpl(ret.value()), attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable);
                } else {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }

        return Object::getOwnProperty(state, P);
    }

    virtual Value getOwnPropertyDescriptor(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (isValidPropertyName(P) && m_propertyHandler->descriptor) {
            auto ret = m_propertyHandler->descriptor(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                     toRef(P.toPlainValue()));
            if (ret.hasValue()) {
                return toImpl(ret.value());
            }
        }

        return Object::getOwnPropertyDescriptor(state, P);
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override
    {
        if (isValidPropertyName(P)) {
            if (m_propertyHandler->definer) {
                auto ret = m_propertyHandler->definer(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data, toRef(P.toPlainValue()), ObjectPropertyDescriptorRef((void*)&desc));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            } else if (m_propertyHandler->setter && desc.isValuePresent() && desc.isWritable() && desc.isEnumerable() && desc.isConfigurable()) {
                auto ret = m_propertyHandler->setter(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data, toRef(P.toPlainValue()), toRef(desc.value()));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            }
        }

        return Object::defineOwnProperty(state, P, desc);
    }

    virtual bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (isValidPropertyName(P) && m_propertyHandler->query) {
            auto attr = m_propertyHandler->query(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                 toRef(P.toPlainValue()));
            return attr & TemplatePropertyAttributeExist;
        }

        return Object::hasOwnProperty(state, P);
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (isValidPropertyName(P)) {
            if (m_propertyHandler->query) {
                auto attr = m_propertyHandler->query(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                     toRef(P.toPlainValue()));
                if (attr & TemplatePropertyAttributeExist) {
                    bool isWritable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable;
                    bool isEnumerable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable;
                    bool isConfigurable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable;

                    Value v;
                    if (m_propertyHandler->getter) {
                        OptionalRef<ValueRef> ret = m_propertyHandler->getter(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                                              toRef(P.toPlainValue()));
                        if (ret.hasValue()) {
                            v = toImpl(ret.value());
                        }
                    }

                    ObjectHasPropertyResult result(ObjectGetResult(v, isWritable, isEnumerable, isConfigurable));
                    return result;
                }
            } else if (m_propertyHandler->getter) {
                OptionalRef<ValueRef> ret = m_propertyHandler->getter(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                                      toRef(P.toPlainValue()));
                if (ret.hasValue()) {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }

        return Object::hasProperty(state, P);
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (isValidPropertyName(P) && m_propertyHandler->deleter) {
            auto ret = m_propertyHandler->deleter(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                  toRef(P.toPlainValue()));
            if (ret.hasValue()) {
                return ret.value()->toBoolean(toRef(&state));
            }
        }

        return Object::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override
    {
        if (m_propertyHandler->enumerator) {
            auto ret = m_propertyHandler->enumerator(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data);
            if (m_propertyHandler->query) {
                for (size_t i = 0; i < ret->size(); i++) {
                    if (shouldSkipSymbolKey && ret->at(i)->isSymbol()) {
                        continue;
                    }

                    auto attr = m_propertyHandler->query(toRef(&state), toRef(this), toRef(this), m_propertyHandler->data,
                                                         ret->at(i));

                    ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor(
                        (ObjectStructurePropertyDescriptor::PresentAttribute)(
                            ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable) ? ObjectStructurePropertyDescriptor::WritablePresent : 0) | ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable) ? ObjectStructurePropertyDescriptor::EnumerablePresent : 0) | ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable) ? ObjectStructurePropertyDescriptor::ConfigurablePresent : 0)));
                    if (!callback(state, this, ObjectPropertyName(state, toImpl(ret->at(i))), desc, data)) {
                        return;
                    }
                }
            } else {
                for (size_t i = 0; i < ret->size(); i++) {
                    if (shouldSkipSymbolKey && ret->at(i)->isSymbol()) {
                        continue;
                    }
                    ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor();
                    if (!callback(state, this, ObjectPropertyName(state, toImpl(ret->at(i))), desc, data)) {
                        return;
                    }
                }
            }
        }

        Object::enumeration(state, callback, data, shouldSkipSymbolKey);
    }

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver) override
    {
        if (isValidPropertyName(P)) {
            auto ret = m_propertyHandler->getter(toRef(&state), toRef(this), toRef(receiver), m_propertyHandler->data,
                                                 toRef(P.toPlainValue()));

            if (ret.hasValue()) {
                if (m_propertyHandler->query) {
                    auto attr = m_propertyHandler->query(toRef(&state), toRef(this), toRef(receiver), m_propertyHandler->data,
                                                         toRef(P.toPlainValue()));
                    return ObjectGetResult(toImpl(ret.value()), attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable);
                } else {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }

        return Object::get(state, P, receiver);
    }

    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override
    {
        if (isValidPropertyName(P) && m_propertyHandler->setter) {
            auto ret = m_propertyHandler->setter(toRef(&state), toRef(this), toRef(receiver), m_propertyHandler->data, toRef(P.toPlainValue()), toRef(v));
            if (ret.hasValue()) {
                return ret.value()->toBoolean(toRef(&state));
            }
        }

        return Object::set(state, P, v, receiver);
    }

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual bool canUseOwnPropertyKeysFastPath() override
    {
        return false;
    }

protected:
    ObjectWithPropertyHandler(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto, ObjectTemplatePropertyHandlerData* data)
        : Object(structure, std::move(values), proto)
        , m_propertyHandler(data)
    {
        ASSERT(!!m_propertyHandler);
    }

    ObjectTemplatePropertyHandlerData* m_propertyHandler;
};

class ObjectWithNamedPropertyHandler : public ObjectWithPropertyHandler {
public:
    ObjectWithNamedPropertyHandler(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto, ObjectTemplatePropertyHandlerData* data)
        : ObjectWithPropertyHandler(structure, std::forward<ObjectPropertyValueVector>(values), proto, data)
    {
    }

    virtual bool isValidPropertyName(const ObjectPropertyName& P) override
    {
        return !P.isIndexString();
    }
};

void* ObjectTemplate::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectTemplate)] = { 0 };
        Template::fillGCDescriptor(objBitmap);
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_constructor));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_propertyHandler));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectTemplate));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectTemplate::ObjectTemplate(Optional<FunctionTemplate*> constructor)
    : m_constructor(constructor)
    , m_propertyHandler(nullptr)
{
}

Object* ObjectTemplate::instantiate(Context* ctx)
{
    if (!m_cachedObjectStructure.m_objectStructure) {
        if (m_constructor && m_constructor->isConstructor() && m_constructor->parent()) {
            // for inheritance, ObjectTemplate inherits its parent's AccessorProperties of InstanceTemplate
            addNativeDataAccessorProperties(m_constructor->parent()->instanceTemplate());
        }
        m_cachedObjectStructure = constructObjectStructure(ctx, nullptr, 0);
    }
    ObjectPropertyValueVector objectPropertyValues;
    constructObjectPropertyValues(ctx, nullptr, 0, objectPropertyValues);

    Object* result;
    Object* proto;

    if (m_constructor && m_constructor->isConstructor()) {
        FunctionObject* fn = m_constructor->instantiate(ctx)->asFunctionObject();
        proto = fn->uncheckedGetOwnDataProperty(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER).asPointerValue()->asObject();
    } else {
        proto = ctx->globalObject()->objectPrototype();
    }

    if (m_propertyHandler) {
        result = new ObjectWithNamedPropertyHandler(m_cachedObjectStructure.m_objectStructure, std::move(objectPropertyValues), proto, m_propertyHandler);
    } else {
        result = new Object(m_cachedObjectStructure.m_objectStructure, std::move(objectPropertyValues), proto);
    }

    postProcessing(result);
    return result;
}

bool ObjectTemplate::installTo(Context* ctx, Object* target)
{
    if (!m_properties.size()) {
        return true;
    }

    struct Sender {
        ObjectTemplate* self;
        Context* ctx;
        Object* target;
    } sender;

    sender.self = this;
    sender.ctx = ctx;
    sender.target = target;

    SandBox sb(ctx);
    auto result = sb.run([](ExecutionState& state, void* data) -> Value {
        Sender* sender = (Sender*)data;
        size_t propertyCount = sender->self->m_properties.size();
        auto properties = sender->self->m_properties;
        for (size_t i = 0; i < propertyCount; i++) {
            auto propertyNameValue = properties[i].first.toValue();
            ObjectStructurePropertyName name;

            if (propertyNameValue.isString()) {
                name = ObjectStructurePropertyName(AtomicString(state, propertyNameValue.asString()));
            } else {
                ASSERT(propertyNameValue.isSymbol());
                name = ObjectStructurePropertyName(propertyNameValue.asSymbol());
            }
            auto a = properties[i].second.presentAttributes();
            int attr = 0;

            if (a & ObjectStructurePropertyDescriptor::PresentAttribute::WritablePresent) {
                attr |= ObjectPropertyDescriptor::WritablePresent;
            }
            if (a & ObjectStructurePropertyDescriptor::PresentAttribute::EnumerablePresent) {
                attr |= ObjectPropertyDescriptor::EnumerablePresent;
            }
            if (a & ObjectStructurePropertyDescriptor::PresentAttribute::ConfigurablePresent) {
                attr |= ObjectPropertyDescriptor::ConfigurablePresent;
            }

            auto type = properties[i].second.propertyType();
            ObjectStructurePropertyDescriptor desc;
            if (type == Template::TemplatePropertyData::PropertyType::PropertyValueData) {
                sender->target->defineOwnPropertyThrowsException(state, name,
                                                                 ObjectPropertyDescriptor(properties[i].second.valueData(), (ObjectPropertyDescriptor::PresentAttribute)attr));
            } else if (type == Template::TemplatePropertyData::PropertyType::PropertyTemplateData) {
                sender->target->defineOwnPropertyThrowsException(state, name,
                                                                 ObjectPropertyDescriptor(properties[i].second.templateData()->instantiate(sender->ctx), (ObjectPropertyDescriptor::PresentAttribute)attr));
            } else if (type == Template::TemplatePropertyData::PropertyType::PropertyNativeAccessorData) {
                sender->target->defineNativeDataAccessorProperty(state, name, properties[i].second.nativeAccessorData(),
                                                                 Value(Value::FromPayload, (intptr_t)properties[i].second.nativeAccessorPrivateData()));
                if (properties[i].second.nativeAccessorData()->m_actsLikeJSGetterSetter) {
                    sender->target->markAsNonInlineCachable();
                }
            } else {
                ASSERT(type == Template::TemplatePropertyData::PropertyType::PropertyAccessorData);
                Value getter = properties[i].second.accessorData().m_getterTemplate ? properties[i].second.accessorData().m_getterTemplate->instantiate(sender->ctx) : Value(Value::EmptyValue);
                Value setter = properties[i].second.accessorData().m_setterTemplate ? properties[i].second.accessorData().m_setterTemplate->instantiate(sender->ctx) : Value(Value::EmptyValue);
                sender->target->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(JSGetterSetter(getter, setter), (ObjectPropertyDescriptor::PresentAttribute)attr));
            }
        }

        return Value();
    },
                         &sender);

    return result.error.isEmpty();
}

void ObjectTemplate::setNamedPropertyHandler(const ObjectTemplatePropertyHandlerData& data)
{
    ASSERT(!m_propertyHandler);
    m_propertyHandler = new (GC) ObjectTemplatePropertyHandlerData(data);
}

void ObjectTemplate::removePropertyHandler()
{
    ASSERT(!m_propertyHandler);
    GC_FREE(m_propertyHandler);
    m_propertyHandler = nullptr;
}
} // namespace Escargot
