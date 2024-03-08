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

#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"
#include "runtime/ObjectStructurePropertyName.h"
#include "runtime/ObjectStructurePropertyDescriptor.h"

namespace Escargot {

class ObjectStructure;

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

#if defined(ESCARGOT_SMALL_CONFIG)
#ifndef ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 2048
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE 12
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE 32
#endif
#else
#ifndef ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 512
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE 36
#endif
#ifndef ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE 32
#endif
#endif

COMPILE_ASSERT(ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE < 65536, "");

class ObjectStructure : public gc {
public:
    virtual ~ObjectStructure() {}

    static bool isTransitionModeAvailable(size_t propertyCount)
    {
        return propertyCount <= ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE;
    }
    static ObjectStructure* create(Context* ctx, ObjectStructureItemTightVector&& properties, bool preferTransition = false);

    std::pair<size_t, Optional<const ObjectStructureItem*>> findProperty(ExecutionState& state, String* propertyName)
    {
        ObjectStructurePropertyName name(state, propertyName);
        return findProperty(name);
    }

    ObjectStructure* addProperty(ExecutionState& state, String* propertyName, const ObjectStructurePropertyDescriptor& desc)
    {
        ObjectStructurePropertyName name(state, propertyName);
        return addProperty(name, desc);
    }

    virtual std::pair<size_t, Optional<const ObjectStructureItem*>> findProperty(const ObjectStructurePropertyName& s) = 0;
    virtual const ObjectStructureItem& readProperty(size_t idx) = 0;
    virtual const ObjectStructureItem* properties() const = 0;
    virtual size_t propertyCount() const = 0;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) = 0;
    virtual ObjectStructure* removeProperty(size_t pIndex) = 0;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) = 0;

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
            auto myProperties = properties();
            auto srcProperties = src->properties();
            for (size_t j = 0; j < myCount; j++) {
                if (myProperties[j].m_propertyName != srcProperties[j].m_propertyName || myProperties[j].m_descriptor != srcProperties[j].m_descriptor) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

protected:
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
        auto properties = x->properties();
        for (size_t i = 0; i < hashPropertyCount; i++) {
            result += properties[i].m_propertyName.hashValue();
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
        size_t propertyCount = m_properties->size();
        ASSERT(propertyCount < 65535);
        if (LIKELY(propertyCount)) {
            m_lastFoundPropertyName = m_properties->back().m_propertyName;
            setLastFoundPropertyIndex(propertyCount - 1);
        }
    }

    virtual std::pair<size_t, Optional<const ObjectStructureItem*>> findProperty(const ObjectStructurePropertyName& s) override;
    virtual const ObjectStructureItem& readProperty(size_t idx) override;
    virtual const ObjectStructureItem* properties() const override;
    virtual size_t propertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
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
    ObjectStructureItemVector* m_properties;
    ObjectStructurePropertyName m_lastFoundPropertyName;
};

class ObjectStructureWithTransition : public ObjectStructure {
public:
    ObjectStructureWithTransition(ObjectStructureItemTightVector&& properties, bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasNonAtomicPropertyName, bool hasEnumerableProperty)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasNonAtomicPropertyName, hasEnumerableProperty)
        , m_properties(std::move(properties))
        , m_transitionTableVectorBuffer(nullptr)
    {
    }

    virtual std::pair<size_t, Optional<const ObjectStructureItem*>> findProperty(const ObjectStructurePropertyName& s) override;
    virtual const ObjectStructureItem& readProperty(size_t idx) override;
    virtual const ObjectStructureItem* properties() const override;
    virtual size_t propertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
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

    ObjectStructureItemTightVector m_properties;
    union {
        ObjectStructureTransitionVectorItem* m_transitionTableVectorBuffer;
        ObjectStructureTransitionTableMap* m_transitionTableMap;
    };
};

COMPILE_ASSERT(ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE <= 32, "");
COMPILE_ASSERT(sizeof(ObjectStructureWithTransition) == sizeof(size_t) * 5, "");

class PropertyNameMapWithCache : protected PropertyNameMap, public gc {
    struct CacheItem {
        ObjectStructurePropertyName m_name;
        size_t m_index;
        CacheItem()
            : m_name()
            , m_index(SIZE_MAX)
        {
        }
    };

public:
    PropertyNameMapWithCache()
        : PropertyNameMap()
    {
    }

    void reserve(size_t t)
    {
        PropertyNameMap::reserve(t);
    }

    size_t size() const
    {
        return PropertyNameMap::size();
    }

    void insert(const ObjectStructurePropertyName& name, size_t idx)
    {
        m_lastItem.m_name = name;
        m_lastItem.m_index = idx;
        PropertyNameMap::insert(std::make_pair(name, idx));
    }

    size_t find(const ObjectStructurePropertyName& name)
    {
        if (name == m_lastItem.m_name) {
#ifndef NDEBUG
            auto iter = PropertyNameMap::find(name);
            if (iter == end()) {
                ASSERT(m_lastItem.m_index == SIZE_MAX);
            } else {
                ASSERT(m_lastItem.m_index == m_lastItem.m_index);
            }
#endif
            return m_lastItem.m_index;
        }
        m_lastItem.m_name = name;
        auto iter = PropertyNameMap::find(name);
        if (iter == end()) {
            m_lastItem.m_index = SIZE_MAX;
            return SIZE_MAX;
        }
        m_lastItem.m_index = iter->second;
        return iter->second;
    }

private:
    CacheItem m_lastItem;
};

class ObjectStructureWithMap : public ObjectStructure {
public:
    ObjectStructureWithMap(ObjectStructureItemVector* properties, Optional<PropertyNameMapWithCache*> map, bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)
        , m_properties(properties)
        , m_propertyNameMap(map)
    {
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
    }

    ObjectStructureWithMap(bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty, const ObjectStructureItemTightVector& properties)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)

    {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
        newProperties->resizeFitWithUninitializedValues(properties.size());
        memcpy(newProperties->data(), properties.data(), properties.size() * sizeof(ObjectStructureItem));

        m_properties = newProperties;
    }

    template <typename ItemVector>
    ObjectStructureWithMap(bool hasIndexPropertyName, bool hasSymbolPropertyName, bool hasEnumerableProperty, ItemVector&& properties)
        : ObjectStructure(hasIndexPropertyName,
                          hasSymbolPropertyName, hasEnumerableProperty)
    {
        m_properties = new ObjectStructureItemVector(std::move(properties));
    }

    virtual std::pair<size_t, Optional<const ObjectStructureItem*>> findProperty(const ObjectStructurePropertyName& s) override;
    virtual const ObjectStructureItem& readProperty(size_t idx) override;
    virtual const ObjectStructureItem* properties() const override;
    virtual size_t propertyCount() const override;
    virtual ObjectStructure* addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc) override;
    virtual ObjectStructure* removeProperty(size_t pIndex) override;
    virtual ObjectStructure* replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    template <typename SourceVectorType>
    static PropertyNameMapWithCache* createPropertyNameMap(SourceVectorType* from)
    {
        PropertyNameMapWithCache* map = new PropertyNameMapWithCache();

        map->reserve(from->size());
        for (size_t i = 0; i < from->size(); i++) {
            map->insert((*from)[i].m_propertyName, i);
        }

        return map;
    }

private:
    ObjectStructureItemVector* m_properties;
    Optional<PropertyNameMapWithCache*> m_propertyNameMap;
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
#endif
