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

class ObjectTemplatePropertyHandlerData : public gc {
    friend class ObjectWithPropertyHandler;

public:
    explicit ObjectTemplatePropertyHandlerData(const ObjectTemplatePropertyHandlerConfiguration& data)
        : m_getter(data.getter)
        , m_setter(data.setter)
        , m_query(data.query)
        , m_deleter(data.deleter)
        , m_enumerator(data.enumerator)
        , m_definer(data.definer)
        , m_descriptor(data.descriptor)
        , m_data(data.data)
    {
    }

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            // only mark m_data
            GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectTemplatePropertyHandlerData)] = { 0 };
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectTemplatePropertyHandlerData, m_data));
            descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectTemplatePropertyHandlerData));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }

private:
    PropertyHandlerGetterCallback m_getter;
    PropertyHandlerSetterCallback m_setter;
    PropertyHandlerQueryCallback m_query;
    PropertyHandlerDeleteCallback m_deleter;
    PropertyHandlerEnumerationCallback m_enumerator;
    PropertyHandlerDefineOwnPropertyCallback m_definer;
    PropertyHandlerGetPropertyDescriptorCallback m_descriptor;
    void* m_data;
};

class ObjectWithPropertyHandler : public DerivedObject {
public:
    ObjectWithPropertyHandler(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto, ObjectTemplatePropertyHandlerData* namedData, ObjectTemplatePropertyHandlerData* indexedData)
        : DerivedObject(structure, std::move(values), proto)
        , m_namedPropertyHandler(namedData)
        , m_indexedPropertyHandler(indexedData)
    {
        ASSERT(namedData || indexedData);
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_getter) {
            auto ret = propertyHandler->m_getter(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                 toRef(P.toPlainValue()));

            if (ret.hasValue()) {
                if (propertyHandler->m_query) {
                    auto attr = propertyHandler->m_query(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                         toRef(P.toPlainValue()));
                    return ObjectGetResult(toImpl(ret.value()), attr & ObjectTemplatePropertyAttribute::PropertyAttributeWritable, attr & ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable, attr & ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
                } else {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }

        return Object::getOwnProperty(state, P);
    }

    virtual Value getOwnPropertyDescriptor(ExecutionState& state, const ObjectPropertyName& P) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_descriptor) {
            auto ret = propertyHandler->m_descriptor(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                     toRef(P.toPlainValue()));
            if (ret.hasValue()) {
                return toImpl(ret.value());
            }
        }

        return Object::getOwnPropertyDescriptor(state, P);
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler) {
            if (propertyHandler->m_definer) {
                auto ret = propertyHandler->m_definer(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data, toRef(P.toPlainValue()), ObjectPropertyDescriptorRef((void*)&desc));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            } else if (propertyHandler->m_setter && desc.isValuePresent() && desc.isWritable() && desc.isEnumerable() && desc.isConfigurable()) {
                auto ret = propertyHandler->m_setter(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data, toRef(P.toPlainValue()), toRef(desc.value()));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            }
        }

