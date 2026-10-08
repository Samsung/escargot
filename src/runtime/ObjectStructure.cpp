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

void* ObjectStructureIndexPropertyVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureIndexPropertyVector)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureIndexPropertyVector, m_buffer));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureIndexPropertyVector));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureIndexDescriptorVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureIndexDescriptorVector)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureIndexDescriptorVector, m_buffer));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureIndexDescriptorVector));
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

        hasNonAtomicPropertyName |= propertyName.hasNonAtomicString();
        hasEnumerableProperty |= properties[i].m_descriptor.isEnumerable();
    }

    if (hasIndexStringAsPropertyName || (hasSymbol && (!preferTransition || !isTransitionModeAvailable(properties.size())))) {
        return new ObjectStructureWithIndexProperties(properties);
    } else if (!isTransitionModeAvailable(properties.size())) {
        return new ObjectStructureWithMap(hasIndexStringAsPropertyName, hasSymbol, hasEnumerableProperty, std::move(properties));
    } else if (preferTransition) {
        if (hasSymbol) {
            ObjectStructureItemTightVector strings;
            ObjectStructureItemTightVector symbols;
            size_t stringCount = 0;
            for (size_t i = 0; i < properties.size(); i++) {
                stringCount += !properties[i].m_propertyName.isSymbol();
            }
            strings.resizeWithUninitializedValues(stringCount);
            symbols.resizeWithUninitializedValues(properties.size() - stringCount);
            size_t stringIndex = 0;
            size_t symbolIndex = 0;
            for (size_t i = 0; i < properties.size(); i++) {
                if (properties[i].m_propertyName.isSymbol()) {
                    symbols[symbolIndex++] = properties[i];
                } else {
                    strings[stringIndex++] = properties[i];
                }
            }
            return ObjectStructureWithTransition::create(std::move(strings), std::move(symbols), hasNonAtomicPropertyName, hasEnumerableProperty);
        }
        return ObjectStructureWithTransition::create(std::move(properties), ObjectStructureItemTightVector(), hasNonAtomicPropertyName, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(new ObjectStructureItemVector(std::move(properties)), hasIndexStringAsPropertyName, hasSymbol, hasNonAtomicPropertyName, hasEnumerableProperty);
    }
}

ObjectStructureFindResult ObjectStructureWithoutTransition::findProperty(const ObjectStructurePropertyName& s)
{
    size_t size = m_properties->size();
    if (m_properties->size() && m_lastFoundPropertyName == s) {
        uint16_t lastIndex = lastFoundPropertyIndex();
        if (lastIndex == std::numeric_limits<uint16_t>::max()) {
            return std::make_pair(std::numeric_limits<size_t>::max(), Optional<const ObjectStructurePropertyDescriptor*>());
        }
        return std::make_pair(lastIndex, &(*m_properties.value())[lastIndex].m_descriptor);
    }
    m_lastFoundPropertyName = s;
    setLastFoundPropertyIndex(std::numeric_limits<uint16_t>::max());

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                if ((*m_properties.value())[i].m_propertyName.rawValue() == s.rawValue()) {
                    setLastFoundPropertyIndex(i);
                    return std::make_pair(i, &(*m_properties.value())[i].m_descriptor);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                if ((*m_properties.value())[i].m_propertyName == as) {
                    setLastFoundPropertyIndex(i);
                    return std::make_pair(i, &(*m_properties.value())[i].m_descriptor);
                }
            }
        }
    } else if (s.isSymbol()) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    } else {
        for (size_t i = 0; i < size; i++) {
            if ((*m_properties.value())[i].m_propertyName == s) {
                setLastFoundPropertyIndex(i);
                return std::make_pair(i, &(*m_properties.value())[i].m_descriptor);
            }
        }
    }

    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

ObjectStructureFindResult ObjectStructureWithoutTransition::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithoutTransition::propertyDescriptor(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_descriptor;
}

bool ObjectStructureWithoutTransition::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithoutTransition::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithoutTransition::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_propertyName;
}

Optional<const ObjectStructureItem*> ObjectStructureWithoutTransition::stringPropertiesData() const
{
    return m_properties->data();
}

size_t ObjectStructureWithoutTransition::propertyCount() const
{
    return m_properties->size();
}

size_t ObjectStructureWithoutTransition::namedPropertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithoutTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    if (name.isSymbol()) {
        ObjectStructureItemVector properties(*m_properties.value(), newItem);
        return new ObjectStructureWithIndexProperties(properties);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool nameIsSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructure* newStructure;
    ObjectStructureItemVector* propertiesForNewStructure;
    if (m_isReferencedByInlineCache) {
        propertiesForNewStructure = new ObjectStructureItemVector(*m_properties.value(), newItem);
    } else {
        m_properties->push_back(newItem);
        propertiesForNewStructure = m_properties.value();
        m_properties = nullptr;
    }

    if (propertiesForNewStructure->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newStructure = new ObjectStructureWithMap(propertiesForNewStructure, nullptr, nameIsIndexString, nameIsSymbol, hasEnumerableProperty);
    } else {
        newStructure = new ObjectStructureWithoutTransition(propertiesForNewStructure, nameIsIndexString, nameIsSymbol, hasNonAtomicName, hasEnumerableProperty);
    }

    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    return new ObjectStructureWithIndexProperties(*m_properties.value(), index, desc);
}

ObjectStructure* ObjectStructureWithoutTransition::removeProperty(size_t pIndex)
{
    size_t ps = m_properties->size();
    ObjectStructureItemVector* newProperties;
    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector();
        newProperties->resizeFitWithUninitializedValues(ps - 1);
    } else {
        // Uncached structures transfer their exclusively owned property storage.
        // Compact it in place instead of allocating a replacement buffer.
        newProperties = m_properties.value();
    }

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties.value())[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties.value())[i].m_propertyName.isSymbol();
        hasNonAtomicName = hasNonAtomicName | (*m_properties.value())[i].m_propertyName.hasNonAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties.value())[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = (*m_properties.value())[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties.value())[i].m_descriptor;
        newIdx++;
    }

    if (!m_isReferencedByInlineCache) {
        newProperties->resizeWithUninitializedValues(ps - 1);
        m_properties = nullptr;
    }
    return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithoutTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = m_properties.value();

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties.value());
    } else {
        m_properties = nullptr;
    }
    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, hasEnumerableProperty);
}

void* ObjectStructureTransitionPropertyVector::Storage::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureTransitionStorage))) {
        GC_word bitmap[(sizeof(Storage) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(Storage, m_buffer) / 4);
        GC_set_bit(bitmap, offsetof(Storage, m_retiredBuffers) / 4);
        GC_set_bit(bitmap, offsetof(Storage, m_map) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureTransitionStorage, size, bitmap, sizeof(Storage) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureTransitionStorage, size);
#else
    return GC_MALLOC(size);
#endif
}

void* ObjectStructureTransitionPropertyVector::RetiredBuffer::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureRetiredBuffer))) {
        GC_word bitmap[(sizeof(RetiredBuffer) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(RetiredBuffer, m_buffer) / 4);
        GC_set_bit(bitmap, offsetof(RetiredBuffer, m_previous) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureRetiredBuffer, size, bitmap, sizeof(RetiredBuffer) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureRetiredBuffer, size);
#else
    return GC_MALLOC(size);
#endif
}

ObjectStructureTransitionPropertyVector::ObjectStructureTransitionPropertyVector(ObjectStructureItemTightVector&& properties)
    : m_size(properties.size())
{
    if (m_size) {
        m_storage = new Storage();
        m_storage->m_buffer = properties.data();
        m_storage->m_size = m_size;
        m_storage->m_capacity = m_size;
        properties.reset(nullptr, 0);
    }
}

ObjectStructureTransitionPropertyVector::ObjectStructureTransitionPropertyVector(const ObjectStructureTransitionPropertyVector& properties, const ObjectStructureItem& newItem)
    : m_size(properties.size() + 1)
{
    if (properties.m_storage && properties.m_size == properties.m_storage->m_size) {
        m_storage = properties.m_storage;
    } else {
        m_storage = new Storage();
        m_storage->m_size = properties.m_size;
    }
    auto* storage = m_storage.value();
    if (m_size > storage->m_capacity) {
        size_t capacity = std::max(size(), std::max(static_cast<size_t>(storage->m_capacity) * 2, static_cast<size_t>(2)));
        auto* buffer = GCUtil::gc_malloc_allocator<ObjectStructureItem>().allocate(capacity);
        if (properties.m_size) {
            memcpy(buffer, properties.data().value(), properties.m_size * sizeof(ObjectStructureItem));
        }
        if (storage->m_buffer) {
            storage->m_retiredBuffers = new RetiredBuffer(storage->m_buffer.value(), storage->m_retiredBuffers);
        }
        storage->m_buffer = buffer;
        storage->m_capacity = capacity;
    }
    storage->m_buffer.value()[properties.m_size] = newItem;
    storage->m_size = m_size;
    if (storage->m_map) {
        storage->m_map->insert(*this);
    }
}

size_t ObjectStructureTransitionPropertyVector::find(const ObjectStructurePropertyName& name) const
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    return findWithBase(name, ThreadLocal::cageBase());
#else
    return findWithBase(name, 0);
#endif
}

