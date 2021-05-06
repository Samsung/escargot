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
#include "api/EscargotPublic.h"
#include "api/internal/ValueAdapter.h"

namespace Escargot {

void* ObjectTemplate::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectTemplate)] = { 0 };
        ObjectTemplate::fillGCDescriptor(objBitmap);
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_constructor));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_namedPropertyHandler));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectTemplate));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectTemplate::ObjectTemplate(Optional<FunctionTemplate*> constructor)
    : m_constructor(constructor)
    , m_namedPropertyHandler(nullptr)
{
}

class ObjectWithNamedPropertyHandler : public Object {
public:
    ObjectWithNamedPropertyHandler(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto, ObjectTemplateNamedPropertyHandlerData* data)
        : Object(structure, std::move(values), proto)
        , m_data(data)
    {
    }

    static TemplatePropertyNameRef toTemplatePropertyNameRef(const ObjectPropertyName& P)
    {
        auto name = P.objectStructurePropertyName();
        ASSERT(name.isPlainString() || name.isSymbol());
        if (name.isPlainString()) {
            return TemplatePropertyNameRef(toRef(name.plainString()));
        }
        return TemplatePropertyNameRef(toRef(name.symbol()));
    }

    static ObjectPropertyName toObjectPropertyName(ExecutionState& state, const TemplatePropertyNameRef& nameRef)
    {
        if (nameRef.value()->isString()) {
            return ObjectPropertyName(state, Value(toImpl(nameRef.value()->asString())));
        } else {
            return ObjectPropertyName(toImpl(nameRef.value()->asSymbol()));
        }
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (!P.isIndexString() && m_data->getter) {
            OptionalRef<ValueRef> ret(m_data->getter(toRef(&state), toRef(this), m_data->data,
                                                     TemplatePropertyNameRef(toTemplatePropertyNameRef(P))));

            if (ret) {
                if (m_data->query) {
                    auto attr = m_data->query(toRef(&state), toRef(this), m_data->data,
                                              TemplatePropertyNameRef(toTemplatePropertyNameRef(P)));
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
        if (m_data->descriptor) {
            auto ret = m_data->descriptor(toRef(&state), toRef(this), m_data->data,
                                          TemplatePropertyNameRef(toTemplatePropertyNameRef(P)));
            if (ret.hasValue()) {
                return toImpl(ret.value());
            } else {
                return Object::getOwnPropertyDescriptor(state, P);
            }
        } else {
            return Object::getOwnPropertyDescriptor(state, P);
        }
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override
    {
        if (!P.isIndexString()) {
            if (m_data->definer) {
                auto ret = m_data->definer(toRef(&state), toRef(this), m_data->data, TemplatePropertyNameRef(toTemplatePropertyNameRef(P)), ObjectPropertyDescriptorRef((void*)&desc));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            } else if (m_data->setter && desc.isValuePresent() && desc.isWritable() && desc.isEnumerable() && desc.isConfigurable()) {
                auto ret = m_data->setter(toRef(&state), toRef(this), m_data->data, TemplatePropertyNameRef(toTemplatePropertyNameRef(P)), toRef(desc.value()));
                if (ret.hasValue()) {
                    return ret.value()->toBoolean(toRef(&state));
                }
            }
        }
        return Object::defineOwnProperty(state, P, desc);
    }

    virtual bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName) override
    {
        if (m_data->query) {
            auto attr = m_data->query(toRef(&state), toRef(this), m_data->data,
                                      TemplatePropertyNameRef(toTemplatePropertyNameRef(propertyName)));
            return attr & TemplatePropertyAttributeExist;
        }
        return Object::hasOwnProperty(state, propertyName);
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (m_data->query) {
            auto attr = m_data->query(toRef(&state), toRef(this), m_data->data,
                                      TemplatePropertyNameRef(toTemplatePropertyNameRef(P)));
            if (attr & TemplatePropertyAttributeExist) {
                bool isWritable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable;
                bool isEnumerable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable;
                bool isConfigurable = attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable;

                Value v;
                if (m_data->getter) {
                    OptionalRef<ValueRef> ret = (m_data->getter(toRef(&state), toRef(this), m_data->data,
                                                                TemplatePropertyNameRef(toTemplatePropertyNameRef(P))));
                    if (ret) {
                        v = toImpl(ret.value());
                    }
                }

                ObjectHasPropertyResult result(ObjectGetResult(v, isWritable, isEnumerable, isConfigurable));
                return result;
            }
        } else if (m_data->getter) {
            OptionalRef<ValueRef> ret = (m_data->getter(toRef(&state), toRef(this), m_data->data,
                                                        TemplatePropertyNameRef(toTemplatePropertyNameRef(P))));
            if (ret) {
                return ObjectGetResult(toImpl(ret.value()), true, true, true);
            }
        }

        return Object::hasProperty(state, P);
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (m_data->deleter) {
            auto ret = m_data->deleter(toRef(&state), toRef(this), m_data->data,
                                       TemplatePropertyNameRef(toTemplatePropertyNameRef(P)));
            if (ret.hasValue()) {
                return ret.value()->toBoolean(toRef(&state));
            }
        }
        return Object::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override
    {
        if (m_data->enumerator) {
            auto ret = m_data->enumerator(toRef(&state), toRef(this), m_data->data);
            if (m_data->query) {
                for (size_t i = 0; i < ret.size(); i++) {
                    if (shouldSkipSymbolKey && ret[i].value()->isSymbol()) {
                        continue;
                    }

                    auto attr = m_data->query(toRef(&state), toRef(this), m_data->data,
                                              TemplatePropertyNameRef(ret[i]));

                    ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor(
                        (ObjectStructurePropertyDescriptor::PresentAttribute)(
                            ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable) ? ObjectStructurePropertyDescriptor::WritablePresent : 0) | ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable) ? ObjectStructurePropertyDescriptor::EnumerablePresent : 0) | ((attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable) ? ObjectStructurePropertyDescriptor::ConfigurablePresent : 0)));
                    callback(state, this, toObjectPropertyName(state, ret[i]), desc, data);
                }
            } else {
                for (size_t i = 0; i < ret.size(); i++) {
                    if (shouldSkipSymbolKey && ret[i].value()->isSymbol()) {
                        continue;
                    }
                    ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor();
                    callback(state, this, toObjectPropertyName(state, ret[i]), desc, data);
                }
            }
        }
        Object::enumeration(state, callback, data, shouldSkipSymbolKey);
    }

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P) override
    {
        if (!P.isIndexString()) {
            OptionalRef<ValueRef> ret(m_data->getter(toRef(&state), toRef(this), m_data->data,
                                                     TemplatePropertyNameRef(toTemplatePropertyNameRef(P))));

            if (ret) {
                if (m_data->query) {
                    auto attr = m_data->query(toRef(&state), toRef(this), m_data->data,
                                              TemplatePropertyNameRef(toTemplatePropertyNameRef(P)));
                    return ObjectGetResult(toImpl(ret.value()), attr & TemplatePropertyAttribute::TemplatePropertyAttributeWritable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable, attr & TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable);
                } else {
                    return ObjectGetResult(toImpl(ret.value()), true, true, true);
                }
            }
        }
        return Object::get(state, P);
    }

    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override
    {
        if (m_data->setter && !P.isIndexString()) {
            auto ret = m_data->setter(toRef(&state), toRef(this), m_data->data, TemplatePropertyNameRef(toTemplatePropertyNameRef(P)), toRef(v));
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
    ObjectTemplateNamedPropertyHandlerData* m_data;
};

Object* ObjectTemplate::instantiate(Context* ctx)
{
    if (!m_cachedObjectStructure) {
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

    if (m_namedPropertyHandler) {
        result = new ObjectWithNamedPropertyHandler(m_cachedObjectStructure, std::move(objectPropertyValues), proto, m_namedPropertyHandler);
    } else {
        result = new Object(m_cachedObjectStructure, std::move(objectPropertyValues), proto);
    }

    postProcessing(result);
    return result;
}

void ObjectTemplate::setNamedPropertyHandler(const ObjectTemplateNamedPropertyHandlerData& data)
{
    m_namedPropertyHandler = new (GC) ObjectTemplateNamedPropertyHandlerData(data);
}

void ObjectTemplate::removeNamedPropertyHandler()
{
    m_namedPropertyHandler = nullptr;
}
} // namespace Escargot
