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
#include "Template.h"
#include "runtime/FunctionTemplate.h"

namespace Escargot {

bool Template::has(const Value& propertyName)
{
    ObjectStructurePropertyName pName(propertyName);
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (m_properties[i].first == pName) {
            return true;
        }
    }
    return false;
}

bool Template::remove(const Value& propertyName)
{
    ASSERT(m_cachedObjectStructure.m_objectStructure == nullptr);
    ObjectStructurePropertyName pName(propertyName);
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (m_properties[i].first == pName) {
            m_properties.erase(i);
            return true;
        }
    }
    return false;
}

void Template::set(const Value& propertyName, const TemplateData& data, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    ASSERT(m_cachedObjectStructure.m_objectStructure == nullptr);
    remove(propertyName);
    m_properties.pushBack(std::make_pair(ObjectStructurePropertyName(propertyName), TemplatePropertyData(data, isWritable, isEnumerable, isConfigurable)));
}

void Template::setAccessorProperty(const Value& propertyName, Optional<FunctionTemplate*> getter, Optional<FunctionTemplate*> setter, bool isEnumerable, bool isConfigurable)
{
    ASSERT(m_cachedObjectStructure.m_objectStructure == nullptr);
    remove(propertyName);
    m_properties.pushBack(std::make_pair(ObjectStructurePropertyName(propertyName), TemplatePropertyData(getter, setter, isEnumerable, isConfigurable)));
}

void Template::setNativeDataAccessorProperty(const Value& propertyName, ObjectPropertyNativeGetterSetterData* nativeGetterSetterData, void* privateData)
{
    ASSERT(m_cachedObjectStructure.m_objectStructure == nullptr);
    remove(propertyName);
    m_properties.pushBack(std::make_pair(ObjectStructurePropertyName(propertyName), TemplatePropertyData(nativeGetterSetterData, privateData)));
}

void Template::addNativeDataAccessorProperties(Template* other)
{
    // add all AccessorProperties from other ObjectTemplate
    // it is used for Template inheritance
    ASSERT(this->isObjectTemplate() && other->isObjectTemplate());

    auto& fromProperties = other->m_properties;

    for (size_t i = 0; i < fromProperties.size(); i++) {
        auto type = fromProperties[i].second.propertyType();
        if (type == Template::TemplatePropertyData::PropertyType::PropertyNativeAccessorData) {
            m_properties.pushBack(std::make_pair(fromProperties[i].first, fromProperties[i].second));
        }
    }
}

Template::CachedObjectStructure Template::constructObjectStructure(Context* ctx, ObjectStructureItem* baseItems, size_t baseItemCount)
{
    size_t propertyCount = m_properties.size() + baseItemCount;
    ObjectStructureItemTightVector structureItemVector;
    structureItemVector.resizeWithUninitializedValues(propertyCount);

    for (size_t i = 0; i < baseItemCount; i++) {
        structureItemVector[i] = baseItems[i];
    }

    bool isInlineNonCacheable = false;
    for (size_t i = baseItemCount; i < propertyCount; i++) {
        auto propertyIndex = i - baseItemCount;
        auto propertyNameValue = m_properties[propertyIndex].first.toValue();

        ObjectStructurePropertyName propertyName;
        if (propertyNameValue.isString()) {
            propertyName = ObjectStructurePropertyName(AtomicString(ctx, propertyNameValue.asString()));
        } else {
            ASSERT(propertyNameValue.isSymbol());
            propertyName = ObjectStructurePropertyName(propertyNameValue.asSymbol());
        }

        auto type = m_properties[propertyIndex].second.propertyType();
        ObjectStructurePropertyDescriptor desc;
        if (type == Template::TemplatePropertyData::PropertyType::PropertyValueData || type == Template::TemplatePropertyData::PropertyType::PropertyTemplateData) {
            desc = ObjectStructurePropertyDescriptor::createDataDescriptor(m_properties[propertyIndex].second.presentAttributes());
        } else if (type == Template::TemplatePropertyData::PropertyType::PropertyNativeAccessorData) {
            desc = ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(m_properties[propertyIndex].second.nativeAccessorData());
            isInlineNonCacheable = isInlineNonCacheable | m_properties[propertyIndex].second.nativeAccessorData()->m_actsLikeJSGetterSetter;
        } else {
            ASSERT(type == Template::TemplatePropertyData::PropertyType::PropertyAccessorData);
            desc = ObjectStructurePropertyDescriptor::createAccessorDescriptor(m_properties[propertyIndex].second.presentAttributes());
        }

        structureItemVector[i] = ObjectStructureItem(propertyName, desc);
    }

    CachedObjectStructure s;
    s.m_objectStructure = ObjectStructure::create(ctx, std::move(structureItemVector));
    s.m_inlineCacheable = !isInlineNonCacheable;
    return s;
}

void Template::constructObjectPropertyValues(Context* ctx, ObjectPropertyValue* baseItems, size_t baseItemCount, ObjectPropertyValueVector& objectPropertyValues)
{
    size_t propertyCount = m_properties.size() + baseItemCount;
    objectPropertyValues.resizeWithUninitializedValues(0, propertyCount);

    for (size_t i = 0; i < baseItemCount; i++) {
        objectPropertyValues[i] = baseItems[i];
    }

    for (size_t i = baseItemCount; i < propertyCount; i++) {
        auto propertyIndex = i - baseItemCount;
        auto type = m_properties[propertyIndex].second.propertyType();
        if (type == Template::TemplatePropertyData::PropertyType::PropertyValueData) {
            objectPropertyValues[i] = m_properties[propertyIndex].second.valueData();
        } else if (type == Template::TemplatePropertyData::PropertyType::PropertyTemplateData) {
            objectPropertyValues[i] = m_properties[propertyIndex].second.templateData()->instantiate(ctx);
        } else if (type == Template::TemplatePropertyData::PropertyType::PropertyNativeAccessorData) {
            objectPropertyValues[i] = Value(Value::FromPayload, (intptr_t)m_properties[propertyIndex].second.nativeAccessorPrivateData());
        } else {
            ASSERT(type == Template::TemplatePropertyData::PropertyType::PropertyAccessorData);
            Value getter = m_properties[propertyIndex].second.accessorData().m_getterTemplate ? m_properties[propertyIndex].second.accessorData().m_getterTemplate->instantiate(ctx) : Value(Value::EmptyValue);
            Value setter = m_properties[propertyIndex].second.accessorData().m_setterTemplate ? m_properties[propertyIndex].second.accessorData().m_setterTemplate->instantiate(ctx) : Value(Value::EmptyValue);
            objectPropertyValues[i] = new JSGetterSetter(getter, setter);
        }
    }
}

void Template::postProcessing(Object* instantiatedObject)
{
    if (m_instanceExtraData) {
        instantiatedObject->setExtraData(m_instanceExtraData);
    }

    if (!m_cachedObjectStructure.m_inlineCacheable) {
        instantiatedObject->markAsNonInlineCachable();
    }
}
} // namespace Escargot