        return DerivedObject::defineOwnProperty(state, P, desc);
    }

    virtual bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_query) {
            auto attr = propertyHandler->m_query(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                 toRef(P.toPlainValue()));
            return attr & ObjectTemplatePropertyAttribute::PropertyAttributeExist;
        }

        return Object::hasOwnProperty(state, P);
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler) {
            if (propertyHandler->m_query) {
                auto attr = propertyHandler->m_query(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                     toRef(P.toPlainValue()));
                if (attr & ObjectTemplatePropertyAttribute::PropertyAttributeExist) {
                    bool isWritable = attr & ObjectTemplatePropertyAttribute::PropertyAttributeWritable;
                    bool isEnumerable = attr & ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable;
                    bool isConfigurable = attr & ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable;

                    Value v;
                    if (propertyHandler->m_getter) {
                        OptionalRef<ValueRef> ret = propertyHandler->m_getter(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                                              toRef(P.toPlainValue()));
                        if (ret.hasValue()) {
                            v = toImpl(ret.value());
                        }
                    }

                    ObjectHasPropertyResult result(ObjectGetResult(v, isWritable, isEnumerable, isConfigurable));
                    return result;
                }
            } else if (propertyHandler->m_getter) {
                OptionalRef<ValueRef> ret = propertyHandler->m_getter(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
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
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_deleter) {
            auto ret = propertyHandler->m_deleter(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                  toRef(P.toPlainValue()));
            if (ret.hasValue()) {
                return ret.value()->toBoolean(toRef(&state));
            }
        }

        return Object::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = m_namedPropertyHandler;
        for (size_t i = 0; i < 2; i++) {
            if (propertyHandler && propertyHandler->m_enumerator) {
                auto ret = propertyHandler->m_enumerator(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data);
                if (propertyHandler->m_query) {
                    for (size_t i = 0; i < ret->size(); i++) {
                        if (shouldSkipSymbolKey && ret->at(i)->isSymbol()) {
                            continue;
                        }

                        auto attr = propertyHandler->m_query(toRef(&state), toRef(this), toRef(this), propertyHandler->m_data,
                                                             ret->at(i));

                        ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor(
                            (ObjectStructurePropertyDescriptor::PresentAttribute)(
                                ((attr & ObjectTemplatePropertyAttribute::PropertyAttributeWritable) ? ObjectStructurePropertyDescriptor::WritablePresent : 0) | ((attr & ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable) ? ObjectStructurePropertyDescriptor::EnumerablePresent : 0) | ((attr & ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable) ? ObjectStructurePropertyDescriptor::ConfigurablePresent : 0)));
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

            propertyHandler = m_indexedPropertyHandler;
        }

        Object::enumeration(state, callback, data, shouldSkipSymbolKey);
    }

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_getter) {
            auto ret = propertyHandler->m_getter(toRef(&state), toRef(this), toRef(receiver), propertyHandler->m_data,
                                                 toRef(P.toPlainValue()));

            if (ret.hasValue()) {
                if (propertyHandler->m_query) {
                    auto attr = propertyHandler->m_query(toRef(&state), toRef(this), toRef(receiver), propertyHandler->m_data,
                                                         toRef(P.toPlainValue()));
                    return ObjectGetResult(toImpl(ret.value()), attr & ObjectTemplatePropertyAttribute::PropertyAttributeWritable, attr & ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable, attr & ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
                } else {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }

        return Object::get(state, P, receiver);
    }

    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override
    {
        ObjectTemplatePropertyHandlerData* propertyHandler = P.isIndexString() ? m_indexedPropertyHandler : m_namedPropertyHandler;
        if (propertyHandler && propertyHandler->m_setter) {
            auto ret = propertyHandler->m_setter(toRef(&state), toRef(this), toRef(receiver), propertyHandler->m_data, toRef(P.toPlainValue()), toRef(v));
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

private:
    ObjectTemplatePropertyHandlerData* m_namedPropertyHandler;
    ObjectTemplatePropertyHandlerData* m_indexedPropertyHandler;
};

void* ObjectTemplate::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectTemplate)] = { 0 };
        Template::fillGCDescriptor(objBitmap);
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_constructor));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_namedPropertyHandler));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_indexedPropertyHandler));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectTemplate));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectTemplate::ObjectTemplate(Optional<FunctionTemplate*> constructor)
    : m_constructor(constructor)
    , m_namedPropertyHandler(nullptr)
    , m_indexedPropertyHandler(nullptr)
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

    if (m_namedPropertyHandler || m_indexedPropertyHandler) {
        result = new ObjectWithPropertyHandler(m_cachedObjectStructure.m_objectStructure, std::move(objectPropertyValues), proto, m_namedPropertyHandler, m_indexedPropertyHandler);
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

void ObjectTemplate::setNamedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data)
{
    ASSERT(!m_namedPropertyHandler);
    m_namedPropertyHandler = new ObjectTemplatePropertyHandlerData(data);
}

void ObjectTemplate::setIndexedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data)
{
    ASSERT(!m_indexedPropertyHandler);
    m_indexedPropertyHandler = new ObjectTemplatePropertyHandlerData(data);
}

void ObjectTemplate::removeNamedPropertyHandler()
{
    // just set nullptr here because property handler might be used in instantiated ObjectWithPropertyHandler
    ASSERT(!m_namedPropertyHandler);
    m_namedPropertyHandler = nullptr;
}

void ObjectTemplate::removeIndexedPropertyHandler()
{
    // just set nullptr here because property handler might be used in instantiated ObjectWithPropertyHandler
    ASSERT(!m_indexedPropertyHandler);
    m_indexedPropertyHandler = nullptr;
}
} // namespace Escargot