size_t ObjectStructureTransitionPropertyVector::findWithBase(const ObjectStructurePropertyName& name, uintptr_t base) const
{
    auto storage = m_storage.getWithBase(base);
    ASSERT(storage);
    auto fullProperties = *this;
    fullProperties.m_size = storage->m_size;
    if (!storage->m_map) {
        storage->m_map = new PropertyNameMapWithCache(fullProperties, base);
    }
    size_t index = storage->m_map.valueWithBase(base)->find(name, fullProperties, base);
    return index < m_size ? index : SIZE_MAX;
}

void* ObjectStructureWithTransition::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureWithTransition))) {
        GC_word bitmap[(sizeof(ObjectStructureWithTransition) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransition, m_properties) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransition, m_transitionTableStorage) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureWithTransition, size, bitmap, sizeof(ObjectStructureWithTransition) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureWithTransition, size);
#else
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithTransition)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_transitionTableStorage));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

ObjectStructureWithTransition* ObjectStructureWithTransition::create(ObjectStructureItemTightVector&& strings, ObjectStructureItemTightVector&& symbols,
                                                                     bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
{
    ObjectStructureTransitionPropertyVector stringProperties(std::move(strings));
    ObjectStructureTransitionPropertyVector symbolProperties(std::move(symbols));
    return create(std::move(stringProperties), std::move(symbolProperties), hasNonAtomicPropertyName, hasEnumerableProperty);
}

ObjectStructureWithTransition* ObjectStructureWithTransition::create(ObjectStructureTransitionPropertyVector&& strings, ObjectStructureTransitionPropertyVector&& symbols,
                                                                     bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
{
    ObjectStructureWithTransition* result;
    if (strings.size() >= ESCARGOT_OBJECT_STRUCTURE_TRANSITION_ACCESS_CACHE_MIN_SIZE
        || symbols.size() >= ESCARGOT_OBJECT_STRUCTURE_TRANSITION_ACCESS_CACHE_MIN_SIZE) {
        result = new ObjectStructureWithTransitionWithMap(std::move(strings), std::move(symbols), hasNonAtomicPropertyName, hasEnumerableProperty);
    } else if (!symbols.empty()) {
        result = new ObjectStructureWithTransitionAndSymbols(std::move(strings), std::move(symbols), hasNonAtomicPropertyName, hasEnumerableProperty);
    } else {
        result = new ObjectStructureWithTransition(std::move(strings), false, false, hasNonAtomicPropertyName, hasEnumerableProperty);
    }
    return result;
}

namespace {

// Present the two key domains without materializing a mixed key buffer.
class TransitionPropertyView {
public:
    explicit TransitionPropertyView(const ObjectStructure& structure)
        : m_structure(structure)
    {
    }

    size_t size() const { return m_structure.propertyCount(); }
    ObjectStructureItem operator[](size_t index) const
    {
        return ObjectStructureItem(m_structure.nonIndexPropertyName(index), m_structure.propertyDescriptor(index));
    }

private:
    const ObjectStructure& m_structure;
};

} // namespace

ObjectStructureFindResult ObjectStructureWithTransition::findProperty(const ObjectStructurePropertyName& s)
{
    size_t size = m_properties.size();
    auto properties = m_properties.dataWithBase(propertyStorageBase());
    if (!size) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    ASSERT(properties);

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                if (properties.value()[i].m_propertyName.rawValue() == s.rawValue()) {
                    return std::make_pair(i, &properties.value()[i].m_descriptor);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                if (properties.value()[i].m_propertyName == as) {
                    return std::make_pair(i, &properties.value()[i].m_descriptor);
                }
            }
        }
    } else if (s.isSymbol()) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    } else {
        for (size_t i = 0; i < size; i++) {
            if (properties.value()[i].m_propertyName == s) {
                return std::make_pair(i, &properties.value()[i].m_descriptor);
            }
        }
    }

    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

ObjectStructureFindResult ObjectStructureWithTransition::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithTransition::propertyDescriptor(size_t valueIndex) const
{
    return m_properties.atWithBase(valueIndex, propertyStorageBase()).m_descriptor;
}

bool ObjectStructureWithTransition::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithTransition::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithTransition::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties.atWithBase(valueIndex, propertyStorageBase()).m_propertyName;
}

Optional<const ObjectStructureItem*> ObjectStructureWithTransition::stringPropertiesData() const
{
    return m_properties.dataWithBase(propertyStorageBase());
}

size_t ObjectStructureWithTransition::propertyCount() const
{
    return m_properties.size();
}

size_t ObjectStructureWithTransition::namedPropertyCount() const
{
    return propertyCount();
}

ObjectStructure* ObjectStructureWithTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    if (m_doesTransitionTableUseMap) {
        auto iter = transitionTableMap()->find(ObjectStructureTransitionMapItem(name, desc));
        if (iter != transitionTableMap()->end()) {
            return iter->second;
        }
    } else {
        size_t len = m_transitionTableVectorBufferSize;
        for (size_t i = 0; i < len; i++) {
            const auto& item = transitionTableVectorBuffer().value()[i];
            if (item.m_descriptor == desc && item.m_propertyName == name) {
                return item.m_structure;
            }
        }
    }

    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();
    ObjectStructure* newObjectStructure;

    size_t nextSize = propertyCount() + 1;
    if (nextSize > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE || nameIsIndexString) {
        if (hasSymbol) {
            auto* partitioned = new ObjectStructureWithIndexProperties(TransitionPropertyView(*this));
            return partitioned->addNonIndexProperty(name, desc);
        }
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties, newItem);
        if (nextSize > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
            newObjectStructure = new ObjectStructureWithMap(newProperties, nullptr, nameIsIndexString, hasSymbol, hasEnumerableProperty);
        } else {
            newObjectStructure = new ObjectStructureWithoutTransition(newProperties, nameIsIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
        }
    } else {
        if (hasSymbol) {
            ObjectStructureTransitionPropertyVector strings = name.isSymbol() ? m_properties : ObjectStructureTransitionPropertyVector(m_properties, newItem);
            auto oldSymbols = symbolProperties();
            ObjectStructureTransitionPropertyVector emptySymbols;
            const auto& symbols = oldSymbols ? *oldSymbols.value() : emptySymbols;
            ObjectStructureTransitionPropertyVector newSymbols = name.isSymbol() ? ObjectStructureTransitionPropertyVector(symbols, newItem) : symbols;
            newObjectStructure = create(std::move(strings), std::move(newSymbols), hasNonAtomicName, hasEnumerableProperty);
        } else {
            ObjectStructureTransitionPropertyVector newProperties(m_properties, newItem);
            newObjectStructure = create(std::move(newProperties), ObjectStructureTransitionPropertyVector(), hasNonAtomicName, hasEnumerableProperty);
        }
        ObjectStructureTransitionVectorItem newTransitionItem(name, desc, newObjectStructure);

        if (m_doesTransitionTableUseMap) {
            transitionTableMap()->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                        newTransitionItem.m_structure));
        } else {
            if (m_transitionTableVectorBufferSize + 1 > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE) {
                ObjectStructureTransitionTableMap* transitionTableMap = new (GC) ObjectStructureTransitionTableMap();
                for (size_t i = 0; i < m_transitionTableVectorBufferSize; i++) {
                    transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(transitionTableVectorBuffer().value()[i].m_propertyName, transitionTableVectorBuffer().value()[i].m_descriptor),
                                                              transitionTableVectorBuffer().value()[i].m_structure));
                }
                transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                          newTransitionItem.m_structure));

                GC_FREE(transitionTableVectorBuffer().value());
                m_doesTransitionTableUseMap = true;
                m_transitionTableStorage = transitionTableMap;
                m_transitionTableVectorBufferCapacity = 0;
                m_transitionTableVectorBufferSize = 0;
            } else {
                if (m_transitionTableVectorBufferCapacity <= (size_t)(m_transitionTableVectorBufferSize + 1)) {
                    m_transitionTableVectorBufferCapacity = std::min(computeVectorAllocateSize(m_transitionTableVectorBufferSize + 1), (size_t)std::numeric_limits<uint8_t>::max());
                    auto buffer = transitionTableVectorBuffer();
                    m_transitionTableStorage = buffer
                        ? (ObjectStructureTransitionVectorItem*)GC_REALLOC_NO_SHRINK(buffer.value(), sizeof(ObjectStructureTransitionVectorItem) * m_transitionTableVectorBufferCapacity)
                        : CompressedPointerAllocator<ObjectStructureTransitionVectorItem>().allocate(m_transitionTableVectorBufferCapacity);
                }
                transitionTableVectorBuffer().value()[m_transitionTableVectorBufferSize] = newTransitionItem;
                m_transitionTableVectorBufferSize++;
            }
        }
    }

    return newObjectStructure;
}

