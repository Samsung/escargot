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

typedef std::unordered_map<ObjectStructureTransitionMapItem, ObjectStructure*, std::hash<ObjectStructureTransitionMapItem>,
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
        m_buffer = nullptr;
        m_capacity = 0;
        m_size = 0;
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

#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 96
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE 48
#define ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE 32

class ObjectStructure : public gc {
public:
    virtual ~ObjectStructure() {}
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

    virtual ObjectStructure* convertToNonTransitionStructure() = 0;

    virtual bool inTransitionMode() = 0;
    virtual bool hasIndexPropertyName() = 0;
};

class ObjectStructureWithoutTransition : public ObjectStructure {
public:
    ObjectStructureWithoutTransition(ObjectStructureItemVector* properties, bool hasIndexPropertyName, bool hasNonAtomicPropertyName)
        : m_hasIndexPropertyName(hasIndexPropertyName)
        , m_hasNonAtomicPropertyName(hasNonAtomicPropertyName)
        , m_properties(properties)
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
        return false;
    }

    virtual bool hasIndexPropertyName() override
    {
        return m_hasIndexPropertyName;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    bool m_hasIndexPropertyName;
    bool m_hasNonAtomicPropertyName;
    ObjectStructureItemVector* m_properties;
};

class ObjectStructureWithTransition : public ObjectStructure {
public:
    ObjectStructureWithTransition(ObjectStructureItemTightVector&& properties, bool hasIndexPropertyName, bool hasNonAtomicPropertyName)
        : m_properties(std::move(properties))
        , m_doesTransitionTableUseMap(false)
        , m_hasIndexPropertyName(hasIndexPropertyName)
        , m_hasNonAtomicPropertyName(hasNonAtomicPropertyName)
        , m_transitionTableVectorBufferSize(0)
        , m_transitionTableVectorBufferCapacity(0)
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

    virtual bool hasIndexPropertyName() override
    {
        return m_hasIndexPropertyName;
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
        return 1 << (base + 1);
    }

    ObjectStructureItemTightVector m_properties;

    bool m_doesTransitionTableUseMap : 1;
    bool m_hasIndexPropertyName : 1;
    bool m_hasNonAtomicPropertyName : 1;
    uint8_t m_transitionTableVectorBufferSize;
    uint8_t m_transitionTableVectorBufferCapacity;

    union {
        ObjectStructureTransitionVectorItem* m_transitionTableVectorBuffer;
        ObjectStructureTransitionTableMap* m_transitionTableMap;
    };
};

COMPILE_ASSERT(ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE <= 32, "");
COMPILE_ASSERT(sizeof(ObjectStructureWithTransition) == sizeof(size_t) * 5, "");

class ObjectStructureWithMap : public ObjectStructure {
public:
    ObjectStructureWithMap(ObjectStructureItemVector* properties, PropertyNameMap* map, bool hasIndexPropertyName)
        : m_hasIndexPropertyName(hasIndexPropertyName)
        , m_properties(properties)
        , m_propertyNameMap(map)
    {
    }

    template <typename SourceProperties>
    ObjectStructureWithMap(bool hasIndexPropertyName, const SourceProperties& properties, const ObjectStructureItem& newItem)
        : m_hasIndexPropertyName(hasIndexPropertyName)
    {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
        newProperties->resizeWithUninitializedValues(properties.size() + 1);
        memcpy(newProperties->data(), properties.data(), properties.size() * sizeof(ObjectStructureItem));
        newProperties->at(properties.size()) = newItem;

        m_properties = newProperties;
        m_propertyNameMap = ObjectStructureWithMap::createPropertyNameMap(newProperties);
    }

    ObjectStructureWithMap(bool hasIndexPropertyName, const ObjectStructureItemTightVector& properties)
        : m_hasIndexPropertyName(hasIndexPropertyName)
    {
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
        newProperties->resizeWithUninitializedValues(properties.size());
        memcpy(newProperties->data(), properties.data(), properties.size() * sizeof(ObjectStructureItem));

        m_properties = newProperties;
        m_propertyNameMap = ObjectStructureWithMap::createPropertyNameMap(newProperties);
    }

    ObjectStructureWithMap(bool hasIndexPropertyName, ObjectStructureItemTightVector&& properties)
        : m_hasIndexPropertyName(hasIndexPropertyName)
    {
        m_properties = new ObjectStructureItemVector(std::move(properties));
        m_propertyNameMap = ObjectStructureWithMap::createPropertyNameMap(m_properties);
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
        return false;
    }
    virtual bool hasIndexPropertyName() override
    {
        return m_hasIndexPropertyName;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    template <typename SourceVectorType>
    static PropertyNameMap* createPropertyNameMap(SourceVectorType* from)
    {
        PropertyNameMap* map = new (GC) PropertyNameMap();

        for (size_t i = 0; i < from->size(); i++) {
            map->insert(std::make_pair((*from)[i].m_propertyName, i));
        }

        return map;
    }

private:
    bool m_hasIndexPropertyName;
    ObjectStructureItemVector* m_properties;
    PropertyNameMap* m_propertyNameMap;
};
}

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
}
#endif
