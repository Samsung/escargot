/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "runtime/PropertyName.h"
#include "runtime/ObjectStructurePropertyDescriptor.h"

namespace Escargot {

class ObjectStructure;

struct ObjectStructureItem : public gc {
    ObjectStructureItem(const PropertyName& as, const ObjectStructurePropertyDescriptor& desc)
        : m_propertyName(as)
        , m_descriptor(desc)
    {
    }

    PropertyName m_propertyName;
    ObjectStructurePropertyDescriptor m_descriptor;
};

struct ObjectStructureTransitionItem : public gc {
    PropertyName m_propertyName;
    ObjectStructurePropertyDescriptor m_descriptor;
    ObjectStructure* m_structure;

    ObjectStructureTransitionItem(const PropertyName& as, const ObjectStructurePropertyDescriptor& desc, ObjectStructure* structure)
        : m_propertyName(as)
        , m_descriptor(desc)
        , m_structure(structure)
    {
    }
};

typedef Vector<ObjectStructureItem, GCUtil::gc_malloc_allocator<ObjectStructureItem>> ObjectStructureItemVector;
typedef Vector<ObjectStructureTransitionItem, GCUtil::gc_malloc_allocator<ObjectStructureTransitionItem>> ObjectStructureTransitionTableVector;

#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 96

class ObjectStructure : public gc {
    friend class Object;
    friend class ArrayObject;

public:
    ObjectStructure(ExecutionState&, bool needsTransitionTable = true)
        : m_isProtectedByTransitionTable(false)
        , m_hasIndexPropertyName(false)
        , m_needsTransitionTable(needsTransitionTable)
        , m_isStructureWithFastAccess(false)
    {
    }

    ObjectStructure(ExecutionState&, ObjectStructureItemVector&& properties, bool needsTransitionTable, bool hasIndexPropertyName)
        : m_isProtectedByTransitionTable(false)
        , m_hasIndexPropertyName(hasIndexPropertyName)
        , m_needsTransitionTable(needsTransitionTable)
        , m_isStructureWithFastAccess(false)
        , m_properties(std::move(properties))
    {
    }

    size_t findProperty(ExecutionState& state, String* propertyName)
    {
        PropertyName name(state, propertyName);
        return findProperty(name);
    }

    size_t findProperty(ExecutionState&, const PropertyName& name)
    {
        return findProperty(name);
    }

