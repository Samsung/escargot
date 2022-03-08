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

namespace Escargot {

void* ObjectStructureItemVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureItemVector)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureItemVector, m_buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureItemVector));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureWithoutTransition::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithoutTransition)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithoutTransition, m_properties));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithoutTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<size_t, Optional<const ObjectStructureItem*>> ObjectStructureWithoutTransition::findProperty(const ObjectStructurePropertyName& s)
{
    size_t size = m_properties->size();

    if (LIKELY(s.hasAtomicString() && !m_hasNonAtomicPropertyName)) {
        for (size_t i = 0; i < size; i++) {
            if ((*m_properties)[i].m_propertyName.rawValue() == s.rawValue()) {
                return std::make_pair(i, &(*m_properties)[i]);
            }
        }
    } else if (LIKELY(s.hasAtomicString())) {
        AtomicString as = s.asAtomicString();
        for (size_t i = 0; i < size; i++) {
            if ((*m_properties)[i].m_propertyName == as) {
                return std::make_pair(i, &(*m_properties)[i]);
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            if ((*m_properties)[i].m_propertyName == s) {
                return std::make_pair(i, &(*m_properties)[i]);
            }
        }
    }

    return std::make_pair(SIZE_MAX, Optional<const ObjectStructureItem*>());
}

const ObjectStructureItem& ObjectStructureWithoutTransition::readProperty(size_t idx)
{
    return m_properties->at(idx);
}

const ObjectStructureItem* ObjectStructureWithoutTransition::properties() const
{
    return m_properties->data();
}

size_t ObjectStructureWithoutTransition::propertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithoutTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : !name.hasAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();
    ObjectStructure* newStructure;
    m_properties->push_back(newItem);

    if (m_properties->size() + 1 > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newStructure = new ObjectStructureWithMap(m_properties, ObjectStructureWithMap::createPropertyNameMap(m_properties), nameIsIndexString, hasEnumerableProperty);
    } else {
        newStructure = new ObjectStructureWithoutTransition(m_properties, nameIsIndexString, hasNonAtomicName, hasEnumerableProperty);
    }

    m_properties = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::removeProperty(size_t pIndex)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasNonAtomicName = hasNonAtomicName | !(*m_properties)[i].m_propertyName.hasAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    auto newStructure = new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasNonAtomicName, hasEnumerableProperty);
    m_properties = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    m_properties->at(idx).m_descriptor = newDesc;
    auto newStructure = new ObjectStructureWithoutTransition(m_properties, m_hasIndexPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
    m_properties = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::convertToNonTransitionStructure()
{
    return this;
}

void* ObjectStructureWithTransition::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithTransition)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_transitionTableVectorBuffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<size_t, Optional<const ObjectStructureItem*>> ObjectStructureWithTransition::findProperty(const ObjectStructurePropertyName& s)
{
    size_t size = m_properties.size();

    if (LIKELY(s.hasAtomicString() && !m_hasNonAtomicPropertyName)) {
        for (size_t i = 0; i < size; i++) {
            if (m_properties[i].m_propertyName.rawValue() == s.rawValue()) {
                return std::make_pair(i, &m_properties[i]);
            }
        }
    } else if (LIKELY(s.hasAtomicString())) {
        AtomicString as = s.asAtomicString();
        for (size_t i = 0; i < size; i++) {
            if (m_properties[i].m_propertyName == as) {
                return std::make_pair(i, &m_properties[i]);
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            if (m_properties[i].m_propertyName == s) {
                return std::make_pair(i, &m_properties[i]);
            }
        }
    }

    return std::make_pair(SIZE_MAX, Optional<const ObjectStructureItem*>());
}

const ObjectStructureItem& ObjectStructureWithTransition::readProperty(size_t idx)
{
    return m_properties[idx];
}

const ObjectStructureItem* ObjectStructureWithTransition::properties() const
{
    return m_properties.data();
}

size_t ObjectStructureWithTransition::propertyCount() const
{
    return m_properties.size();
}

ObjectStructure* ObjectStructureWithTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    if (m_doesTransitionTableUseMap) {
        auto iter = m_transitionTableMap->find(ObjectStructureTransitionMapItem(name, desc));
        if (iter != m_transitionTableMap->end()) {
            return iter->second;
        }
    } else {
        size_t len = m_transitionTableVectorBufferSize;
        for (size_t i = 0; i < len; i++) {
            const auto& item = m_transitionTableVectorBuffer[i];
            if (item.m_descriptor == desc && item.m_propertyName == name) {
                return item.m_structure;
            }
        }
    }

    ObjectStructureItem newItem(name, desc);
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : !name.hasAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();
    ObjectStructure* newObjectStructure;

    size_t nextSize = m_properties.size() + 1;
    if (nextSize > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newObjectStructure = new ObjectStructureWithMap(nameIsIndexString, hasEnumerableProperty, m_properties, newItem);
    } else if (nextSize > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE) {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithoutTransition(newProperties, nameIsIndexString, hasNonAtomicName, hasEnumerableProperty);
    } else {
        ObjectStructureItemTightVector newProperties(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithTransition(std::move(newProperties), nameIsIndexString, hasNonAtomicName, hasEnumerableProperty);
        ObjectStructureTransitionVectorItem newTransitionItem(name, desc, newObjectStructure);

        if (m_doesTransitionTableUseMap) {
            m_transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                        newTransitionItem.m_structure));
        } else {
            if (m_transitionTableVectorBufferSize + 1 > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE) {
                ObjectStructureTransitionTableMap* transitionTableMap = new (GC) ObjectStructureTransitionTableMap();
                for (size_t i = 0; i < m_transitionTableVectorBufferSize; i++) {
                    transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(m_transitionTableVectorBuffer[i].m_propertyName, m_transitionTableVectorBuffer[i].m_descriptor),
                                                              m_transitionTableVectorBuffer[i].m_structure));
                }
                transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                          newTransitionItem.m_structure));

                GC_FREE(m_transitionTableVectorBuffer);
                m_doesTransitionTableUseMap = true;
                m_transitionTableMap = transitionTableMap;
                m_transitionTableVectorBufferCapacity = 0;
                m_transitionTableVectorBufferSize = 0;
            } else {
                if (m_transitionTableVectorBufferCapacity <= (size_t)(m_transitionTableVectorBufferSize + 1)) {
                    m_transitionTableVectorBufferCapacity = std::min(computeVectorAllocateSize(m_transitionTableVectorBufferSize + 1), (size_t)std::numeric_limits<uint8_t>::max());
                    m_transitionTableVectorBuffer = (ObjectStructureTransitionVectorItem*)GC_REALLOC(m_transitionTableVectorBuffer, sizeof(ObjectStructureTransitionVectorItem) * m_transitionTableVectorBufferCapacity);
                }
                m_transitionTableVectorBuffer[m_transitionTableVectorBufferSize] = newTransitionItem;
                m_transitionTableVectorBufferSize++;
            }
        }
    }

    return newObjectStructure;
}

