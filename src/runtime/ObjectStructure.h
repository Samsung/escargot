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

#ifndef __EscargotObjectStructure__
#define __EscargotObjectStructure__

#include "runtime/String.h"
#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"
#include "runtime/ObjectStructurePropertyName.h"
#include "runtime/ObjectStructurePropertyDescriptor.h"
#include "runtime/CompressibleHeapPointer.h"

namespace Escargot {

class ObjectStructure;

using ObjectStructureFindResult = std::pair<size_t, Optional<const ObjectStructurePropertyDescriptor*>>;

struct ObjectStructureItem : public gc {
    ObjectStructureItem(const ObjectStructurePropertyName& as, const ObjectStructurePropertyDescriptor& desc)
        : m_propertyName(as)
        , m_descriptor(desc)
    {
    }

    ObjectStructurePropertyName m_propertyName;
    ObjectStructurePropertyDescriptor m_descriptor;
};

struct ObjectStructureTransitionVectorItem : public gc {
    ObjectStructurePropertyName m_propertyName;
    ObjectStructurePropertyDescriptor m_descriptor;
    ObjectStructure* m_structure;

    ObjectStructureTransitionVectorItem(const ObjectStructurePropertyName& as, const ObjectStructurePropertyDescriptor& desc, ObjectStructure* structure)
        : m_propertyName(as)
        , m_descriptor(desc)
        , m_structure(structure)
    {
    }
};

struct ObjectStructureTransitionMapItem : public gc {
    ObjectStructurePropertyName m_propertyName;
    ObjectStructurePropertyDescriptor m_descriptor;

    ObjectStructureTransitionMapItem(const ObjectStructurePropertyName& as, const ObjectStructurePropertyDescriptor& desc)
        : m_propertyName(as)
        , m_descriptor(desc)
    {
    }
};

typedef HashMap<ObjectStructureTransitionMapItem, ObjectStructure*, std::hash<ObjectStructureTransitionMapItem>,
                std::equal_to<ObjectStructureTransitionMapItem>, GCUtil::gc_malloc_allocator<std::pair<ObjectStructureTransitionMapItem const, ObjectStructure*>>>
    ObjectStructureTransitionTableMap;

typedef TightVector<ObjectStructureItem, GCUtil::gc_malloc_allocator<ObjectStructureItem>> ObjectStructureItemTightVector;

class PropertyNameMapWithCache;

// Each structure sees an immutable prefix. Only a view of the complete
// storage can append; branching from a shorter prefix creates new storage.
class ObjectStructureTransitionPropertyVector {
public:
    ObjectStructureTransitionPropertyVector() = default;
    ObjectStructureTransitionPropertyVector(ObjectStructureItemTightVector&& properties);
    ObjectStructureTransitionPropertyVector(const ObjectStructureTransitionPropertyVector& properties, const ObjectStructureItem& newItem);

    size_t size() const { return m_size; }
    bool empty() const { return !m_size; }
    Optional<const ObjectStructureItem*> data() const { return m_storage ? m_storage->m_buffer.unwrap() : nullptr; }
    const ObjectStructureItem& operator[](size_t index) const
    {
        ASSERT(index < m_size);
        return data().value()[index];
    }
    size_t find(const ObjectStructurePropertyName& name) const;

private:
    // Keep old allocations alive when a callback grows the shared storage
    // while an enumeration or descriptor reference still uses its old buffer.
    struct RetiredBuffer : public gc {
        RetiredBuffer(ObjectStructureItem* buffer, Optional<RetiredBuffer*> previous)
            : m_buffer(buffer)
            , m_previous(previous)
        {
        }
        ObjectStructureItem* m_buffer;
        Optional<RetiredBuffer*> m_previous;
    };

    struct Storage : public gc {
        Optional<ObjectStructureItem*> m_buffer;
        size_t m_size{ 0 };
        size_t m_capacity{ 0 };
        Optional<RetiredBuffer*> m_retiredBuffers;
        Optional<PropertyNameMapWithCache*> m_map;
    };

    Optional<Storage*> m_storage;
    size_t m_size{ 0 };
};

// Structure GC descriptors trace the first word, now the shared storage
// pointer instead of the TightVector buffer pointer.
COMPILE_ASSERT(sizeof(ObjectStructureTransitionPropertyVector) == sizeof(ObjectStructureItemTightVector), "");

class ObjectStructureItemVector : public Vector<ObjectStructureItem, GCUtil::gc_malloc_allocator<ObjectStructureItem>> {
    typedef Vector<ObjectStructureItem, GCUtil::gc_malloc_allocator<ObjectStructureItem>> ObjectStructureItemVectorType;

public:
    ObjectStructureItemVector()
    {
    }