    size_t findProperty(const PropertyName& s)
    {
        if (UNLIKELY(m_isStructureWithFastAccess)) {
            return findPropertyWithMap(s);
        }

        size_t siz = m_properties.size();
        for (size_t i = 0; i < siz; i++) {
            if (m_properties[i].m_propertyName == s) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    ObjectStructureItem readProperty(ExecutionState& state, String* propertyName)
    {
        return readProperty(state, findProperty(state, propertyName));
    }

    const ObjectStructureItem& readProperty(ExecutionState&, size_t idx)
    {
        return m_properties[idx];
    }

    ObjectStructure* addProperty(ExecutionState& state, String* propertyName, const ObjectStructurePropertyDescriptor& desc)
    {
        PropertyName name(state, propertyName);
        return addProperty(state, name, desc);
    }

    ObjectStructure* addProperty(ExecutionState& state, const PropertyName& name, const ObjectStructurePropertyDescriptor& desc);
    ObjectStructure* removeProperty(ExecutionState& state, size_t pIndex);
    ObjectStructure* escapeTransitionMode(ExecutionState& state);
    ObjectStructure* convertToWithFastAccess(ExecutionState& state);

    bool inTransitionMode()
    {
        return m_needsTransitionTable;
    }

    bool hasIndexPropertyName()
    {
        return m_hasIndexPropertyName;
    }

    bool isStructureWithFastAccess()
    {
        return m_isStructureWithFastAccess;
    }

    bool isProtectedByTransitionTable()
    {
        return m_isProtectedByTransitionTable;
    }

    size_t propertyCount() const
    {
        return m_properties.size();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    bool m_isProtectedByTransitionTable : 1;

protected:
    bool m_hasIndexPropertyName : 1;
    bool m_needsTransitionTable : 1;
    bool m_isStructureWithFastAccess : 1;
    ObjectStructureItemVector m_properties;
    ObjectStructureTransitionTableVector m_transitionTable;

    size_t searchTransitionTable(const PropertyName& s, const ObjectStructurePropertyDescriptor& desc)
    {
        ASSERT(m_needsTransitionTable);
        size_t len = m_transitionTable.size();
        for (size_t i = 0; i < len; i++) {
            if (m_transitionTable[i].m_descriptor == desc && m_transitionTable[i].m_propertyName == s) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    size_t findPropertyWithMap(const PropertyName& s)
    {
        const PropertyNameMap& map = propertyNameMap();
        auto iter = map.find(s);
        if (map.end() == iter) {
            return SIZE_MAX;
        } else {
            return iter->second;
        }
    }

    PropertyNameMap& propertyNameMap();
};

class ObjectStructureWithFastAccess : public ObjectStructure {
    friend class ByteCodeInterpreter;
    friend class ObjectStructure;

public:
    explicit ObjectStructureWithFastAccess(ExecutionState& state)
        : ObjectStructure(state, false)
        , m_propertyNameMap(new (GC) PropertyNameMap())
    {
        m_isStructureWithFastAccess = true;
        buildPropertyNameMap();
    }

    ObjectStructureWithFastAccess(ExecutionState& state, ObjectStructureItemVector&& properties, bool hasIndexPropertyName)
        : ObjectStructure(state, std::move(properties), false, hasIndexPropertyName)
        , m_propertyNameMap(new (GC) PropertyNameMap())
    {
        m_isStructureWithFastAccess = true;
        buildPropertyNameMap();
    }

    ObjectStructureWithFastAccess(ExecutionState& state, ObjectStructureWithFastAccess& old)
        : ObjectStructure(state, std::move(old.m_properties), old.m_needsTransitionTable, old.m_hasIndexPropertyName)
        , m_propertyNameMap(old.m_propertyNameMap)
    {
        m_isStructureWithFastAccess = true;
        old.m_propertyNameMap = nullptr;
    }

    void buildPropertyNameMap()
    {
        m_propertyNameMap->clear();
        size_t len = m_properties.size();
        for (size_t i = 0; i < len; i++) {
            m_propertyNameMap->insert(std::make_pair(m_properties[i].m_propertyName, i));
        }
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    PropertyNameMap* m_propertyNameMap;
};

inline PropertyNameMap& ObjectStructure::propertyNameMap()
{
    ASSERT(m_isStructureWithFastAccess);
    ObjectStructureWithFastAccess* self = (ObjectStructureWithFastAccess*)this;
    return *self->m_propertyNameMap;
}

inline ObjectStructure* ObjectStructure::addProperty(ExecutionState& state, const PropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    if (m_isStructureWithFastAccess) {
        m_properties.pushBack(newItem);
        bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
        m_hasIndexPropertyName = m_hasIndexPropertyName | nameIsIndexString;
        propertyNameMap().insert(std::make_pair(name, m_properties.size() - 1));
        ObjectStructureWithFastAccess* self = (ObjectStructureWithFastAccess*)this;
        ObjectStructureWithFastAccess* newSelf = new ObjectStructureWithFastAccess(state, *self);
        return newSelf;
    }

    if (m_needsTransitionTable) {
        size_t r = searchTransitionTable(name, desc);
        if (r != SIZE_MAX) {
            return m_transitionTable[r].m_structure;
        }
    } else {
        ASSERT(m_transitionTable.size() == 0);
    }

    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    ObjectStructureItemVector newProperties(m_properties, newItem);
    ObjectStructure* newObjectStructure;

    if (newProperties.size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE)
        newObjectStructure = new ObjectStructureWithFastAccess(state, std::move(newProperties), m_hasIndexPropertyName | nameIsIndexString);
    else
        newObjectStructure = new ObjectStructure(state, std::move(newProperties), m_needsTransitionTable, m_hasIndexPropertyName | nameIsIndexString);

    if (m_needsTransitionTable && !newObjectStructure->isStructureWithFastAccess()) {
        ObjectStructureTransitionItem newTransitionItem(name, desc, newObjectStructure);
        newObjectStructure->m_isProtectedByTransitionTable = true;
        m_transitionTable.pushBack(newTransitionItem);
    }

    return newObjectStructure;
}

inline ObjectStructure* ObjectStructure::removeProperty(ExecutionState& state, size_t pIndex)
{
    if (m_isStructureWithFastAccess) {
        m_properties.erase(pIndex);
        propertyNameMap().clear();
        ObjectStructureWithFastAccess* self = (ObjectStructureWithFastAccess*)this;
        self->buildPropertyNameMap();
        ObjectStructureWithFastAccess* newSelf = new ObjectStructureWithFastAccess(state, *self);
        return newSelf;
    }

    ObjectStructureItemVector newProperties;
    newProperties.resizeWithUninitializedValues(m_properties.size() - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | m_properties[i].m_propertyName.isIndexString();
        newProperties[newIdx].m_propertyName = m_properties[i].m_propertyName;
        newProperties[newIdx].m_descriptor = m_properties[i].m_descriptor;
        newIdx++;
    }

    return new ObjectStructure(state, std::move(newProperties), false, hasIndexString);
}

inline ObjectStructure* ObjectStructure::escapeTransitionMode(ExecutionState& state)
{
    if (m_isStructureWithFastAccess) {
        return this;
    }

    ASSERT(inTransitionMode());
    ObjectStructureItemVector newItem(m_properties);
    return new ObjectStructure(state, std::move(newItem), false, m_hasIndexPropertyName);
}

inline ObjectStructure* ObjectStructure::convertToWithFastAccess(ExecutionState& state)
{
    ASSERT(!m_isStructureWithFastAccess);
    ObjectStructureItemVector v = m_properties;
    return new ObjectStructureWithFastAccess(state, std::move(v), m_hasIndexPropertyName);
}
}

namespace std {

template <>
struct is_fundamental<Escargot::ObjectStructureItem> : public true_type {
};

template <>
struct is_fundamental<Escargot::ObjectStructureTransitionItem> : public true_type {
};
}
#endif
