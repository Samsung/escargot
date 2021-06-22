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

#include "Escargot.h"
#include "ObjectPrivateMemberStructure.h"

namespace Escargot {

Optional<size_t> ObjectPrivateMemberStructure::findProperty(AtomicString name)
{
    for (size_t i = 0; i < m_properties.size(); i++) {
        if (m_properties[i].propertyName() == name) {
            return i;
        }
    }
    return nullptr;
}

ObjectPrivateMemberStructureItem ObjectPrivateMemberStructure::readProperty(size_t idx)
{
    return m_properties[idx];
}

ObjectPrivateMemberStructure* ObjectPrivateMemberStructure::addProperty(const ObjectPrivateMemberStructureItem& newItem)
{
    ASSERT(!findProperty(newItem.propertyName()).hasValue());

    auto as = newItem.propertyName();
    for (size_t i = 0; i < m_transitionInfo.size(); i++) {
        if (m_transitionInfo[i].m_propertyInfo.propertyName() == as
            && m_transitionInfo[i].m_propertyInfo.kind() == newItem.kind()) {
            return m_transitionInfo[i].m_structure;
        }
    }

    ObjectPrivateMemberStructure* newStructure = new ObjectPrivateMemberStructure();
    newStructure->m_properties.resize(m_properties.size() + 1);

    memcpy(newStructure->m_properties.data(), m_properties.data(), m_properties.size() * sizeof(ObjectPrivateMemberStructureItem));
    newStructure->m_properties[m_properties.size()] = newItem;

    m_transitionInfo.pushBack(ObjectPrivateMemberStructureTransitionVectorItem(newItem, newStructure));

    return newStructure;
}

} // namespace Escargot