ObjectStructure* ObjectStructureWithTransition::removeProperty(size_t pIndex)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    newProperties->resizeWithUninitializedValues(m_properties.size() - 1);
    size_t pc = m_properties.size();

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < pc; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | m_properties[i].m_propertyName.isIndexString();
        hasNonAtomicName = hasNonAtomicName | !m_properties[i].m_propertyName.hasAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | m_properties[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = m_properties[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = m_properties[i].m_descriptor;
        newIdx++;
    }

    return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasNonAtomicName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    newProperties->at(idx).m_descriptor = newDesc;
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::convertToNonTransitionStructure()
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
}

void* ObjectStructureWithMap::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithMap)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithMap, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithMap, m_propertyNameMap));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithMap));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}


std::pair<size_t, Optional<const ObjectStructureItem*>> ObjectStructureWithMap::findProperty(const ObjectStructurePropertyName& s)
{
    auto iter = m_propertyNameMap->find(s);
    if (iter == m_propertyNameMap->end()) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructureItem*>());
    }
    return std::make_pair(iter->second, &(m_properties->data()[iter->second]));
}

const ObjectStructureItem& ObjectStructureWithMap::readProperty(size_t idx)
{
    return m_properties->at(idx);
}

const ObjectStructureItem* ObjectStructureWithMap::properties() const
{
    return m_properties->data();
}

size_t ObjectStructureWithMap::propertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithMap::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    m_propertyNameMap->insert(std::make_pair(name, m_properties->size()));
    m_properties->push_back(newItem);
    ObjectStructure* newStructure = new ObjectStructureWithMap(m_properties, m_propertyNameMap, nameIsIndexString, hasEnumerableProperty);
    m_properties = nullptr;
    m_propertyNameMap = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::removeProperty(size_t pIndex)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    ObjectStructure* newStructure = new ObjectStructureWithMap(newProperties, ObjectStructureWithMap::createPropertyNameMap(newProperties), hasIndexString, hasEnumerableProperty);
    m_properties = nullptr;
    m_propertyNameMap = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    m_properties->at(idx).m_descriptor = newDesc;
    ObjectStructure* newStructure = new ObjectStructureWithMap(m_properties, m_propertyNameMap, m_hasIndexPropertyName, m_hasEnumerableProperty);
    m_properties = nullptr;
    m_propertyNameMap = nullptr;
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::convertToNonTransitionStructure()
{
    return this;
}
} // namespace Escargot