ObjectStructure* ObjectStructureWithTransition::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    return new ObjectStructureWithIndexProperties(TransitionPropertyView(*this), index, desc);
}

ObjectStructure* ObjectStructureWithTransition::removeProperty(size_t pIndex)
{
    if (m_hasSymbolPropertyName) {
        return convertToNonTransitionStructure()->removeProperty(pIndex);
    }
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
        hasNonAtomicName = hasNonAtomicName | m_properties[i].m_propertyName.hasNonAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | m_properties[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = m_properties[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = m_properties[i].m_descriptor;
        newIdx++;
    }

    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, hasIndexString, hasSymbol, hasEnumerableProperty);
    }
    return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    if (m_hasSymbolPropertyName) {
        return convertToNonTransitionStructure()->replacePropertyDescriptor(idx, newDesc);
    }
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, m_hasIndexPropertyName, m_hasSymbolPropertyName, hasEnumerableProperty);
    }
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::convertToNonTransitionStructure()
{
    if (m_hasSymbolPropertyName) {
        return new ObjectStructureWithIndexProperties(TransitionPropertyView(*this));
    }
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasEnumerableProperty);
    }
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
}

void* ObjectStructureWithTransitionAndSymbols::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureWithTransitionAndSymbols))) {
        GC_word bitmap[(sizeof(ObjectStructureWithTransitionAndSymbols) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionAndSymbols, m_properties) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionAndSymbols, m_transitionTableStorage) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionAndSymbols, m_symbolProperties) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureWithTransitionAndSymbols, size, bitmap, sizeof(ObjectStructureWithTransitionAndSymbols) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureWithTransitionAndSymbols, size);
#else
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureWithTransitionAndSymbols)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionAndSymbols, m_properties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionAndSymbols, m_symbolProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionAndSymbols, m_transitionTableStorage));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureWithTransitionAndSymbols));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

