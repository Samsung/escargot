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

ObjectStructurePropertyName TemplatePropertyName::toObjectStructurePropertyName(Context* c)
{
    if (m_ptr->isString()) {
        return ObjectStructurePropertyName(AtomicString(c, m_ptr->asString()));
    } else {
        return ObjectStructurePropertyName(m_ptr->asSymbol());
    }
}

bool Template::has(const TemplatePropertyName& name)
{
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (m_properties[i].first == name) {
            return true;
        }
    }
    return false;
}

bool Template::remove(const TemplatePropertyName& name)
{
    ASSERT(m_cachedObjectStructure == nullptr);
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (m_properties[i].first == name) {
            m_properties.erase(i);
            return true;
        }
    }
    return false;
}

void Template::set(const TemplatePropertyName& name, const TemplateData& data, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    ASSERT(m_cachedObjectStructure == nullptr);
    remove(name);
    m_properties.pushBack(std::make_pair(name, TemplatePropertyData(data, isWritable, isEnumerable, isConfigurable)));
}

void Template::setAccessorProperty(const TemplatePropertyName& name, Optional<FunctionTemplate*> getter, Optional<FunctionTemplate*> setter, bool isEnumerable, bool isConfigurable)
{
    ASSERT(m_cachedObjectStructure == nullptr);
    remove(name);
    m_properties.pushBack(std::make_pair(name, TemplatePropertyData(getter, setter, isEnumerable, isConfigurable)));
}

void Template::setNativeDataAccessorProperty(const TemplatePropertyName& name, ObjectPropertyNativeGetterSetterData* nativeGetterSetterData, void* privateData)
{
    ASSERT(m_cachedObjectStructure == nullptr);
    remove(name);
    m_properties.pushBack(std::make_pair(name, TemplatePropertyData(nativeGetterSetterData, privateData)));
}

ObjectStructure* Template::constructObjectStructure(Context* ctx, ObjectStructureItem* baseItems, size_t baseItemCount)
{
    size_t propertyCount = m_properties.size() + baseItemCount;
    ObjectStructureItemTightVector structureItemVector;
    structureItemVector.resizeWithUninitializedValues(propertyCount);

    for (size_t i = 0; i < baseItemCount; i++) {
        structureItemVector[i] = baseItems[i];
    }

    bool hasIndexStringAsPropertyName = false;
    bool hasNonAtomicPropertyName = false;
    for (size_t i = baseItemCount; i < propertyCount; i++) {
        auto propertyIndex = i - baseItemCount;
        auto propertyName = m_properties[propertyIndex].first.toObjectStructurePropertyName(ctx);
        if (!hasIndexStringAsPropertyName) {
            hasIndexStringAsPropertyName |= propertyName.isIndexString();
        }

        hasNonAtomicPropertyName |= !propertyName.hasAtomicString();

        auto type = m_properties[propertyIndex].second.propertyType();
        ObjectStructurePropertyDescriptor desc;
        if (type == Template::TemplatePropertyData::PropertyType::PropertyValueData || type == Template::TemplatePropertyData::PropertyType::PropertyTemplateData) {
            desc = ObjectStructurePropertyDescriptor::createDataDescriptor(m_properties[propertyIndex].second.presentAttributes());
        } else if (type == Template::TemplatePropertyData::PropertyType::PropertyNativeAccessorData) {
            desc = ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(m_properties[propertyIndex].second.nativeAccessorData());
        } else {
            ASSERT(type == Template::TemplatePropertyData::PropertyType::PropertyAccessorData);
            desc = ObjectStructurePropertyDescriptor::createAccessorDescriptor(m_properties[propertyIndex].second.presentAttributes());
        }
        structureItemVector[i] = ObjectStructureItem(propertyName, desc);
    }

    ObjectStructure* newObjectStructure;
    if (propertyCount > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newObjectStructure = new ObjectStructureWithMap(hasIndexStringAsPropertyName, std::move(structureItemVector));
    } else {
        newObjectStructure = new ObjectStructureWithTransition(std::move(structureItemVector), hasIndexStringAsPropertyName, hasNonAtomicPropertyName);
    }

    return newObjectStructure;
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
}
} // namespace Escargot
