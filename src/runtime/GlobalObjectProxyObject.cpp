/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "GlobalObjectProxyObject.h"

namespace Escargot {

ObjectGetResult GlobalObjectProxyObject::get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    }
    return m_target->get(state, P, receiver);
}

bool GlobalObjectProxyObject::set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, Optional<AtomicString>());
    }
    return m_target->Object::set(state, P, v, receiver);
}

ObjectHasPropertyResult GlobalObjectProxyObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    }
    return m_target->hasProperty(state, P);
}

ObjectGetResult GlobalObjectProxyObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    }
    return m_target->getOwnProperty(state, P);
}

bool GlobalObjectProxyObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, Optional<AtomicString>());
    }
    return m_target->defineOwnProperty(state, P, desc);
}

bool GlobalObjectProxyObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    if (!P.isUIntType() && P.objectStructurePropertyName().hasAtomicString()) {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, P.objectStructurePropertyName().asAtomicString());
    } else {
        checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, Optional<AtomicString>());
    }
    return m_target->deleteOwnProperty(state, P);
}

Value GlobalObjectProxyObject::getPrototype(ExecutionState& state)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    return m_target->getPrototype(state);
}

Object* GlobalObjectProxyObject::getPrototypeObject(ExecutionState& state)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    return m_target->getPrototypeObject(state);
}

bool GlobalObjectProxyObject::setPrototype(ExecutionState& state, const Value& proto)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, Optional<AtomicString>());
    return m_target->Object::setPrototype(state, proto);
}

Object::OwnPropertyKeyVector GlobalObjectProxyObject::ownPropertyKeys(ExecutionState& state)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    return m_target->ownPropertyKeys(state);
}

void GlobalObjectProxyObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    m_target->enumeration(state, callback, data, shouldSkipSymbolKey);
}

bool GlobalObjectProxyObject::isExtensible(ExecutionState& state)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Read, Optional<AtomicString>());
    return m_target->isExtensible(state);
}

bool GlobalObjectProxyObject::preventExtensions(ExecutionState& state)
{
    checkSecurity(state, GlobalObjectProxyObjectRef::AccessOperationType::Write, Optional<AtomicString>());
    return m_target->preventExtensions(state);
}


} // namespace Escargot