ObjectStructureFindResult ObjectStructureWithTransitionAndSymbols::findProperty(const ObjectStructurePropertyName& s)
{
    if (!s.isSymbol()) {
        return ObjectStructureWithTransition::findProperty(s);
    }
    auto properties = m_symbolProperties.dataWithBase(propertyStorageBase());
    ASSERT(m_symbolProperties.empty() || properties);
    for (size_t i = 0; i < m_symbolProperties.size(); i++) {
        if (properties.value()[i].m_propertyName.rawValue() == s.rawValue()) {
            return std::make_pair(m_properties.size() + i, &properties.value()[i].m_descriptor);
        }
    }
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithTransitionAndSymbols::propertyDescriptor(size_t valueIndex) const
{
    if (valueIndex < m_properties.size()) {
        return m_properties.atWithBase(valueIndex, propertyStorageBase()).m_descriptor;
    }
    return m_symbolProperties.atWithBase(valueIndex - m_properties.size(), propertyStorageBase()).m_descriptor;
}

const ObjectStructurePropertyName& ObjectStructureWithTransitionAndSymbols::nonIndexPropertyName(size_t valueIndex) const
{
    if (valueIndex < m_properties.size()) {
        return m_properties.atWithBase(valueIndex, propertyStorageBase()).m_propertyName;
    }
    return m_symbolProperties.atWithBase(valueIndex - m_properties.size(), propertyStorageBase()).m_propertyName;
}

namespace {
class PropertyBufferView {
public:
    PropertyBufferView(Optional<const ObjectStructureItem*> data, size_t size)
        : m_data(data)
        , m_size(size)
    {
        ASSERT(!size || data);
    }
    size_t size() const { return m_size; }
    const ObjectStructureItem& operator[](size_t index) const
    {
        ASSERT(index < m_size && m_data);
        return m_data.value()[index];
    }

private:
    Optional<const ObjectStructureItem*> m_data;
    size_t m_size;
};
} // namespace

uint8_t PropertyNameMapWithCache::entryWidth(size_t count)
{
    // Zero marks an empty bucket; all live entries store propertyIndex + 1.
    if (count <= UINT8_MAX) {
        return sizeof(uint8_t);
    }
    if (count <= UINT16_MAX) {
        return sizeof(uint16_t);
    }
    ASSERT(count <= UINT32_MAX);
    return sizeof(uint32_t);
}

PropertyNameMapWithCache::PropertyNameMapWithCache(const ObjectStructureItemVector& properties)
{
    rebuild(properties);
}

PropertyNameMapWithCache::PropertyNameMapWithCache(const ObjectStructureItemTightVector& properties)
{
    rebuild(properties);
}

PropertyNameMapWithCache::PropertyNameMapWithCache(const ObjectStructureTransitionPropertyVector& properties)
{
    rebuild(PropertyBufferView(properties.data(), properties.size()));
}

PropertyNameMapWithCache::PropertyNameMapWithCache(const ObjectStructureTransitionPropertyVector& properties, uintptr_t base)
{
    rebuild(PropertyBufferView(properties.dataWithBase(base), properties.size()));
}

size_t PropertyNameMapWithCache::hash(const ObjectStructurePropertyName& name) const
{
    // Atomic names normally use pointer identity. If a template contributed
    // non-atomic strings, hash all strings by content so equal names agree.
    size_t value;
    if (LIKELY(!m_hasNonAtomicNames)) {
        // Atomic strings use exact pointers. Symbols use tagged pointers and
        // numeric names use their immediate representation, all of which are
        // stable identity values without repeating the type dispatch.
        value = name.rawValue();
    } else {
        value = name.isPlainString() ? name.plainString()->hashValue() : name.rawValue();
    }
    // Mix aligned pointers before masking off the low bits. Use 32-bit
    // arithmetic here to keep this inexpensive on ARM32 as well.
    uint32_t mixed = static_cast<uint32_t>(value ^ (value >> 16));
    mixed *= 0x9e3779b9U;
    return mixed ^ (mixed >> 16);
}

template <typename Entry>
void PropertyNameMapWithCache::insertEntry(const ObjectStructurePropertyName& name, size_t index)
{
    auto entries = static_cast<Entry*>(m_entries.value());
    if (m_denseCapacity) {
        uint32_t numeric = name.tryToUseAsIndexProperty();
        if (numeric < m_denseCapacity) {
            entries[m_capacity + numeric] = static_cast<Entry>(index + 1);
            return;
        }
    }
    size_t slot = hash(name) & (m_capacity - 1);
    while (entries[slot]) {
        slot = (slot + 1) & (m_capacity - 1);
    }
    entries[slot] = static_cast<Entry>(index + 1);
    m_occupied++;
}

template <typename Entry, typename Properties>
size_t PropertyNameMapWithCache::findEntry(const ObjectStructurePropertyName& name, const Properties& properties) const
{
    auto entries = static_cast<const Entry*>(m_entries.value());
    if (m_denseCapacity) {
        uint32_t numeric = name.tryToUseAsIndexProperty();
        if (numeric < m_denseCapacity) {
            auto entry = entries[m_capacity + numeric];
            return entry ? static_cast<size_t>(entry) - 1 : SIZE_MAX;
        }
    }
    size_t slot = hash(name) & (m_capacity - 1);
    while (auto entry = entries[slot]) {
        size_t index = static_cast<size_t>(entry) - 1;
        const auto& candidate = properties[index].m_propertyName;
        if (candidate.rawValue() == name.rawValue()
            || (m_hasNonAtomicNames && name.isPlainString() && candidate == name)) {
            return index;
        }
        slot = (slot + 1) & (m_capacity - 1);
    }
    return SIZE_MAX;
}

template <typename Properties>
void PropertyNameMapWithCache::rebuild(const Properties& properties)
{
    m_size = properties.size();
    m_entryWidth = entryWidth(m_size);

    // A bounded low-index region, enabled only when at least half full.
    // A sparse key such as "4294967294" must never size this allocation.
    size_t denseCapacity = 16;
    while (denseCapacity < m_size) {
        denseCapacity *= 2;
    }
    size_t denseCount = 0;
    for (size_t i = 0; i < m_size; i++) {
        const auto& name = properties[i].m_propertyName;
        m_hasNonAtomicNames |= name.hasNonAtomicString();
        denseCount += name.tryToUseAsIndexProperty() < denseCapacity;
    }
    if (denseCount < denseCapacity / 2) {
        denseCapacity = 0;
        denseCount = 0;
    }
    m_denseCapacity = denseCapacity;
    m_capacity = 4;
    while (m_size - denseCount > m_capacity - std::max(m_capacity / 4, static_cast<size_t>(1))) {
        m_capacity *= 2;
    }

    auto oldEntries = m_entries;
    size_t bytes = (m_capacity + m_denseCapacity) * m_entryWidth;
    m_entries = GC_MALLOC_ATOMIC(bytes);
    memset(m_entries.value(), 0, bytes);
    m_occupied = 0;
    for (size_t i = 0; i < m_size; i++) {
        const auto& name = properties[i].m_propertyName;
        if (m_entryWidth == sizeof(uint8_t)) {
            insertEntry<uint8_t>(name, i);
        } else if (m_entryWidth == sizeof(uint16_t)) {
            insertEntry<uint16_t>(name, i);
        } else {
            insertEntry<uint32_t>(name, i);
        }
    }
    if (oldEntries) {
        GC_FREE(oldEntries.value());
    }
    if (m_size) {
        m_lastName = properties[m_size - 1].m_propertyName;
        m_lastIndex = m_size - 1;
    }
}

void PropertyNameMapWithCache::insert(const ObjectStructureItemVector& properties)
{
    insertInProperties(properties);
}

void PropertyNameMapWithCache::insert(const ObjectStructureTransitionPropertyVector& properties)
{
    insertInProperties(PropertyBufferView(properties.data(), properties.size()));
}

template <typename Properties>
void PropertyNameMapWithCache::insertInProperties(const Properties& properties)
{
    ASSERT(properties.size() == m_size + 1);
    const auto& name = properties[properties.size() - 1].m_propertyName;
    bool dense = m_denseCapacity && name.tryToUseAsIndexProperty() < m_denseCapacity;
    if (entryWidth(properties.size()) != m_entryWidth
        || (!dense && m_occupied + 1 > m_capacity - std::max(m_capacity / 4, static_cast<size_t>(1)))
        || (!m_hasNonAtomicNames && name.hasNonAtomicString())) {
        rebuild(properties);
        return;
    }
    if (m_entryWidth == sizeof(uint8_t)) {
        insertEntry<uint8_t>(name, m_size);
    } else if (m_entryWidth == sizeof(uint16_t)) {
        insertEntry<uint16_t>(name, m_size);
    } else {
        insertEntry<uint32_t>(name, m_size);
    }
    m_lastName = name;
    m_lastIndex = m_size++;
}

size_t PropertyNameMapWithCache::find(const ObjectStructurePropertyName& name, const ObjectStructureItemVector& properties)
{
    return findInProperties(name, properties);
}

size_t PropertyNameMapWithCache::find(const ObjectStructurePropertyName& name, const ObjectStructureItemTightVector& properties)
{
    return findInProperties(name, properties);
}

size_t PropertyNameMapWithCache::find(const ObjectStructurePropertyName& name, const ObjectStructureTransitionPropertyVector& properties)
{
    return findInProperties(name, PropertyBufferView(properties.data(), properties.size()));
}

size_t PropertyNameMapWithCache::find(const ObjectStructurePropertyName& name, const ObjectStructureTransitionPropertyVector& properties, uintptr_t base)
{
    return findInProperties(name, PropertyBufferView(properties.dataWithBase(base), properties.size()));
}

template <typename Properties>
size_t PropertyNameMapWithCache::findInProperties(const ObjectStructurePropertyName& name, const Properties& properties)
{
    if (name == m_lastName) {
        return m_lastIndex;
    }
    // Switch to content hashes once a non-atomic query appears, so equal
    // atomic and non-atomic strings share buckets without a linear fallback.
    if (UNLIKELY(!m_hasNonAtomicNames && name.hasNonAtomicString())) {
        m_hasNonAtomicNames = true;
        rebuild(properties);
    }
    m_lastName = name;
    if (m_entryWidth == sizeof(uint8_t)) {
        m_lastIndex = findEntry<uint8_t>(name, properties);
    } else if (m_entryWidth == sizeof(uint16_t)) {
        m_lastIndex = findEntry<uint16_t>(name, properties);
    } else {
        m_lastIndex = findEntry<uint32_t>(name, properties);
    }
    return m_lastIndex;
}

void* ObjectStructureWithTransitionWithMap::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureWithTransitionWithMap))) {
        GC_word bitmap[(sizeof(ObjectStructureWithTransitionWithMap) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionWithMap, m_properties) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionWithMap, m_transitionTableStorage) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithTransitionWithMap, m_symbolProperties) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureWithTransitionWithMap, size, bitmap, sizeof(ObjectStructureWithTransitionWithMap) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureWithTransitionWithMap, size);
#else
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureWithTransitionWithMap)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionWithMap, m_properties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionWithMap, m_symbolProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithTransitionWithMap, m_transitionTableStorage));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureWithTransitionWithMap));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

ObjectStructureFindResult ObjectStructureWithTransitionWithMap::findProperty(const ObjectStructurePropertyName& name)
{
    bool isSymbol = name.isSymbol();
    const auto& properties = isSymbol ? m_symbolProperties : m_properties;
    if (properties.empty()) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    if (properties.size() < ESCARGOT_OBJECT_STRUCTURE_TRANSITION_ACCESS_CACHE_MIN_SIZE) {
        return ObjectStructureWithTransitionAndSymbols::findProperty(name);
    }
    size_t index = properties.findWithBase(name, propertyStorageBase());
    if (index == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(index + (isSymbol ? m_properties.size() : 0), &properties.atWithBase(index, propertyStorageBase()).m_descriptor);
}

void* ObjectStructureWithMap::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureWithMap))) {
        GC_word bitmap[(sizeof(ObjectStructureWithMap) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
        GC_set_bit(bitmap, offsetof(ObjectStructureWithMap, m_properties) / 4);
        GC_set_bit(bitmap, offsetof(ObjectStructureWithMap, m_propertyNameMap) / 4);
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureWithMap, size, bitmap, sizeof(ObjectStructureWithMap) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureWithMap, size);
#else
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
#endif
}


ObjectStructureFindResult ObjectStructureWithMap::findProperty(const ObjectStructurePropertyName& s)
{
    if (s.isSymbol()) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    if (!m_propertyNameMap) {
        m_propertyNameMap = createPropertyNameMap(m_properties.value());
    }
    auto idx = m_propertyNameMap->find(s, *m_properties.value());
    if (idx == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(idx, &(m_properties->data()[idx].m_descriptor));
}

ObjectStructureFindResult ObjectStructureWithMap::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithMap::propertyDescriptor(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_descriptor;
}

bool ObjectStructureWithMap::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithMap::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithMap::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_propertyName;
}

Optional<const ObjectStructureItem*> ObjectStructureWithMap::stringPropertiesData() const
{
    return m_properties->data();
}

size_t ObjectStructureWithMap::propertyCount() const
{
    return m_properties->size();
}

size_t ObjectStructureWithMap::namedPropertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithMap::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    if (name.isSymbol()) {
        ObjectStructureItemVector properties(*m_properties.value(), newItem);
        return new ObjectStructureWithIndexProperties(properties);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructureItemVector* newProperties;
    Optional<PropertyNameMapWithCache*> newPropertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties.value(), newItem);
    } else {
        newProperties = m_properties.value();
        newProperties->push_back(newItem);
        m_properties = nullptr;
        if (m_propertyNameMap) {
            m_propertyNameMap->insert(*newProperties);
            ASSERT(m_propertyNameMap->size() == newProperties->size());
            newPropertyNameMap = m_propertyNameMap;
            m_propertyNameMap = nullptr;
        }
    }

    ObjectStructure* newStructure = new ObjectStructureWithMap(newProperties, newPropertyNameMap, nameIsIndexString, hasSymbol, hasEnumerableProperty);
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    return new ObjectStructureWithIndexProperties(*m_properties.value(), index, desc);
}

