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
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

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
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithoutTransition, m_lastFoundPropertyName));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithoutTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectStructure* ObjectStructure::create(Context* ctx, ObjectStructureItemTightVector&& properties, bool preferTransition)
{
    bool hasIndexStringAsPropertyName = false;
    bool hasSymbol = false;
    bool hasNonAtomicPropertyName = false;
    bool hasEnumerableProperty = true;

    for (size_t i = 0; i < properties.size(); i++) {
        const ObjectStructurePropertyName& propertyName = properties[i].m_propertyName;
#ifndef NDEBUG
        // there should be no duplicated properties
        for (size_t j = i + 1; j < properties.size(); j++) {
            ASSERT(propertyName != properties[j].m_propertyName);
        }
#endif
        if (propertyName.isSymbol()) {
            hasSymbol = true;
        }
        if (!hasIndexStringAsPropertyName) {
            hasIndexStringAsPropertyName |= propertyName.isIndexString();
        }

        hasNonAtomicPropertyName |= !propertyName.hasAtomicString();
        hasEnumerableProperty |= properties[i].m_descriptor.isEnumerable();
    }

    if (!isTransitionModeAvailable(properties.size())) {
        return new ObjectStructureWithMap(hasIndexStringAsPropertyName, hasSymbol, hasEnumerableProperty, std::move(properties));
    } else if (preferTransition) {
        return new ObjectStructureWithTransition(std::move(properties), hasIndexStringAsPropertyName, hasSymbol, hasNonAtomicPropertyName, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(new ObjectStructureItemVector(std::move(properties)), hasIndexStringAsPropertyName, hasSymbol, hasNonAtomicPropertyName, hasEnumerableProperty);
    }
}

std::pair<size_t, Optional<const ObjectStructureItem*>> ObjectStructureWithoutTransition::findProperty(const ObjectStructurePropertyName& s)
{
    if (m_properties->size() && m_lastFoundPropertyName == s) {
        uint16_t lastIndex = lastFoundPropertyIndex();
        if (lastIndex == std::numeric_limits<uint16_t>::max()) {
            return std::make_pair(std::numeric_limits<size_t>::max(), Optional<const ObjectStructureItem*>());
        }
        return std::make_pair(lastIndex, &(*m_properties)[lastIndex]);
    }
    m_lastFoundPropertyName = s;
    setLastFoundPropertyIndex(std::numeric_limits<uint16_t>::max());
    size_t size = m_properties->size();

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                if ((*m_properties)[i].m_propertyName.rawValue() == s.rawValue()) {
                    setLastFoundPropertyIndex(i);
                    return std::make_pair(i, &(*m_properties)[i]);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                if ((*m_properties)[i].m_propertyName == as) {
                    setLastFoundPropertyIndex(i);
                    return std::make_pair(i, &(*m_properties)[i]);
                }
            }
        }
    } else if (s.isSymbol()) {
        if (m_hasSymbolPropertyName) {
            for (size_t i = 0; i < size; i++) {
                if ((*m_properties)[i].m_propertyName == s) {
                    setLastFoundPropertyIndex(i);
                    return std::make_pair(i, &(*m_properties)[i]);
                }
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            if ((*m_properties)[i].m_propertyName == s) {
                setLastFoundPropertyIndex(i);
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
    bool nameIsSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : !name.hasAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructure* newStructure;
    ObjectStructureItemVector* propertiesForNewStructure;
    if (m_isReferencedByInlineCache) {
        propertiesForNewStructure = new ObjectStructureItemVector(*m_properties, newItem);
    } else {
        m_properties->push_back(newItem);
        propertiesForNewStructure = m_properties;
        m_properties = nullptr;
    }

    if (propertiesForNewStructure->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newStructure = new ObjectStructureWithMap(propertiesForNewStructure, nullptr, nameIsIndexString, nameIsSymbol, hasEnumerableProperty);
    } else {
        newStructure = new ObjectStructureWithoutTransition(propertiesForNewStructure, nameIsIndexString, nameIsSymbol, hasNonAtomicName, hasEnumerableProperty);
    }

    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::removeProperty(size_t pIndex)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeFitWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties)[i].m_propertyName.isSymbol();
        hasNonAtomicName = hasNonAtomicName | !(*m_properties)[i].m_propertyName.hasAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    auto newStructure = new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    if (!m_isReferencedByInlineCache) {
        m_properties = nullptr;
    }
    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = m_properties;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties);
    } else {
        m_properties = nullptr;
    }
    newProperties->at(idx).m_descriptor = newDesc;
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
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

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                if (m_properties[i].m_propertyName.rawValue() == s.rawValue()) {
                    return std::make_pair(i, &m_properties[i]);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                if (m_properties[i].m_propertyName == as) {
                    return std::make_pair(i, &m_properties[i]);
                }
            }
        }
    } else if (s.isSymbol()) {
        if (m_hasSymbolPropertyName) {
            for (size_t i = 0; i < size; i++) {
                if (m_properties[i].m_propertyName == s) {
                    return std::make_pair(i, &m_properties[i]);
                }
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
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : !name.hasAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();
    ObjectStructure* newObjectStructure;

    size_t nextSize = m_properties.size() + 1;
    // ObjectStructureWithTransition cannot directly convert to ObjectStructureWithMap by just adding one property
    ASSERT(nextSize < ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE);
    if (nextSize > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE || nameIsIndexString) {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithoutTransition(newProperties, nameIsIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    } else {
        ObjectStructureItemTightVector newProperties(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithTransition(std::move(newProperties), nameIsIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
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
    newProperties->resizeFitWithUninitializedValues(m_properties.size() - 1);
    size_t pc = m_properties.size();

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < pc; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | m_properties[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | m_properties[i].m_propertyName.isSymbol();
        hasNonAtomicName = hasNonAtomicName | !m_properties[i].m_propertyName.hasAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | m_properties[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = m_properties[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = m_properties[i].m_descriptor;
        newIdx++;
    }

    return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    newProperties->at(idx).m_descriptor = newDesc;
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::convertToNonTransitionStructure()
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
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
    if (!m_propertyNameMap) {
        m_propertyNameMap = createPropertyNameMap(m_properties);
    }
    auto idx = m_propertyNameMap->find(s);
    if (idx == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructureItem*>());
    }
    return std::make_pair(idx, &(m_properties->data()[idx]));
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
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructureItemVector* newProperties;
    Optional<PropertyNameMapWithCache*> newPropertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties, newItem);
    } else {
        newProperties = m_properties;
        newProperties->push_back(newItem);
        m_properties = nullptr;
        if (m_propertyNameMap) {
            m_propertyNameMap->insert(name, newProperties->size() - 1);
            ASSERT(m_propertyNameMap->size() == newProperties->size());
            newPropertyNameMap = m_propertyNameMap;
            m_propertyNameMap = nullptr;
        }
    }

    ObjectStructure* newStructure = new ObjectStructureWithMap(newProperties, newPropertyNameMap, nameIsIndexString, hasSymbol, hasEnumerableProperty);
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::removeProperty(size_t pIndex)
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeFitWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties)[i].m_propertyName.isSymbol();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        hasNonAtomicName = hasNonAtomicName | !(*m_properties)[i].m_propertyName.hasAtomicString();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    if (!m_isReferencedByInlineCache) {
        m_properties = nullptr;
        m_propertyNameMap = nullptr;
    }
    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, hasIndexString, hasSymbol, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    }
}

ObjectStructure* ObjectStructureWithMap::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = m_properties;
    auto newPropertyNameMap = m_propertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties);
        newPropertyNameMap = nullptr;
    } else {
        m_properties = nullptr;
        m_propertyNameMap = nullptr;
    }

    newProperties->at(idx).m_descriptor = newDesc;
    return new ObjectStructureWithMap(newProperties, newPropertyNameMap, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasEnumerableProperty);
}
} // namespace Escargot
