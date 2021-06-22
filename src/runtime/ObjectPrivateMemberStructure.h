/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotObjectPrivateMemberStructure__
#define __EscargotObjectPrivateMemberStructure__

#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"

namespace Escargot {

class ObjectPrivateMemberStructure;

enum class ObjectPrivateMemberStructureItemKind {
    Field = 0,
    GetterSetter = 1,
    Method = 3
};

struct ObjectPrivateMemberStructureItem {
    ObjectPrivateMemberStructureItem()
        : ObjectPrivateMemberStructureItem(AtomicString(), ObjectPrivateMemberStructureItemKind::Field)
    {
    }

    ObjectPrivateMemberStructureItem(const AtomicString& as, ObjectPrivateMemberStructureItemKind kind)
    {
#if defined(ESCARGOT_SMALL_CONFIG)
        m_payload = (size_t)as.string() + (size_t)kind;
#else
        m_propetyName = as;
        m_kind = kind;
#endif
    }

    AtomicString propertyName() const
    {
#if defined(ESCARGOT_SMALL_CONFIG)
        size_t p = m_payload;
        // remove last 2 bits
#if defined(ESCARGOT_32)
        p = p & ~3UL;
#else
        p = p & ~3ULL;
#endif
        return AtomicString::fromPayload((void*)p);
#else
        return m_propetyName;
#endif
    }

    ObjectPrivateMemberStructureItemKind kind() const
    {
#if defined(ESCARGOT_SMALL_CONFIG)
        return (ObjectPrivateMemberStructureItemKind)(m_payload & 3);
#else
        return m_kind;
#endif
    }

private:
#if defined(ESCARGOT_SMALL_CONFIG)
    size_t m_payload;
#else
    AtomicString m_propetyName;
    ObjectPrivateMemberStructureItemKind m_kind;
#endif
};

struct ObjectPrivateMemberStructureTransitionVectorItem {
    ObjectPrivateMemberStructureItem m_propertyInfo;
    ObjectPrivateMemberStructure* m_structure;

    ObjectPrivateMemberStructureTransitionVectorItem(const ObjectPrivateMemberStructureItem& info, ObjectPrivateMemberStructure* structure)
        : m_propertyInfo(info)
        , m_structure(structure)
    {
    }
};

typedef TightVector<ObjectPrivateMemberStructureItem,
                    GCUtil::gc_malloc_atomic_allocator<ObjectPrivateMemberStructureItem>>
    ObjectPrivateMemberStructureItemVector;

typedef Vector<ObjectPrivateMemberStructureTransitionVectorItem,
               GCUtil::gc_malloc_allocator<ObjectPrivateMemberStructureTransitionVectorItem>>
    ObjectPrivateMemberStructureTransitionItemVector;

class ObjectPrivateMemberStructure : public gc {
public:
    Optional<size_t> findProperty(AtomicString name);
    ObjectPrivateMemberStructureItem readProperty(size_t idx);
    size_t propertyCount() const
    {
        return m_properties.size();
    }
    ObjectPrivateMemberStructure* addProperty(const ObjectPrivateMemberStructureItem& name);

private:
    ObjectPrivateMemberStructureItemVector m_properties;
    ObjectPrivateMemberStructureTransitionItemVector m_transitionInfo;
};
} // namespace Escargot

#endif