ObjectStructure* ObjectStructureWithMap::removeProperty(size_t pIndex)
{
    size_t ps = m_properties->size();
    ObjectStructureItemVector* newProperties;
    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector();
        newProperties->resizeFitWithUninitializedValues(ps - 1);
    } else {
        // Uncached structures transfer their exclusively owned property storage.
        // Compact it in place instead of allocating a replacement buffer.
        newProperties = m_properties.value();
    }

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties.value())[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties.value())[i].m_propertyName.isSymbol();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties.value())[i].m_descriptor.isEnumerable();
        hasNonAtomicName = hasNonAtomicName | (*m_properties.value())[i].m_propertyName.hasNonAtomicString();
        (*newProperties)[newIdx].m_propertyName = (*m_properties.value())[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties.value())[i].m_descriptor;
        newIdx++;
    }

    if (!m_isReferencedByInlineCache) {
        newProperties->resizeWithUninitializedValues(ps - 1);
        m_properties = nullptr;
        Optional<PropertyNameMapWithCache*> oldMap = m_propertyNameMap;
        m_propertyNameMap = nullptr;
        if (oldMap) {
            delete oldMap.value();
        }
    }
    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, hasIndexString, hasSymbol, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    }
}

ObjectStructure* ObjectStructureWithMap::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* newProperties = m_properties.value();
    Optional<PropertyNameMapWithCache*> newPropertyNameMap = m_propertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties.value());
        newPropertyNameMap = nullptr;
    } else {
        m_properties = nullptr;
        m_propertyNameMap = nullptr;
    }

    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    return new ObjectStructureWithMap(newProperties, newPropertyNameMap, m_hasIndexPropertyName, m_hasSymbolPropertyName, hasEnumerableProperty);
}

constexpr size_t IndexPropertyOrderChunk::Capacity;

void* IndexPropertyOrderChunk::operator new(size_t size)
{
    return GC_MALLOC_ATOMIC(size);
}

IndexPropertyMapWithCache::IndexPropertyMapWithCache(const ObjectStructureIndexPropertyVector& properties)
{
    ASSERT(properties.size() < UINT32_MAX);
    rebuildHash(properties);
    rebuildOrder(properties);
}

void IndexPropertyMapWithCache::insert(const ObjectStructureIndexPropertyVector& properties)
{
    ASSERT(properties.size() == m_size + 1);
    ASSERT(m_size < UINT32_MAX);
    uint32_t index = properties[properties.size() - 1];
    insertOrder(properties, static_cast<uint32_t>(m_size));
    if (properties.size() > m_capacity - m_capacity / 4) {
        rebuildHash(properties);
        return;
    }
    insertHashEntry(index, m_size);
    m_size++;
}

void IndexPropertyMapWithCache::rebuildOrder(const ObjectStructureIndexPropertyVector& properties)
{
    m_orderChunks.clear();
    if (properties.empty()) {
        return;
    }

    uint32_t* ordinals = static_cast<uint32_t*>(GC_MALLOC_ATOMIC(sizeof(uint32_t) * properties.size()));
    for (size_t i = 0; i < properties.size(); i++) {
        ordinals[i] = static_cast<uint32_t>(i);
    }
    std::sort(ordinals, ordinals + properties.size(),
              [&properties](uint32_t a, uint32_t b) {
                  return properties[a] < properties[b];
              });
    for (size_t offset = 0; offset < properties.size(); offset += IndexPropertyOrderChunk::Capacity) {
        auto* chunk = new IndexPropertyOrderChunk();
        chunk->m_size = std::min(IndexPropertyOrderChunk::Capacity, properties.size() - offset);
        memcpy(chunk->m_ordinals, ordinals + offset, sizeof(uint32_t) * chunk->m_size);
        m_orderChunks.push_back(chunk);
    }
    GC_FREE(ordinals);
}

void IndexPropertyMapWithCache::insertOrder(const ObjectStructureIndexPropertyVector& properties, uint32_t ordinal)
{
    uint32_t index = properties[ordinal];
    size_t chunkIndex = static_cast<size_t>(std::lower_bound(m_orderChunks.begin(), m_orderChunks.end(), index,
                                                             [&properties](const IndexPropertyOrderChunk* chunk, uint32_t value) {
                                                                 uint32_t lastOrdinal = chunk->m_ordinals[chunk->m_size - 1];
                                                                 return properties[lastOrdinal] < value;
                                                             })
                                            - m_orderChunks.begin());
    if (chunkIndex == m_orderChunks.size()) {
        chunkIndex--;
    }

    IndexPropertyOrderChunk* chunk = m_orderChunks[chunkIndex];
    if (chunk->m_size == IndexPropertyOrderChunk::Capacity) {
        auto* right = new IndexPropertyOrderChunk();
        size_t leftSize = IndexPropertyOrderChunk::Capacity / 2;
        right->m_size = chunk->m_size - leftSize;
        memcpy(right->m_ordinals, chunk->m_ordinals + leftSize, sizeof(uint32_t) * right->m_size);
        chunk->m_size = leftSize;
        m_orderChunks.insert(chunkIndex + 1, right);
        if (index > properties[chunk->m_ordinals[chunk->m_size - 1]]) {
            chunk = right;
        }
    }

    size_t position = static_cast<size_t>(std::lower_bound(chunk->m_ordinals, chunk->m_ordinals + chunk->m_size, index,
                                                           [&properties](uint32_t existingOrdinal, uint32_t value) {
                                                               return properties[existingOrdinal] < value;
                                                           })
                                          - chunk->m_ordinals);
    memmove(chunk->m_ordinals + position + 1,
            chunk->m_ordinals + position,
            sizeof(uint32_t) * (chunk->m_size - position));
    chunk->m_ordinals[position] = ordinal;
    chunk->m_size++;
}

void IndexPropertyMapWithCache::insertHashEntry(uint32_t index, size_t ordinal)
{
    uint32_t mixed = index ^ (index >> 16);
    mixed *= 0x9e3779b9U;
    mixed ^= mixed >> 16;
    size_t slot = mixed & (m_capacity - 1);
    while (m_entries.value()[slot]) {
        slot = (slot + 1) & (m_capacity - 1);
    }
    ASSERT(ordinal < UINT32_MAX);
    m_entries.value()[slot] = static_cast<uint32_t>(ordinal + 1);
}

void IndexPropertyMapWithCache::rebuildHash(const ObjectStructureIndexPropertyVector& properties)
{
    size_t capacity = 16;
    while (properties.size() > capacity - capacity / 4) {
        capacity *= 2;
    }
    auto oldEntries = m_entries;
    m_capacity = capacity;
    m_size = properties.size();
    m_entries = static_cast<uint32_t*>(GC_MALLOC_ATOMIC(sizeof(uint32_t) * m_capacity));
    memset(m_entries.value(), 0, sizeof(uint32_t) * m_capacity);
    for (size_t i = 0; i < properties.size(); i++) {
        insertHashEntry(properties[i], i);
    }
    if (oldEntries) {
        GC_FREE(oldEntries.value());
    }
}

