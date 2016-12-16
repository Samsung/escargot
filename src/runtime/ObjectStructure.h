#ifndef __EscargotObjectStructure__
#define __EscargotObjectStructure__

#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"
#include "runtime/PropertyName.h"
#include "runtime/ObjectPropertyDescriptor.h"

namespace Escargot {

class ObjectStructure;

struct ObjectStructureItem : public gc {
    ObjectStructureItem(const PropertyName& as, const ObjectPropertyDescriptor& desc)
        : m_propertyName(as)
        , m_descriptor(desc)
    {
    }

    PropertyName m_propertyName;
    ObjectPropertyDescriptor m_descriptor;
};

struct ObjectStructureTransitionItem : public gc {
    PropertyName m_propertyName;
    ObjectPropertyDescriptor m_descriptor;
    ObjectStructure* m_structure;

    ObjectStructureTransitionItem(const PropertyName& as, const ObjectPropertyDescriptor& desc, ObjectStructure* structure)
        : m_propertyName(as)
        , m_descriptor(desc)
        , m_structure(structure)
    {
    }
};

typedef Vector<ObjectStructureItem, gc_malloc_atomic_ignore_off_page_allocator<ObjectStructureItem>> ObjectStructureItemVector;
typedef Vector<ObjectStructureTransitionItem, gc_malloc_ignore_off_page_allocator<ObjectStructureTransitionItem>> ObjectStructureTransitionTableVector;

#define ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE 96

class ObjectStructure : public gc {
    friend class Object;

public:
    ObjectStructure(ExecutionState& state, bool needsTransitionTable = true)
    {
        m_needsTransitionTable = needsTransitionTable;
        m_hasIndexPropertyName = false;
        m_isStructureWithFastAccess = false;
        m_version = 0;
    }

    ObjectStructure(ExecutionState& state, ObjectStructureItemVector&& properties, bool needsTransitionTable, bool hasIndexPropertyName)
        : m_properties(properties)
    {
        m_needsTransitionTable = needsTransitionTable;
        m_hasIndexPropertyName = hasIndexPropertyName;
        m_isStructureWithFastAccess = false;
        m_version = 0;
    }

    size_t findProperty(ExecutionState& state, String* propertyName)
    {
        PropertyName name(state, propertyName);
        return findProperty(name);
    }

    size_t findProperty(ExecutionState& state, const PropertyName& name)
    {
        return findProperty(name);
    }

    ObjectStructureItem readProperty(ExecutionState& state, String* propertyName)
    {
        return readProperty(state, findProperty(state, propertyName));
    }

    const ObjectStructureItem& readProperty(ExecutionState& state, size_t idx)
    {
        return m_properties[idx];
    }

    ObjectStructure* addProperty(ExecutionState& state, String* propertyName, const ObjectPropertyDescriptor& desc)
    {
        PropertyName name(state, propertyName);
        return addProperty(state, name, desc);
    }

    ObjectStructure* addProperty(ExecutionState& state, const PropertyName& name, const ObjectPropertyDescriptor& desc);
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

    size_t propertyCount() const
    {
        return m_properties.size();
    }

    size_t version() const
    {
        return m_version;
    }

protected:
    bool m_needsTransitionTable;
    bool m_hasIndexPropertyName;
    bool m_isStructureWithFastAccess;
    size_t m_version;
    ObjectStructureItemVector m_properties;
    ObjectStructureTransitionTableVector m_transitionTable;

    size_t searchTransitionTable(const PropertyName& s, const ObjectPropertyDescriptor& desc)
    {
        ASSERT(m_needsTransitionTable);
        for (size_t i = 0; i < m_transitionTable.size(); i++) {
            if (m_transitionTable[i].m_descriptor == desc && m_transitionTable[i].m_propertyName == s) {
                return i;
            }
        }
        return SIZE_MAX;
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

public:
    ObjectStructureWithFastAccess(ExecutionState& state)
        : ObjectStructure(state, false)
    {
        m_isStructureWithFastAccess = true;
        buildPropertyNameMap();
    }

    ObjectStructureWithFastAccess(ExecutionState& state, ObjectStructureItemVector&& properties, bool hasIndexPropertyName)
        : ObjectStructure(state, std::move(properties), false, hasIndexPropertyName)
    {
        m_isStructureWithFastAccess = true;
        buildPropertyNameMap();
    }

    void buildPropertyNameMap()
    {
        m_propertyNameMap.clear();
        size_t len = m_properties.size();
        for (size_t i = 0; i < len; i++) {
            m_propertyNameMap.insert(std::make_pair(m_properties[i].m_propertyName, i));
        }
    }

    PropertyNameMap m_propertyNameMap;
};

inline PropertyNameMap& ObjectStructure::propertyNameMap()
{
    ASSERT(m_isStructureWithFastAccess);
    ObjectStructureWithFastAccess* self = (ObjectStructureWithFastAccess*)this;
    return self->m_propertyNameMap;
}

inline ObjectStructure* ObjectStructure::addProperty(ExecutionState& state, const PropertyName& name, const ObjectPropertyDescriptor& desc)
{
    ObjectStructureItem newItem(name, desc);
    if (m_isStructureWithFastAccess) {
        m_properties.pushBack(newItem);
        m_hasIndexPropertyName = m_hasIndexPropertyName | isIndexString(name.string());
        propertyNameMap().insert(std::make_pair(name, m_properties.size() - 1));
        ObjectStructureWithFastAccess* self = (ObjectStructureWithFastAccess*)this;
        self->m_version++;
        if (UNLIKELY(self->m_version == SIZE_MAX))
            self->m_version = 0;
        return this;
    }

    ASSERT(name.string()->length());
    if (m_needsTransitionTable) {
        size_t r = searchTransitionTable(name, desc);
        if (r != SIZE_MAX) {
            return m_transitionTable[r].m_structure;
        }
    } else {
        ASSERT(m_transitionTable.size() == 0);
    }

    ObjectStructureItemVector newProperties(m_properties, newItem);
    ObjectStructure* newObjectStructure;

    if (newProperties.size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE)
        newObjectStructure = new ObjectStructureWithFastAccess(state, std::move(newProperties), m_hasIndexPropertyName | isIndexString(name.string()));
    else
        newObjectStructure = new ObjectStructure(state, std::move(newProperties), m_needsTransitionTable, m_hasIndexPropertyName | isIndexString(name.string()));

    if (m_needsTransitionTable && !newObjectStructure->isStructureWithFastAccess()) {
        ObjectStructureTransitionItem newTransitionItem(name, desc, newObjectStructure);
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
        self->m_version++;
        if (UNLIKELY(self->m_version == SIZE_MAX))
            self->m_version = 0;
        return this;
    }

    ObjectStructureItemVector newProperties;
    newProperties.resizeWithUninitializedValues(m_properties.size() - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | isIndexString(m_properties[i].m_propertyName.string());
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

#endif