    ObjectStructureItemVector(const ObjectStructureItemTightVector& other)
    {
        assign(other.data(), other.data() + other.size());
    }

    ObjectStructureItemVector(const ObjectStructureTransitionPropertyVector& other)
    {
        if (!other.empty()) {
            assign(other.data().value(), other.data().value() + other.size());
        }
    }

    ObjectStructureItemVector(const ObjectStructureTransitionPropertyVector& other, const ObjectStructureItem& newItem)
    {
        resizeFitWithUninitializedValues(other.size() + 1);
        if (!other.empty()) {
            memcpy(data(), other.data().value(), other.size() * sizeof(ObjectStructureItem));
        }
        back() = newItem;
    }

    ObjectStructureItemVector(const ObjectStructureItemVector& other)
        : ObjectStructureItemVectorType(other)
    {
    }

    ObjectStructureItemVector(const ObjectStructureItemVector& other, const ObjectStructureItem& newItem)
        : ObjectStructureItemVectorType(other, newItem)
    {
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class ObjectStructureIndexPropertyVector : public Vector<uint32_t, GCUtil::gc_malloc_atomic_allocator<uint32_t>> {
    using Base = Vector<uint32_t, GCUtil::gc_malloc_atomic_allocator<uint32_t>>;

public:
    ObjectStructureIndexPropertyVector() = default;
    ObjectStructureIndexPropertyVector(const ObjectStructureIndexPropertyVector& other)
        : Base(other)
    {
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class ObjectStructureIndexDescriptorVector : public Vector<ObjectStructurePropertyDescriptor, GCUtil::gc_malloc_allocator<ObjectStructurePropertyDescriptor>> {
    using Base = Vector<ObjectStructurePropertyDescriptor, GCUtil::gc_malloc_allocator<ObjectStructurePropertyDescriptor>>;

public:
    ObjectStructureIndexDescriptorVector() = default;
    ObjectStructureIndexDescriptorVector(const ObjectStructureIndexDescriptorVector& other)
        : Base(other)
    {
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

#if defined(ESCARGOT_SMALL_CONFIG)
#ifndef ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 2048
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE 128
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE 32
#endif
#else
#ifndef ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 64
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE 128
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE 32
#endif
#endif

COMPILE_ASSERT(ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE < 65536, "");

#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_ACCESS_CACHE_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_ACCESS_CACHE_MIN_SIZE 48
#endif

class ObjectStructure : public gc {
public:
    virtual ~ObjectStructure() {}

    static bool isTransitionModeAvailable(size_t propertyCount)
    {
        return propertyCount <= ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE;
    }
    static ObjectStructure* create(Context* ctx, ObjectStructureItemTightVector&& properties, bool preferTransition = false);

    ObjectStructureFindResult findProperty(ExecutionState& state, String* propertyName)
    {
        if (UNLIKELY(m_hasIndexPropertyName)) {
            uint32_t index = propertyName->tryToUseAsIndexProperty();
            if (index != Value::InvalidIndexPropertyValue) {
                return findIndexProperty(index);
            }
        }
        ObjectStructurePropertyName name(state, propertyName);
        return findNonIndexProperty(name);
    }

    ObjectStructure* addProperty(ExecutionState& state, String* propertyName, const ObjectStructurePropertyDescriptor& desc)
    {
        uint32_t index = propertyName->tryToUseAsIndexProperty();
        if (index != Value::InvalidIndexPropertyValue) {
            return addIndexProperty(index, desc);
        }
        ObjectStructurePropertyName name(state, propertyName);
        return addNonIndexProperty(name, desc);
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) = 0;
    virtual ObjectStructureFindResult findNonIndexProperty(const ObjectStructurePropertyName& s)
    {
        return findProperty(s);
    }
    virtual ObjectStructureFindResult findIndexProperty(uint32_t index) = 0;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const = 0;
    virtual bool isIndexProperty(size_t valueIndex) const = 0;
    virtual uint32_t indexPropertyName(size_t valueIndex) const = 0;
    // Value indexes span the string and symbol buffers; array indexes have a
    // separate accessor. Each data accessor returns only its own key domain.
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const = 0;
    virtual Optional<const ObjectStructureItem*> stringPropertiesData() const
    {
        return nullptr;
    }
    virtual Optional<const ObjectStructureItem*> symbolPropertiesData() const
    {
        return nullptr;
    }
    virtual size_t propertyCount() const = 0;
    virtual size_t namedPropertyCount() const = 0;
    virtual size_t indexPropertyCount() const
    {
        return propertyCount() - namedPropertyCount();
    }
    virtual void fillIndexPropertyOrdinalsInOrder(uint32_t* ordinals) const
    {
        ASSERT(indexPropertyCount() < UINT32_MAX);
        for (size_t i = 0; i < indexPropertyCount(); i++) {
            ordinals[i] = static_cast<uint32_t>(i);
        }
    }

    ObjectStructurePropertyName propertyName(ExecutionState& state, size_t valueIndex) const
    {
        if (isIndexProperty(valueIndex)) {
            return ObjectStructurePropertyName(state, String::fromUint32(indexPropertyName(valueIndex), state));
        }
        return nonIndexPropertyName(valueIndex);
    }
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) = 0;
    virtual ObjectStructure* addNonIndexProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
    {
        return addProperty(name, desc);
    }
    virtual ObjectStructure* addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc) = 0;
    virtual ObjectStructure* removeProperty(size_t pIndex) = 0;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) = 0;

    virtual bool hasPartitionedNonIndexProperties() const
    {
        return false;
    }

    virtual size_t stringPropertyCount() const
    {
        ASSERT(!hasSymbolPropertyName());
        return propertyCount();
    }

    virtual ObjectStructure* convertToNonTransitionStructure()
    {
        return this;
    }

    virtual bool inTransitionMode()
    {
        return false;
    }

    void markReferencedByInlineCache()
    {
        m_isReferencedByInlineCache = true;
    }

    bool hasIndexPropertyName() const
    {
        return m_hasIndexPropertyName;
    }

    bool hasSymbolPropertyName() const
    {
        return m_hasSymbolPropertyName;
    }

    bool hasEnumerableProperty() const
    {
        return m_hasEnumerableProperty;
    }

    bool isReferencedByInlineCache() const
    {
        return m_isReferencedByInlineCache;
    }

    bool hasSamePropertiesTo(const ObjectStructure* src) const
    {
        size_t myCount = propertyCount();
        size_t srcCount = src->propertyCount();
        if (myCount == srcCount) {
            for (size_t j = 0; j < myCount; j++) {
                bool myIndex = isIndexProperty(j);
                bool srcIndex = src->isIndexProperty(j);
                if (myIndex != srcIndex || propertyDescriptor(j) != src->propertyDescriptor(j)) {
                    return false;
                }
                if (myIndex ? indexPropertyName(j) != src->indexPropertyName(j)
                            : nonIndexPropertyName(j) != src->nonIndexPropertyName(j)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

protected:
    template <typename Properties>
    static void assertPropertyDomain(const Properties& properties, bool isSymbol)
    {
#ifndef NDEBUG
        for (size_t i = 0; i < properties.size(); i++) {
            const auto& item = properties[i];
            ASSERT(item.m_propertyName.isSymbol() == isSymbol);
            ASSERT(item.m_propertyName.tryToUseAsIndexProperty() == Value::InvalidIndexPropertyValue);
        }
#endif
    }

    ObjectStructure(bool hasIndexPropertyName,
                    bool hasSymbolPropertyName, bool hasEnumerableProperty)
        : m_doesTransitionTableUseMap(false)
        , m_hasIndexPropertyName(hasIndexPropertyName)
        , m_hasSymbolPropertyName(hasSymbolPropertyName)
        , m_hasNonAtomicPropertyName(false)
        , m_hasEnumerableProperty(hasEnumerableProperty)
        , m_isReferencedByInlineCache(false)
        , m_transitionTableVectorBufferSize(0)
        , m_transitionTableVectorBufferCapacity(0)
    {
    }

    ObjectStructure(bool hasIndexPropertyName,
                    bool hasSymbolPropertyName, bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : m_doesTransitionTableUseMap(false)
        , m_hasIndexPropertyName(hasIndexPropertyName)
        , m_hasSymbolPropertyName(hasSymbolPropertyName)
        , m_hasNonAtomicPropertyName(hasNonAtomicPropertyName)
        , m_hasEnumerableProperty(hasEnumerableProperty)
        , m_isReferencedByInlineCache(false)
        , m_transitionTableVectorBufferSize(0)
        , m_transitionTableVectorBufferCapacity(0)
    {
    }

    // every class should share flag members
    // this way can reduce size of ObjectStructureWithTransition
    bool m_doesTransitionTableUseMap : 1;
    bool m_hasIndexPropertyName : 1;
    bool m_hasSymbolPropertyName : 1;
    bool m_hasNonAtomicPropertyName : 1;
    bool m_hasEnumerableProperty : 1;
    bool m_isReferencedByInlineCache : 1;
    uint8_t m_transitionTableVectorBufferSize : 8;
    uint8_t m_transitionTableVectorBufferCapacity : 8;
};

struct ObjectStructureEqualTo {
    bool operator()(const ObjectStructure* x, const ObjectStructure* y) const
    {
        return x->hasSamePropertiesTo(y);
    }
};

struct ObjectStructureHash {
    size_t operator()(const ObjectStructure* x) const
    {
        size_t propertyCount = x->propertyCount();
        size_t result = propertyCount;
        size_t hashPropertyCount = std::min(propertyCount, static_cast<size_t>(6));
        for (size_t i = 0; i < hashPropertyCount; i++) {
            result += x->isIndexProperty(i) ? x->indexPropertyName(i) : x->nonIndexPropertyName(i).hashValue();
        }
        return result;
    }
};

class ObjectStructureWithoutTransition : public ObjectStructure {
public:
    ObjectStructureWithoutTransition(ObjectStructureItemVector* properties, bool hasIndexPropertyName,
                                     bool hasSymbolPropertyName, bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasNonAtomicPropertyName, hasEnumerableProperty)
        , m_properties(properties)
    {
        ASSERT(!hasSymbolPropertyName);
        assertPropertyDomain(*m_properties.value(), false);
        size_t propertyCount = m_properties->size();
        ASSERT(propertyCount < 65535);
        if (LIKELY(propertyCount)) {
            m_lastFoundPropertyName = m_properties->back().m_propertyName;
            setLastFoundPropertyIndex(propertyCount - 1);
        }
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) override;
    virtual ObjectStructureFindResult findIndexProperty(uint32_t index) override;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const override;
    virtual bool isIndexProperty(size_t valueIndex) const override;
    virtual uint32_t indexPropertyName(size_t valueIndex) const override;
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const override;
    virtual Optional<const ObjectStructureItem*> stringPropertiesData() const override;
    virtual size_t propertyCount() const override;
    virtual size_t namedPropertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* removeProperty(size_t pIndex) override;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    void setLastFoundPropertyIndex(uint16_t s)
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(&s);
        m_transitionTableVectorBufferSize = *p;
        p++;
        m_transitionTableVectorBufferCapacity = *p;
    }

    uint16_t lastFoundPropertyIndex()
    {
        uint16_t s;
        uint8_t* p = reinterpret_cast<uint8_t*>(&s);
        *p = m_transitionTableVectorBufferSize;
        p++;
        *p = m_transitionTableVectorBufferCapacity;
        return s;
    }

private:
    Optional<ObjectStructureItemVector*> m_properties;
    ObjectStructurePropertyName m_lastFoundPropertyName;
};

class ObjectStructureWithTransition : public ObjectStructure {
public:
    static ObjectStructureWithTransition* create(ObjectStructureItemTightVector&& strings, ObjectStructureItemTightVector&& symbols,
                                                 bool hasNonAtomicPropertyName, bool hasEnumerableProperty);
    static ObjectStructureWithTransition* create(ObjectStructureTransitionPropertyVector&& strings, ObjectStructureTransitionPropertyVector&& symbols,
                                                 bool hasNonAtomicPropertyName, bool hasEnumerableProperty);

    ObjectStructureWithTransition(ObjectStructureTransitionPropertyVector&& properties, bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasNonAtomicPropertyName, hasEnumerableProperty)
        , m_properties(std::move(properties))
        , m_transitionTableVectorBuffer(nullptr)
    {
        ASSERT(!hasIndexPropertyName);
        assertPropertyDomain(m_properties, false);
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) override;
    virtual ObjectStructureFindResult findIndexProperty(uint32_t index) override;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const override;
    virtual bool isIndexProperty(size_t valueIndex) const override;
    virtual uint32_t indexPropertyName(size_t valueIndex) const override;
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const override;
    virtual Optional<const ObjectStructureItem*> stringPropertiesData() const override;
    virtual size_t propertyCount() const override;
    virtual size_t namedPropertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* removeProperty(size_t pIndex) override;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) override;
    virtual ObjectStructure* convertToNonTransitionStructure() override;

    virtual bool inTransitionMode() override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    size_t computeVectorAllocateSize(size_t newSize)
    {
        if (newSize == 0) {
            return 1;
        }
        size_t base = FAST_LOG2_UINT(newSize);
        return size_t(1) << (base + 1);
    }

protected:
    virtual Optional<const ObjectStructureTransitionPropertyVector*> symbolProperties() const
    {
        return nullptr;
    }

    ObjectStructureTransitionPropertyVector m_properties;
    union {
        Optional<ObjectStructureTransitionVectorItem*> m_transitionTableVectorBuffer;
        ObjectStructureTransitionTableMap* m_transitionTableMap;
    };
};

// Keep symbol keys in their own buffer while sharing the transition machinery.
class ObjectStructureWithTransitionAndSymbols : public ObjectStructureWithTransition {
public:
    ObjectStructureWithTransitionAndSymbols(ObjectStructureTransitionPropertyVector&& strings, ObjectStructureTransitionPropertyVector&& symbols,
                                            bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : ObjectStructureWithTransition(std::move(strings), false, !symbols.empty(), hasNonAtomicPropertyName, hasEnumerableProperty)
        , m_symbolProperties(std::move(symbols))
    {
        assertPropertyDomain(m_symbolProperties, true);
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) override;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const override;
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const override;
    virtual Optional<const ObjectStructureItem*> symbolPropertiesData() const override
    {
        return m_symbolProperties.data();
    }
    virtual size_t propertyCount() const override
    {
        return m_properties.size() + m_symbolProperties.size();
    }
    virtual bool hasPartitionedNonIndexProperties() const override
    {
        return hasSymbolPropertyName();
    }
    virtual size_t stringPropertyCount() const override
    {
        return m_properties.size();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    virtual Optional<const ObjectStructureTransitionPropertyVector*> symbolProperties() const override
    {
        return &m_symbolProperties;
    }

    ObjectStructureTransitionPropertyVector m_symbolProperties;
};

COMPILE_ASSERT(ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE <= 32, "");
COMPILE_ASSERT(sizeof(ObjectStructureWithTransition) == sizeof(size_t) * 5, "");

// An index into the insertion-ordered properties, not a second copy of the
// keys. The bucket storage contains only integers and is not scanned by GC.
class PropertyNameMapWithCache : public gc {
public:
    explicit PropertyNameMapWithCache(const ObjectStructureItemVector& properties);
    explicit PropertyNameMapWithCache(const ObjectStructureItemTightVector& properties);
    explicit PropertyNameMapWithCache(const ObjectStructureTransitionPropertyVector& properties);

    ~PropertyNameMapWithCache()
    {
        GC_FREE(m_entries.unwrap());
    }

    size_t size() const
    {
        return m_size;
    }

    void insert(const ObjectStructureItemVector& properties);
    void insert(const ObjectStructureTransitionPropertyVector& properties);
    size_t find(const ObjectStructurePropertyName& name, const ObjectStructureItemVector& properties);
    size_t find(const ObjectStructurePropertyName& name, const ObjectStructureItemTightVector& properties);
    size_t find(const ObjectStructurePropertyName& name, const ObjectStructureTransitionPropertyVector& properties);

private:
    static uint8_t entryWidth(size_t count);
    size_t hash(const ObjectStructurePropertyName& name) const;
    template <typename Properties>
    void rebuild(const Properties& properties);
    template <typename Properties>
    void insertInProperties(const Properties& properties);
    template <typename Properties>
    size_t findInProperties(const ObjectStructurePropertyName& name, const Properties& properties);
    template <typename Entry>
    void insertEntry(const ObjectStructurePropertyName& name, size_t index);
    template <typename Entry, typename Properties>
    size_t findEntry(const ObjectStructurePropertyName& name, const Properties& properties) const;

    Optional<void*> m_entries;
    size_t m_capacity{ 0 };
    size_t m_denseCapacity{ 0 };
    size_t m_occupied{ 0 };
    size_t m_size{ 0 };
    ObjectStructurePropertyName m_lastName;
    size_t m_lastIndex{ SIZE_MAX };
    uint8_t m_entryWidth{ 1 };
    bool m_hasNonAtomicNames{ false };
};

// Small structures retain their compact layout. Large key domains lazily
// allocate bucket indexes into the existing, separately stored keys.
class ObjectStructureWithTransitionWithMap : public ObjectStructureWithTransitionAndSymbols {
public:
    ObjectStructureWithTransitionWithMap(ObjectStructureTransitionPropertyVector&& strings, ObjectStructureTransitionPropertyVector&& symbols,
                                         bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : ObjectStructureWithTransitionAndSymbols(std::move(strings), std::move(symbols), hasNonAtomicPropertyName, hasEnumerableProperty)
    {
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& name) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class ObjectStructureWithMap : public ObjectStructure {
public:
    ObjectStructureWithMap(ObjectStructureItemVector* properties, Optional<PropertyNameMapWithCache*> map, bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)
        , m_properties(properties)
        , m_propertyNameMap(map)
    {
        ASSERT(!hasSymbolPropertyName);
        assertPropertyDomain(*m_properties.value(), false);
    }

    template <typename SourceProperties>
    ObjectStructureWithMap(bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty, const SourceProperties& properties, const ObjectStructureItem& newItem)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)

    {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
        newProperties->resizeWithUninitializedValues(properties.size() + 1);
        memcpy(newProperties->data(), properties.data(), properties.size() * sizeof(ObjectStructureItem));
        newProperties->at(properties.size()) = newItem;

        m_properties = newProperties;
        ASSERT(!hasSymbolPropertyName);
        assertPropertyDomain(*m_properties.value(), false);
    }

    ObjectStructureWithMap(bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty, const ObjectStructureItemTightVector& properties)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)

    {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
        newProperties->resizeFitWithUninitializedValues(properties.size());
        memcpy(newProperties->data(), properties.data(), properties.size() * sizeof(ObjectStructureItem));

        m_properties = newProperties;
        ASSERT(!hasSymbolPropertyName);
        assertPropertyDomain(*m_properties.value(), false);
    }

    template <typename ItemVector>
    ObjectStructureWithMap(bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty, ItemVector&& properties)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)
    {
        m_properties = new ObjectStructureItemVector(std::move(properties));
        ASSERT(!hasSymbolPropertyName);
        assertPropertyDomain(*m_properties.value(), false);
    }

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) override;
    virtual ObjectStructureFindResult findIndexProperty(uint32_t index) override;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const override;
    virtual bool isIndexProperty(size_t valueIndex) const override;
    virtual uint32_t indexPropertyName(size_t valueIndex) const override;
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const override;
    virtual Optional<const ObjectStructureItem*> stringPropertiesData() const override;
    virtual size_t propertyCount() const override;
    virtual size_t namedPropertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* removeProperty(size_t pIndex) override;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static PropertyNameMapWithCache* createPropertyNameMap(ObjectStructureItemVector* from)
    {
        return new PropertyNameMapWithCache(*from);
    }

private:
    CompressibleHeapPointer<ObjectStructureItemVector> m_properties;
    CompressibleHeapPointer<PropertyNameMapWithCache> m_propertyNameMap;
};

// Property metadata is partitioned into string, symbol, and canonical
// array-index areas. Values use the same string -> symbol -> index partition
// in Object::m_values; enumeration visits indexes first without sorting the
// string/symbol areas.
class IndexPropertyOrderChunk : public gc {
public:
    static constexpr size_t Capacity = 128;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    size_t m_size{ 0 };
    uint32_t m_ordinals[Capacity];
};

class IndexPropertyMapWithCache : public gc {
public:
    explicit IndexPropertyMapWithCache(const ObjectStructureIndexPropertyVector& properties);

    size_t find(uint32_t index, const ObjectStructureIndexPropertyVector& properties) const;
    void insert(const ObjectStructureIndexPropertyVector& properties);
    void fillOrdinalsInOrder(uint32_t* ordinals) const;

private:
    void rebuildHash(const ObjectStructureIndexPropertyVector& properties);
    void insertHashEntry(uint32_t index, size_t ordinal);
    void rebuildOrder(const ObjectStructureIndexPropertyVector& properties);
    void insertOrder(const ObjectStructureIndexPropertyVector& properties, uint32_t ordinal);

    Vector<IndexPropertyOrderChunk*, GCUtil::gc_malloc_allocator<IndexPropertyOrderChunk*>> m_orderChunks;
    Optional<uint32_t*> m_entries;
    size_t m_capacity{ 0 };
    size_t m_size{ 0 };
};

class ObjectStructureWithIndexProperties : public ObjectStructure {
public:
    struct InlineIndexProperties {
        static constexpr size_t Capacity = 2;

        uint32_t m_keys[Capacity]{ 0, 0 };
        uint8_t m_size{ 0 };
    };

    template <typename SourceProperties>
    explicit ObjectStructureWithIndexProperties(const SourceProperties& properties)
        : ObjectStructure(false, false, false, false)
        , m_namedProperties(emptyProperties())
        , m_symbolProperties(emptyProperties())
        , m_indexProperties(nullptr)
        , m_indexDescriptors(nullptr)
    {
        size_t indexPropertyCount = 0;
        size_t namedPropertyCount = 0;
        size_t symbolPropertyCount = 0;
        for (size_t i = 0; i < properties.size(); i++) {
            const auto& name = properties[i].m_propertyName;
            if (name.tryToUseAsIndexProperty() != Value::InvalidIndexPropertyValue) {
                indexPropertyCount++;
            } else if (name.isSymbol()) {
                symbolPropertyCount++;
            } else {
                namedPropertyCount++;
            }
        }
        if (indexPropertyCount > InlineIndexProperties::Capacity) {
            m_indexProperties = new ObjectStructureIndexPropertyVector();
            m_indexProperties->reserve(indexPropertyCount);
        }
        if (namedPropertyCount) {
            m_namedProperties = new ObjectStructureItemVector();
            m_namedProperties->reserve(namedPropertyCount);
        }
        if (symbolPropertyCount) {
            m_symbolProperties = new ObjectStructureItemVector();
            m_symbolProperties->reserve(symbolPropertyCount);
        }
        for (size_t i = 0; i < properties.size(); i++) {
            appendPartitioned(properties[i]);
        }
        sortIndexProperties();
        finishConstruction();
    }

    template <typename SourceProperties>
    ObjectStructureWithIndexProperties(const SourceProperties& properties, uint32_t index, const ObjectStructurePropertyDescriptor& desc)
        : ObjectStructure(true, false, false, false)
        , m_namedProperties(emptyProperties())
        , m_symbolProperties(emptyProperties())
        , m_indexProperties(nullptr)
        , m_indexDescriptors(nullptr)
    {
        size_t indexPropertyCount = 1;
        size_t namedPropertyCount = 0;
        size_t symbolPropertyCount = 0;
        for (size_t i = 0; i < properties.size(); i++) {
            const auto& name = properties[i].m_propertyName;
            if (name.tryToUseAsIndexProperty() != Value::InvalidIndexPropertyValue) {
                indexPropertyCount++;
            } else if (name.isSymbol()) {
                symbolPropertyCount++;
            } else {
                namedPropertyCount++;
            }
        }
        if (indexPropertyCount > InlineIndexProperties::Capacity) {
            m_indexProperties = new ObjectStructureIndexPropertyVector();
            m_indexProperties->reserve(indexPropertyCount);
        }
        if (namedPropertyCount) {
            m_namedProperties = new ObjectStructureItemVector();
            m_namedProperties->reserve(namedPropertyCount);
        }
        if (symbolPropertyCount) {
            m_symbolProperties = new ObjectStructureItemVector();
            m_symbolProperties->reserve(symbolPropertyCount);
        }
        for (size_t i = 0; i < properties.size(); i++) {
            appendPartitioned(properties[i]);
        }
        appendIndexProperty(index, desc);
        sortIndexProperties();
        finishConstruction();
    }

    ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                       ObjectStructureItemVector* symbolProperties,
                                       Optional<ObjectStructureIndexPropertyVector*> indexProperties,
                                       const InlineIndexProperties& inlineIndexProperties,
                                       Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors,
                                       Optional<PropertyNameMapWithCache*> namedMap,
                                       Optional<IndexPropertyMapWithCache*> indexMap);
    ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                       ObjectStructureItemVector* symbolProperties,
                                       Optional<ObjectStructureIndexPropertyVector*> indexProperties,
                                       const InlineIndexProperties& inlineIndexProperties,
                                       Optional<ObjectStructureIndexDescriptorVector*> indexDescriptors,
                                       Optional<PropertyNameMapWithCache*> namedMap,
                                       Optional<IndexPropertyMapWithCache*> indexMap,
                                       bool hasNonAtomicPropertyName,
                                       bool hasEnumerableProperty);

    virtual ObjectStructureFindResult findProperty(const ObjectStructurePropertyName& s) override;
    virtual ObjectStructureFindResult findNonIndexProperty(const ObjectStructurePropertyName& s) override;
    virtual ObjectStructureFindResult findIndexProperty(uint32_t index) override;
    virtual const ObjectStructurePropertyDescriptor& propertyDescriptor(size_t valueIndex) const override;
    virtual bool isIndexProperty(size_t valueIndex) const override;
    virtual uint32_t indexPropertyName(size_t valueIndex) const override;
    virtual const ObjectStructurePropertyName& nonIndexPropertyName(size_t valueIndex) const override;
    virtual Optional<const ObjectStructureItem*> stringPropertiesData() const override
    {
        return m_namedProperties->data();
    }
    virtual Optional<const ObjectStructureItem*> symbolPropertiesData() const override
    {
        return m_symbolProperties->data();
    }
    virtual size_t propertyCount() const override;
    virtual size_t namedPropertyCount() const override;
    virtual void fillIndexPropertyOrdinalsInOrder(uint32_t* ordinals) const override;
    virtual bool hasPartitionedNonIndexProperties() const override
    {
        return true;
    }
    virtual size_t stringPropertyCount() const override
    {
        return m_namedProperties->size();
    }
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* addNonIndexProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* removeProperty(size_t valueIndex) override;
    virtual ObjectStructure* replacePropertyDescriptor(size_t valueIndex, const ObjectStructurePropertyDescriptor& newDesc) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    static ObjectStructureItemVector* emptyProperties()
    {
        static ObjectStructureItemVector empty;
        return &empty;
    }
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    // Null in the heap slot denotes the existing static empty vector.
    // The static address is reconstructed only when C++ reads the field.
    class PropertyVectorPointer {
    public:
        PropertyVectorPointer(ObjectStructureItemVector* pointer = nullptr) { set(pointer); }
        PropertyVectorPointer(Optional<ObjectStructureItemVector*> pointer) { set(pointer); }
        PropertyVectorPointer(const PropertyVectorPointer&) = delete;
        PropertyVectorPointer& operator=(const PropertyVectorPointer&) = delete;

        void set(Optional<ObjectStructureItemVector*> pointer)
        {
            if (!pointer || pointer.value() == emptyProperties()) {
                m_pointer.set(nullptr);
            } else {
                m_pointer.set(pointer);
            }
        }

        PropertyVectorPointer& operator=(ObjectStructureItemVector* pointer)
        {
            set(pointer);
            return *this;
        }

        PropertyVectorPointer& operator=(Optional<ObjectStructureItemVector*> pointer)
        {
            set(pointer);
            return *this;
        }

        ObjectStructureItemVector* value() const
        {
            return m_pointer ? m_pointer.value() : emptyProperties();
        }

        ObjectStructureItemVector* operator->() const { return value(); }
        operator Optional<ObjectStructureItemVector*>() const { return value(); }

    private:
        CompressibleHeapPointer<ObjectStructureItemVector> m_pointer;
    };
    static_assert(sizeof(PropertyVectorPointer) == 4, "empty-aware pointer slot must be four bytes");
#else
    using PropertyVectorPointer = Optional<ObjectStructureItemVector*>;
#endif

    static ObjectStructureItemVector* copyProperties(ObjectStructureItemVector* properties)
    {
        return properties == emptyProperties() ? properties : new ObjectStructureItemVector(*properties);
    }

    static const ObjectStructurePropertyDescriptor& defaultIndexPropertyDescriptor();
    static bool isDefaultIndexPropertyDescriptor(const ObjectStructurePropertyDescriptor& descriptor);
    size_t indexPropertyStorageSize() const;
    uint32_t indexPropertyAt(size_t ordinal) const;
    void appendIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& descriptor);
    void appendPartitioned(const ObjectStructureItem& item)
    {
        uint32_t index = item.m_propertyName.tryToUseAsIndexProperty();
        if (index != Value::InvalidIndexPropertyValue) {
            appendIndexProperty(index, item.m_descriptor);
        } else if (item.m_propertyName.isSymbol()) {
            m_symbolProperties->push_back(item);
        } else {
            m_namedProperties->push_back(item);
        }
    }
    void sortIndexProperties();
    void finishConstruction();

    PropertyVectorPointer m_namedProperties;
    PropertyVectorPointer m_symbolProperties;
    // One- and two-key numeric structures keep their sorted keys in the
    // structure itself. The external atomic vector starts at the third key.
    InlineIndexProperties m_inlineIndexProperties;
    CompressibleHeapPointer<ObjectStructureIndexPropertyVector> m_indexProperties;
    // An absent vector means every numeric property has the common plain-data W/E/C
    // descriptor. Materialize the parallel vector only for an exception.
    CompressibleHeapPointer<ObjectStructureIndexDescriptorVector> m_indexDescriptors;
    CompressibleHeapPointer<PropertyNameMapWithCache> m_namedPropertyMap;
    CompressibleHeapPointer<IndexPropertyMapWithCache> m_indexPropertyMap;
};
} // namespace Escargot

namespace std {

template <>
struct is_fundamental<Escargot::ObjectStructureItem> : public true_type {
};

template <>
struct is_fundamental<Escargot::ObjectStructureTransitionVectorItem> : public true_type {
};

template <>
struct hash<Escargot::ObjectStructureTransitionMapItem> {
    size_t operator()(Escargot::ObjectStructureTransitionMapItem const& x) const
    {
        return x.m_propertyName.hashValue() + x.m_descriptor.rawValue();
    }
};

template <>
struct equal_to<Escargot::ObjectStructureTransitionMapItem> {
    bool operator()(Escargot::ObjectStructureTransitionMapItem const& a, Escargot::ObjectStructureTransitionMapItem const& b) const
    {
        return a.m_descriptor == b.m_descriptor && a.m_propertyName == b.m_propertyName;
    }
};
} // namespace std

namespace tsl {
namespace detail_robin_hash {
template <>
struct should_never_store_hash<Escargot::ObjectStructure*> : std::false_type {
};
} // namespace detail_robin_hash
} // namespace tsl
#endif