size_t IndexPropertyMapWithCache::find(uint32_t index, const ObjectStructureIndexPropertyVector& properties) const
{
    uint32_t mixed = index ^ (index >> 16);
    mixed *= 0x9e3779b9U;
    mixed ^= mixed >> 16;
    size_t slot = mixed & (m_capacity - 1);
    while (uint32_t entry = m_entries.value()[slot]) {
        size_t ordinal = static_cast<size_t>(entry) - 1;
        if (properties[ordinal] == index) {
            return ordinal;
        }
        slot = (slot + 1) & (m_capacity - 1);
    }
    return SIZE_MAX;
}

void IndexPropertyMapWithCache::fillOrdinalsInOrder(uint32_t* ordinals) const
{
    size_t position = 0;
    for (const auto* chunk : m_orderChunks) {
        memcpy(ordinals + position, chunk->m_ordinals, sizeof(uint32_t) * chunk->m_size);
        position += chunk->m_size;
    }
    ASSERT(position == m_size);
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithIndexProperties::defaultIndexPropertyDescriptor()
{
    static const ObjectStructurePropertyDescriptor descriptor = ObjectStructurePropertyDescriptor::createDataDescriptor();
    return descriptor;
}

bool ObjectStructureWithIndexProperties::isDefaultIndexPropertyDescriptor(const ObjectStructurePropertyDescriptor& descriptor)
{
    return descriptor == defaultIndexPropertyDescriptor();
}

size_t ObjectStructureWithIndexProperties::indexPropertyStorageSize() const
{
    return m_indexProperties ? m_indexProperties->size() : m_inlineIndexProperties.m_size;
}

uint32_t ObjectStructureWithIndexProperties::indexPropertyAt(size_t ordinal) const
{
    ASSERT(ordinal < indexPropertyStorageSize());
    return m_indexProperties ? (*m_indexProperties.value())[ordinal] : m_inlineIndexProperties.m_keys[ordinal];
}

void ObjectStructureWithIndexProperties::appendIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& descriptor)
{
    size_t previousCount = indexPropertyStorageSize();
    if (m_indexDescriptors) {
        m_indexDescriptors->push_back(descriptor);
    } else if (!isDefaultIndexPropertyDescriptor(descriptor)) {
        m_indexDescriptors = new ObjectStructureIndexDescriptorVector();
        m_indexDescriptors->reserve(m_indexProperties ? m_indexProperties->capacity() : InlineIndexProperties::Capacity);
        for (size_t i = 0; i < previousCount; i++) {
            m_indexDescriptors->push_back(defaultIndexPropertyDescriptor());
        }
        m_indexDescriptors->push_back(descriptor);
    }
    if (m_indexProperties) {
        m_indexProperties->push_back(index);
    } else {
        ASSERT(previousCount < InlineIndexProperties::Capacity);
        m_inlineIndexProperties.m_keys[previousCount] = index;
        m_inlineIndexProperties.m_size++;
    }
}

void ObjectStructureWithIndexProperties::sortIndexProperties()
{
    if (!m_indexProperties) {
        ASSERT(m_inlineIndexProperties.m_size <= InlineIndexProperties::Capacity);
        if (m_inlineIndexProperties.m_size == 2 && m_inlineIndexProperties.m_keys[0] > m_inlineIndexProperties.m_keys[1]) {
            std::swap(m_inlineIndexProperties.m_keys[0], m_inlineIndexProperties.m_keys[1]);
            if (m_indexDescriptors) {
                std::swap((*m_indexDescriptors.value())[0], (*m_indexDescriptors.value())[1]);
            }
        }
        return;
    }
    if (!m_indexDescriptors) {
        std::sort(m_indexProperties->begin(), m_indexProperties->end());
        return;
    }

    Vector<uint32_t, GCUtil::gc_malloc_atomic_allocator<uint32_t>> ordinals;
    ordinals.resize(m_indexProperties->size());
    for (size_t i = 0; i < ordinals.size(); i++) {
        ordinals[i] = static_cast<uint32_t>(i);
    }
    std::sort(ordinals.begin(), ordinals.end(), [this](uint32_t a, uint32_t b) {
        return (*m_indexProperties.value())[a] < (*m_indexProperties.value())[b];
    });

    auto* sortedProperties = new ObjectStructureIndexPropertyVector();
    auto* sortedDescriptors = new ObjectStructureIndexDescriptorVector();
    sortedProperties->reserve(m_indexProperties->size());
    sortedDescriptors->reserve(m_indexDescriptors->size());
    for (uint32_t ordinal : ordinals) {
        sortedProperties->push_back((*m_indexProperties.value())[ordinal]);
        sortedDescriptors->push_back((*m_indexDescriptors.value())[ordinal]);
    }
    auto* oldProperties = m_indexProperties.value();
    auto* oldDescriptors = m_indexDescriptors.value();
    m_indexProperties = sortedProperties;
    m_indexDescriptors = sortedDescriptors;
    delete oldProperties;
    delete oldDescriptors;
}

void ObjectStructureWithIndexProperties::finishConstruction()
{
    m_hasIndexPropertyName = indexPropertyStorageSize() != 0;
    m_hasSymbolPropertyName = !m_symbolProperties->empty();
    m_hasNonAtomicPropertyName = false;
    m_hasEnumerableProperty = false;
    for (const auto& item : *m_namedProperties.value()) {
        ASSERT(!item.m_propertyName.isSymbol());
        ASSERT(item.m_propertyName.tryToUseAsIndexProperty() == Value::InvalidIndexPropertyValue);
        m_hasNonAtomicPropertyName |= item.m_propertyName.hasNonAtomicString();
        m_hasEnumerableProperty |= item.m_descriptor.isEnumerable();
    }
    for (const auto& item : *m_symbolProperties.value()) {
        ASSERT(item.m_propertyName.isSymbol());
        m_hasEnumerableProperty |= item.m_descriptor.isEnumerable();
    }
    if (indexPropertyStorageSize()) {
        if (!m_indexDescriptors) {
            m_hasEnumerableProperty = true;
        } else {
            for (const auto& descriptor : *m_indexDescriptors.value()) {
                m_hasEnumerableProperty |= descriptor.isEnumerable();
            }
        }
    }
    if (indexPropertyStorageSize() > 8 && !m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        m_indexPropertyMap = new IndexPropertyMapWithCache(*m_indexProperties.value());
    }
}

ObjectStructureWithIndexProperties::ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                                                       ObjectStructureItemVector* symbolProperties,
                                                                       Optional<ObjectStructureIndexPropertyVector*> indexProperties,
                                                                       const InlineIndexProperties& inlineIndexProperties,
                                                                       Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors,
                                                                       Optional<PropertyNameMapWithCache*> namedMap,
                                                                       Optional<IndexPropertyMapWithCache*> indexMap)
    : ObjectStructure(indexProperties ? !indexProperties->empty() : inlineIndexProperties.m_size != 0, false, false, false)
    , m_namedProperties(namedProperties)
    , m_symbolProperties(symbolProperties)
    , m_inlineIndexProperties(inlineIndexProperties)
    , m_indexProperties(indexProperties)
    , m_indexDescriptors(indexDescriptors)
    , m_namedPropertyMap(namedMap)
    , m_indexPropertyMap(indexMap)
{
    finishConstruction();
}

ObjectStructureWithIndexProperties::ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                                                       ObjectStructureItemVector* symbolProperties,
                                                                       Optional<ObjectStructureIndexPropertyVector*> indexProperties,
                                                                       const InlineIndexProperties& inlineIndexProperties,
                                                                       Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors,
                                                                       Optional<PropertyNameMapWithCache*> namedMap,
                                                                       Optional<IndexPropertyMapWithCache*> indexMap,
                                                                       bool hasNonAtomicPropertyName,
                                                                       bool hasEnumerableProperty)
    : ObjectStructure(indexProperties ? !indexProperties->empty() : inlineIndexProperties.m_size != 0,
                      !symbolProperties->empty(), hasNonAtomicPropertyName, hasEnumerableProperty)
    , m_namedProperties(namedProperties)
    , m_symbolProperties(symbolProperties)
    , m_inlineIndexProperties(inlineIndexProperties)
    , m_indexProperties(indexProperties)
    , m_indexDescriptors(indexDescriptors)
    , m_namedPropertyMap(namedMap)
    , m_indexPropertyMap(indexMap)
{
    assertPropertyDomain(*m_namedProperties.value(), false);
    assertPropertyDomain(*m_symbolProperties.value(), true);
    if (indexPropertyStorageSize() > 8 && !m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        m_indexPropertyMap = new IndexPropertyMapWithCache(*m_indexProperties.value());
    }
}

void* ObjectStructureWithIndexProperties::operator new(size_t size)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    static_assert(sizeof(CompressibleHeapPointer<ObjectStructureItemVector>) == 4, "pointer slot width");
    if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ObjectStructureWithIndexProperties))) {
        GC_word bitmap[(sizeof(ObjectStructureWithIndexProperties) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
#define SET_COMPRESSED_FIELD(field) GC_set_bit(bitmap, offsetof(ObjectStructureWithIndexProperties, field) / 4)
        SET_COMPRESSED_FIELD(m_namedProperties);
        SET_COMPRESSED_FIELD(m_symbolProperties);
        SET_COMPRESSED_FIELD(m_indexProperties);
        SET_COMPRESSED_FIELD(m_indexDescriptors);
        SET_COMPRESSED_FIELD(m_namedPropertyMap);
        SET_COMPRESSED_FIELD(m_indexPropertyMap);
#undef SET_COMPRESSED_FIELD
        Heap::initializeCompressedType(Heap::CompressedType::ObjectStructureWithIndexProperties, size, bitmap, sizeof(ObjectStructureWithIndexProperties) / 4);
    }
    return Heap::mallocCompressed(Heap::CompressedType::ObjectStructureWithIndexProperties, size);
#else
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureWithIndexProperties)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_namedProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_symbolProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexDescriptors));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_namedPropertyMap));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexPropertyMap));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureWithIndexProperties));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findProperty(const ObjectStructurePropertyName& name)
{
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return findIndexProperty(index);
    }
    return findNonIndexProperty(name);
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findNonIndexProperty(const ObjectStructurePropertyName& name)
{
    if (name.isSymbol()) {
        for (size_t i = 0; i < m_symbolProperties->size(); i++) {
            if ((*m_symbolProperties.value())[i].m_propertyName == name) {
                return std::make_pair(m_namedProperties->size() + i, &(*m_symbolProperties.value())[i].m_descriptor);
            }
        }
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }

    size_t namedIndex = SIZE_MAX;
    if (m_namedProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        if (!m_namedPropertyMap) {
            m_namedPropertyMap = new PropertyNameMapWithCache(*m_namedProperties.value());
        }
        namedIndex = m_namedPropertyMap->find(name, *m_namedProperties.value());
    } else {
        for (size_t i = 0; i < m_namedProperties->size(); i++) {
            if ((*m_namedProperties.value())[i].m_propertyName == name) {
                namedIndex = i;
                break;
            }
        }
    }
    if (namedIndex == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(namedIndex, &(*m_namedProperties.value())[namedIndex].m_descriptor);
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findIndexProperty(uint32_t index)
{
    size_t ordinal = SIZE_MAX;
    if (m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        ordinal = m_indexPropertyMap->find(index, *m_indexProperties.value());
    } else if (m_indexProperties) {
        auto it = std::lower_bound(m_indexProperties->begin(), m_indexProperties->end(), index);
        if (it != m_indexProperties->end() && *it == index) {
            ordinal = static_cast<size_t>(it - m_indexProperties->begin());
        }
    } else {
        auto* begin = m_inlineIndexProperties.m_keys;
        auto* end = begin + m_inlineIndexProperties.m_size;
        auto* it = std::lower_bound(begin, end, index);
        if (it != end && *it == index) {
            ordinal = static_cast<size_t>(it - begin);
        }
    }
    if (ordinal == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(namedPropertyCount() + ordinal,
                          m_indexDescriptors ? &(*m_indexDescriptors.value())[ordinal] : &defaultIndexPropertyDescriptor());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithIndexProperties::propertyDescriptor(size_t valueIndex) const
{
    if (valueIndex < m_namedProperties->size()) {
        return (*m_namedProperties.value())[valueIndex].m_descriptor;
    }
    valueIndex -= m_namedProperties->size();
    if (valueIndex < m_symbolProperties->size()) {
        return (*m_symbolProperties.value())[valueIndex].m_descriptor;
    }
    size_t indexOrdinal = valueIndex - m_symbolProperties->size();
    return m_indexDescriptors ? (*m_indexDescriptors.value())[indexOrdinal] : defaultIndexPropertyDescriptor();
}

bool ObjectStructureWithIndexProperties::isIndexProperty(size_t valueIndex) const
{
    return valueIndex >= namedPropertyCount();
}

uint32_t ObjectStructureWithIndexProperties::indexPropertyName(size_t valueIndex) const
{
    ASSERT(isIndexProperty(valueIndex));
    return indexPropertyAt(valueIndex - namedPropertyCount());
}

const ObjectStructurePropertyName& ObjectStructureWithIndexProperties::nonIndexPropertyName(size_t valueIndex) const
{
    ASSERT(!isIndexProperty(valueIndex));
    if (valueIndex < m_namedProperties->size()) {
        return (*m_namedProperties.value())[valueIndex].m_propertyName;
    }
    return (*m_symbolProperties.value())[valueIndex - m_namedProperties->size()].m_propertyName;
}

size_t ObjectStructureWithIndexProperties::propertyCount() const
{
    return namedPropertyCount() + indexPropertyStorageSize();
}

size_t ObjectStructureWithIndexProperties::namedPropertyCount() const
{
    return m_namedProperties->size() + m_symbolProperties->size();
}

void ObjectStructureWithIndexProperties::fillIndexPropertyOrdinalsInOrder(uint32_t* ordinals) const
{
    if (m_indexPropertyMap) {
        m_indexPropertyMap->fillOrdinalsInOrder(ordinals);
        return;
    }
    ObjectStructure::fillIndexPropertyOrdinalsInOrder(ordinals);
}

ObjectStructure* ObjectStructureWithIndexProperties::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    return addNonIndexProperty(name, desc);
}

ObjectStructure* ObjectStructureWithIndexProperties::addNonIndexProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    Optional<ObjectStructureIndexPropertyVector*> indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;

    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties.value());
        symbolProperties = copyProperties(m_symbolProperties.value());
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties.value()) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors.value()) : nullptr;
    } else {
        namedProperties = m_namedProperties.value();
        symbolProperties = m_symbolProperties.value();
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }

    if (name.isSymbol()) {
        if (symbolProperties == emptyProperties()) {
            symbolProperties = new ObjectStructureItemVector();
        }
        symbolProperties->push_back(ObjectStructureItem(name, desc));
    } else {
        if (namedProperties == emptyProperties()) {
            namedProperties = new ObjectStructureItemVector();
        }
        namedProperties->push_back(ObjectStructureItem(name, desc));
        if (namedMap) {
            namedMap->insert(*namedProperties);
        }
    }
    bool hasNonAtomicPropertyName = m_hasNonAtomicPropertyName || name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty || desc.isEnumerable();
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  hasNonAtomicPropertyName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithIndexProperties::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    Optional<ObjectStructureIndexPropertyVector*> indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;

    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties.value());
        symbolProperties = copyProperties(m_symbolProperties.value());
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties.value()) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors.value()) : nullptr;
    } else {
        namedProperties = m_namedProperties.value();
        symbolProperties = m_symbolProperties.value();
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }

    size_t previousIndexCount = indexProperties ? indexProperties->size() : inlineIndexProperties.m_size;
    if (!indexDescriptors && !isDefaultIndexPropertyDescriptor(desc)) {
        indexDescriptors = new ObjectStructureIndexDescriptorVector();
        indexDescriptors->reserve(indexProperties ? indexProperties->capacity() : InlineIndexProperties::Capacity);
        for (size_t i = 0; i < previousIndexCount; i++) {
            indexDescriptors->push_back(defaultIndexPropertyDescriptor());
        }
    }
    if (!indexProperties && previousIndexCount < InlineIndexProperties::Capacity) {
        uint32_t* begin = inlineIndexProperties.m_keys;
        size_t insertionIndex = static_cast<size_t>(std::lower_bound(begin, begin + previousIndexCount, index) - begin);
        for (size_t i = previousIndexCount; i > insertionIndex; i--) {
            inlineIndexProperties.m_keys[i] = inlineIndexProperties.m_keys[i - 1];
        }
        inlineIndexProperties.m_keys[insertionIndex] = index;
        inlineIndexProperties.m_size++;
        if (indexDescriptors) {
            indexDescriptors->insert(insertionIndex, desc);
        }
    } else {
        if (!indexProperties) {
            ASSERT(previousIndexCount == InlineIndexProperties::Capacity);
            indexProperties = new ObjectStructureIndexPropertyVector();
            indexProperties->reserve(8);
            indexProperties->push_back(inlineIndexProperties.m_keys[0]);
            indexProperties->push_back(inlineIndexProperties.m_keys[1]);
            inlineIndexProperties.m_size = 0;
        }
        if (indexMap || previousIndexCount >= 8) {
            // Once an order index exists, value slots and descriptor metadata
            // stay in append order even if deletes later take the live count
            // below the hash threshold. Only compact ordinals are reordered.
            indexProperties->push_back(index);
            if (indexDescriptors) {
                indexDescriptors->push_back(desc);
            }
            if (indexMap) {
                indexMap->insert(*indexProperties.value());
            }
        } else {
            size_t insertionIndex = static_cast<size_t>(std::lower_bound(indexProperties->begin(), indexProperties->end(), index)
                                                        - indexProperties->begin());
            indexProperties->insert(insertionIndex, index);
            if (indexDescriptors) {
                indexDescriptors->insert(insertionIndex, desc);
            }
        }
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  m_hasNonAtomicPropertyName, m_hasEnumerableProperty || desc.isEnumerable());
}

ObjectStructure* ObjectStructureWithIndexProperties::removeProperty(size_t valueIndex)
{
    bool removesNamedProperty = valueIndex < m_namedProperties->size();
    bool removesSymbolProperty = !removesNamedProperty && valueIndex < namedPropertyCount();
    bool removesIndexProperty = valueIndex >= namedPropertyCount();
    auto* namedProperties = removesNamedProperty ? emptyProperties() : copyProperties(m_namedProperties.value());
    auto* symbolProperties = removesSymbolProperty ? emptyProperties() : copyProperties(m_symbolProperties.value());
    Optional<ObjectStructureIndexPropertyVector*> indexProperties = !removesIndexProperty && m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties.value()) : nullptr;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors = !removesIndexProperty && m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors.value()) : nullptr;
    if (valueIndex < m_namedProperties->size()) {
        auto* filtered = new ObjectStructureItemVector();
        filtered->reserve(m_namedProperties->size() - 1);
        for (size_t i = 0; i < m_namedProperties->size(); i++) {
            if (i != valueIndex) {
                filtered->push_back((*m_namedProperties.value())[i]);
            }
        }
        namedProperties = filtered;
    } else if (valueIndex < namedPropertyCount()) {
        size_t removedSymbolOrdinal = valueIndex - m_namedProperties->size();
        auto* filtered = new ObjectStructureItemVector();
        filtered->reserve(m_symbolProperties->size() - 1);
        for (size_t i = 0; i < m_symbolProperties->size(); i++) {
            if (i != removedSymbolOrdinal) {
                filtered->push_back((*m_symbolProperties.value())[i]);
            }
        }
        symbolProperties = filtered;
    } else {
        size_t removedIndexOrdinal = valueIndex - namedPropertyCount();
        size_t oldIndexCount = indexPropertyStorageSize();
        size_t newIndexCount = oldIndexCount - 1;
        if (!m_indexPropertyMap && newIndexCount <= InlineIndexProperties::Capacity) {
            indexProperties = nullptr;
            inlineIndexProperties.m_size = 0;
            for (size_t i = 0; i < oldIndexCount; i++) {
                if (i != removedIndexOrdinal) {
                    inlineIndexProperties.m_keys[inlineIndexProperties.m_size++] = indexPropertyAt(i);
                }
            }
        } else {
            auto* filtered = new ObjectStructureIndexPropertyVector();
            filtered->reserve(newIndexCount);
            for (size_t i = 0; i < oldIndexCount; i++) {
                if (i != removedIndexOrdinal) {
                    filtered->push_back(indexPropertyAt(i));
                }
            }
            indexProperties = filtered;
            inlineIndexProperties.m_size = 0;
        }
        if (m_indexDescriptors) {
            auto* filteredDescriptors = new ObjectStructureIndexDescriptorVector();
            filteredDescriptors->reserve(m_indexDescriptors->size() - 1);
            bool allDefault = true;
            for (size_t i = 0; i < m_indexDescriptors->size(); i++) {
                if (i != removedIndexOrdinal) {
                    const auto& descriptor = (*m_indexDescriptors.value())[i];
                    filteredDescriptors->push_back(descriptor);
                    allDefault &= isDefaultIndexPropertyDescriptor(descriptor);
                }
            }
            if (allDefault) {
                delete filteredDescriptors;
            } else {
                indexDescriptors = filteredDescriptors;
            }
        }
    }

    size_t remainingIndexCount = indexProperties ? indexProperties->size() : inlineIndexProperties.m_size;
    if (!remainingIndexCount && symbolProperties->empty()) {
        if (namedProperties == emptyProperties()) {
            namedProperties = new ObjectStructureItemVector();
        }
        bool hasNonAtomic = false;
        bool hasEnumerable = false;
        for (const auto& item : *namedProperties) {
            hasNonAtomic |= item.m_propertyName.hasNonAtomicString();
            hasEnumerable |= item.m_descriptor.isEnumerable();
        }
        if (namedProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
            return new ObjectStructureWithMap(namedProperties, nullptr, false, false, hasEnumerable);
        }
        return new ObjectStructureWithoutTransition(namedProperties, false, false, hasNonAtomic, hasEnumerable);
    }
    Optional<IndexPropertyMapWithCache*> indexMap;
    if (m_indexPropertyMap && remainingIndexCount) {
        ASSERT(indexProperties);
        indexMap = new IndexPropertyMapWithCache(*indexProperties.value());
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, nullptr, indexMap);
}

ObjectStructure* ObjectStructureWithIndexProperties::replacePropertyDescriptor(size_t valueIndex, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    Optional<ObjectStructureIndexPropertyVector*> indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;
    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties.value());
        symbolProperties = copyProperties(m_symbolProperties.value());
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties.value()) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors.value()) : nullptr;
    } else {
        namedProperties = m_namedProperties.value();
        symbolProperties = m_symbolProperties.value();
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }
    if (valueIndex < namedProperties->size()) {
        (*namedProperties)[valueIndex].m_descriptor = newDesc;
    } else if (valueIndex < namedProperties->size() + symbolProperties->size()) {
        (*symbolProperties)[valueIndex - namedProperties->size()].m_descriptor = newDesc;
    } else {
        size_t indexOrdinal = valueIndex - namedProperties->size() - symbolProperties->size();
        if (!indexDescriptors && !isDefaultIndexPropertyDescriptor(newDesc)) {
            indexDescriptors = new ObjectStructureIndexDescriptorVector();
            indexDescriptors->resize(indexProperties ? indexProperties->size() : inlineIndexProperties.m_size, defaultIndexPropertyDescriptor());
        }
        if (indexDescriptors) {
            (*indexDescriptors.value())[indexOrdinal] = newDesc;
            if (isDefaultIndexPropertyDescriptor(newDesc)) {
                bool allDefault = true;
                for (const auto& descriptor : *indexDescriptors.value()) {
                    allDefault &= isDefaultIndexPropertyDescriptor(descriptor);
                }
                if (allDefault) {
                    auto* discardedDescriptors = indexDescriptors.value();
                    indexDescriptors = nullptr;
                    delete discardedDescriptors;
                }
            }
        }
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  m_hasNonAtomicPropertyName, m_hasEnumerableProperty || newDesc.isEnumerable());
}
} // namespace Escargot
